// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/features.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_error.h"
#include "ios/chrome/browser/safe_browsing/safe_browsing_service.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/thread/web_task_traits.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using security_interstitials::UnsafeResource;

namespace {
// Creates a PolicyDecision that cancels a navigation to show a safe browsing
// error.
web::WebStatePolicyDecider::PolicyDecision CreateSafeBrowsingErrorDecision() {
  return web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
      [NSError errorWithDomain:kSafeBrowsingErrorDomain
                          code:kUnsafeResourceErrorCode
                      userInfo:nil]);
}

// Returns a canonicalized version of |url| as used by the SafeBrowsingService.
GURL GetCanonicalizedUrl(const GURL& url) {
  std::string hostname;
  std::string path;
  std::string query;
  safe_browsing::V4ProtocolManagerUtil::CanonicalizeUrl(url, &hostname, &path,
                                                        &query);

  GURL::Replacements replacements;
  if (hostname.length())
    replacements.SetHostStr(hostname);
  if (path.length())
    replacements.SetPathStr(path);
  if (query.length())
    replacements.SetQueryStr(query);
  replacements.ClearRef();

  return url.ReplaceComponents(replacements);
}
}  // namespace

#pragma mark - SafeBrowsingTabHelper

SafeBrowsingTabHelper::SafeBrowsingTabHelper(web::WebState* web_state)
    : policy_decider_(web_state),
      query_observer_(web_state, &policy_decider_),
      navigation_observer_(web_state, &policy_decider_) {}

SafeBrowsingTabHelper::~SafeBrowsingTabHelper() = default;

WEB_STATE_USER_DATA_KEY_IMPL(SafeBrowsingTabHelper)

#pragma mark - SafeBrowsingTabHelper::PolicyDecider

SafeBrowsingTabHelper::PolicyDecider::PolicyDecider(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state),
      query_manager_(SafeBrowsingQueryManager::FromWebState(web_state)) {}

SafeBrowsingTabHelper::PolicyDecider::~PolicyDecider() = default;

bool SafeBrowsingTabHelper::PolicyDecider::IsQueryStale(
    const SafeBrowsingQueryManager::Query& query) {
  bool is_main_frame = query.IsMainFrame();
  const GURL& url = query.url;
  if (is_main_frame) {
    return !GetOldestPendingMainFrameQuery(url);
  } else {
    web::NavigationItem* last_committed_item =
        web_state()->GetNavigationManager()->GetLastCommittedItem();
    return !last_committed_item ||
           last_committed_item->GetUniqueID() != query.main_frame_item_id ||
           pending_sub_frame_queries_.find(url) ==
               pending_sub_frame_queries_.end();
  }
}

void SafeBrowsingTabHelper::PolicyDecider::HandlePolicyDecision(
    const SafeBrowsingQueryManager::Query& query,
    const web::WebStatePolicyDecider::PolicyDecision& policy_decision) {
  DCHECK(!IsQueryStale(query));
  if (query.IsMainFrame()) {
    OnMainFrameUrlQueryDecided(query.url, policy_decision);
  } else {
    OnSubFrameUrlQueryDecided(query.url, policy_decision);
  }
}

void SafeBrowsingTabHelper::PolicyDecider::UpdateForMainFrameDocumentChange() {
  // Since a new main frame document was loaded, sub frame navigations from the
  // previous page are cancelled and their policy decisions can be erased.
  pending_sub_frame_queries_.clear();
}

void SafeBrowsingTabHelper::PolicyDecider::UpdateForMainFrameServerRedirect() {
  // The current |pending_main_frame_query_| is a server redirect from
  // |previous_main_frame_query_|, so add the latter to the pending redirect
  // chain. However, when a URL redirects to itself, ShouldAllowRequest may not
  // be called again, and in that case |previous_main_frame_query_| will not
  // have a new URL to add to the redirect chain.
  if (previous_main_frame_query_) {
    pending_main_frame_redirect_chain_.push_back(
        std::move(*previous_main_frame_query_));
    previous_main_frame_query_ = base::nullopt;
  }
}

#pragma mark web::WebStatePolicyDecider

web::WebStatePolicyDecider::PolicyDecision
SafeBrowsingTabHelper::PolicyDecider::ShouldAllowRequest(
    NSURLRequest* request,
    const web::WebStatePolicyDecider::RequestInfo& request_info) {
  // Allow navigations for URLs that cannot be checked by the service.
  GURL request_url = GetCanonicalizedUrl(net::GURLWithNSURL(request.URL));
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service->CanCheckUrl(request_url))
    return web::WebStatePolicyDecider::PolicyDecision::Allow();

  // Track all pending URL queries.
  bool is_main_frame = request_info.target_frame_is_main;
  if (is_main_frame) {
    if (pending_main_frame_query_)
      previous_main_frame_query_ = std::move(pending_main_frame_query_);

    pending_main_frame_query_ = MainFrameUrlQuery(request_url);
  } else if (pending_sub_frame_queries_.find(request_url) ==
             pending_sub_frame_queries_.end()) {
    pending_sub_frame_queries_.insert({request_url, SubFrameUrlQuery()});
  }

  // If there is a pre-existing main frame unsafe resource for |request_url|
  // that haven't yet resulted in an error page, this resource can be used to
  // show the error page for the current load.  This can occur in back/forward
  // navigations to safe browsing error pages, where ShouldAllowRequest() is
  // called multiple times consecutively for the same URL.
  SafeBrowsingUnsafeResourceContainer* unsafe_resource_container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(web_state());
  if (is_main_frame) {
    const security_interstitials::UnsafeResource* main_frame_resource =
        unsafe_resource_container->GetMainFrameUnsafeResource();
    if (main_frame_resource && main_frame_resource->url == request_url) {
      // TODO(crbug.com/1064803): This should directly return the safe browsing
      // error decision once error pages for cancelled requests are supported.
      // For now, only cancelled response errors are displayed properly.
      pending_main_frame_query_->decision = CreateSafeBrowsingErrorDecision();
      return web::WebStatePolicyDecider::PolicyDecision::Allow();
    }

    // Error pages for unsafe subframes are triggered by associating an
    // UnsafeResource with the corresponding main frame item and reloading that
    // item. Check to see if this main frame request is a reload and has an
    // associated sub frame UnsafeResource.
    web::NavigationManager* navigation_manager =
        web_state()->GetNavigationManager();
    web::NavigationItem* reloaded_item = navigation_manager->GetPendingItem();
    if (ui::PageTransitionCoreTypeIs(request_info.transition_type,
                                     ui::PAGE_TRANSITION_RELOAD) &&
        reloaded_item == navigation_manager->GetLastCommittedItem() &&
        unsafe_resource_container->GetSubFrameUnsafeResource(reloaded_item)) {
      // Store the safe browsing error decision without re-checking the URL.
      // TODO(crbug.com/1064803): This should directly return the safe browsing
      // error decision once error pages for cancelled requests are supported.
      // For now, only cancelled response errors are displayed properly.
      pending_main_frame_query_->decision = CreateSafeBrowsingErrorDecision();
      return web::WebStatePolicyDecider::PolicyDecision::Allow();
    }
  }

  // Start the URL check.
  int main_frame_item_id = -1;
  if (!is_main_frame) {
    if (web::NavigationItem* item =
            web_state()->GetNavigationManager()->GetLastCommittedItem()) {
      main_frame_item_id = item->GetUniqueID();
    } else {
      return web::WebStatePolicyDecider::PolicyDecision::Allow();
    }
  }

  query_manager_->StartQuery(SafeBrowsingQueryManager::Query(
      request_url, base::SysNSStringToUTF8([request HTTPMethod]),
      main_frame_item_id));

  // Allow all requests to continue.  If a safe browsing error is detected, the
  // navigation will be cancelled for using the response policy decision.
  return web::WebStatePolicyDecider::PolicyDecision::Allow();
}

void SafeBrowsingTabHelper::PolicyDecider::ShouldAllowResponse(
    NSURLResponse* response,
    bool for_main_frame,
    base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
        callback) {
  // Allow navigations for URLs that cannot be checked by the service.
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  GURL response_url = GetCanonicalizedUrl(net::GURLWithNSURL(response.URL));
  if (!safe_browsing_service->CanCheckUrl(response_url)) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  if (for_main_frame) {
    HandleMainFrameResponsePolicy(response_url, std::move(callback));
  } else {
    HandleSubFrameResponsePolicy(response_url, std::move(callback));
  }
}

#pragma mark Response Policy Decision Helpers

void SafeBrowsingTabHelper::PolicyDecider::HandleMainFrameResponsePolicy(
    const GURL& url,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  DCHECK(pending_main_frame_query_);
  // When there's a server redirect, a ShouldAllowRequest call sometimes
  // doesn't happen for the target of the redirection. This seems to be fixed
  // in trunk WebKit.
  if (pending_main_frame_redirect_chain_.empty()) {
    // Only check that the hosts match, since a ServiceWorker that handles a
    // request can set a different URL in the response that it produces, without
    // triggering a redirect.
    DCHECK_EQ(pending_main_frame_query_->url.host(), url.host());
  } else {
    bool matching_hosts = pending_main_frame_query_->url.host() == url.host();
    UMA_HISTOGRAM_BOOLEAN(
        "IOS.SafeBrowsing.RedirectedRequestResponseHostsMatch", matching_hosts);
  }
  // If the previous query wasn't added to a pending redirect chain, the
  // pending chain is no longer active, since DidRedirectNavigation() is
  // guaranteed to be called before ShouldAllowResponse() is called for the
  // redirection target.
  if (previous_main_frame_query_) {
    // The previous query was never added to a redirect chain, so the current
    // query is not a redirect.
    previous_main_frame_query_ = base::nullopt;
    pending_main_frame_redirect_chain_.clear();
  }

  auto decision = MainFrameRedirectChainDecision();
  if (decision) {
    std::move(callback).Run(*decision);
    pending_main_frame_redirect_chain_.clear();
  } else {
    pending_main_frame_query_->response_callback = std::move(callback);
  }
}

void SafeBrowsingTabHelper::PolicyDecider::HandleSubFrameResponsePolicy(
    const GURL& url,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  // Sub frame response policy decisions are expected to always be requested
  // after a request policy decision for |url|.
  DCHECK(pending_sub_frame_queries_.find(url) !=
         pending_sub_frame_queries_.end());

  SubFrameUrlQuery& sub_frame_query = pending_sub_frame_queries_[url];
  if (sub_frame_query.decision) {
    std::move(callback).Run(*(sub_frame_query.decision));
  } else {
    sub_frame_query.response_callbacks.push_back(std::move(callback));
  }
}

#pragma mark URL Check Completion Helpers

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery*
SafeBrowsingTabHelper::PolicyDecider::GetOldestPendingMainFrameQuery(
    const GURL& url) {
  for (auto& query : pending_main_frame_redirect_chain_) {
    if (query.url == url && !query.decision) {
      return &query;
    }
  }

  if (pending_main_frame_query_ && pending_main_frame_query_->url == url &&
      !pending_main_frame_query_->decision) {
    return &pending_main_frame_query_.value();
  }

  return nullptr;
}

void SafeBrowsingTabHelper::PolicyDecider::OnMainFrameUrlQueryDecided(
    const GURL& url,
    web::WebStatePolicyDecider::PolicyDecision decision) {
  GetOldestPendingMainFrameQuery(url)->decision = decision;

  // If ShouldAllowResponse() has already been called for this URL, and if
  // an overall decision for the redirect chain can be computed, invoke this
  // URL's callback with the overall decision.
  auto& response_callback = pending_main_frame_query_->response_callback;
  if (!response_callback.is_null()) {
    base::Optional<web::WebStatePolicyDecider::PolicyDecision>
        overall_decision = MainFrameRedirectChainDecision();
    if (overall_decision) {
      std::move(response_callback).Run(*overall_decision);
      pending_main_frame_redirect_chain_.clear();
    }
  }

  // When a prendered page is unsafe, cancel the prerender.
  PrerenderService* prerender_service =
      PrerenderServiceFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(web_state()->GetBrowserState()));
  if (prerender_service &&
      prerender_service->IsWebStatePrerendered(web_state()) &&
      decision.ShouldCancelNavigation()) {
    prerender_service->CancelPrerender();
  }
}

void SafeBrowsingTabHelper::PolicyDecider::OnSubFrameUrlQueryDecided(
    const GURL& url,
    web::WebStatePolicyDecider::PolicyDecision decision) {
  web::NavigationManager* navigation_manager =
      web_state()->GetNavigationManager();
  web::NavigationItem* main_frame_item =
      navigation_manager->GetLastCommittedItem();

  // The URL check is expected to have been registered for the sub frame.
  DCHECK(pending_sub_frame_queries_.find(url) !=
         pending_sub_frame_queries_.end());

  // Store the decision for |url| and run all the response callbacks that have
  // been received before the URL check completion.
  SubFrameUrlQuery& sub_frame_query = pending_sub_frame_queries_[url];
  sub_frame_query.decision = decision;
  for (auto& response_callback : sub_frame_query.response_callbacks) {
    std::move(response_callback).Run(decision);
  }
  sub_frame_query.response_callbacks.clear();

  // When a subframe in a prerendered page is unsafe, cancel the prerender.
  PrerenderService* prerender_service =
      PrerenderServiceFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(web_state()->GetBrowserState()));
  if (prerender_service &&
      prerender_service->IsWebStatePrerendered(web_state()) &&
      decision.ShouldCancelNavigation()) {
    prerender_service->CancelPrerender();
    return;
  }

  // Error pages are only shown for cancelled main frame navigations, so
  // executing the sub frame response callbacks with the decision will not
  // actually show the safe browsing blocking page.  To trigger the blocking
  // page, reload the last committed item so that the stored sub frame unsafe
  // resource can be used to populate an error page in the main frame upon
  // reloading.
  if (decision.ShouldCancelNavigation() && decision.ShouldDisplayError()) {
    DCHECK(SafeBrowsingUnsafeResourceContainer::FromWebState(web_state())
               ->GetSubFrameUnsafeResource(main_frame_item));
    navigation_manager->DiscardNonCommittedItems();
    navigation_manager->Reload(web::ReloadType::NORMAL,
                               /*check_for_repost=*/false);
  }
}

base::Optional<web::WebStatePolicyDecider::PolicyDecision>
SafeBrowsingTabHelper::PolicyDecider::MainFrameRedirectChainDecision() {
  if (pending_main_frame_query_->decision &&
      pending_main_frame_query_->decision->ShouldCancelNavigation()) {
    return pending_main_frame_query_->decision;
  }

  // If some query has received a decision to cancel the navigation or if
  // every query has received a decision to allow the navigation, there is
  // enough information to make an overall decision.
  base::Optional<web::WebStatePolicyDecider::PolicyDecision> decision =
      pending_main_frame_query_->decision;
  for (auto& query : pending_main_frame_redirect_chain_) {
    if (!query.decision) {
      decision = base::nullopt;
    } else if (query.decision->ShouldCancelNavigation()) {
      decision = query.decision;
      break;
    }
  }

  return decision;
}

#pragma mark SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::MainFrameUrlQuery(
    const GURL& url)
    : url(url) {}

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::MainFrameUrlQuery(
    MainFrameUrlQuery&& query) = default;

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery&
SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::operator=(
    MainFrameUrlQuery&& other) = default;

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::~MainFrameUrlQuery() {
  if (!response_callback.is_null()) {
    std::move(response_callback)
        .Run(web::WebStatePolicyDecider::PolicyDecision::Cancel());
  }
}

#pragma mark SafeBrowsingTabHelper::PolicyDecider::SubFrameUrlQuery

SafeBrowsingTabHelper::PolicyDecider::SubFrameUrlQuery::SubFrameUrlQuery() =
    default;

SafeBrowsingTabHelper::PolicyDecider::SubFrameUrlQuery::SubFrameUrlQuery(
    SubFrameUrlQuery&& query) = default;

SafeBrowsingTabHelper::PolicyDecider::SubFrameUrlQuery::~SubFrameUrlQuery() {
  for (auto& response_callback : response_callbacks) {
    std::move(response_callback)
        .Run(web::WebStatePolicyDecider::PolicyDecision::Cancel());
  }
}

#pragma mark - SafeBrowsingTabHelper::QueryObserver

SafeBrowsingTabHelper::QueryObserver::QueryObserver(web::WebState* web_state,
                                                    PolicyDecider* decider)
    : web_state_(web_state), policy_decider_(decider) {
  DCHECK(policy_decider_);
  scoped_observer_.Add(SafeBrowsingQueryManager::FromWebState(web_state));
}

SafeBrowsingTabHelper::QueryObserver::~QueryObserver() = default;

void SafeBrowsingTabHelper::QueryObserver::SafeBrowsingQueryFinished(
    SafeBrowsingQueryManager* manager,
    const SafeBrowsingQueryManager::Query& query,
    const SafeBrowsingQueryManager::Result& result) {
  if (policy_decider_->IsQueryStale(query))
    return;

  // Create a policy decision using the query result.
  web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  if (result.show_error_page) {
    policy_decision = CreateSafeBrowsingErrorDecision();
  } else if (!result.proceed) {
    policy_decision = web::WebStatePolicyDecider::PolicyDecision::Cancel();
  }

  // If an error page needs to be displayed, record the pending decision and
  // store the unsafe resource.
  if (policy_decision.ShouldDisplayError()) {
    DCHECK(result.resource);

    // Store the navigation URL for the resource.
    bool is_main_frame = query.IsMainFrame();
    web::NavigationItem* item =
        web_state_->GetNavigationManager()->GetLastCommittedItem();
    UnsafeResource resource = *result.resource;
    resource.navigation_url =
        is_main_frame ? query.url : GetCanonicalizedUrl(item->GetURL());

    // Allow list decisions for sub frame threats should be associated with the
    // navigation URL.
    SafeBrowsingUrlAllowList::FromWebState(web_state_)
        ->AddPendingUnsafeNavigationDecision(resource.navigation_url,
                                             resource.threat_type);

    // Store the UnsafeResource to be fetched later to populate the error page.
    SafeBrowsingUnsafeResourceContainer* container =
        SafeBrowsingUnsafeResourceContainer::FromWebState(web_state_);
    if (is_main_frame) {
      container->StoreMainFrameUnsafeResource(resource);
    } else {
      DCHECK_EQ(query.main_frame_item_id, item->GetUniqueID());
      container->StoreSubFrameUnsafeResource(resource, item);
    }
  }

  policy_decider_->HandlePolicyDecision(query, policy_decision);
}

void SafeBrowsingTabHelper::QueryObserver::SafeBrowsingQueryManagerDestroyed(
    SafeBrowsingQueryManager* manager) {
  scoped_observer_.Remove(manager);
}

#pragma mark - SafeBrowsingTabHelper::NavigationObserver

SafeBrowsingTabHelper::NavigationObserver::NavigationObserver(
    web::WebState* web_state,
    PolicyDecider* policy_decider)
    : policy_decider_(policy_decider) {
  DCHECK(policy_decider_);
  scoped_observer_.Add(web_state);
}

SafeBrowsingTabHelper::NavigationObserver::~NavigationObserver() = default;

void SafeBrowsingTabHelper::NavigationObserver::DidRedirectNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  policy_decider_->UpdateForMainFrameServerRedirect();
}

void SafeBrowsingTabHelper::NavigationObserver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->HasCommitted() &&
      !navigation_context->IsSameDocument()) {
    policy_decider_->UpdateForMainFrameDocumentChange();
  }
}

void SafeBrowsingTabHelper::NavigationObserver::WebStateDestroyed(
    web::WebState* web_state) {
  scoped_observer_.Remove(web_state);
}
