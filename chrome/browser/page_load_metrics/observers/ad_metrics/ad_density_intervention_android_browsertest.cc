// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/widget/screen_info.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace {

const char kAdsInterventionRecordedHistogram[] =
    "SubresourceFilter.PageLoad.AdsInterventionTriggered";

// Gets the body height of the document embedded in |web_contents|.
int GetDocumentHeight(content::WebContents* web_contents) {
  return EvalJs(web_contents, "document.body.scrollHeight").ExtractInt();
}

// Scales the rect by the web content's render widget host's device scale
// factor.
gfx::Rect ScaleRectByDeviceScaleFactor(const gfx::Rect& rect,
                                       content::WebContents* web_contents) {
  blink::ScreenInfo screen_info;
  web_contents->GetRenderWidgetHostView()->GetScreenInfo(&screen_info);
  return gfx::ScaleToRoundedRect(rect, screen_info.device_scale_factor);
}

void CreateAndWaitForIframeAtRect(
    content::WebContents* web_contents,
    page_load_metrics::PageLoadMetricsTestWaiter* waiter,
    net::EmbeddedTestServer* embedded_test_server,
    const gfx::Rect& rect) {
  // The intersections returned by the renderer are scaled to the device's
  // scale factor.
  gfx::Rect scaled_rect = ScaleRectByDeviceScaleFactor(rect, web_contents);

  // The renderer propagates values scaled by the device scale factor.
  // Wait on these values.
  waiter->AddMainFrameIntersectionExpectation(scaled_rect);

  // Create the frame with b.com as origin to not get caught by
  // restricted ad tagging.
  EXPECT_TRUE(ExecJs(
      web_contents,
      content::JsReplace(
          "let frame = createAdIframeAtRect($1, $2, $3, $4); "
          "frame.src = $5",
          rect.x(), rect.y(), rect.width(), rect.height(),
          embedded_test_server->GetURL("b.com", "/ads_observer/pixel.png")
              .spec())));

  waiter->Wait();
}

}  // namespace

class AdDensityViolationBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdDensityViolationBrowserTest() = default;

  void SetUp() override {
    std::vector<base::Feature> enabled = {
        subresource_filter::kAdTagging,
        subresource_filter::kAdsInterventionsEnforced};
    std::vector<base::Feature> disabled = {};

    feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    AdDensityViolationBrowserTest,
    MobilePageAdDensityByHeightAbove30_AdInterventionTriggered) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents);
  const GURL url(embedded_test_server()->GetURL(
      "a.com", "/ads_observer/blank_with_adiframe_writer.html"));

  waiter->AddMainFrameIntersectionExpectation();
  EXPECT_TRUE(content::NavigateToURL(web_contents, url));
  waiter->Wait();

  int document_height = GetDocumentHeight(web_contents);

  int frame_width = 100;  // Ad density by height is independent of frame width.
  int frame_height = document_height * 0.45;

  CreateAndWaitForIframeAtRect(web_contents, waiter.get(),
                               embedded_test_server(),
                               gfx::Rect(0, 0, frame_width, frame_height));

  // Delete the page load metrics test waiter instead of reinitializing it
  // for the next page load.
  waiter.reset();

  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  // blank_with_adiframe_writer loads a script tagged as an ad, verify it is not
  // loaded and the subresource filter UI for ad blocking is shown.
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));
  EXPECT_EQ(InfoBarService::FromWebContents(web_contents)->infobar_count(), 1u);
  EXPECT_EQ(InfoBarService::FromWebContents(web_contents)
                ->infobar_at(0)
                ->delegate()
                ->GetIdentifier(),
            infobars::InfoBarDelegate::ADS_BLOCKED_INFOBAR_DELEGATE_ANDROID);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30),
      1);
}

IN_PROC_BROWSER_TEST_F(
    AdDensityViolationBrowserTest,
    MobilePageAdDensityByHeightBelow30_AdInterventionNotTriggered) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents);
  const GURL url(embedded_test_server()->GetURL(
      "a.com", "/ads_observer/blank_with_adiframe_writer.html"));

  waiter->AddMainFrameIntersectionExpectation();
  EXPECT_TRUE(content::NavigateToURL(web_contents, url));
  waiter->Wait();

  int document_height = GetDocumentHeight(web_contents);

  int frame_width = 100;  // Ad density by height is independent of frame width.
  int frame_height = document_height * 0.25;

  CreateAndWaitForIframeAtRect(web_contents, waiter.get(),
                               embedded_test_server(),
                               gfx::Rect(0, 0, frame_width, frame_height));

  // Delete the page load metrics test waiter instead of reinitializing it
  // for the next page load.
  waiter.reset();

  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  // blank_with_adiframe_writer loads a script tagged as an ad, verify it is
  // loaded as ads are not blocked and the subresource filter UI is not
  // shown.
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));

  // No ads blocked infobar should be shown as we have not triggered the
  // intervention.
  EXPECT_EQ(InfoBarService::FromWebContents(web_contents)->infobar_count(), 0u);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
}

class AdDensityViolationBrowserTestWithoutEnforcement
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdDensityViolationBrowserTestWithoutEnforcement() = default;

  void SetUp() override {
    std::vector<base::Feature> enabled = {subresource_filter::kAdTagging};
    std::vector<base::Feature> disabled = {
        subresource_filter::kAdsInterventionsEnforced};

    feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    AdDensityViolationBrowserTestWithoutEnforcement,
    MobilePageAdDensityByHeightAbove30_NoAdInterventionTriggered) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents);
  const GURL url(embedded_test_server()->GetURL(
      "a.com", "/ads_observer/blank_with_adiframe_writer.html"));

  waiter->AddMainFrameIntersectionExpectation();
  EXPECT_TRUE(content::NavigateToURL(web_contents, url));
  waiter->Wait();

  int document_height = GetDocumentHeight(web_contents);

  int frame_width = 100;  // Ad density by height is independent of frame width.
  int frame_height = document_height * 0.45;

  CreateAndWaitForIframeAtRect(web_contents, waiter.get(),
                               embedded_test_server(),
                               gfx::Rect(0, 0, frame_width, frame_height));

  // Delete the page load metrics test waiter instead of reinitializing it
  // for the next page load.
  waiter.reset();

  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  // We are not enforcing ad blocking on ads violations, site should load
  // as expected without subresource filter UI.
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));

  // No ads blocked infobar should be shown as we have not triggered the
  // intervention.
  EXPECT_EQ(InfoBarService::FromWebContents(web_contents)->infobar_count(), 0u);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30),
      1);
}
