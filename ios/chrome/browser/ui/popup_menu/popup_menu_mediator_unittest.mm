// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/default_clock.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_overlay.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"
#include "ios/chrome/browser/policy/enterprise_policy_test_helper.h"
#include "ios/chrome/browser/policy/policy_features.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_text_item.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_web_state.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/font_size_tab_helper.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/text_zoom_provider.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using java_script_dialog_overlays::JavaScriptDialogRequest;

@interface FakePopupMenuConsumer : NSObject <PopupMenuConsumer>
@property(nonatomic, strong)
    NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>* popupMenuItems;
@end

@implementation FakePopupMenuConsumer

@synthesize itemToHighlight;

- (void)itemsHaveChanged:(NSArray<TableViewItem<PopupMenuItem>*>*)items {
  // Do nothing.
}

@end

namespace {
const int kNumberOfWebStates = 3;
}  // namespace

@interface TestPopupMenuMediator
    : PopupMenuMediator<CRWWebStateObserver, WebStateListObserving>
@end

@implementation TestPopupMenuMediator
@end

class PopupMenuMediatorTest : public ChromeWebTest {
 public:
  PopupMenuMediatorTest() : ChromeWebTest(std::make_unique<ChromeWebClient>()) {
    reading_list_model_.reset(new ReadingListModelImpl(
        nullptr, nullptr, base::DefaultClock::GetInstance()));
    popup_menu_ = OCMClassMock([PopupMenuTableViewController class]);
    popup_menu_strict_ =
        OCMStrictClassMock([PopupMenuTableViewController class]);
    OCMExpect([popup_menu_strict_ setPopupMenuItems:[OCMArg any]]);
    OCMExpect([popup_menu_strict_ setDelegate:[OCMArg any]]);
    SetUpWebStateList();

    // Set up the TestBrowser.
    TestChromeBrowserState::Builder browser_state_builder;
    browser_state_ = browser_state_builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get(),
                                             web_state_list_.get());
    // Set up the OverlayPresenter.
    OverlayPresenter::FromBrowser(browser_.get(),
                                  OverlayModality::kWebContentArea)
        ->SetPresentationContext(&presentation_context_);
  }

  // Explicitly disconnect the mediator so there won't be any WebStateList
  // observers when web_state_list_ gets dealloc.
  ~PopupMenuMediatorTest() override {
    [mediator_ disconnect];
  }

 protected:
  PopupMenuMediator* CreateMediator(PopupMenuType type,
                                    BOOL is_incognito,
                                    BOOL trigger_incognito_hint) {
    mediator_ =
        [[PopupMenuMediator alloc] initWithType:type
                                    isIncognito:is_incognito
                               readingListModel:reading_list_model_.get()
                      triggerNewIncognitoTabTip:trigger_incognito_hint
                         browserPolicyConnector:nil];
    return mediator_;
  }

  PopupMenuMediator* CreateMediatorWithBrowserPolicyConnector(
      PopupMenuType type,
      BOOL is_incognito,
      BOOL trigger_incognito_hint,
      BrowserPolicyConnectorIOS* browser_policy_connector) {
    mediator_ =
        [[PopupMenuMediator alloc] initWithType:type
                                    isIncognito:is_incognito
                               readingListModel:reading_list_model_.get()
                      triggerNewIncognitoTabTip:trigger_incognito_hint
                         browserPolicyConnector:browser_policy_connector];
    return mediator_;
  }

  void CreatePrefs() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(
        bookmarks::prefs::kEditBookmarksEnabled,
        /*default_value=*/true);
  }

  void SetUpBookmarks() {
    browser_state_->CreateBookmarkModel(false);
    bookmark_model_ =
        ios::BookmarkModelFactory::GetForBrowserState(browser_state_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    mediator_.bookmarkModel = bookmark_model_;
  }

  void SetUpWebStateList() {
    auto navigation_manager = std::make_unique<ToolbarTestNavigationManager>();
    navigation_manager_ = navigation_manager.get();

    navigation_item_ = web::NavigationItem::Create();
    navigation_item_->SetURL(GURL("http://chromium.org"));
    navigation_manager->SetVisibleItem(navigation_item_.get());

    std::unique_ptr<ToolbarTestWebState> test_web_state =
        std::make_unique<ToolbarTestWebState>();
    test_web_state->SetNavigationManager(std::move(navigation_manager));
    test_web_state->SetLoading(true);
    web_state_ = test_web_state.get();

    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
    web_state_list_->InsertWebState(0, std::move(test_web_state),
                                    WebStateList::INSERT_FORCE_INDEX,
                                    WebStateOpener());
    for (int i = 1; i < kNumberOfWebStates; i++) {
      InsertNewWebState(i);
    }
  }

  void InsertNewWebState(int index) {
    auto web_state = std::make_unique<web::FakeWebState>();
    GURL url("http://test/" + std::to_string(index));
    web_state->SetCurrentURL(url);
    web_state_list_->InsertWebState(index, std::move(web_state),
                                    WebStateList::INSERT_FORCE_INDEX,
                                    WebStateOpener());
  }

  void SetUpActiveWebState() {
    // PopupMenuMediator expects an language::IOSLanguageDetectionTabHelper for
    // the currently active WebState.
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        web_state_list_->GetWebStateAt(0), /*url_language_histogram=*/nullptr);

    web_state_list_->ActivateWebStateAt(0);
  }

  // Checks that the popup_menu_ is receiving a number of items corresponding to
  // |number_items|.
  void CheckMediatorSetItems(NSArray<NSNumber*>* number_items) {
    mediator_.webStateList = web_state_list_.get();
    SetUpActiveWebState();
    auto same_number_items = ^BOOL(id items) {
      if (![items isKindOfClass:[NSArray class]])
        return NO;
      if ([items count] != number_items.count)
        return NO;
      for (NSUInteger index = 0; index < number_items.count; index++) {
        NSArray* section = [items objectAtIndex:index];
        if (section.count != number_items[index].unsignedIntegerValue)
          return NO;
      }
      return YES;
    };
    OCMExpect([popup_menu_
        setPopupMenuItems:[OCMArg checkWithBlock:same_number_items]]);
    mediator_.popupMenu = popup_menu_;
    EXPECT_OCMOCK_VERIFY(popup_menu_);
  }

  bool HasItem(FakePopupMenuConsumer* consumer,
               NSString* accessibility_identifier,
               BOOL enabled) {
    for (NSArray* innerArray in consumer.popupMenuItems) {
      for (PopupMenuToolsItem* item in innerArray) {
        if (item.accessibilityIdentifier == accessibility_identifier)
          return item.enabled == enabled;
      }
    }
    return NO;
  }

  bool HasEnterpriseInfoItem(FakePopupMenuConsumer* consumer) {
    for (NSArray* innerArray in consumer.popupMenuItems) {
      for (PopupMenuTextItem* item in innerArray) {
        if (item.accessibilityIdentifier == kTextMenuEnterpriseInfo)
          return YES;
      }
    }
    return NO;
  }

  FakeOverlayPresentationContext presentation_context_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  PopupMenuMediator* mediator_;
  BookmarkModel* bookmark_model_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  ToolbarTestWebState* web_state_;
  ToolbarTestNavigationManager* navigation_manager_;
  std::unique_ptr<web::NavigationItem> navigation_item_;
  id popup_menu_;
  // Mock refusing all calls except -setPopupMenuItems:.
  id popup_menu_strict_;
};

// Tests that the feature engagement tracker get notified when the mediator is
// disconnected and the tracker wants the notification badge displayed.
TEST_F(PopupMenuMediatorTest, TestFeatureEngagementDisconnect) {
  CreateMediator(PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/NO);
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, ShouldTriggerHelpUI(testing::_))
      .WillRepeatedly(testing::Return(true));
  mediator_.popupMenu = popup_menu_;
  mediator_.engagementTracker = &tracker;

  // There may be one or more Tools Menu items that use engagement trackers.
  EXPECT_CALL(tracker, Dismissed(testing::_)).Times(testing::AtLeast(1));
  [mediator_ disconnect];
}

// Tests that the mediator is returning the right number of items and sections
// for the Tools Menu type.
TEST_F(PopupMenuMediatorTest, TestToolsMenuItemsCount) {
  CreateMediator(PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/NO);
  NSUInteger number_of_action_items = 7;
  if (ios::GetChromeBrowserProvider()
          ->GetUserFeedbackProvider()
          ->IsUserFeedbackEnabled()) {
    number_of_action_items++;
  }

  if (ios::GetChromeBrowserProvider()
          ->GetTextZoomProvider()
          ->IsTextZoomEnabled()) {
    number_of_action_items++;
  }

  // Checks that Tools Menu has the right number of items in each section.
  CheckMediatorSetItems(@[
    // Stop/Reload, New Tab, New Incognito Tab.
    @(3),
    // 4 collections, Downloads, Settings.
    @(6),
    // Other actions, depending on configuration.
    @(number_of_action_items)
  ]);
}

// Tests that the mediator is returning the right number of items and sections
// for the Tab Grid type, in non-incognito.
TEST_F(PopupMenuMediatorTest, TestTabGridMenuNonIncognito) {
  CreateMediator(PopupMenuTypeTabGrid, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/NO);
  CheckMediatorSetItems(@[
    // New Tab, New Incognito Tab
    @(2),
    // Close Tab
    @(1)
  ]);
}

// Tests that the mediator is returning the right number of items and sections
// for the Tab Grid type, in incognito.
TEST_F(PopupMenuMediatorTest, TestTabGridMenuIncognito) {
  CreateMediator(PopupMenuTypeTabGrid, /*is_incognito=*/YES,
                 /*trigger_incognito_hint=*/NO);
  CheckMediatorSetItems(@[
    // New Tab, New Incognito Tab
    @(2),
    // Close Tab
    @(1)
  ]);
}

// Tests that the mediator is asking for an item to be highlighted when asked.
TEST_F(PopupMenuMediatorTest, TestNewIncognitoHint) {
  CreateMediator(PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/YES);
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  OCMExpect([popup_menu_ setItemToHighlight:[OCMArg isNotNil]]);
  mediator_.popupMenu = popup_menu_;
  EXPECT_OCMOCK_VERIFY(popup_menu_);
}

// Test that the mediator isn't asking for an highlighted item.
TEST_F(PopupMenuMediatorTest, TestNewIncognitoNoHint) {
  CreateMediator(PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/NO);
  [[popup_menu_ reject] setItemToHighlight:[OCMArg any]];
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.popupMenu = popup_menu_;
}

// Tests that the mediator is asking for an item to be highlighted when asked.
TEST_F(PopupMenuMediatorTest, TestNewIncognitoHintTabGrid) {
  CreateMediator(PopupMenuTypeTabGrid, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/YES);
  OCMExpect([popup_menu_ setItemToHighlight:[OCMArg isNotNil]]);
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.popupMenu = popup_menu_;
  EXPECT_OCMOCK_VERIFY(popup_menu_);
}

// Tests that the items returned by the mediator are correctly enabled on a
// WebPage.
TEST_F(PopupMenuMediatorTest, TestItemsStatusOnWebPage) {
  CreateMediator(PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/NO);
  mediator_.webStateList = web_state_list_.get();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  SetUpActiveWebState();

  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  EXPECT_TRUE(HasItem(consumer, kToolsMenuNewTabId, /*enabled=*/YES));
  EXPECT_TRUE(HasItem(consumer, kToolsMenuSiteInformation, /*enabled=*/YES));
}

// Tests that the items returned by the mediator are correctly enabled on the
// NTP.
TEST_F(PopupMenuMediatorTest, TestItemsStatusOnNTP) {
  CreateMediator(PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/NO);
  mediator_.webStateList = web_state_list_.get();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  SetUpActiveWebState();

  navigation_item_->SetVirtualURL(GURL("chrome://newtab"));
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  EXPECT_TRUE(HasItem(consumer, kToolsMenuNewTabId, /*enabled=*/YES));
  EXPECT_TRUE(HasItem(consumer, kToolsMenuSiteInformation, /*enabled=*/YES));
}

// Tests that the "Read Later" button is disabled while overlay UI is displayed
// in OverlayModality::kWebContentArea.
TEST_F(PopupMenuMediatorTest, TestReadLaterDisabled) {
  const GURL kUrl("https://chromium.test");
  web_state_->SetCurrentURL(kUrl);
  CreatePrefs();
  CreateMediator(PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/NO);
  mediator_.webStateList = web_state_list_.get();
  mediator_.webContentAreaOverlayPresenter = OverlayPresenter::FromBrowser(
      browser_.get(), OverlayModality::kWebContentArea);
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  mediator_.prefService = prefs_.get();
  SetUpActiveWebState();
  ASSERT_TRUE(HasItem(consumer, kToolsMenuReadLater, /*enabled=*/YES));

  // Present a JavaScript alert over the WebState and verify that the page is no
  // longer shareable.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_, OverlayModality::kWebContentArea);
  queue->AddRequest(OverlayRequest::CreateWithConfig<JavaScriptDialogRequest>(
      web::JAVASCRIPT_DIALOG_TYPE_ALERT, web_state_, kUrl,
      /*is_main_frame=*/true, @"message",
      /*default_text_field_value=*/nil));
  EXPECT_TRUE(HasItem(consumer, kToolsMenuReadLater, /*enabled=*/NO));

  // Cancel the request and verify that the "Read Later" button is enabled.
  queue->CancelAllRequests();
  EXPECT_TRUE(HasItem(consumer, kToolsMenuReadLater, /*enabled=*/YES));
}

// Tests that the "Text Zoom..." button is disabled on non-HTML pages.
TEST_F(PopupMenuMediatorTest, TestTextZoomDisabled) {
  CreateMediator(PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/NO);
  mediator_.webStateList = web_state_list_.get();

  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  FontSizeTabHelper::CreateForWebState(web_state_list_->GetWebStateAt(0));
  SetUpActiveWebState();
  EXPECT_TRUE(HasItem(consumer, kToolsMenuTextZoom, /*enabled=*/YES));

  web_state_->SetContentIsHTML(false);
  // Fake a navigationFinished to force the popup menu items to update.
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);
  EXPECT_TRUE(HasItem(consumer, kToolsMenuTextZoom, /*enabled=*/NO));
}

// Tests that the "Managed by..." item is hidden when none of the policies is
// set.
TEST_F(PopupMenuMediatorTest, TestEnterpriseInfoHidden) {
  // Enabled the feature flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableIOSManagedSettingsUI);

  CreateMediator(PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/NO);

  mediator_.webStateList = web_state_list_.get();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  SetUpActiveWebState();

  ASSERT_FALSE(HasEnterpriseInfoItem(consumer));
}

// Tests that the "Managed by..." item is shown.
TEST_F(PopupMenuMediatorTest, TestEnterpriseInfoShown) {
  // Enabled the feature flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableIOSManagedSettingsUI);

  // Set a policy.
  base::ScopedTempDir state_directory;
  ASSERT_TRUE(state_directory.CreateUniqueTempDir());

  std::unique_ptr<EnterprisePolicyTestHelper> enterprise_policy_helper =
      std::make_unique<EnterprisePolicyTestHelper>(state_directory.GetPath());
  BrowserPolicyConnectorIOS* connector =
      enterprise_policy_helper->GetBrowserPolicyConnector();

  policy::PolicyMap map;
  map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
          base::Value("hello"), nullptr);
  enterprise_policy_helper->GetPolicyProvider()->UpdateChromePolicy(map);

  CreateMediatorWithBrowserPolicyConnector(
      PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
      /*trigger_incognito_hint=*/NO, connector);

  mediator_.webStateList = web_state_list_.get();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  SetUpActiveWebState();
  ASSERT_TRUE(HasEnterpriseInfoItem(consumer));
}

// Tests that 1) the tools menu has an enabled 'Add to Bookmarks' button when
// the current URL is not in bookmarks 2) the bookmark button changes to an
// enabled 'Edit bookmark' button when navigating to a bookmarked URL, 3) the
// bookmark button changes to 'Add to Bookmarks' when the bookmark is removed.
TEST_F(PopupMenuMediatorTest, TestBookmarksToolsMenuButtons) {
  const GURL url("https://bookmarked.url");
  web_state_->SetCurrentURL(url);
  CreateMediator(PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/NO);
  CreatePrefs();
  SetUpBookmarks();
  bookmarks::AddIfNotBookmarked(bookmark_model_, url,
                                base::SysNSStringToUTF16(@"Test bookmark"));
  mediator_.webStateList = web_state_list_.get();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  mediator_.prefService = prefs_.get();

  EXPECT_TRUE(HasItem(consumer, kToolsMenuAddToBookmarks, /*enabled=*/YES));

  SetUpActiveWebState();
  EXPECT_FALSE(HasItem(consumer, kToolsMenuAddToBookmarks, /*enabled=*/YES));
  EXPECT_TRUE(HasItem(consumer, kToolsMenuEditBookmark, /*enabled=*/YES));

  bookmark_model_->RemoveAllUserBookmarks();
  EXPECT_TRUE(HasItem(consumer, kToolsMenuAddToBookmarks, /*enabled=*/YES));
  EXPECT_FALSE(HasItem(consumer, kToolsMenuEditBookmark, /*enabled=*/YES));
}

// Tests that the bookmark button is disabled when EditBookmarksEnabled pref is
// changed to false.
TEST_F(PopupMenuMediatorTest, TestDisableBookmarksButton) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEditBookmarksIOS);

  CreateMediator(PopupMenuTypeToolsMenu, /*is_incognito=*/NO,
                 /*trigger_incognito_hint=*/NO);
  CreatePrefs();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  mediator_.prefService = prefs_.get();

  EXPECT_TRUE(HasItem(consumer, kToolsMenuAddToBookmarks, /*enabled=*/YES));

  prefs_->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
  EXPECT_TRUE(HasItem(consumer, kToolsMenuAddToBookmarks, /*enabled=*/NO));
}
