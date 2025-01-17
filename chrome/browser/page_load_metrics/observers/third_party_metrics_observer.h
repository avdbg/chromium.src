// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "url/gurl.h"
#include "url/origin.h"

// Records metrics about third-party storage accesses to a page.
class ThirdPartyMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // TODO(crbug.com/1115657): kUnknown is mostly unused except for passing it as
  // a "dummy" type to RecordUseCounters.  After we factor out AccessType from
  // that method (see other TODOs), we should be able to remove it.
  enum class AccessType {
    kCookieRead,
    kCookieWrite,
    kLocalStorage,
    kSessionStorage,
    kFileSystem,
    kIndexedDb,
    kCacheStorage,
    kUnknown,
    kMaxValue = kUnknown
  };

  ThirdPartyMetricsObserver();
  ~ThirdPartyMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void FrameReceivedFirstUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override;
  void OnCookiesRead(const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy) override;
  void OnCookieChange(const GURL& url,
                      const GURL& first_party_url,
                      const net::CanonicalCookie& cookie,
                      bool blocked_by_policy) override;
  void OnStorageAccessed(const GURL& url,
                         const GURL& first_party_url,
                         bool blocked_by_policy,
                         page_load_metrics::StorageType storage_type) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  // The info about the types of activities for a third party.
  struct ThirdPartyInfo {
    ThirdPartyInfo();
    ThirdPartyInfo(const ThirdPartyInfo&);
    std::bitset<static_cast<size_t>(AccessType::kMaxValue)> access_types;
    bool activation = false;
  };

  // Returns a pointer to the ThirdPartyInfo in all_third_party_info_ for |url|
  // and |first_party_url|, adding an entry as necessary. The out parameter
  // |is_third_party| indicates whether the two inputs are third party one
  // another and may be true with a nullptr return if the map is full.
  ThirdPartyInfo* GetThirdPartyInfo(const GURL& url,
                                    const GURL& first_party_url,
                                    bool& is_third_party);

  void OnCookieOrStorageAccess(const GURL& url,
                               const GURL& first_party_url,
                               bool blocked_by_policy,
                               AccessType access_type);
  void RecordMetrics(
      const page_load_metrics::mojom::PageLoadTiming& main_frame_timing);

  // Records feature usage for teh |access_type|, and also, when present, for
  // generic access and activation for the |third_party_info|.
  void RecordUseCounters(AccessType access_type,
                         const ThirdPartyInfo* third_party_info);

  AccessType StorageTypeToAccessType(
      page_load_metrics::StorageType storage_type);

  // A map of third parties and the types of activities they have performed.
  //
  // A third party document.cookie / window.localStorage /
  // window.sessionStorage happens when the context's scheme://eTLD+1
  // differs from the main frame's. A third party resource request happens
  // when the URL request's scheme://eTLD+1 differs from the main frame's.
  // For URLs which have no registrable domain, the hostname is used
  // instead.
  std::map<GURL, ThirdPartyInfo> all_third_party_info_;

  // A set of RenderFrameHosts that we've recorded timing data for. The
  // RenderFrameHosts are later removed when they navigate again or are deleted.
  std::set<content::RenderFrameHost*> recorded_frames_;

  // If the page has any blocked_by_policy cookie or DOM storage access (e.g.,
  // block third-party cookies is enabled) then we don't want to record any
  // metrics for the page.
  bool should_record_metrics_ = true;

  // True if this page loaded a third-party font.
  bool third_party_font_loaded_ = false;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_
