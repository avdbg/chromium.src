// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_error_notifier_ash.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// Notification ID corresponding to kProfileSyncNotificationId + the test
// profile's name.
const char kNotificationId[] = "chrome://settings/sync/testing_profile";

class FakeLoginUIService: public LoginUIService {
 public:
  FakeLoginUIService() : LoginUIService(nullptr) {}
  ~FakeLoginUIService() override = default;
};

class FakeLoginUI : public LoginUIService::LoginUI {
 public:
  FakeLoginUI() = default;
  ~FakeLoginUI() override = default;

  void FocusUI() override {}
};

std::unique_ptr<KeyedService> BuildFakeLoginUIService(
    content::BrowserContext* profile) {
  return std::make_unique<FakeLoginUIService>();
}

class SyncErrorNotifierTest : public BrowserWithTestWindowTest {
 public:
  SyncErrorNotifierTest() = default;
  ~SyncErrorNotifierTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    FakeLoginUIService* login_ui_service = static_cast<FakeLoginUIService*>(
        LoginUIServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildFakeLoginUIService)));
    login_ui_service->SetLoginUI(&login_ui_);

    error_notifier_ = std::make_unique<SyncErrorNotifier>(&service_, profile());

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
  }

  void TearDown() override {
    error_notifier_->Shutdown();

    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  void ExpectNotificationShown(bool expected_notification) {
    base::Optional<message_center::Notification> notification =
        display_service_->GetNotification(kNotificationId);
    if (expected_notification) {
      ASSERT_TRUE(notification);
      EXPECT_FALSE(notification->title().empty());
      EXPECT_FALSE(notification->message().empty());
    } else {
      ASSERT_FALSE(notification);
    }
  }

  std::unique_ptr<SyncErrorNotifier> error_notifier_;
  syncer::TestSyncService service_;
  FakeLoginUI login_ui_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  user_manager::ScopedUserManager scoped_user_manager_{
      std::make_unique<chromeos::MockUserManager>()};

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncErrorNotifierTest);
};

TEST_F(SyncErrorNotifierTest, NoNotificationWhenNoPassphrase) {
  service_.SetPassphraseRequiredForPreferredDataTypes(false);
  service_.SetFirstSetupComplete(true);
  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(false);
}

TEST_F(SyncErrorNotifierTest, NoNotificationWhenSyncDisabled) {
  service_.SetPassphraseRequiredForPreferredDataTypes(true);
  service_.SetFirstSetupComplete(false);
  service_.GetUserSettings()->SetOsSyncFeatureEnabled(false);
  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(false);
}

TEST_F(SyncErrorNotifierTest, NotificationShownWhenBrowserSyncEnabled) {
  service_.SetPassphraseRequiredForPreferredDataTypes(true);
  service_.SetFirstSetupComplete(true);
  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(true);
}

TEST_F(SyncErrorNotifierTest, NotificationShownWhenOsSyncEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kSplitSettingsSync);
  service_.SetPassphraseRequiredForPreferredDataTypes(true);
  service_.GetUserSettings()->SetOsSyncFeatureEnabled(true);
  service_.SetFirstSetupComplete(false);
  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(true);
}

TEST_F(SyncErrorNotifierTest, NotificationShownOnce) {
  service_.SetPassphraseRequiredForPreferredDataTypes(true);
  service_.GetUserSettings()->SetOsSyncFeatureEnabled(true);
  service_.SetFirstSetupComplete(true);
  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(true);

  // Close the notification and verify it isn't shown again.
  display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                       kNotificationId, true /* by_user */);
  error_notifier_->OnStateChanged(&service_);
  ExpectNotificationShown(false);
}

}  // namespace
