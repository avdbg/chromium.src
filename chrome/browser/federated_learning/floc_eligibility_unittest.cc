// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/federated_learning/floc_eligibility_observer.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/page_load_metrics/observers/ad_metrics/floc_page_load_metrics_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/federated_learning/floc_sorting_lsh_clusters_service.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_service.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"

namespace federated_learning {

class FlocEligibilityUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  FlocEligibilityUnitTest() = default;
  ~FlocEligibilityUnitTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    TestingBrowserProcess::GetGlobal()->SetFlocSortingLshClustersService(
        std::make_unique<federated_learning::FlocSortingLshClustersService>());

    ASSERT_TRUE(profile()->CreateHistoryService());

    InitWebContents();

    tester_ =
        std::make_unique<page_load_metrics::PageLoadMetricsObserverTester>(
            GetWebContents(), this,
            base::BindRepeating(&FlocEligibilityUnitTest::RegisterObservers,
                                base::Unretained(this)));
  }

  // Can be overridden in child class to initialize an incognito web_contents.
  virtual void InitWebContents() {}

  virtual content::WebContents* GetWebContents() { return web_contents(); }

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::EXPLICIT_ACCESS);
  }

  bool IsUrlVisitEligibleToComputeFloc(const GURL& url) {
    history::QueryURLResult result = QueryUrl(url);
    EXPECT_EQ(1u, result.visits.size());
    return result.visits[0].floc_allowed;
  }

  history::QueryURLResult QueryUrl(const GURL& url) {
    history::QueryURLResult query_url_result;

    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    history_service()->QueryURL(
        url, /*want_visits=*/true,
        base::BindLambdaForTesting([&](history::QueryURLResult result) {
          query_url_result = std::move(result);
          run_loop.Quit();
        }),
        &tracker);
    run_loop.Run();

    return query_url_result;
  }

  void SimulateResourceDataUseUpdate(bool is_ad_resource) {
    page_load_metrics::mojom::ResourceDataUpdatePtr resource =
        page_load_metrics::mojom::ResourceDataUpdate::New();
    resource->reported_as_ad_resource = is_ad_resource;
    resource->received_data_length = 1;

    std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
    resources.push_back(std::move(resource));

    tester_->SimulateResourceDataUseUpdate(resources,
                                           GetWebContents()->GetMainFrame());
  }

  void NavigateToPage(const GURL& url,
                      bool publicly_routable,
                      bool floc_feature_policy_enabled) {
    auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
        url, GetWebContents());
    simulator->SetTransition(ui::PageTransition::PAGE_TRANSITION_TYPED);

    if (!publicly_routable) {
      net::IPAddress address;
      EXPECT_TRUE(address.AssignFromIPLiteral("0.0.0.0"));
      simulator->SetSocketAddress(net::IPEndPoint(address, /*port=*/0));
    }

    if (!floc_feature_policy_enabled) {
      simulator->SetFeaturePolicyHeader(
          {{blink::mojom::FeaturePolicyFeature::kInterestCohort,
            /*values=*/{}, /*matches_all_origins=*/false,
            /*matches_opaque_src=*/false}});
    }

    simulator->Commit();

    history_service()->AddPage(
        url, base::Time::Now(),
        history::ContextIDForWebContents(GetWebContents()),
        GetWebContents()
            ->GetController()
            .GetLastCommittedEntry()
            ->GetUniqueID(),
        /*referrer=*/GURL(),
        /*redirects=*/{}, ui::PageTransition::PAGE_TRANSITION_TYPED,
        history::VisitSource::SOURCE_BROWSED,
        /*did_replace_entry=*/false,
        /*floc_allowed=*/false);
  }

  FlocEligibilityObserver* GetFlocEligibilityObserver() {
    return FlocEligibilityObserver::GetOrCreateForCurrentDocument(
        GetWebContents()->GetMainFrame());
  }

 private:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) {
    auto floc_plm_observer = std::make_unique<FlocPageLoadMetricsObserver>();
    floc_plm_observer_ = floc_plm_observer.get();
    tracker->AddObserver(std::move(floc_plm_observer));
  }

  FlocPageLoadMetricsObserver* floc_plm_observer_;
  std::unique_ptr<page_load_metrics::PageLoadMetricsObserverTester> tester_;
};

TEST_F(FlocEligibilityUnitTest, OnInterestCohortApiUsed) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*floc_feature_policy_enabled=*/true);

  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(url));

  GetFlocEligibilityObserver()->OnInterestCohortApiUsed();
  EXPECT_TRUE(IsUrlVisitEligibleToComputeFloc(url));
}

TEST_F(FlocEligibilityUnitTest, OnAdResourceObserved) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*floc_feature_policy_enabled=*/true);

  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(url));

  SimulateResourceDataUseUpdate(/*is_ad_resource=*/true);
  EXPECT_TRUE(IsUrlVisitEligibleToComputeFloc(url));
}

TEST_F(FlocEligibilityUnitTest, OnNonAdResourceObserved) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*floc_feature_policy_enabled=*/true);

  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(url));

  SimulateResourceDataUseUpdate(/*is_ad_resource=*/false);
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(url));
}

TEST_F(FlocEligibilityUnitTest, StopObservingPrivateIP) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/false,
                 /*floc_feature_policy_enabled=*/true);

  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(url));

  SimulateResourceDataUseUpdate(/*is_ad_resource=*/true);
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(url));

  GetFlocEligibilityObserver()->OnInterestCohortApiUsed();
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(url));
}

TEST_F(FlocEligibilityUnitTest, StopObservingFlocFeaturePolicyDisabled) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*floc_feature_policy_enabled=*/false);

  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(url));

  SimulateResourceDataUseUpdate(/*is_ad_resource=*/true);
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(url));

  GetFlocEligibilityObserver()->OnInterestCohortApiUsed();
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(url));
}

class FlocEligibilityIncognitoUnitTest : public FlocEligibilityUnitTest {
 public:
  FlocEligibilityIncognitoUnitTest() = default;
  ~FlocEligibilityIncognitoUnitTest() override = default;

  void InitWebContents() override {
    TestingProfile::Builder().BuildIncognito(profile());
    incognito_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile()->GetPrimaryOTRProfile(),
        content::SiteInstance::Create(profile()->GetPrimaryOTRProfile()));
  }

  content::WebContents* GetWebContents() override {
    return incognito_web_contents_.get();
  }

  void TearDown() override {
    incognito_web_contents_.reset();
    FlocEligibilityUnitTest::TearDown();
  }

 private:
  std::unique_ptr<content::WebContents> incognito_web_contents_;
};

TEST_F(FlocEligibilityIncognitoUnitTest, SkipSettingFlocAllowedInIncognito) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*floc_feature_policy_enabled=*/true);

  SimulateResourceDataUseUpdate(/*is_ad_resource=*/true);

  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(url));
}

}  // namespace federated_learning
