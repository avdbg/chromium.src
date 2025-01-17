// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/page_load_metrics/page_load_metrics_initialize.h"

#include "base/macros.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_base.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace page_load_metrics {
class PageLoadMetricsMemoryTracker;
}  // namespace page_load_metrics

namespace android_webview {

namespace {

class PageLoadMetricsEmbedder
    : public page_load_metrics::PageLoadMetricsEmbedderBase {
 public:
  explicit PageLoadMetricsEmbedder(content::WebContents* web_contents);
  ~PageLoadMetricsEmbedder() override;

  // page_load_metrics::PageLoadMetricsEmbedderBase:
  bool IsNewTabPageUrl(const GURL& url) override;
  bool IsPrerender(content::WebContents* web_contents) override;
  bool IsExtensionUrl(const GURL& url) override;
  page_load_metrics::PageLoadMetricsMemoryTracker*
  GetMemoryTrackerForBrowserContext(
      content::BrowserContext* browser_context) override;

 protected:
  // page_load_metrics::PageLoadMetricsEmbedderBase:
  void RegisterEmbedderObservers(
      page_load_metrics::PageLoadTracker* tracker) override;
  bool IsPrerendering() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsEmbedder);
};

PageLoadMetricsEmbedder::PageLoadMetricsEmbedder(
    content::WebContents* web_contents)
    : PageLoadMetricsEmbedderBase(web_contents) {}

PageLoadMetricsEmbedder::~PageLoadMetricsEmbedder() = default;

void PageLoadMetricsEmbedder::RegisterEmbedderObservers(
    page_load_metrics::PageLoadTracker* tracker) {}

bool PageLoadMetricsEmbedder::IsPrerendering() const {
  return false;
}

bool PageLoadMetricsEmbedder::IsNewTabPageUrl(const GURL& url) {
  return false;
}

bool PageLoadMetricsEmbedder::IsPrerender(content::WebContents* web_contents) {
  return false;
}

bool PageLoadMetricsEmbedder::IsExtensionUrl(const GURL& url) {
  return false;
}

page_load_metrics::PageLoadMetricsMemoryTracker*
PageLoadMetricsEmbedder::GetMemoryTrackerForBrowserContext(
    content::BrowserContext* browser_context) {
  return nullptr;
}

}  // namespace

void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents) {
  // Change this method? consider to modify the peer in
  // chrome/browser/page_load_metrics/page_load_metrics_initialize.cc
  // weblayer/browser/page_load_metrics_initialize.cc
  // as well.
  page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
      web_contents, std::make_unique<PageLoadMetricsEmbedder>(web_contents));
}

}  // namespace android_webview
