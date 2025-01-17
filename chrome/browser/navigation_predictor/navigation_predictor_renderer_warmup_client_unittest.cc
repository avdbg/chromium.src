// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_renderer_warmup_client.h"

#include <memory>
#include <vector>

#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kGoogleSearchURL[] = "https://www.google.com/search?q=test";
constexpr char kOriginA[] = "https://a.test";
constexpr char kOriginB[] = "https://b.test";

const base::Feature kNavigationPredictorRendererWarmup{
    "NavigationPredictorRendererWarmup", base::FEATURE_DISABLED_BY_DEFAULT};

NavigationPredictorKeyedService::Prediction CreateValidPrediction(
    content::WebContents* web_contents,
    const GURL& src_url,
    const std::vector<GURL>& predicted_urls) {
  return NavigationPredictorKeyedService::Prediction(
      web_contents, src_url, base::nullopt,
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      predicted_urls);
}

class TestNavigationPredictorRendererWarmupClient
    : public NavigationPredictorRendererWarmupClient {
 public:
  TestNavigationPredictorRendererWarmupClient(Profile* profile,
                                              const base::TickClock* clock)
      : NavigationPredictorRendererWarmupClient(profile, clock) {}
  ~TestNavigationPredictorRendererWarmupClient() override = default;

  bool DidDoRendererWarmup() const {
    base::RunLoop().RunUntilIdle();
    return did_renderer_warmup_;
  }

  void Reset() { did_renderer_warmup_ = false; }

  void SetBrowserHasSpareRenderer(bool has_spare) { has_spare_ = has_spare; }

 protected:
  void DoRendererWarmpup() override { did_renderer_warmup_ = true; }
  bool BrowserHasSpareRenderer() const override { return has_spare_; }

 private:
  bool did_renderer_warmup_ = false;
  bool has_spare_ = false;
};

// Each set of tests that use the same features will be implemented by a
// subclass of this class. See comment for |scoped_feature_list_| below.
// Running with gtest_filter=NavigationPredictorRenderer*.* will run all
// the tests in this file.
class NavigationPredictorRendererWarmupClientTestBase
    : public ChromeRenderViewHostTestHarness {
 public:
  NavigationPredictorRendererWarmupClientTestBase() = default;
  ~NavigationPredictorRendererWarmupClientTestBase() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

    // Set the clock to a non-zero value.
    clock_.SetNowTicks(base::TimeTicks::Now());
  }

  void VerifyNoUKM() {
    auto entries = ukm_recorder_.GetEntriesByName(
        ukm::builders::NavigationPredictorRendererWarmup::kEntryName);
    EXPECT_TRUE(entries.empty());
  }

  void VerifyUKMEntry(const std::string& metric_name,
                      base::Optional<int64_t> expected_value,
                      size_t entry_index = 0) {
    std::string format = "metric_name=%s, index=%zu";
    SCOPED_TRACE(
        base::StringPrintf(format.c_str(), metric_name.c_str(), entry_index));

    auto entries = ukm_recorder_.GetEntriesByName(
        ukm::builders::NavigationPredictorRendererWarmup::kEntryName);
    ASSERT_EQ(entry_index + 1, entries.size());

    const auto* entry = entries[entry_index];
    const int64_t* value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);
    EXPECT_EQ(value != nullptr, expected_value.has_value());

    if (!expected_value.has_value())
      return;

    EXPECT_EQ(*value, expected_value.value());
  }

  TestNavigationPredictorRendererWarmupClient* client() {
    if (!client_) {
      client_ = std::make_unique<TestNavigationPredictorRendererWarmupClient>(
          profile(), &clock_);
    }
    return client_.get();
  }

  base::SimpleTestTickClock* clock() { return &clock_; }

 protected:
  // This needs to be initialized by child classes before
  // NavigationPredictorRendererWarmupClientTestBase::SetUp() is called.
  // SetUp() runs some code that checks if features are enabled. Thus, to avoid
  // tsan-reported data races on the feature list, we need to initialize
  // |scoped_feature_list_| first.
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  base::SimpleTestTickClock clock_;
  std::unique_ptr<TestNavigationPredictorRendererWarmupClient> client_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
};

class NavigationPredictorRendererWarmupClientTest
    : public NavigationPredictorRendererWarmupClientTestBase {
 public:
  NavigationPredictorRendererWarmupClientTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kNavigationPredictorRendererWarmup,
        {
            {"counterfactual", "false"},
            {"mem_threshold_mb", "0"},
            {"warmup_on_dse", "true"},
            {"use_navigation_predictions", "true"},
            {"examine_top_n_predictions", "10"},
            {"prediction_crosss_origin_threshold", "0.5"},
            {"cooldown_duration_ms", "60000"},
        });
  }
  ~NavigationPredictorRendererWarmupClientTest() override = default;
};

TEST_F(NavigationPredictorRendererWarmupClientTest, SuccessCase_Search) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kGoogleSearchURL), {}));
  EXPECT_TRUE(client()->DidDoRendererWarmup());

  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, base::nullopt);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 1);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 1);
}

TEST_F(NavigationPredictorRendererWarmupClientTest, SuccessCase_CrossOrigin) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kOriginA), {GURL(kOriginB)}));
  EXPECT_TRUE(client()->DidDoRendererWarmup());

  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, 100);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 1);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 0);
}

TEST_F(NavigationPredictorRendererWarmupClientTest, NullPrediction) {
  client()->OnPredictionUpdated(base::nullopt);
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  VerifyNoUKM();
}

TEST_F(NavigationPredictorRendererWarmupClientTest, NoWebContents) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(nullptr, GURL(kOriginA), {GURL(kOriginB)}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  VerifyNoUKM();
}

TEST_F(NavigationPredictorRendererWarmupClientTest, BadPredictionSrc) {
  client()->OnPredictionUpdated(NavigationPredictorKeyedService::Prediction(
      nullptr, base::nullopt, std::vector<std::string>{""},
      NavigationPredictorKeyedService::PredictionSource::kExternalAndroidApp,
      {}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  VerifyNoUKM();
}

TEST_F(NavigationPredictorRendererWarmupClientTest, CoolDown) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kGoogleSearchURL), {}));
  EXPECT_TRUE(client()->DidDoRendererWarmup());
  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, base::nullopt,
                 /*entry_index=*/0);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 1, /*entry_index=*/0);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0,
                 /*entry_index=*/0);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 1, /*entry_index=*/0);

  client()->Reset();

  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kGoogleSearchURL), {}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, base::nullopt,
                 /*entry_index=*/1);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 0, /*entry_index=*/1);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0b0001,
                 /*entry_index=*/1);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 1, /*entry_index=*/1);
}

TEST_F(NavigationPredictorRendererWarmupClientTest, HasSpareRenderer) {
  client()->SetBrowserHasSpareRenderer(true);
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kGoogleSearchURL), {}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, base::nullopt);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 0);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0b0010);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 1);
}

TEST_F(NavigationPredictorRendererWarmupClientTest, NotSearchURL) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL("http://test.com/"), {}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, base::nullopt);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 0);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 0);
}

TEST_F(NavigationPredictorRendererWarmupClientTest, InvalidCrossOrigins) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kOriginA), {GURL()}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, 0);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 0);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 0);
}

TEST_F(NavigationPredictorRendererWarmupClientTest, NonHTTPCrossOrigins) {
  client()->OnPredictionUpdated(CreateValidPrediction(
      web_contents(), GURL(kOriginA), {GURL("ftp://test.com")}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, 0);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 0);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 0);
}

TEST_F(NavigationPredictorRendererWarmupClientTest,
       CrossOriginsBelowThreshold) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kOriginA),
                            {GURL(kOriginA), GURL(kOriginA), GURL(kOriginB)}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, 33);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 0);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 0);
}

class NavigationPredictorRendererAfterCooldownTest
    : public NavigationPredictorRendererWarmupClientTestBase {
 public:
  NavigationPredictorRendererAfterCooldownTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kNavigationPredictorRendererWarmup,
        {
            {"counterfactual", "false"},
            {"mem_threshold_mb", "0"},
            {"warmup_on_dse", "true"},
            {"use_navigation_predictions", "true"},
            {"examine_top_n_predictions", "10"},
            {"prediction_crosss_origin_threshold", "0.5"},
            {"cooldown_duration_ms", "100"},
        });
  }
  ~NavigationPredictorRendererAfterCooldownTest() override = default;
};

TEST_F(NavigationPredictorRendererAfterCooldownTest, SuccessCase) {
  base::test::ScopedFeatureList scoped_feature_list;
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kGoogleSearchURL), {}));
  EXPECT_TRUE(client()->DidDoRendererWarmup());

  // Verify first UKM entry.
  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, base::nullopt,
                 /*entry_index=*/0);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 1, /*entry_index=*/0);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0,
                 /*entry_index=*/0);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 1, /*entry_index=*/0);

  client()->Reset();

  clock()->Advance(base::TimeDelta::FromMilliseconds(101));

  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kGoogleSearchURL), {}));
  EXPECT_TRUE(client()->DidDoRendererWarmup());

  // Verify second UKM entry.
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, base::nullopt,
                 /*entry_index=*/1);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 1, /*entry_index=*/1);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0,
                 /*entry_index=*/1);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 1, /*entry_index=*/1);
}

class NavigationPredictorRendererWarmupTest
    : public NavigationPredictorRendererWarmupClientTestBase {
 public:
  NavigationPredictorRendererWarmupTest() {
    scoped_feature_list_.InitAndDisableFeature(
        kNavigationPredictorRendererWarmup);
  }
  ~NavigationPredictorRendererWarmupTest() override = default;
};

TEST_F(NavigationPredictorRendererWarmupTest, FeatureOff) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kGoogleSearchURL), {}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  VerifyNoUKM();
}

class NavigationPredictorRendererDSEWarmupTest
    : public NavigationPredictorRendererWarmupClientTestBase {
 public:
  NavigationPredictorRendererDSEWarmupTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kNavigationPredictorRendererWarmup,
        {
            {"counterfactual", "false"},
            {"mem_threshold_mb", "0"},
            {"warmup_on_dse", "false"},
            {"use_navigation_predictions", "true"},
            {"examine_top_n_predictions", "10"},
            {"prediction_crosss_origin_threshold", "0.5"},
            {"cooldown_duration_ms", "60000"},
        });
  }
  ~NavigationPredictorRendererDSEWarmupTest() override = default;
};

TEST_F(NavigationPredictorRendererDSEWarmupTest, DSEWarmupNotEnabled) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kGoogleSearchURL), {}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, base::nullopt);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 0);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 1);
}

class NavigationPredictorRendererCrossOriginTest
    : public NavigationPredictorRendererWarmupClientTestBase {
 public:
  NavigationPredictorRendererCrossOriginTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kNavigationPredictorRendererWarmup,
        {
            {"counterfactual", "false"},
            {"mem_threshold_mb", "0"},
            {"warmup_on_dse", "true"},
            {"use_navigation_predictions", "false"},
            {"examine_top_n_predictions", "10"},
            {"prediction_crosss_origin_threshold", "0.5"},
            {"cooldown_duration_ms", "60000"},
        });
  }
  ~NavigationPredictorRendererCrossOriginTest() override = default;
};

TEST_F(NavigationPredictorRendererCrossOriginTest, CrossOriginNotEnabled) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kOriginA), {GURL(kOriginB)}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, 100);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 0);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 0);
}

class NavigationPredictorRendererCounterfactualTest
    : public NavigationPredictorRendererWarmupClientTestBase {
 public:
  NavigationPredictorRendererCounterfactualTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kNavigationPredictorRendererWarmup,
        {
            {"counterfactual", "true"},
            {"mem_threshold_mb", "0"},
            {"warmup_on_dse", "true"},
            {"use_navigation_predictions", "true"},
            {"examine_top_n_predictions", "10"},
            {"prediction_crosss_origin_threshold", "0.5"},
            {"cooldown_duration_ms", "60000"},
        });
  }
  ~NavigationPredictorRendererCounterfactualTest() override = default;
};

TEST_F(NavigationPredictorRendererCounterfactualTest, CounterfactualEnabled) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kGoogleSearchURL), {}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, base::nullopt);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 1);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 1);
}

class NavigationPredictorRendererMemThresholdTest
    : public NavigationPredictorRendererWarmupClientTestBase {
 public:
  NavigationPredictorRendererMemThresholdTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kNavigationPredictorRendererWarmup,
        {
            {"counterfactual", "true"},
            {"mem_threshold_mb", "999999999"},
            {"warmup_on_dse", "true"},
            {"use_navigation_predictions", "true"},
            {"examine_top_n_predictions", "10"},
            {"prediction_crosss_origin_threshold", "0.5"},
            {"cooldown_duration_ms", "60000"},
        });
  }
  ~NavigationPredictorRendererMemThresholdTest() override = default;
};

TEST_F(NavigationPredictorRendererMemThresholdTest, NonZeroThreshold) {
  client()->OnPredictionUpdated(
      CreateValidPrediction(web_contents(), GURL(kGoogleSearchURL), {}));
  EXPECT_FALSE(client()->DidDoRendererWarmup());

  using UkmEntry = ukm::builders::NavigationPredictorRendererWarmup;
  VerifyUKMEntry(UkmEntry::kCrossOriginLinksRatioName, base::nullopt);
  VerifyUKMEntry(UkmEntry::kDidWarmupName, 0);
  VerifyUKMEntry(UkmEntry::kPageIndependentStatusBitMaskName, 0b0100);
  VerifyUKMEntry(UkmEntry::kWasDSESRPName, 1);
}

}  // namespace
