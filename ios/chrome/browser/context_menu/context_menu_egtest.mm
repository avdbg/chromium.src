// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/bind.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_app_interface.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/public/test/element_selector.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContextMenuCopyButton;
using chrome_test_util::OmniboxText;
using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::SystemSelectionCalloutCopyButton;
using chrome_test_util::WebViewMatcher;

namespace {
// Directory containing the |kLogoPagePath| and |kLogoPageImageSourcePath|
// resources.
// const char kServerFilesDir[] = "ios/testing/data/http_server_files/";
// Path to a page containing the chromium logo and the text |kLogoPageText|.
const char kLogoPagePath[] = "/chromium_logo_page.html";
// Path to the chromium logo.
const char kLogoPageImageSourcePath[] = "/chromium_logo.png";
// The DOM element ID of the chromium image on the logo page.
const char kLogoPageChromiumImageId[] = "chromium_image";
// The text of the message on the logo page.
const char kLogoPageText[] = "Page with some text and the chromium logo image.";

// URL to a page with a static message.
const char kDestinationPageUrl[] = "/destination";
// HTML content of the destination page.
const char kDestinationHtml[] =
    "<html><body><script>document.title='new doc'</script>"
    "<center><span id=\"message\">You made it!</span></center>"
    "</body></html>";
// The DOM element ID of the message on the destination page.
const char kDestinationPageTextId[] = "message";
// The text of the message on the destination page.
const char kDestinationPageText[] = "You made it!";

// URL to a page with a link to the destination page.
const char kInitialPageUrl[] = "/scenarioContextMenuOpenInNewSurface";
// HTML content of a page with a link to the destination page.
const char kInitialPageHtml[] =
    "<html><body><a style='margin-left:150px' href='/destination' id='link'>"
    "link</a></body></html>";
// The DOM element ID of the link to the destination page.
const char kInitialPageDestinationLinkId[] = "link";
// The text of the link to the destination page.
const char kInitialPageDestinationLinkText[] = "link";

// Template HTML, URLs, and link and title values for title truncation tests.
// (Use NSString for easier format printing and matching).
// Template params:
//    [0] NSString - link href.
//    [1] char[]   - link element ID.
//    [2] NSString - image title
//    [3] char[]   - image element ID.
NSString* const kTruncationTestPageTemplateHtml =
    @"<html><body><p style='margin-bottom:50px'>Short title test.</p>"
     "<p><a style='margin-left:150px' href='%@' id='%s'>LINK</a></p>"
     "<img src='chromium_logo.png' title='%@' id='%s'/>"
     "</body></html>";

const char kShortTruncationPageUrl[] = "/shortTruncation";

const char kLongTruncationPageUrl[] = "/longTruncation";

NSString* const kShortLinkHref = @"/destination";

NSString* const kShortImgTitile = @"Chromium logo with a short title";

// Long titles should be > 100 chars to test truncation.
NSString* const kLongLinkHref =
    @"/destination?linkspam=10%E4%BB%A3%E5%B7%A5%E5%AD%A6%E3%81%AF%E6%9C%AA%E6"
     "%9D%A5%E3%81%AE%E8%A3%BD%E5%93%81%E3%81%A8%E3%82%B3%E3%83%9F%E3%83%A5%E3"
     "%83%8B%E3%82%B1%E3%83%BC%E3%82%B7%E3%83%A7%E3%83%B3%E3%82%92%E7%94%9F%E3"
     "%81%BF%E5%87%BA%E3%81%99%E3%82%B9%E3%82%BF%E3%82%B8%E3%82%AA%E3%81%A7%E3"
     "%81%99%E3%80&padding=qwertyuiopasdfghjklzxcvbnmqwertyuiopasdfghjklzxcvbn";

NSString* const kLongImgTitle =
    @"Chromium logo with a long title, well in excess of one hundred "
     "characters, so formulated as to thest the very limits of the context "
     "menu layout system, and to ensure that all users can enjoy the full "
     "majesty of image titles, however sesquipedalian they may be!";

// Matcher for the open image button in the context menu.
id<GREYMatcher> OpenImageButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CONTENT_CONTEXT_OPENIMAGE);
}

// Matcher for the open image in new tab button in the context menu.
id<GREYMatcher> OpenImageInNewTabButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
}

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kInitialPageUrl) {
    // The initial page contains a link to the destination page.
    http_response->set_content(kInitialPageHtml);
  } else if (request.relative_url == kDestinationPageUrl) {
    http_response->set_content(kDestinationHtml);
  } else if (request.relative_url == kShortTruncationPageUrl) {
    NSString* content = [NSString
        stringWithFormat:kTruncationTestPageTemplateHtml, kShortLinkHref,
                         kInitialPageDestinationLinkId, kShortImgTitile,
                         kLogoPageChromiumImageId];
    http_response->set_content(base::SysNSStringToUTF8(content));
  } else if (request.relative_url == kLongTruncationPageUrl) {
    NSString* content =
        [NSString stringWithFormat:kTruncationTestPageTemplateHtml,
                                   kLongLinkHref, kInitialPageDestinationLinkId,
                                   kLongImgTitle, kLogoPageChromiumImageId];
    http_response->set_content(base::SysNSStringToUTF8(content));
  } else {
    return nullptr;
  }

  return std::move(http_response);
}

// Long presses on |element_id| to trigger context menu.
void LongPressElement(const char* element_id) {
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:element_id],
                        true /* menu should appear */)];
}

// Taps on the web view to dismiss the context menu without using anything on
// it.
void ClearContextMenu() {
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];
}

// Taps on |context_menu_item_button| context menu item.
void TapOnContextMenuButton(id<GREYMatcher> context_menu_item_button) {
  [[EarlGrey selectElementWithMatcher:context_menu_item_button]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:context_menu_item_button]
      performAction:grey_tap()];
}

}  // namespace

// Context menu tests for Chrome.
@interface ContextMenuTestCase : ChromeTestCase
@end

@implementation ContextMenuTestCase

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [ChromeEarlGrey setContentSettings:CONTENT_SETTING_ALLOW];
}

+ (void)tearDown {
  [ChromeEarlGrey setContentSettings:CONTENT_SETTING_DEFAULT];
  [ChromeEarlGrey closeAllExtraWindows];
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that selecting "Open Image" from the context menu properly opens the
// image in the current tab.
- (void)testOpenImageInCurrentTabFromContextMenu {
  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  LongPressElement(kLogoPageChromiumImageId);
  TapOnContextMenuButton(OpenImageButton());
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify url.
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that selecting "Open Image in New Tab" from the context menu properly
// opens the image in a new background tab.
- (void)testOpenImageInNewTabFromContextMenu {
  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  LongPressElement(kLogoPageChromiumImageId);
  TapOnContextMenuButton(OpenImageInNewTabButton());

  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:1];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify url.
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests "Open in New Tab" on context menu.
- (void)testContextMenuOpenInNewTab {
  // TODO(crbug.com/1107513): Test fails in some iPads.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iPad.");
  }
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];

  LongPressElement(kInitialPageDestinationLinkId);
  TapOnContextMenuButton(OpenLinkInNewTabButton());

  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:1];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationPageText];

  // Verify url.
  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that the context menu is displayed for an image url.
- (void)testContextMenuDisplayedOnImage {
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [ChromeEarlGrey loadURL:imageURL];

  // Calculate a point inside the displayed image.
  CGFloat topInset = 0.0;
  if ([ChromeEarlGrey webStateWebViewUsesContentInset] ||
      [FullscreenAppInterface isFullscreenInitialized]) {
    topInset = [FullscreenAppInterface currentViewportInsets].top;
  }
  CGPoint pointOnImage = CGPointZero;
  // Offset by at least status bar height.
  pointOnImage.y = topInset + 25.0;
  pointOnImage.x = [ChromeEarlGrey webStateWebViewSize].width / 2.0;

  // Duration should match |kContextMenuLongPressDuration| as defined in
  // web_view_actions.mm.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:grey_longPressAtPointWithDuration(pointOnImage, 1.0)];

  TapOnContextMenuButton(OpenImageInNewTabButton());
  [ChromeEarlGrey waitForMainTabCount:2];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-result"
  // TODO(crbug.com/643792): Remove this wait the bug is fixed.
  // Delay for 1 second before selecting tab.
  [[GREYCondition conditionWithName:@"delay"
                              block:^BOOL {
                                return NO;
                              }] waitWithTimeout:1];
#pragma clang diagnostic pop
  [ChromeEarlGrey selectTabAtIndex:1];

  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify url.
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests context menu title truncation cases.
- (void)testContextMenuTtitleTruncation {
  const GURL shortTtileURL = self.testServer->GetURL(kShortTruncationPageUrl);
  [ChromeEarlGrey loadURL:shortTtileURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  LongPressElement(kLogoPageChromiumImageId);
  [[EarlGrey selectElementWithMatcher:grey_text(kShortImgTitile)]
      assertWithMatcher:grey_notNil()];
  ClearContextMenu();

  LongPressElement(kInitialPageDestinationLinkId);
  // Links get prefixed with the hostname, so check for partial text match
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ContainsPartialText(
                                          kShortLinkHref)]
      assertWithMatcher:grey_notNil()];
  ClearContextMenu();

  const GURL longTtileURL = self.testServer->GetURL(kLongTruncationPageUrl);
  [ChromeEarlGrey loadURL:longTtileURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  LongPressElement(kLogoPageChromiumImageId);
  [[EarlGrey selectElementWithMatcher:grey_text(kLongImgTitle)]
      assertWithMatcher:grey_notNil()];
  ClearContextMenu();

  LongPressElement(kInitialPageDestinationLinkId);
  // Expect the link to be truncated, so the matcher for the full text of the
  // link returns nil.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ContainsPartialText(
                                          kLongLinkHref)]
      assertWithMatcher:grey_nil()];
  // But expect that some of the link is visible in the title.
  NSString* startOfTitle = [kLongLinkHref substringToIndex:30];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ContainsPartialText(
                                          startOfTitle)]
      assertWithMatcher:grey_notNil()];
  ClearContextMenu();
}

// Tests that system touches are cancelled when the context menu is shown.
- (void)testContextMenuCancelSystemTouchesMetric {
  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  LongPressElement(kLogoPageChromiumImageId);
  TapOnContextMenuButton(OpenImageButton());
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that system touches were cancelled by ensuring the system text
  // selection callout is not displayed.
  [[EarlGrey selectElementWithMatcher:SystemSelectionCalloutCopyButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that the system selected text callout is displayed instead of the
// context menu when user long presses on plain text.
- (void)testContextMenuSelectedTextCallout {
  // Load the destination page directly because it has a plain text message on
  // it.
  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  [ChromeEarlGrey loadURL:destinationURL];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationPageText];

  LongPressElement(kDestinationPageTextId);

  // Verify that context menu is not shown.
  [[EarlGrey selectElementWithMatcher:ContextMenuCopyButton()]
      assertWithMatcher:grey_nil()];

  // Verify that system text selection callout is displayed.
  [[EarlGrey selectElementWithMatcher:SystemSelectionCalloutCopyButton()]
      assertWithMatcher:grey_notNil()];
}

// Tests cancelling the context menu.
- (void)testDismissContextMenu {
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];

  // Display the context menu twice.
  for (NSInteger i = 0; i < 2; i++) {
    LongPressElement(kInitialPageDestinationLinkId);

    // Make sure the context menu appeared.
    [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
        assertWithMatcher:grey_notNil()];

    if ([ChromeEarlGrey isIPadIdiom]) {
      // Tap the tools menu to dismiss the popover.
      [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
          performAction:grey_tap()];
    } else {
      TapOnContextMenuButton(chrome_test_util::CancelButton());
    }

    // Make sure the context menu disappeared.
    [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
        assertWithMatcher:grey_nil()];
  }

  // Display the context menu one last time.
  LongPressElement(kInitialPageDestinationLinkId);

  // Make sure the context menu appeared.
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      assertWithMatcher:grey_notNil()];
}

// Checks that all the options are displayed in the context menu.
- (void)testAppropriateContextMenu {
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];

  LongPressElement(kInitialPageDestinationLinkId);

  // Check the different buttons.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_COPY)]
      assertWithMatcher:grey_sufficientlyVisible()];
  if (![ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(IDS_CANCEL)]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

// Checks that "open in new window" shows up on a long press of a url link
// and that it actually opens in a new window.
- (void)testOpenLinkInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // Loads url in first window.
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL inWindowWithNumber:0];

  [ChromeEarlGrey waitForWebStateContainingText:kInitialPageDestinationLinkText
                             inWindowWithNumber:0];

  // Display the context menu.
  LongPressElement(kInitialPageDestinationLinkId);

  // Open link in new window.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OpenLinkInNewWindowButton()]
      performAction:grey_tap()];

  // Assert there's a second window with expected content.
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationPageText
                             inWindowWithNumber:1];
}

@end
