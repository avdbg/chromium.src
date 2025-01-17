// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/browser_actions_bar_browsertest.h"

#include <stddef.h>
#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace {

const char* kInjectionSucceededMessage = "injection succeeded";

scoped_refptr<const extensions::Extension> CreateExtension(
    const std::string& name,
    bool has_browser_action) {
  extensions::ExtensionBuilder builder(name);
  if (has_browser_action)
    builder.SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION);
  return builder.Build();
}

class BlockedActionWaiter
    : public extensions::ExtensionActionRunner::TestObserver {
 public:
  explicit BlockedActionWaiter(extensions::ExtensionActionRunner* runner)
      : runner_(runner), run_loop_(std::make_unique<base::RunLoop>()) {
    runner_->set_observer_for_testing(this);
  }
  ~BlockedActionWaiter() { runner_->set_observer_for_testing(nullptr); }

  void WaitAndReset() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
  // ExtensionActionRunner::TestObserver:
  void OnBlockedActionAdded() override { run_loop_->Quit(); }

  extensions::ExtensionActionRunner* runner_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BlockedActionWaiter);
};

}  // namespace

// BrowserActionsBarBrowserTest:

BrowserActionsBarBrowserTest::BrowserActionsBarBrowserTest()
    : toolbar_model_(nullptr) {}

BrowserActionsBarBrowserTest::~BrowserActionsBarBrowserTest() {
}

void BrowserActionsBarBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Note: The ScopedFeatureList needs to be instantiated before the rest of
  // set up happens.
  // This suite relies on behavior specific to ToolbarActionsBar. See
  // ExtensionsMenuViewBrowserTest and ExtensionsMenuViewUnitTest for new tests.
  feature_list_.InitAndDisableFeature(features::kExtensionsToolbarMenu);

  extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
  ToolbarActionsBar::disable_animations_for_testing_ = true;
}

void BrowserActionsBarBrowserTest::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();
  browser_actions_bar_ = ExtensionActionTestHelper::Create(browser());
  toolbar_model_ = ToolbarActionsModel::Get(profile());
}

void BrowserActionsBarBrowserTest::TearDownOnMainThread() {
  ToolbarActionsBar::disable_animations_for_testing_ = false;
  extensions::ExtensionBrowserTest::TearDownOnMainThread();
}

void BrowserActionsBarBrowserTest::LoadExtensions() {
  // Create three extensions with browser actions.
  extension_a_ = CreateExtension("alpha", true);
  extension_b_ = CreateExtension("beta", true);
  extension_c_ = CreateExtension("gamma", true);

  const extensions::Extension* extensions[] =
      { extension_a(), extension_b(), extension_c() };
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  // Add each, and verify that it is both correctly added to the extension
  // registry and to the browser actions container.
  for (size_t i = 0; i < base::size(extensions); ++i) {
    extension_service()->AddExtension(extensions[i]);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(extensions[i]->id())) <<
        extensions[i]->name();
    EXPECT_EQ(static_cast<int>(i + 1),
              browser_actions_bar_->NumberOfBrowserActions());
    EXPECT_TRUE(browser_actions_bar_->HasIcon(i));
    EXPECT_EQ(static_cast<int>(i + 1),
              browser_actions_bar()->VisibleBrowserActions());
  }
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest,
                       BrowserActionPopupTest) {
  // Load up two extensions that have browser action popups.
  base::FilePath data_dir =
      test_data_dir_.AppendASCII("api_test").AppendASCII("browser_action");
  const extensions::Extension* first_extension =
      LoadExtension(data_dir.AppendASCII("open_popup"));
  ASSERT_TRUE(first_extension);
  const extensions::Extension* second_extension =
      LoadExtension(data_dir.AppendASCII("remove_popup"));
  ASSERT_TRUE(second_extension);

  // Verify state: two actions, in the order of [first, second].
  RunScheduledLayouts();
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(first_extension->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(second_extension->id(), browser_actions_bar()->GetExtensionId(1));

  // Do a little piping to get at the underlying ExtensionActionViewControllers.
  ToolbarActionsBar* toolbar_actions_bar =
      browser_actions_bar()->GetToolbarActionsBar();
  const std::vector<ToolbarActionViewController*>& toolbar_actions =
      toolbar_actions_bar->GetActions();
  ASSERT_EQ(2u, toolbar_actions.size());
  EXPECT_EQ(first_extension->id(), toolbar_actions[0]->GetId());
  EXPECT_EQ(second_extension->id(), toolbar_actions[1]->GetId());
  ExtensionActionViewController* first_controller =
      static_cast<ExtensionActionViewController*>(toolbar_actions[0]);
  ExtensionActionViewController* second_controller =
      static_cast<ExtensionActionViewController*>(toolbar_actions[1]);

  // Neither should yet be showing a popup.
  EXPECT_FALSE(browser_actions_bar()->HasPopup());
  EXPECT_FALSE(first_controller->IsShowingPopup());
  EXPECT_FALSE(second_controller->IsShowingPopup());

  // Click on the first extension's browser action. This should open a popup.
  browser_actions_bar()->Press(0);
  EXPECT_TRUE(browser_actions_bar()->HasPopup());
  EXPECT_TRUE(first_controller->IsShowingPopup());
  EXPECT_FALSE(second_controller->IsShowingPopup());

  {
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::NotificationService::AllSources());
    // Clicking on the second extension's browser action should open the
    // second's popup. Since we only allow one extension popup at a time, this
    // should also close the first popup.
    browser_actions_bar()->Press(1);
    // Closing an extension popup isn't always synchronous; wait for a
    // notification.
    observer.Wait();
    EXPECT_TRUE(browser_actions_bar()->HasPopup());
    EXPECT_FALSE(first_controller->IsShowingPopup());
    EXPECT_TRUE(second_controller->IsShowingPopup());
  }
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest,
                       OverflowedBrowserActionPopupTest) {
  std::unique_ptr<ExtensionActionTestHelper> overflow_bar =
      browser_actions_bar()->CreateOverflowBar(browser());

  // Load up two extensions that have browser action popups.
  base::FilePath data_dir =
      test_data_dir_.AppendASCII("api_test").AppendASCII("browser_action");
  const extensions::Extension* first_extension =
      LoadExtension(data_dir.AppendASCII("open_popup"));
  ASSERT_TRUE(first_extension);
  const extensions::Extension* second_extension =
      LoadExtension(data_dir.AppendASCII("remove_popup"));
  ASSERT_TRUE(second_extension);

  // Verify state: two actions, in the order of [first, second].
  RunScheduledLayouts();
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(first_extension->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(second_extension->id(), browser_actions_bar()->GetExtensionId(1));

  // Do a little piping to get at the underlying ExtensionActionViewControllers.
  ToolbarActionsBar* main_tab = browser_actions_bar()->GetToolbarActionsBar();
  std::vector<ToolbarActionViewController*> toolbar_actions =
      main_tab->GetActions();
  ASSERT_EQ(2u, toolbar_actions.size());
  EXPECT_EQ(first_extension->id(), toolbar_actions[0]->GetId());
  EXPECT_EQ(second_extension->id(), toolbar_actions[1]->GetId());
  ExtensionActionViewController* first_controller_main =
      static_cast<ExtensionActionViewController*>(toolbar_actions[0]);
  ExtensionActionViewController* second_controller_main =
      static_cast<ExtensionActionViewController*>(toolbar_actions[1]);

  ToolbarActionsBar* overflow_tab = overflow_bar->GetToolbarActionsBar();
  toolbar_actions = overflow_tab->GetActions();
  ExtensionActionViewController* second_controller_overflow =
      static_cast<ExtensionActionViewController*>(toolbar_actions[1]);

  toolbar_model()->SetVisibleIconCount(0);
  RunScheduledLayouts();
  overflow_bar->LayoutForOverflowBar();
  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(2, overflow_bar->VisibleBrowserActions());

  // Neither should yet be showing a popup.
  EXPECT_FALSE(browser_actions_bar()->HasPopup());
  EXPECT_FALSE(second_controller_main->IsShowingPopup());
  EXPECT_FALSE(second_controller_overflow->IsShowingPopup());

  // Click on the first extension's browser action. This should open a popup.
  overflow_bar->Press(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(browser_actions_bar()->HasPopup());
  EXPECT_FALSE(overflow_bar->HasPopup());
  EXPECT_TRUE(second_controller_main->IsShowingPopup());
  EXPECT_FALSE(second_controller_overflow->IsShowingPopup());

  RunScheduledLayouts();
  overflow_bar->LayoutForOverflowBar();
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(1u, main_tab->GetIconCount());
  EXPECT_EQ(second_controller_main->GetId(),
            browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(1, overflow_bar->VisibleBrowserActions());
  EXPECT_EQ(2u, overflow_tab->GetIconCount());
  EXPECT_EQ(first_controller_main->GetId(),
            overflow_bar->GetExtensionId(0));

  {
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::NotificationService::AllSources());
    second_controller_main->HidePopup();
    observer.Wait();
  }

  RunScheduledLayouts();
  overflow_bar->LayoutForOverflowBar();
  EXPECT_FALSE(browser_actions_bar()->HasPopup());
  EXPECT_FALSE(overflow_bar->HasPopup());
  EXPECT_FALSE(second_controller_main->IsShowingPopup());
  EXPECT_FALSE(second_controller_overflow->IsShowingPopup());
  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(2, overflow_bar->VisibleBrowserActions());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(first_controller_main->GetId(),
            browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(second_controller_main->GetId(),
            browser_actions_bar()->GetExtensionId(1));
}

// A test that runs in incognito mode.
class BrowserActionsBarIncognitoTest : public BrowserActionsBarBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BrowserActionsBarBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("incognito");
  }
};

// Tests that first loading an extension action in an incognito profile, then
// removing the incognito profile and using the extension action in a normal
// profile doesn't crash.
// Regression test for crbug.com/663726.
IN_PROC_BROWSER_TEST_F(BrowserActionsBarIncognitoTest, IncognitoMode) {
  EXPECT_TRUE(browser()->profile()->IsOffTheRecord());
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action_with_icon"),
      {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  Browser* second_browser = CreateBrowser(profile()->GetOriginalProfile());
  EXPECT_FALSE(second_browser->profile()->IsOffTheRecord());

  CloseBrowserSynchronously(browser());

  ToolbarActionsBar* const toolbar_actions_bar =
      ToolbarActionsBar::FromBrowserWindow(second_browser->window());
  std::vector<ToolbarActionViewController*> actions =
      toolbar_actions_bar->GetActions();
  ASSERT_EQ(1u, actions.size());
  gfx::Image icon = actions[0]->GetIcon(
      second_browser->tab_strip_model()->GetActiveWebContents(),
      toolbar_actions_bar->GetViewSize());
  const gfx::ImageSkia* skia = icon.ToImageSkia();
  ASSERT_TRUE(skia);
  // Force the image to try and load a representation.
  skia->GetRepresentation(2.0);
}

class BrowserActionsBarRuntimeHostPermissionsBrowserTest
    : public BrowserActionsBarBrowserTest {
 public:
  enum class ContentScriptRunLocation {
    DOCUMENT_START,
    DOCUMENT_IDLE,
  };

  BrowserActionsBarRuntimeHostPermissionsBrowserTest() = default;
  ~BrowserActionsBarRuntimeHostPermissionsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    BrowserActionsBarBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void LoadAllUrlsExtension(ContentScriptRunLocation run_location) {
    std::string run_location_str;
    switch (run_location) {
      case ContentScriptRunLocation::DOCUMENT_START:
        run_location_str = "document_start";
        break;
      case ContentScriptRunLocation::DOCUMENT_IDLE:
        run_location_str = "document_idle";
        break;
    }
    extension_dir_.WriteManifest(base::StringPrintf(R"({
             "name": "All Urls Extension",
             "description": "Runs a content script everywhere",
             "manifest_version": 2,
             "version": "0.1",
             "content_scripts": [{
               "matches": ["<all_urls>"],
               "js": ["script.js"],
               "run_at": "%s"
             }]
           })",
                                                    run_location_str.c_str()));
    extension_dir_.WriteFile(
        FILE_PATH_LITERAL("script.js"),
        base::StringPrintf("chrome.test.sendMessage('%s');",
                           kInjectionSucceededMessage));
    extension_ = LoadExtension(extension_dir_.UnpackedPath());
    ASSERT_TRUE(extension_);
    extensions::ScriptingPermissionsModifier(profile(), extension_)
        .SetWithholdHostPermissions(true);
  }

  const extensions::Extension* extension() const { return extension_.get(); }

  extensions::ExtensionContextMenuModel* GetExtensionContextMenu() {
    ToolbarActionsBar* toolbar_actions_bar =
        browser_actions_bar()->GetToolbarActionsBar();
    const std::vector<ToolbarActionViewController*>& toolbar_actions =
        toolbar_actions_bar->GetActions();
    if (toolbar_actions.size() != 1)
      return nullptr;
    EXPECT_EQ(extension()->id(), toolbar_actions[0]->GetId());
    return static_cast<extensions::ExtensionContextMenuModel*>(
        toolbar_actions[0]->GetContextMenu());
  }

 private:
  extensions::TestExtensionDir extension_dir_;
  scoped_refptr<const extensions::Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionsBarRuntimeHostPermissionsBrowserTest);
};

// Tests page access modifications through the context menu which require a page
// refresh.
IN_PROC_BROWSER_TEST_F(BrowserActionsBarRuntimeHostPermissionsBrowserTest,
                       ContextMenuPageAccess_RefreshRequired) {
  LoadAllUrlsExtension(ContentScriptRunLocation::DOCUMENT_START);

  ExtensionTestMessageListener injection_listener(kInjectionSucceededMessage,
                                                  false /* will_reply */);
  injection_listener.set_extension_id(extension()->id());

  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  extensions::ExtensionActionRunner* runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  BlockedActionWaiter blocked_action_waiter(runner);
  {
    content::TestNavigationObserver observer(web_contents);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Access to |url| should have been withheld.
  blocked_action_waiter.WaitAndReset();
  EXPECT_TRUE(runner->WantsToRun(extension()));
  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension());
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(url));
  EXPECT_FALSE(injection_listener.was_satisfied());

  extensions::ExtensionContextMenuModel* extension_menu =
      GetExtensionContextMenu();
  ASSERT_TRUE(extension_menu);

  // Allow the extension to run on this site. This should show a refresh page
  // bubble. Accept the bubble.
  {
    content::TestNavigationObserver observer(web_contents);
    runner->set_default_bubble_close_action_for_testing(
        std::make_unique<ToolbarActionsBarBubbleDelegate::CloseAction>(
            ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE));
    extension_menu->ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE,
        0 /* event_flags */);
    observer.WaitForNavigationFinished();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // The extension should have injected and the extension should no longer want
  // to run.
  ASSERT_TRUE(injection_listener.WaitUntilSatisfied());
  injection_listener.Reset();
  EXPECT_TRUE(permissions_modifier.HasGrantedHostPermission(url));
  EXPECT_FALSE(runner->WantsToRun(extension()));

  // Now navigate to a different host. The extension should have blocked
  // actions.
  {
    url = embedded_test_server()->GetURL("abc.com", "/title1.html");
    content::TestNavigationObserver observer(web_contents);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
  blocked_action_waiter.WaitAndReset();
  EXPECT_TRUE(runner->WantsToRun(extension()));
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(url));
  EXPECT_FALSE(injection_listener.was_satisfied());

  // Allow the extension to run on all sites this time. This should again show a
  // refresh bubble. Dismiss it.
  runner->set_default_bubble_close_action_for_testing(
      std::make_unique<ToolbarActionsBarBubbleDelegate::CloseAction>(
          ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION));
  extension_menu->ExecuteCommand(
      extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_ALL_SITES,
      0 /* event_flags */);

  // Permissions to the extension shouldn't have been granted, and the extension
  // should still be in wants-to-run state.
  EXPECT_TRUE(runner->WantsToRun(extension()));
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(url));
  EXPECT_FALSE(injection_listener.was_satisfied());
}

// Tests page access modifications through the context menu which don't require
// a page refresh.
IN_PROC_BROWSER_TEST_F(BrowserActionsBarRuntimeHostPermissionsBrowserTest,
                       ContextMenuPageAccess_RefreshNotRequired) {
  LoadAllUrlsExtension(ContentScriptRunLocation::DOCUMENT_IDLE);
  ExtensionTestMessageListener injection_listener(kInjectionSucceededMessage,
                                                  false /* will_reply */);
  injection_listener.set_extension_id(extension()->id());

  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  extensions::ExtensionActionRunner* runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  BlockedActionWaiter blocked_action_waiter(runner);
  {
    content::TestNavigationObserver observer(web_contents);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Access to |url| should have been withheld.
  blocked_action_waiter.WaitAndReset();
  EXPECT_TRUE(runner->WantsToRun(extension()));
  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension());
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(url));
  EXPECT_FALSE(injection_listener.was_satisfied());

  extensions::ExtensionContextMenuModel* extension_menu =
      GetExtensionContextMenu();
  ASSERT_TRUE(extension_menu);

  // Allow the extension to run on this site. Since the blocked actions don't
  // require a refresh, the permission should be granted and the page actions
  // should run.
  extension_menu->ExecuteCommand(
      extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE,
      0 /* event_flags */);
  ASSERT_TRUE(injection_listener.WaitUntilSatisfied());
  EXPECT_FALSE(runner->WantsToRun(extension()));
  EXPECT_TRUE(permissions_modifier.HasGrantedHostPermission(url));
}
