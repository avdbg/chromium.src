// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/external_arc/message_center/arc_notification_manager.h"
#include "ash/public/cpp/message_center/arc_notification_manager_delegate.h"
#include "ash/public/cpp/message_center/arc_notifications_host_initializer.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/arc_apps.h"
#include "chrome/browser/apps/app_service/arc_apps_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper_factory.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_handler.h"
#include "chrome/browser/extensions/api/notifications/notifications_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/profile_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/connection_holder.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using apps::mojom::OptionalBool;
using extensions::Extension;
using extensions::ExtensionNotificationDisplayHelper;
using extensions::ExtensionNotificationDisplayHelperFactory;

namespace {

constexpr char kTestAppName1[] = "Test ARC App1";
constexpr char kTestAppName2[] = "Test ARC App2";
constexpr char kTestAppPackage1[] = "test.arc.app1.package";
constexpr char kTestAppPackage2[] = "test.arc.app2.package";
constexpr char kTestAppActivity1[] = "test.arc.app1.package.activity";
constexpr char kTestAppActivity2[] = "test.arc.app2.package.activity";

std::string GetTestAppId(const std::string& package_name,
                         const std::string& activity) {
  return ArcAppListPrefs::GetAppId(package_name, activity);
}

std::vector<arc::mojom::AppInfoPtr> GetTestAppsList() {
  std::vector<arc::mojom::AppInfoPtr> apps;

  arc::mojom::AppInfoPtr app(arc::mojom::AppInfo::New());
  app->name = kTestAppName1;
  app->package_name = kTestAppPackage1;
  app->activity = kTestAppActivity1;
  app->sticky = false;
  apps.push_back(std::move(app));

  app = arc::mojom::AppInfo::New();
  app->name = kTestAppName2;
  app->package_name = kTestAppPackage2;
  app->activity = kTestAppActivity2;
  app->sticky = false;
  apps.push_back(std::move(app));

  return apps;
}

OptionalBool HasBadge(Profile* profile, const std::string& app_id) {
  auto has_badge = OptionalBool::kUnknown;
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->FlushMojoCallsForTesting();
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&has_badge](const apps::AppUpdate& update) {
        has_badge = update.HasBadge();
      });
  return has_badge;
}

void RemoveNotification(Profile* profile, const std::string& notification_id) {
  const std::string profile_notification_id =
      ProfileNotification::GetProfileNotificationId(
          notification_id, NotificationUIManager::GetProfileID(profile));
  message_center::MessageCenter::Get()->RemoveNotification(
      profile_notification_id, true);
}

void UninstallApp(Profile* profile, const std::string& app_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->UninstallSilently(app_id, apps::mojom::UninstallSource::kUser);
  proxy->FlushMojoCallsForTesting();
}

}  // namespace

class AppNotificationsExtensionApiTest : public extensions::ExtensionApiTest {
 public:
  const Extension* LoadExtensionAndWait(const std::string& test_name) {
    base::FilePath extdir = test_data_dir_.AppendASCII(test_name);
    content::WindowedNotificationObserver page_created(
        extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
        content::NotificationService::AllSources());
    const extensions::Extension* extension = LoadExtension(extdir);
    if (extension) {
      page_created.Wait();
    }
    return extension;
  }

  const Extension* LoadAppWithWindowState(const std::string& test_name) {
    const std::string& create_window_options =
        base::StringPrintf("{\"state\":\"normal\"}");
    base::FilePath extdir = test_data_dir_.AppendASCII(test_name);
    const extensions::Extension* extension = LoadExtension(extdir);
    EXPECT_TRUE(extension);

    ExtensionTestMessageListener launched_listener("launched", true);
    apps::AppServiceProxyFactory::GetForProfile(profile())->Launch(
        extension->id(), ui::EF_SHIFT_DOWN,
        apps::mojom::LaunchSource::kFromTest);
    EXPECT_TRUE(launched_listener.WaitUntilSatisfied());
    launched_listener.Reply(create_window_options);

    return extension;
  }

  ExtensionNotificationDisplayHelper* GetDisplayHelper() {
    return ExtensionNotificationDisplayHelperFactory::GetForProfile(profile());
  }

 protected:
  // Returns the notification that's being displayed for |extension|, or nullptr
  // when the notification count is not equal to one. It's not safe to rely on
  // the Notification pointer after closing the notification, but a copy can be
  // made to continue to be able to access the underlying information.
  message_center::Notification* GetNotificationForExtension(
      const extensions::Extension* extension) {
    DCHECK(extension);

    std::set<std::string> notifications =
        GetDisplayHelper()->GetNotificationIdsForExtension(extension->url());
    if (notifications.size() != 1)
      return nullptr;

    return GetDisplayHelper()->GetByNotificationId(*notifications.begin());
  }
};

IN_PROC_BROWSER_TEST_F(AppNotificationsExtensionApiTest,
                       AddAndRemoveNotification) {
  // Load the permission app which should not generate notifications.
  const Extension* extension1 =
      LoadExtensionAndWait("notifications/api/permission");
  ASSERT_TRUE(extension1);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), extension1->id()));

  // Load the basic app to generate a notification.
  ExtensionTestMessageListener notification_created_listener("created", false);
  const Extension* extension2 =
      LoadAppWithWindowState("notifications/api/basic_app");
  ASSERT_TRUE(extension2);
  ASSERT_TRUE(notification_created_listener.WaitUntilSatisfied());

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), extension1->id()));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), extension2->id()));

  message_center::Notification* notification =
      GetNotificationForExtension(extension2);
  ASSERT_TRUE(notification);

  RemoveNotification(profile(), notification->id());
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), extension1->id()));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), extension2->id()));
}

IN_PROC_BROWSER_TEST_F(AppNotificationsExtensionApiTest,
                       InstallAndUninstallApp) {
  // Load the permission app which should not generate notifications.
  const Extension* extension1 =
      LoadExtensionAndWait("notifications/api/permission");
  ASSERT_TRUE(extension1);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), extension1->id()));

  // Load the basic app to generate a notification.
  ExtensionTestMessageListener notification_created_listener1("created", false);
  const Extension* extension2 =
      LoadAppWithWindowState("notifications/api/basic_app");
  ASSERT_TRUE(extension2);
  ASSERT_TRUE(notification_created_listener1.WaitUntilSatisfied());

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), extension1->id()));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), extension2->id()));

  // Uninstall the basic app.
  UninstallApp(profile(), extension2->id());

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), extension1->id()));

  // Re-load the basic app to generate a notification again.
  ExtensionTestMessageListener notification_created_listener2("created", false);
  const Extension* extension3 =
      LoadAppWithWindowState("notifications/api/basic_app");
  ASSERT_TRUE(extension3);
  ASSERT_TRUE(notification_created_listener2.WaitUntilSatisfied());

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), extension1->id()));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), extension3->id()));

  // Remove the notification.
  message_center::Notification* notification =
      GetNotificationForExtension(extension3);
  ASSERT_TRUE(notification);

  RemoveNotification(profile(), notification->id());
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), extension1->id()));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), extension3->id()));
}

class AppNotificationsWebNotificationTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  AppNotificationsWebNotificationTest() = default;
  ~AppNotificationsWebNotificationTest() override = default;

  void SetUp() override {
    base::test::ScopedFeatureList scoped_feature_list_;
    scoped_feature_list_.InitAndDisableFeature(
        features::kDesktopPWAsAttentionBadgingCrOS);
    extensions::PlatformAppBrowserTest::SetUp();
  }

  // extensions::PlatformAppBrowserTest:
  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  std::string CreateWebApp(const GURL& url, const GURL& scope) const {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = url;
    web_app_info->scope = scope;
    std::string app_id =
        web_app::InstallWebApp(browser()->profile(), std::move(web_app_info));
    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.StartWatchingNewWebContents();
    web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
    navigation_observer.WaitForNavigationFinished();
    return app_id;
  }

  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& notification_id,
      const GURL& origin) {
    return std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
        base::string16(), base::string16(), gfx::Image(),
        base::UTF8ToUTF16(origin.host()), origin,
        message_center::NotifierId(origin),
        message_center::RichNotificationData(), nullptr);
  }

  void UninstallWebApp(const std::string& app_id) const {
    web_app::UninstallWebApp(browser()->profile(), app_id);
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->FlushMojoCallsForTesting();
  }

  GURL GetOrigin() const { return https_server_.GetURL("app.com", "/"); }

  GURL GetUrl1() const {
    return https_server_.GetURL("app.com", "/ssl/google.html");
  }

  GURL GetScope1() const { return https_server_.GetURL("app.com", "/ssl/"); }

  GURL GetUrl2() const {
    return https_server_.GetURL("app.com", "/google/google.html");
  }

  GURL GetScope2() const { return https_server_.GetURL("app.com", "/google/"); }

  GURL GetUrl3() const {
    return https_server_.GetURL("app1.com", "/google/google.html");
  }

  GURL GetScope3() const {
    return https_server_.GetURL("app1.com", "/google/");
  }

 private:
  // For mocking a secure site.
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(AppNotificationsWebNotificationTest,
                       AddAndRemovePersistentNotification) {
  std::string app_id1 = CreateWebApp(GetUrl1(), GetScope1());
  std::string app_id2 = CreateWebApp(GetUrl2(), GetScope2());
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  const GURL origin = GetOrigin();
  std::string notification_id = "notification-id1";
  auto notification = CreateNotification(notification_id, origin);

  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = GetScope1();

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_PERSISTENT, *notification,
      std::move(metadata));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  NotificationDisplayService::GetForProfile(profile())->Close(
      NotificationHandler::Type::WEB_PERSISTENT, notification_id);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  notification_id = "notification-id2";
  notification = CreateNotification(notification_id, origin);
  metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = GetScope2();

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_PERSISTENT, *notification,
      std::move(metadata));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  NotificationDisplayService::GetForProfile(profile())->Close(
      NotificationHandler::Type::WEB_PERSISTENT, notification_id);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));
}

IN_PROC_BROWSER_TEST_F(AppNotificationsWebNotificationTest,
                       PersistentNotificationWhenInstallAndUninstallApp) {
  // Send a notification before installing apps.
  const GURL origin = GetOrigin();
  std::string notification_id = "notification-id2";
  auto notification = CreateNotification(notification_id, origin);

  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = GetScope2();

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_PERSISTENT, *notification,
      std::move(metadata));

  // Install apps, and verify the notification badge is not set.
  std::string app_id1 = CreateWebApp(GetUrl1(), GetScope1());
  std::string app_id2 = CreateWebApp(GetUrl2(), GetScope2());
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  // Remove the notification. It should not affect the notification badge.
  NotificationDisplayService::GetForProfile(profile())->Close(
      NotificationHandler::Type::WEB_PERSISTENT, notification_id);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  // Send a notification for the installed app 2.
  notification_id = "notification-id3";
  notification = CreateNotification(notification_id, origin);

  metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = GetScope2();

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_PERSISTENT, *notification,
      std::move(metadata));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  // Uninstall the app 2.
  UninstallApp(profile(), app_id2);

  // Re-install the app 2.
  app_id2 = CreateWebApp(GetUrl2(), GetScope2());
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  // Remove the notification.
  NotificationDisplayService::GetForProfile(profile())->Close(
      NotificationHandler::Type::WEB_PERSISTENT, notification_id);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  // Resend the notifications for both apps.
  std::string notification_id1 = "notification-id4";
  notification = CreateNotification(notification_id1, origin);

  metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = GetScope1();

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_PERSISTENT, *notification,
      std::move(metadata));

  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  std::string notification_id2 = "notification-id5";
  notification = CreateNotification(notification_id2, origin);

  metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = GetScope2();

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_PERSISTENT, *notification,
      std::move(metadata));

  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  // Remove notifications.
  NotificationDisplayService::GetForProfile(profile())->Close(
      NotificationHandler::Type::WEB_PERSISTENT, notification_id1);

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  NotificationDisplayService::GetForProfile(profile())->Close(
      NotificationHandler::Type::WEB_PERSISTENT, notification_id2);

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));
}

IN_PROC_BROWSER_TEST_F(AppNotificationsWebNotificationTest,
                       AddAndRemoveNonPersistentNotificationForOneApp) {
  base::HistogramTester histogram_tester;

  const GURL origin = GetOrigin();
  std::string app_id1 = CreateWebApp(GetUrl1(), GetScope1());
  std::string app_id3 = CreateWebApp(GetUrl3(), GetScope3());

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));

  const std::string notification_id = "notification-id";
  auto notification = CreateNotification(notification_id, origin);

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_NON_PERSISTENT, *notification,
      /*metadata=*/nullptr);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));

  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Apps.NumberOfAppsForNotification", false, 1);

  RemoveNotification(profile(), notification_id);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));
}

IN_PROC_BROWSER_TEST_F(AppNotificationsWebNotificationTest,
                       AddAndRemoveNonPersistentNotification) {
  base::HistogramTester histogram_tester;

  const GURL origin = GetOrigin();
  std::string app_id1 = CreateWebApp(GetUrl1(), GetScope1());
  std::string app_id2 = CreateWebApp(GetUrl2(), GetScope2());
  std::string app_id3 = CreateWebApp(GetUrl3(), GetScope3());

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));

  const std::string notification_id = "notification-id";
  auto notification = CreateNotification(notification_id, origin);

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_NON_PERSISTENT, *notification,
      /*metadata=*/nullptr);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));

  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Apps.NumberOfAppsForNotification", true, 1);

  RemoveNotification(profile(), notification_id);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));
}

IN_PROC_BROWSER_TEST_F(AppNotificationsWebNotificationTest,
                       NonPersistentNotificationWhenInstallAndUninstallApp) {
  base::HistogramTester histogram_tester;

  // Send the notification 1 before installing apps.
  const GURL origin = GetOrigin();
  const std::string notification_id1 = "notification-id1";
  auto notification = CreateNotification(notification_id1, origin);

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_NON_PERSISTENT, *notification,
      /*metadata=*/nullptr);

  // Install apps.
  std::string app_id1 = CreateWebApp(GetUrl1(), GetScope1());
  std::string app_id2 = CreateWebApp(GetUrl2(), GetScope2());
  std::string app_id3 = CreateWebApp(GetUrl3(), GetScope3());

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));

  histogram_tester.ExpectTotalCount("ChromeOS.Apps.NumberOfAppsForNotification",
                                    0);

  // Send the notification 2.
  const std::string notification_id2 = "notification-id2";
  notification = CreateNotification(notification_id2, origin);

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_NON_PERSISTENT, *notification,
      /*metadata=*/nullptr);

  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));

  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Apps.NumberOfAppsForNotification", true, 1);

  // Uninstall the app 1. The notification badge for app 2 and app 3 should not
  // be affected.
  UninstallWebApp(app_id1);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));

  // Re-install the app 1.
  app_id1 = CreateWebApp(GetUrl1(), GetScope1());
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));

  // Send the notification 3.
  const std::string notification_id3 = "notification-id3";
  notification = CreateNotification(notification_id3, origin);

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_NON_PERSISTENT, *notification,
      /*metadata=*/nullptr);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));

  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Apps.NumberOfAppsForNotification", true, 2);

  // Remove the notification 3
  RemoveNotification(profile(), notification_id3);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));

  // Remove the notification 1
  RemoveNotification(profile(), notification_id1);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));

  // Remove the notification 2
  RemoveNotification(profile(), notification_id2);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id3));
}

class WebAppBadgingTest : public AppNotificationsWebNotificationTest,
                          public ::testing::WithParamInterface<std::string> {
 protected:
  WebAppBadgingTest() = default;
  ~WebAppBadgingTest() override = default;

  void SetUp() override {
    base::test::ScopedFeatureList scoped_feature_list_;
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kDesktopPWAsAttentionBadgingCrOS,
        {{"badge-source", GetParam()}});
    extensions::PlatformAppBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_P(WebAppBadgingTest, SetAndClearBadgeWithApi) {
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManager* badge_manager_ =
      badging::BadgeManagerFactory::GetForProfile(profile());

  std::string app_id = CreateWebApp(GetUrl1(), GetScope1());
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id));

  badge_manager_->SetBadgeForTesting(app_id, 1, &test_recorder);
  if (GetParam() ==
      switches::kDesktopPWAsAttentionBadgingCrOSNotificationsOnly) {
    ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id));
  } else {
    ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id));
  }

  badge_manager_->ClearBadgeForTesting(app_id, &test_recorder);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id));
}

IN_PROC_BROWSER_TEST_P(WebAppBadgingTest,
                       SetAndClearBadgeWithApiAndNotifications) {
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManager* badge_manager_ =
      badging::BadgeManagerFactory::GetForProfile(profile());

  std::string app_id = CreateWebApp(GetUrl1(), GetScope1());
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id));

  badge_manager_->SetBadgeForTesting(app_id, 1, &test_recorder);
  if (GetParam() ==
      switches::kDesktopPWAsAttentionBadgingCrOSNotificationsOnly) {
    ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id));
  } else {
    ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id));
  }

  const std::string notification_id = "notification-id";
  auto notification = CreateNotification(notification_id, GetOrigin());

  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = GetScope1();

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_PERSISTENT, *notification,
      std::move(metadata));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id));

  badge_manager_->ClearBadgeForTesting(app_id, &test_recorder);
  if (GetParam() == switches::kDesktopPWAsAttentionBadgingCrOSApiOnly) {
    ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id));
  } else {
    ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id));
  }

  NotificationDisplayService::GetForProfile(profile())->Close(
      NotificationHandler::Type::WEB_PERSISTENT, notification_id);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppBadgingTest,
    ::testing::Values(
        switches::kDesktopPWAsAttentionBadgingCrOSApiOnly,
        switches::kDesktopPWAsAttentionBadgingCrOSApiAndNotifications,
        switches::kDesktopPWAsAttentionBadgingCrOSNotificationsOnly));

class FakeArcNotificationManagerDelegate
    : public ash::ArcNotificationManagerDelegate {
 public:
  FakeArcNotificationManagerDelegate() = default;
  ~FakeArcNotificationManagerDelegate() override = default;

  // ArcNotificationManagerDelegate:
  bool IsPublicSessionOrKiosk() const override { return false; }
  void ShowMessageCenter() override {}
  void HideMessageCenter() override {}
};

class AppNotificationsArcNotificationTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  // extensions::PlatformAppBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    extensions::PlatformAppBrowserTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    arc::SetArcPlayStoreEnabledForProfile(profile(), true);

    // This ensures app_prefs()->GetApp() below never returns nullptr.
    base::RunLoop run_loop;
    app_prefs()->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    StartInstance();
    arc_notification_manager_ = std::make_unique<ash::ArcNotificationManager>();
    arc_notification_manager_->Init(
        std::make_unique<FakeArcNotificationManagerDelegate>(),
        EmptyAccountId(), message_center::MessageCenter::Get());

    ash::ArcNotificationsHostInitializer::Observer* observer =
        apps::ArcAppsFactory::GetInstance()->GetForProfile(profile());
    observer->OnSetArcNotificationsInstance(arc_notification_manager_.get());
  }

  void TearDownOnMainThread() override {
    arc_notification_manager_.reset();
    StopInstance();
    base::RunLoop().RunUntilIdle();

    extensions::PlatformAppBrowserTest::TearDownOnMainThread();
  }

  void InstallTestApps() {
    app_host()->OnAppListRefreshed(GetTestAppsList());

    SendPackageAdded(kTestAppPackage1, false);
    SendPackageAdded(kTestAppPackage2, false);
  }

  void SendPackageAdded(const std::string& package_name, bool package_synced) {
    auto package_info = arc::mojom::ArcPackageInfo::New();
    package_info->package_name = package_name;
    package_info->package_version = 1;
    package_info->last_backup_android_id = 1;
    package_info->last_backup_time = 1;
    package_info->sync = package_synced;
    package_info->system = false;
    app_instance_->SendPackageAdded(std::move(package_info));
    base::RunLoop().RunUntilIdle();
  }

  void SendPackageRemoved(const std::string& package_name) {
    app_host()->OnPackageRemoved(package_name);

    // Ensure async callbacks from the resulting observer calls are run.
    base::RunLoop().RunUntilIdle();
  }

  void StartInstance() {
    app_instance_ = std::make_unique<arc::FakeAppInstance>(app_host());
    arc_bridge_service()->app()->SetInstance(app_instance_.get());
  }

  void StopInstance() {
    if (app_instance_)
      arc_bridge_service()->app()->CloseInstance(app_instance_.get());
    arc_session_manager()->Shutdown();
  }

  void CreateNotificationWithKey(const std::string& key,
                                 const std::string& package_name) {
    auto data = arc::mojom::ArcNotificationData::New();
    data->key = key;
    data->title = "TITLE";
    data->message = "MESSAGE";
    data->package_name = package_name;
    arc_notification_manager_->OnNotificationPosted(std::move(data));
  }

  void RemoveNotificationWithKey(const std::string& key) {
    arc_notification_manager_->OnNotificationRemoved(key);
  }

  ArcAppListPrefs* app_prefs() { return ArcAppListPrefs::Get(profile()); }

  // Returns as AppHost interface in order to access to private implementation
  // of the interface.
  arc::mojom::AppHost* app_host() { return app_prefs(); }

 private:
  arc::ArcSessionManager* arc_session_manager() {
    return arc::ArcSessionManager::Get();
  }

  arc::ArcBridgeService* arc_bridge_service() {
    return arc::ArcServiceManager::Get()->arc_bridge_service();
  }

  std::unique_ptr<ash::ArcNotificationManager> arc_notification_manager_;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
};

IN_PROC_BROWSER_TEST_F(AppNotificationsArcNotificationTest,
                       AddAndRemoveNotification) {
  // Install app to remember existing apps.
  InstallTestApps();
  const std::string app_id1 = GetTestAppId(kTestAppPackage1, kTestAppActivity1);
  const std::string app_id2 = GetTestAppId(kTestAppPackage2, kTestAppActivity2);

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  const std::string notification_key1 = "notification_key1";
  CreateNotificationWithKey(notification_key1, kTestAppPackage1);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  const std::string notification_key2 = "notification_key2";
  CreateNotificationWithKey(notification_key2, kTestAppPackage2);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  RemoveNotificationWithKey(notification_key1);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  RemoveNotificationWithKey(notification_key2);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));
}

IN_PROC_BROWSER_TEST_F(AppNotificationsArcNotificationTest,
                       MultipleNotificationsWhenUninstallApp) {
  // Install apps to remember existing apps.
  InstallTestApps();
  const std::string app_id1 = GetTestAppId(kTestAppPackage1, kTestAppActivity1);
  const std::string app_id2 = GetTestAppId(kTestAppPackage2, kTestAppActivity2);

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  // Sent 2 notifications for the app 1.
  const std::string notification_key1 = "notification_key1";
  CreateNotificationWithKey(notification_key1, kTestAppPackage1);

  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  const std::string notification_key2 = "notification_key2";
  CreateNotificationWithKey(notification_key2, kTestAppPackage1);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  // Remove the app 1.
  SendPackageRemoved(app_id1);

  // Sent 1 notification for the app 2.
  const std::string notification_key3 = "notification_key3";
  CreateNotificationWithKey(notification_key3, kTestAppPackage2);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  // Remove the notification for the app 2.
  RemoveNotificationWithKey(notification_key3);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  // Sent 2 notifications for the app 2.
  const std::string notification_key4 = "notification_key4";
  CreateNotificationWithKey(notification_key4, kTestAppPackage2);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  const std::string notification_key5 = "notification_key5";
  CreateNotificationWithKey(notification_key5, kTestAppPackage1);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  // Remove notifications for the app2.
  RemoveNotificationWithKey(notification_key5);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  RemoveNotificationWithKey(notification_key4);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  // Remove the app 2.
  SendPackageRemoved(app_id2);
}

IN_PROC_BROWSER_TEST_F(AppNotificationsArcNotificationTest,
                       MultipleNotificationsWhenInstallAndUninstallApp) {
  // Install apps to remember existing apps.
  InstallTestApps();
  const std::string app_id1 = GetTestAppId(kTestAppPackage1, kTestAppActivity1);
  const std::string app_id2 = GetTestAppId(kTestAppPackage2, kTestAppActivity2);

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  // Sent 2 notifications for the app 1, and 1 notification for the app 2.
  const std::string notification_key1 = "notification_key1";
  CreateNotificationWithKey(notification_key1, kTestAppPackage1);

  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  const std::string notification_key2 = "notification_key2";
  CreateNotificationWithKey(notification_key2, kTestAppPackage1);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  // Sent 1 notification for the app 2.
  const std::string notification_key3 = "notification_key3";
  CreateNotificationWithKey(notification_key3, kTestAppPackage2);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  RemoveNotificationWithKey(notification_key1);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  // Uninstall the app 2.
  UninstallApp(profile(), app_id2);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));

  // Uninstall the app 1.
  UninstallApp(profile(), app_id1);

  // Reinstall apps
  InstallTestApps();

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));

  // Sent 2 notifications for the app 2, and 1 notification for the app 1.
  const std::string notification_key4 = "notification_key4";
  CreateNotificationWithKey(notification_key4, kTestAppPackage2);

  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  const std::string notification_key5 = "notification_key5";
  CreateNotificationWithKey(notification_key5, kTestAppPackage1);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  const std::string notification_key6 = "notification_key6";
  CreateNotificationWithKey(notification_key6, kTestAppPackage2);
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  // Remove notifications
  RemoveNotificationWithKey(notification_key5);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  RemoveNotificationWithKey(notification_key4);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kTrue, HasBadge(profile(), app_id2));

  RemoveNotificationWithKey(notification_key6);
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id1));
  ASSERT_EQ(OptionalBool::kFalse, HasBadge(profile(), app_id2));
}
