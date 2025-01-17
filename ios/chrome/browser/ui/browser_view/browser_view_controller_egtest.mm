// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/feature_list.h"
#import "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This test suite only tests javascript in the omnibox. Nothing to do with BVC
// really, the name is a bit misleading.
@interface BrowserViewControllerTestCase : WebHttpServerChromeTestCase
@end

@implementation BrowserViewControllerTestCase

- (void)tearDown {
  // No-op if only one window presents.
  [ChromeEarlGrey closeAllExtraWindows];
  [super tearDown];
}

// Tests that the NTP is interactable even when multiple NTP are opened during
// the animation of the first NTP opening. See crbug.com/1032544.
- (void)testPageInteractable {
  // Scope for the synchronization disabled.
  {
    ScopedSynchronizationDisabler syncDisabler;

    [ChromeEarlGrey openNewTab];

    // Wait for 0.05s before opening the new one.
    GREYCondition* myCondition = [GREYCondition conditionWithName:@"Wait block"
                                                            block:^BOOL {
                                                              return NO;
                                                            }];
    BOOL success = [myCondition waitWithTimeout:0.05];
    success = NO;

    [ChromeEarlGrey openNewTab];
  }  // End of the sync disabler scope.

  [ChromeEarlGrey waitForMainTabCount:3];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey selectTabAtIndex:1];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that evaluating JavaScript in the omnibox (e.g, a bookmarklet) works.
- (void)testJavaScriptInOmnibox {
  // TODO(crbug.com/703855): Keyboard entry inside the omnibox fails only on
  // iPad running iOS 10.
  if ([ChromeEarlGrey isIPadIdiom])
    return;

  // Preps the http server with two URLs serving content.
  std::map<GURL, std::string> responses;
  const GURL startURL = web::test::HttpServer::MakeUrl("http://origin");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  responses[startURL] = "Start";
  responses[destinationURL] = "You've arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  // Just load the first URL.
  [ChromeEarlGrey loadURL:startURL];

  // Waits for the page to load and check it is the expected content.
  [ChromeEarlGrey waitForWebStateContainingText:responses[startURL]];

  // In the omnibox, the URL should be present, without the http:// prefix.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(startURL.GetContent())];

  // Types some javascript in the omnibox to trigger a navigation.
  NSString* script =
      [NSString stringWithFormat:@"javascript:location.href='%s'\n",
                                 destinationURL.spec().c_str()];
  [ChromeEarlGreyUI focusOmniboxAndType:script];

  // In the omnibox, the new URL should be present, without the http:// prefix.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(
                            destinationURL.GetContent())];

  // Verifies that the navigation to the destination page happened.
  GREYAssertEqual(destinationURL, [ChromeEarlGrey webStateVisibleURL],
                  @"Did not navigate to the destination url.");

  // Verifies that the destination page is shown.
  [ChromeEarlGrey waitForWebStateContainingText:responses[destinationURL]];
}

// Tests the fix for the regression reported in https://crbug.com/801165.  The
// bug was triggered by opening an HTML file picker and then dismissing it.
- (void)testFixForCrbug801165 {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no action sheet on tablet)");
  }

  std::map<GURL, std::string> responses;
  const GURL testURL = web::test::HttpServer::MakeUrl("http://origin");
  responses[testURL] = "File Picker Test <input id=\"file\" type=\"file\">";
  web::test::SetUpSimpleHttpServer(responses);

  // Load the test page.
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"File Picker Test"];

  // Invoke the file picker.
  [ChromeEarlGrey tapWebStateElementWithID:@"file"];

  if (@available(iOS 14, *)) {
    // Tap on the toolbar to dismiss the file picker on iOS14.  In iOS14 a
    // UIDropShadowView covers the entire app, so tapping anywhere should
    // dismiss the file picker.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::PrimaryToolbar()]
        performAction:grey_tap()];
  } else {
    // Tap on the "Cancel" button to dismiss the file picker before iOS14.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
        performAction:grey_tap()];
  }
  [ChromeEarlGreyUI waitForAppToIdle];
}

#pragma mark - Open URL

// Tests that BVC properly handles open URL. When NTP is visible, the URL
// should be opened in the same tab (not create a new tab).
- (void)testOpenURLFromNTP {
  [ChromeEarlGrey applicationOpenURL:GURL("https://anything")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "https://anything")]
      assertWithMatcher:grey_notNil()];
  // TODO(crbug.com/931280): This should be 1, but for the time being will be 2
  // to work around an NTP bug.
  int mainTabCount = 1;
  if ([ChromeEarlGrey isBlockNewTabPagePendingLoadEnabled])
    mainTabCount = 2;
  [ChromeEarlGrey waitForMainTabCount:mainTabCount];
}

// Tests that BVC properly handles open URL. When BVC is showing a non-NTP
// tab, the URL should be opened in a new tab, adding to the tab count.
- (void)testOpenURLFromTab {
  [ChromeEarlGrey loadURL:GURL("https://invalid")];
  [ChromeEarlGrey applicationOpenURL:GURL("https://anything")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "https://anything")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that BVC properly handles open URL. When tab switcher is showing,
// the URL should be opened in a new tab, and BVC should be shown.
- (void)testOpenURLFromTabSwitcher {
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey waitForMainTabCount:0];
  [ChromeEarlGrey applicationOpenURL:GURL("https://anything")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "https://anything")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

#pragma mark - Multiwindow

- (void)testMultiWindowURLLoading {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // Preps the http server with two URLs serving content.
  std::map<GURL, std::string> responses;
  const GURL firstURL = web::test::HttpServer::MakeUrl("http://first");
  const GURL secondURL = web::test::HttpServer::MakeUrl("http://second");
  responses[firstURL] = "First window";
  responses[secondURL] = "Second window";
  web::test::SetUpSimpleHttpServer(responses);

  // Loads url in first window.
  [ChromeEarlGrey loadURL:firstURL inWindowWithNumber:0];

  // Opens second window and loads url.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey loadURL:secondURL inWindowWithNumber:1];

  // Checks loads worked.
  [ChromeEarlGrey waitForWebStateContainingText:responses[firstURL]
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:responses[secondURL]
                             inWindowWithNumber:1];

  // Closes first window and renumbers second window as first
  [ChromeEarlGrey closeWindowWithNumber:0];
  [ChromeEarlGrey waitForForegroundWindowCount:1];
  [ChromeEarlGrey changeWindowWithNumber:1 toNewNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:responses[secondURL]
                             inWindowWithNumber:0];

  // Opens a 'new' second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Loads urls in both windows, and verifies.
  [ChromeEarlGrey loadURL:firstURL inWindowWithNumber:0];
  [ChromeEarlGrey loadURL:secondURL inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:responses[firstURL]
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:responses[secondURL]
                             inWindowWithNumber:1];
}

@end
