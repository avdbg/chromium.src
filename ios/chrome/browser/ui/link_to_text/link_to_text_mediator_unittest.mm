// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/link_to_text/link_to_text_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/link_to_text/link_generation_outcome.h"
#import "ios/chrome/browser/link_to_text/link_to_text_constants.h"
#import "ios/chrome/browser/link_to_text/link_to_text_payload.h"
#import "ios/chrome/browser/link_to_text/link_to_text_tab_helper.h"
#import "ios/chrome/browser/ui/link_to_text/link_to_text_consumer.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using shared_highlighting::TextFragment;
using web::FakeWebState;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using shared_highlighting::LinkGenerationError;

namespace {
const CGFloat kCaretWidth = 4.0;
const CGFloat kFakeLeftInset = 50;
const CGFloat kFakeTopInset = 100;
const char kJavaScriptFunctionName[] = "linkToText.getLinkToText";
const char kTestQuote[] = "some selected text on a page";
const char kTestHighlightURL[] =
    "https://www.chromium.org/#:~:text=selected%20text";
const char kTestBaseURL[] = "https://www.chromium.org/";
const TextFragment kTestTextFragment = TextFragment("selected text");

const char kSuccessUkmMetric[] = "Success";
const char kErrorUkmMetric[] = "Error";

class FakeWebStateListDelegate : public WebStateListDelegate {
  void WillAddWebState(web::WebState* web_state) override {}
  void WebStateDetached(web::WebState* web_state) override {}
};
}  // namespace

class LinkToTextMediatorTest : public PlatformTest {
 protected:
  LinkToTextMediatorTest()
      : web_state_list_delegate_(), web_state_list_(&web_state_list_delegate_) {
    feature_list_.InitAndEnableFeature(kSharedHighlightingIOS);
    mocked_consumer_ = OCMStrictProtocolMock(@protocol(LinkToTextConsumer));

    auto web_state = std::make_unique<FakeWebState>();
    web_state_ = web_state.get();
    web_state_list_.InsertWebState(0, std::move(web_state),
                                   WebStateList::INSERT_ACTIVATE,
                                   WebStateOpener());

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web_state_->SetWebFramesManager(std::move(web_frames_manager));

    auto main_frame = std::make_unique<web::FakeWebFrame>(
        web::kMainFakeFrameId, true, GURL("https://chromium.org/"));
    main_frame_ = main_frame.get();
    web_frames_manager_->AddWebFrame(std::move(main_frame));

    fake_scroll_view_ = [[UIScrollView alloc] init];
    CRWWebViewScrollViewProxy* scrollview_proxy =
        [[CRWWebViewScrollViewProxy alloc] init];
    [scrollview_proxy setScrollView:fake_scroll_view_];

    id mocked_webview_proxy = OCMStrictProtocolMock(@protocol(CRWWebViewProxy));
    [[[mocked_webview_proxy stub] andReturn:scrollview_proxy] scrollViewProxy];

    web_state_->SetWebViewProxy(mocked_webview_proxy);
    web_state_->SetCurrentURL(GURL(kTestBaseURL));

    // Fake Navigation End for UKM setup.
    ukm::InitializeSourceUrlRecorderForWebState(web_state_);
    web::FakeNavigationContext context;
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    web_state_->OnNavigationStarted(&context);
    web_state_->OnNavigationFinished(&context);

    LinkToTextTabHelper::CreateForWebState(web_state_);

    mediator_ =
        [[LinkToTextMediator alloc] initWithWebStateList:&web_state_list_
                                                consumer:mocked_consumer_];
  }

  void SetLinkToTextResponse(std::unique_ptr<base::Value> value,
                             CGFloat zoom_scale) {
    main_frame_->AddJsResultForFunctionCall(std::move(value),
                                            kJavaScriptFunctionName);

    fake_scroll_view_.contentInset =
        UIEdgeInsetsMake(kFakeTopInset, kFakeLeftInset, 0, 0);

    id scroll_view_mock = OCMPartialMock(fake_scroll_view_);
    [[[scroll_view_mock stub] andReturnValue:@(zoom_scale)] zoomScale];

    fake_view_ = [[UIView alloc] init];
    web_state_->SetView(fake_view_);
  }

  std::unique_ptr<base::Value> CreateSuccessResponse(
      const std::string& selected_text,
      CGRect selection_rect) {
    base::Value rect_value(base::Value::Type::DICTIONARY);
    rect_value.SetDoubleKey("x", selection_rect.origin.x);
    rect_value.SetDoubleKey("y", selection_rect.origin.y);
    rect_value.SetDoubleKey("width", selection_rect.size.width);
    rect_value.SetDoubleKey("height", selection_rect.size.height);

    std::unique_ptr<base::Value> response_value =
        std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
    response_value->SetDoubleKey(
        "status", static_cast<double>(LinkGenerationOutcome::kSuccess));
    response_value->SetKey("fragment", kTestTextFragment.ToValue());
    response_value->SetStringKey("selectedText", selected_text);
    response_value->SetKey("selectionRect", std::move(rect_value));
    return response_value;
  }

  void SetCanonicalUrl(base::Value* value, const std::string& canonical_url) {
    value->SetStringKey("canonicalUrl", canonical_url);
  }

  std::unique_ptr<base::Value> CreateErrorResponse(
      LinkGenerationOutcome outcome) {
    std::unique_ptr<base::Value> response_value =
        std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
    response_value->SetDoubleKey("status", static_cast<double>(outcome));
    return response_value;
  }

  void ValidateLinkGeneratedSuccessUkm() {
    auto entries = ukm_recorder_.GetEntriesByName(
        ukm::builders::SharedHighlights_LinkGenerated::kEntryName);
    ASSERT_EQ(1u, entries.size());
    const ukm::mojom::UkmEntry* entry = entries[0];
    EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
    ukm_recorder_.ExpectEntryMetric(entry, kSuccessUkmMetric, true);
    EXPECT_FALSE(ukm_recorder_.GetEntryMetric(entry, kErrorUkmMetric));
  }

  void ValidateLinkGeneratedErrorUkm(LinkGenerationError error) {
    auto entries = ukm_recorder_.GetEntriesByName(
        ukm::builders::SharedHighlights_LinkGenerated::kEntryName);
    ASSERT_EQ(1u, entries.size());
    const ukm::mojom::UkmEntry* entry = entries[0];
    EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
    ukm_recorder_.ExpectEntryMetric(entry, kSuccessUkmMetric, false);
    ukm_recorder_.ExpectEntryMetric(entry, kErrorUkmMetric,
                                    static_cast<int64_t>(error));
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  FakeWebState* web_state_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  web::FakeWebFramesManager* web_frames_manager_;
  web::FakeWebFrame* main_frame_;
  UIView* fake_view_;
  LinkToTextMediator* mediator_;
  UIScrollView* fake_scroll_view_;
  id mocked_consumer_;
};

// Tests that the mediator should offer link to text to HTML pages that can
// call JavaScript functions.
TEST_F(LinkToTextMediatorTest, ShouldOfferLinkToText) {
  // In the single-threaded test environment, the JS callback can't fire until
  // after the dispatched timeout, so this returns false. However, JS execution
  // is the last step, so we can verify that the right method was invoked, and
  // infer from this that all the other browser-side checks passed as well.
  [mediator_ shouldOfferLinkToText];
  EXPECT_EQ(main_frame_->GetLastJavaScriptCall(),
            "__gCrWeb.linkToText.checkPreconditions();");
}

// Tests that the mediator should not offer link to text to pages that are not
// HTML.
TEST_F(LinkToTextMediatorTest, ShouldNotOfferLinkToTextNotHTML) {
  web_state_->SetContentIsHTML(false);
  EXPECT_FALSE([mediator_ shouldOfferLinkToText]);
  // JavaScript shouldn't be invoked for non-HTML pages.
  EXPECT_EQ(main_frame_->GetLastJavaScriptCall(), "");
}

// Tests that the mediator should not offer link to text when, for some reason,
// the main frame cannot call JavaScript functions.
TEST_F(LinkToTextMediatorTest,
       ShouldNotOfferLinkToTextCannotExecuteJavaScript) {
  main_frame_->set_can_call_function(false);
  EXPECT_FALSE([mediator_ shouldOfferLinkToText]);
  EXPECT_EQ(main_frame_->GetLastJavaScriptCall(), "");
}

// Tests that the shareHighlight command is triggered with the right parameters
// when the view is not zoomed in.
TEST_F(LinkToTextMediatorTest, HandleLinkToTextSelectionTriggersCommandNoZoom) {
  base::HistogramTester histogram_tester;

  CGFloat zoom = 1;
  CGRect selection_rect = CGRectMake(100, 150, 250, 250);
  CGRect expected_client_rect = CGRectMake(150, 250, 250 + kCaretWidth, 250);

  std::unique_ptr<base::Value> fake_response =
      CreateSuccessResponse(kTestQuote, selection_rect);
  SetLinkToTextResponse(std::move(fake_response), zoom);

  __block BOOL callback_invoked = NO;

  [[mocked_consumer_ expect]
      generatedPayload:[OCMArg checkWithBlock:^BOOL(
                                   LinkToTextPayload* payload) {
        EXPECT_TRUE(kTestHighlightURL == payload.URL);
        EXPECT_EQ(kTestQuote, base::SysNSStringToUTF8(payload.selectedText));
        EXPECT_EQ(fake_view_, payload.sourceView);
        EXPECT_TRUE(
            CGRectEqualToRect(expected_client_rect, payload.sourceRect));
        callback_invoked = YES;
        return YES;
      }]];

  [mediator_ handleLinkToTextSelection];

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^BOOL {
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));

  [mocked_consumer_ verify];

  // Make sure the correct metric were recorded.
  histogram_tester.ExpectUniqueSample("SharedHighlights.LinkGenerated", true,
                                      1);
  ValidateLinkGeneratedSuccessUkm();
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.TimeToGenerate", 1);
}

// Tests that the shareHighlight command is triggered with the right parameters
// when the current view is zoomed in.
TEST_F(LinkToTextMediatorTest,
       HandleLinkToTextSelectionTriggersCommandWithZoom) {
  base::HistogramTester histogram_tester;

  CGFloat zoom = 1.5;
  CGRect selection_rect = CGRectMake(100, 150, 250, 250);
  CGRect expected_client_rect = CGRectMake(200, 325, 375 + kCaretWidth, 375);

  std::unique_ptr<base::Value> fake_response =
      CreateSuccessResponse(kTestQuote, selection_rect);
  SetLinkToTextResponse(std::move(fake_response), zoom);

  __block BOOL callback_invoked = NO;

  [[mocked_consumer_ expect]
      generatedPayload:[OCMArg checkWithBlock:^BOOL(
                                   LinkToTextPayload* payload) {
        EXPECT_TRUE(kTestHighlightURL == payload.URL);
        EXPECT_EQ(kTestQuote, base::SysNSStringToUTF8(payload.selectedText));
        EXPECT_EQ(fake_view_, payload.sourceView);
        EXPECT_TRUE(
            CGRectEqualToRect(expected_client_rect, payload.sourceRect));
        callback_invoked = YES;
        return YES;
      }]];

  [mediator_ handleLinkToTextSelection];

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^BOOL {
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));

  [mocked_consumer_ verify];

  // Make sure the correct metric were recorded.
  histogram_tester.ExpectUniqueSample("SharedHighlights.LinkGenerated", true,
                                      1);
  ValidateLinkGeneratedSuccessUkm();
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.TimeToGenerate", 1);
}

// Tests that the consumer is informed of a failure to generate a link when an
// error is returned from JavaScript.
TEST_F(LinkToTextMediatorTest, LinkGenerationError) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<base::Value> error_response =
      CreateErrorResponse(LinkGenerationOutcome::kInvalidSelection);
  SetLinkToTextResponse(std::move(error_response), /*zoom=*/1.0);

  __block BOOL callback_invoked = NO;
  [[[mocked_consumer_ expect] andDo:^(NSInvocation*) {
    callback_invoked = YES;
  }] linkGenerationFailed];

  [mediator_ handleLinkToTextSelection];

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^BOOL {
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));

  [mocked_consumer_ verify];

  // Make sure the correct metric were recorded.
  LinkGenerationError error = LinkGenerationError::kIncorrectSelector;
  histogram_tester.ExpectUniqueSample("SharedHighlights.LinkGenerated", false,
                                      1);
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                     error, 1);
  ValidateLinkGeneratedErrorUkm(error);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.Error.TimeToGenerate", 1);
}

// Tests that the consumer is informed of a failure to generate a link when an
// an empty response is returned from JavaScript.
TEST_F(LinkToTextMediatorTest, EmptyResponseLinkGenerationError) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<base::Value> empty_response = std::make_unique<base::Value>();
  SetLinkToTextResponse(std::move(empty_response), /*zoom=*/1.0);

  __block BOOL callback_invoked = NO;
  [[[mocked_consumer_ expect] andDo:^(NSInvocation*) {
    callback_invoked = YES;
  }] linkGenerationFailed];

  [mediator_ handleLinkToTextSelection];

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^BOOL {
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));

  [mocked_consumer_ verify];

  // Make sure the correct metric were recorded.
  LinkGenerationError error = LinkGenerationError::kUnknown;
  histogram_tester.ExpectUniqueSample("SharedHighlights.LinkGenerated", false,
                                      1);
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                     error, 1);
  ValidateLinkGeneratedErrorUkm(error);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.Error.TimeToGenerate", 1);
}

// Tests that the consumer is informed of a failure to generate a link when an
// a malformed response is returned from JavaScript.
TEST_F(LinkToTextMediatorTest, BadResponseLinkGenerationError) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<base::Value> malformed_response =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  malformed_response->SetStringKey("somethingElse", "abc");
  SetLinkToTextResponse(std::move(malformed_response), /*zoom=*/1.0);

  __block BOOL callback_invoked = NO;
  [[[mocked_consumer_ expect] andDo:^(NSInvocation*) {
    callback_invoked = YES;
  }] linkGenerationFailed];

  [mediator_ handleLinkToTextSelection];

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^BOOL {
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));

  [mocked_consumer_ verify];

  // Make sure the correct metric were recorded.
  LinkGenerationError error = LinkGenerationError::kUnknown;
  histogram_tester.ExpectUniqueSample("SharedHighlights.LinkGenerated", false,
                                      1);
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                     error, 1);
  ValidateLinkGeneratedErrorUkm(error);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.Error.TimeToGenerate", 1);
}

// Tests that the consumer is informed of a failure to generate a link when an
// a string response is returned from JavaScript.
TEST_F(LinkToTextMediatorTest, StringResponseLinkGenerationError) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<base::Value> string_response =
      std::make_unique<base::Value>("someValue");
  SetLinkToTextResponse(std::move(string_response), /*zoom=*/1.0);

  __block BOOL callback_invoked = NO;
  [[[mocked_consumer_ expect] andDo:^(NSInvocation*) {
    callback_invoked = YES;
  }] linkGenerationFailed];

  [mediator_ handleLinkToTextSelection];

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^BOOL {
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));

  [mocked_consumer_ verify];

  // Make sure the correct metric were recorded.
  LinkGenerationError error = LinkGenerationError::kUnknown;
  histogram_tester.ExpectUniqueSample("SharedHighlights.LinkGenerated", false,
                                      1);
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                     error, 1);
  ValidateLinkGeneratedErrorUkm(error);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.Error.TimeToGenerate", 1);
}

// Tests that the consumer is informed of a failure to generate a link when a
// success status is returned, but no payload.
TEST_F(LinkToTextMediatorTest, LinkGenerationSuccessButNoPayload) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<base::Value> success_response =
      CreateErrorResponse(LinkGenerationOutcome::kSuccess);
  SetLinkToTextResponse(std::move(success_response), /*zoom=*/1.0);

  __block BOOL callback_invoked = NO;
  [[[mocked_consumer_ expect] andDo:^(NSInvocation*) {
    callback_invoked = YES;
  }] linkGenerationFailed];

  [mediator_ handleLinkToTextSelection];

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^BOOL {
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));

  [mocked_consumer_ verify];

  // Make sure the correct metric were recorded.
  LinkGenerationError error = LinkGenerationError::kUnknown;
  histogram_tester.ExpectUniqueSample("SharedHighlights.LinkGenerated", false,
                                      1);
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                     error, 1);
  ValidateLinkGeneratedErrorUkm(error);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.Error.TimeToGenerate", 1);
}

// Tests that a timeout error is recorded when the link generation requests
// actually times out.
TEST_F(LinkToTextMediatorTest, LinkGenerationTimeout) {
  base::HistogramTester histogram_tester;

  // Set-up with any response, which doesn't matter since the mocked WebFrame
  // will simply invoke the callback with nullptr (due to a timeout).
  std::unique_ptr<base::Value> success_response =
      CreateErrorResponse(LinkGenerationOutcome::kSuccess);
  SetLinkToTextResponse(std::move(success_response), /*zoom=*/1.0);

  main_frame_->set_force_timeout(true);

  __block BOOL callback_invoked = NO;
  [[[mocked_consumer_ expect] andDo:^(NSInvocation*) {
    callback_invoked = YES;
  }] linkGenerationFailed];

  [mediator_ handleLinkToTextSelection];

  // Advance time to skip waiting for the timeout.
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(
      link_to_text::kLinkGenerationTimeoutInMs + 10));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^BOOL {
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));

  [mocked_consumer_ verify];

  // Make sure the correct metric were recorded.
  LinkGenerationError error = LinkGenerationError::kTimeout;
  histogram_tester.ExpectUniqueSample("SharedHighlights.LinkGenerated", false,
                                      1);
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                     error, 1);
  ValidateLinkGeneratedErrorUkm(error);
  histogram_tester.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.Error.TimeToGenerate", 1);
}

// Tests that a canonical URL is being used as base for the generated link when
// the current page is HTTPS.
TEST_F(LinkToTextMediatorTest, WithHttpsAndCanonicalUrl) {
  CGFloat zoom = 1;
  CGRect selection_rect = CGRectMake(100, 150, 250, 250);

  std::unique_ptr<base::Value> fake_response =
      CreateSuccessResponse(kTestQuote, selection_rect);
  std::string canonical_url = "https://www.example.com/";
  SetCanonicalUrl(fake_response.get(), canonical_url);
  SetLinkToTextResponse(std::move(fake_response), zoom);

  __block BOOL callback_invoked = NO;

  [[mocked_consumer_ expect]
      generatedPayload:[OCMArg checkWithBlock:^BOOL(
                                   LinkToTextPayload* payload) {
        // Validate that the generated URL is based on the canonical URL.
        EXPECT_TRUE(payload.URL.is_valid());
        EXPECT_TRUE(GURL(canonical_url).EqualsIgnoringRef(payload.URL));
        callback_invoked = YES;
        return YES;
      }]];

  [mediator_ handleLinkToTextSelection];

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^BOOL {
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));

  [mocked_consumer_ verify];
}

// Tests that a canonical URL is not being used as base for the generated link
// when the current page is not HTTPS.
TEST_F(LinkToTextMediatorTest, NotHttpsAndCanonicalUrl) {
  CGFloat zoom = 1;
  CGRect selection_rect = CGRectMake(100, 150, 250, 250);

  // Set WebState's URL to something not HTTPS.
  GURL new_base_url("http://chromium.org");
  web_state_->SetCurrentURL(new_base_url);

  std::unique_ptr<base::Value> fake_response =
      CreateSuccessResponse(kTestQuote, selection_rect);
  std::string canonical_url = "https://www.example.com/";
  SetCanonicalUrl(fake_response.get(), canonical_url);
  SetLinkToTextResponse(std::move(fake_response), zoom);

  __block BOOL callback_invoked = NO;

  [[mocked_consumer_ expect]
      generatedPayload:[OCMArg checkWithBlock:^BOOL(
                                   LinkToTextPayload* payload) {
        // Validate that the generated URL is not based on the canonical URL.
        EXPECT_TRUE(payload.URL.is_valid());
        EXPECT_TRUE(new_base_url.EqualsIgnoringRef(payload.URL));
        callback_invoked = YES;
        return YES;
      }]];

  [mediator_ handleLinkToTextSelection];

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^BOOL {
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));

  [mocked_consumer_ verify];
}
