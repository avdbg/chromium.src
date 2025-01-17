// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>
#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/activity_services/activity_params.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ios_unittest.h"
#import "ios/chrome/browser/ui/commands/bookmark_page_command.h"
#import "ios/chrome/browser/ui/commands/bookmarks_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/generate_qr_code_command.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

class MockWebState : public web::FakeWebState {
 public:
  MockWebState() : web::FakeWebState() {
    SetNavigationManager(std::make_unique<web::FakeNavigationManager>());
  }

  MOCK_METHOD2(ExecuteJavaScript,
               void(const base::string16&, JavaScriptResultCallback));
};

}  // namespace

// Test fixture for testing SharingCoordinator.
class SharingCoordinatorTest : public BookmarkIOSUnitTest {
 protected:
  SharingCoordinatorTest()
      : base_view_controller_([[UIViewController alloc] init]),
        fake_origin_view_([[UIView alloc] init]),
        test_scenario_(ActivityScenario::TabShareButton) {
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  void SetUp() override {
    BookmarkIOSUnitTest::SetUp();
    snackbar_handler_ = OCMStrictProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];
  }

  void AppendNewWebState(std::unique_ptr<web::FakeWebState> web_state) {
    browser_->GetWebStateList()->InsertWebState(
        WebStateList::kInvalidIndex, std::move(web_state),
        WebStateList::INSERT_ACTIVATE, WebStateOpener());
  }

  // Validates that |trigger_block| gets a Edit Bookmark VC to be presented,
  // and that |delegate| controls its dismissal.
  void ValidateEditBookmark(ProceduralBlock trigger_block,
                            id<BookmarkEditCoordinatorDelegate> delegate) {
    id vc_partial_mock = OCMPartialMock(base_view_controller_);
    __block BookmarkEditViewController* bookmarkEditVC;
    [[vc_partial_mock expect]
        presentViewController:[OCMArg checkWithBlock:^BOOL(
                                          UIViewController* viewController) {
          if ([viewController
                  isKindOfClass:[TableViewNavigationController class]]) {
            TableViewNavigationController* navController =
                (TableViewNavigationController*)viewController;
            if ([navController.tableViewController
                    isKindOfClass:[BookmarkEditViewController class]]) {
              bookmarkEditVC = (BookmarkEditViewController*)
                                   navController.tableViewController;
              return YES;
            }
            return NO;
          }
          return NO;
        }]
                     animated:YES
                   completion:nil];

    trigger_block();

    [vc_partial_mock verify];

    [[vc_partial_mock expect] dismissViewControllerAnimated:YES completion:nil];

    // Dismiss the ViewController.
    ASSERT_NE(nil, bookmarkEditVC);
    [bookmarkEditVC dismiss];

    [vc_partial_mock verify];
  }

  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  UIView* fake_origin_view_;
  id snackbar_handler_;
  ActivityScenario test_scenario_;
};

// Tests that the start method shares the current page and ends up presenting
// a UIActivityViewController.
TEST_F(SharingCoordinatorTest, Start_ShareCurrentPage) {
  // Create a test web state.
  GURL test_url = GURL("https://example.com");
  base::Value url_value = base::Value(test_url.spec());
  auto test_web_state = std::make_unique<MockWebState>();
  test_web_state->SetCurrentURL(test_url);
  test_web_state->SetBrowserState(browser_->GetBrowserState());

  EXPECT_CALL(*test_web_state, ExecuteJavaScript(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const base::string16& javascript,
              base::OnceCallback<void(const base::Value*)> callback) {
            std::move(callback).Run(&url_value);
          }));

  AppendNewWebState(std::move(test_web_state));

  ActivityParams* params =
      [[ActivityParams alloc] initWithScenario:test_scenario_];

  SharingCoordinator* coordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                          params:params
                      originView:fake_origin_view_];

  // Pointer to allow us to grab the VC instance in our validation callback.
  __block UIActivityViewController* activityViewController;

  id vc_partial_mock = OCMPartialMock(base_view_controller_);
  [[vc_partial_mock expect]
      presentViewController:[OCMArg checkWithBlock:^BOOL(
                                        UIViewController* viewController) {
        if ([viewController isKindOfClass:[UIActivityViewController class]]) {
          activityViewController = (UIActivityViewController*)viewController;
          return YES;
        }
        return NO;
      }]
                   animated:YES
                 completion:nil];

  [coordinator start];

  [vc_partial_mock verify];

  // Verify that the positioning is correct.
  auto activityHandler =
      static_cast<id<ActivityServicePositioner, ActivityServicePresentation>>(
          coordinator);
  EXPECT_EQ(fake_origin_view_, activityHandler.sourceView);
  EXPECT_TRUE(
      CGRectEqualToRect(fake_origin_view_.bounds, activityHandler.sourceRect));

  // Verify that the presentation protocol works too.
  id activity_vc_partial_mock = OCMPartialMock(activityViewController);
  [[activity_vc_partial_mock expect] dismissViewControllerAnimated:YES
                                                        completion:nil];

  [activityHandler activityServiceDidEndPresenting];

  [activity_vc_partial_mock verify];
}

// Tests that the coordinator handles the QRGenerationCommands protocol.
TEST_F(SharingCoordinatorTest, GenerateQRCode) {
  ActivityParams* params =
      [[ActivityParams alloc] initWithScenario:test_scenario_];
  SharingCoordinator* coordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                          params:params
                      originView:fake_origin_view_];

  id vc_partial_mock = OCMPartialMock(base_view_controller_);
  [[vc_partial_mock expect] presentViewController:[OCMArg any]
                                         animated:YES
                                       completion:nil];

  auto handler = static_cast<id<QRGenerationCommands>>(coordinator);
  [handler generateQRCode:[[GenerateQRCodeCommand alloc]
                              initWithURL:GURL("https://example.com")
                                    title:@"Some Title"]];

  [vc_partial_mock verify];

  [[vc_partial_mock expect] dismissViewControllerAnimated:YES completion:nil];

  [handler hideQRCode];

  [vc_partial_mock verify];
}

// Tests that the start method shares the given URL and ends up presenting
// a UIActivityViewController.
TEST_F(SharingCoordinatorTest, Start_ShareURL) {
  GURL testURL = GURL("https://example.com");
  NSString* testTitle = @"Some title";
  ActivityParams* params = [[ActivityParams alloc] initWithURL:testURL
                                                         title:testTitle
                                                      scenario:test_scenario_];
  SharingCoordinator* coordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                          params:params
                      originView:fake_origin_view_];

  // Pointer to allow us to grab the VC instance in our validation callback.
  __block UIActivityViewController* activityViewController;

  id vc_partial_mock = OCMPartialMock(base_view_controller_);
  [[vc_partial_mock expect]
      presentViewController:[OCMArg checkWithBlock:^BOOL(
                                        UIViewController* viewController) {
        if ([viewController isKindOfClass:[UIActivityViewController class]]) {
          activityViewController = (UIActivityViewController*)viewController;
          return YES;
        }
        return NO;
      }]
                   animated:YES
                 completion:nil];

  [coordinator start];

  [vc_partial_mock verify];
}

// Tests that the coordinator can handle adding a new bookmark, and the edit
// action is hooked-up properly.
TEST_F(SharingCoordinatorTest, AddBookmark_EditViaSnackbar) {
  @autoreleasepool {
    ActivityParams* params =
        [[ActivityParams alloc] initWithScenario:test_scenario_];
    SharingCoordinator* coordinator = [[SharingCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                            params:params
                        originView:fake_origin_view_];

    __block ProceduralBlock edit_action = nil;
    [[snackbar_handler_ expect]
        showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                        MDCSnackbarMessage* message) {
          edit_action = message.action.handler;
          return YES;
        }]];

    GURL test_url("https://wwww.chromium.org");
    NSString* test_title = @"Test Title";
    BookmarkPageCommand* command =
        [[BookmarkPageCommand alloc] initWithURL:test_url title:test_title];

    ASSERT_EQ(nil,
              bookmark_model_->GetMostRecentlyAddedUserNodeForURL(command.URL));

    auto handler = static_cast<id<BookmarksCommands>>(coordinator);
    [handler bookmarkPage:command];

    const BookmarkNode* bookmark =
        bookmark_model_->GetMostRecentlyAddedUserNodeForURL(command.URL);

    ASSERT_NE(nil, bookmark);
    EXPECT_EQ(test_url, bookmark->url());
    EXPECT_EQ(base::SysNSStringToUTF16(test_title), bookmark->GetTitle());

    [snackbar_handler_ verify];
    ASSERT_NE(nil, edit_action);

    // Verify snackbar message's Edit action.
    auto bookmark_delegate =
        static_cast<id<BookmarkEditCoordinatorDelegate>>(coordinator);

    ValidateEditBookmark(edit_action, bookmark_delegate);
  }
}

// Tests that the coordinator can handle editing an existing bookmark via the
// bookmarkPage command.
TEST_F(SharingCoordinatorTest, EditExistingBookmark) {
  @autoreleasepool {
    ActivityParams* params =
        [[ActivityParams alloc] initWithScenario:test_scenario_];
    SharingCoordinator* coordinator = [[SharingCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                            params:params
                        originView:fake_origin_view_];

    const BookmarkNode* bookmark =
        AddBookmark(bookmark_model_->mobile_node(), @"Some Other Title");

    NSString* test_title = @"Test Title";
    BookmarkPageCommand* command =
        [[BookmarkPageCommand alloc] initWithURL:bookmark->url()
                                           title:test_title];

    ASSERT_EQ(bookmark,
              bookmark_model_->GetMostRecentlyAddedUserNodeForURL(command.URL));

    auto handler = static_cast<id<BookmarksCommands>>(coordinator);

    ProceduralBlock trigger = ^{
      [handler bookmarkPage:command];
    };
    auto bookmark_delegate =
        static_cast<id<BookmarkEditCoordinatorDelegate>>(coordinator);

    ValidateEditBookmark(trigger, bookmark_delegate);
  }
}
