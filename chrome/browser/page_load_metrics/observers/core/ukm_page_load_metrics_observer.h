// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CORE_UKM_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CORE_UKM_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/memories/core/visit_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_event.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/site_instance_process_assignment.h"
#include "net/http/http_response_info.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "ui/base/page_transition_types.h"

namespace content {
class BrowserContext;
}

namespace network {
class NetworkQualityTracker;
}

namespace ukm {
namespace builders {
class PageLoad;
}
}  // namespace ukm

// This enum represents the type of page load: abort, non-abort, or neither.
// A page is of type NEVER_FOREGROUND if it was never in the foreground.
// A page is of type ABORT if it was in the foreground at some point but did not
// reach FCP. A page is of type REACHED_FCP if it was in the foreground at some
// point and reached FCP. These values are persisted to logs. Entries should not
// be renumbered and numeric values should never be reused. For any additions,
// also update the corresponding enum in enums.xml.
enum class PageLoadType {
  kNeverForegrounded = 0,
  kAborted = 1,
  kReachedFCP = 2,
  kMaxValue = kReachedFCP,
};

// If URL-Keyed-Metrics (UKM) is enabled in the system, this is used to
// populate it with top-level page-load metrics.
class UkmPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // Returns a UkmPageLoadMetricsObserver, or nullptr if it is not needed.
  static std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
  CreateIfNeeded();

  explicit UkmPageLoadMetricsObserver(
      network::NetworkQualityTracker* network_quality_tracker);
  ~UkmPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;

  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;

  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;

  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  ObservePolicy OnShown() override;

  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info)
      override;

  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnResourceDataUseObserved(
      content::RenderFrameHost* content,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;

  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override;

  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void SetUpSharedMemoryForSmoothness(
      const base::ReadOnlySharedMemoryRegion& shared_memory) override;

  void OnCpuTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::CpuTiming& timing) override;

  void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                 int behavior_flags) override;

  void OnEventOccurred(page_load_metrics::PageLoadMetricsEvent event) override;

  void DidActivatePortal(base::TimeTicks activation_time) override;

  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  // Whether the current page load is an Offline Preview. Must be called from
  // OnCommit. Virtual for testing.
  virtual bool IsOfflinePreview(content::WebContents* web_contents) const;

 private:
  void RecordNavigationTimingMetrics();

  // Records page load timing related metrics available in PageLoadTiming, such
  // as first contentful paint.
  void RecordTimingMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing);

  // Records page load internal timing metrics, which are used for debugging.
  void RecordInternalTimingMetrics(
      const page_load_metrics::ContentfulPaintTimingInfo&
          all_frames_largest_contentful_paint,
      const page_load_metrics::ContentfulPaintTimingInfo&
          all_frames_experimental_largest_contentful_paint);

  // Records metrics based on the page load information exposed by the observer
  // delegate, as well as updating the URL. |app_background_time| should be set
  // to a timestamp if the app was backgrounded, otherwise it should be set to
  // a null TimeTicks.
  void RecordPageLoadMetrics(base::TimeTicks app_background_time);

  // Records metrics related to how the renderer process was used for the
  // navigation.
  void RecordRendererUsageMetrics();

  // Adds main resource timing metrics to |builder|.
  void ReportMainResourceTimingMetrics(ukm::builders::PageLoad& builder);

  void ReportLayoutStability();

  void ReportPerfectHeuristicsMetrics();

  void RecordAbortMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      base::TimeTicks page_end_time,
      ukm::builders::PageLoad* builder);

  void RecordMemoriesMetrics(ukm::builders::PageLoad& builder);

  void RecordInputTimingMetrics();
  void RecordSmoothnessMetrics();

  void RecordMobileFriendlinessMetrics();

  // Captures the site engagement score for the committed URL and
  // returns the score rounded to the nearest 10.
  base::Optional<int64_t> GetRoundedSiteEngagementScore() const;

  // Returns whether third party cookie blocking is enabled for the committed
  // URL. This is only recorded for users who have prefs::kCookieControlsEnabled
  // set to true.
  base::Optional<bool> GetThirdPartyCookieBlockingEnabled() const;

  // Records the metrics for the nostate prefetch to an event with UKM source ID
  // |source_id|.
  void RecordNoStatePrefetchMetrics(
      content::NavigationHandle* navigation_handle,
      ukm::SourceId source_id);

  // Records the metrics related to Generate URLs (Home page, default search
  // engine) for starting URL and committed URL.
  void RecordGeneratedNavigationUKM(ukm::SourceId source_id,
                                    const GURL& committed_url);

  // Records some metrics at the end of a page, even for failed provisional
  // loads.
  void RecordPageEndMetrics(
      const page_load_metrics::mojom::PageLoadTiming* timing,
      base::TimeTicks page_end_time,
      bool app_entered_background);

  // Records a score from the SiteEngagementService. Called when the page
  // becomes hidden, or at the end of the session if the page is never hidden.
  void RecordSiteEngagement() const;

  // Guaranteed to be non-null during the lifetime of |this|.
  network::NetworkQualityTracker* network_quality_tracker_;

  // The number of body (not header) prefilter bytes consumed by requests for
  // the page.
  int64_t cache_bytes_ = 0;
  int64_t network_bytes_ = 0;

  // Sum of decoded body lengths of JS resources in bytes.
  int64_t js_decoded_bytes_ = 0;

  // Max decoded body length of JS resources in bytes.
  int64_t js_max_decoded_bytes_ = 0;

  // Network data use broken down by resource type.
  int64_t image_total_bytes_ = 0;
  int64_t image_subframe_bytes_ = 0;
  int64_t media_bytes_ = 0;

  // Network quality estimates.
  net::EffectiveConnectionType effective_connection_type_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  base::Optional<int32_t> http_response_code_;
  base::Optional<base::TimeDelta> http_rtt_estimate_;
  base::Optional<base::TimeDelta> transport_rtt_estimate_;
  base::Optional<int32_t> downstream_kbps_estimate_;

  // Total CPU wall time used by the page while in the foreground.
  base::TimeDelta total_foreground_cpu_time_;

  // Load timing metrics of the main frame resource request.
  content::NavigationHandleTiming navigation_handle_timing_;
  base::Optional<net::LoadTimingInfo> main_frame_timing_;

  // How the SiteInstance for the committed page was assigned a renderer.
  base::Optional<content::SiteInstanceProcessAssignment>
      render_process_assignment_;

  // PAGE_TRANSITION_LINK is the default PageTransition value.
  ui::PageTransition page_transition_ = ui::PAGE_TRANSITION_LINK;

  // True if the page started hidden, or ever became hidden.
  bool was_hidden_ = false;

  // True if the page main resource was served from disk cache.
  bool was_cached_ = false;

  // Whether the first URL in the redirect chain matches the default search
  // engine template.
  bool start_url_is_default_search_ = false;

  // Whether the first URL in the redirect chain matches the user's home page
  // URL.
  bool start_url_is_home_page_ = false;

  // The number of main frame redirects that occurred before commit.
  uint32_t main_frame_request_redirect_count_ = 0;

  // The browser context this navigation is operating in.
  content::BrowserContext* browser_context_ = nullptr;

  // Whether the navigation resulted in the main frame being hosted in
  // a different process.
  bool navigation_is_cross_process_ = false;

  // True if this page was loaded in a portal and never activated. UKMs are
  // not recorded for this page unless the portal is activated.
  bool is_portal_ = false;

  // Difference between indices of the previous and current navigation entries
  // (i.e. item history for the current tab).
  // Typically -1/0/1 for back navigations / reloads / forward navigations.
  // 0 for most of navigations with replacement (e.g. location.replace).
  // 1 for regular navigations (link click / omnibox / etc).
  int navigation_entry_offset_ = 0;

  // Id for the main document, which persists across history navigations to the
  // same document.
  // Unique across the lifetime of the browser process.
  int main_document_sequence_number_ = -1;

  // These are to capture observed LoadingBehaviorFlags.
  bool delay_async_script_execution_before_finished_parsing_seen_ = false;
  bool delay_competing_low_priority_requests_seen_ = false;

  bool currently_in_foreground_ = false;
  // The last time the page became foregrounded, or navigation start if the page
  // started in the foreground and has not been backgrounded.
  base::TimeTicks last_time_shown_;
  base::TimeDelta total_foreground_duration_;

  // The connection info for the committed URL.
  base::Optional<net::HttpResponseInfo::ConnectionInfo> connection_info_;

  base::ReadOnlySharedMemoryMapping ukm_smoothness_data_;

  // Collected by this observer during the page lifetime. Shipped to UKM and
  // History. Also save the URL and commit timestamp to align with History.
  GURL committed_url_;
  // Meant to correspond with the timestamp recorded in HistoryService.
  base::Time committed_history_timestamp_;
  memories::VisitContextSignals memories_signals_;

  DISALLOW_COPY_AND_ASSIGN(UkmPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CORE_UKM_PAGE_LOAD_METRICS_OBSERVER_H_
