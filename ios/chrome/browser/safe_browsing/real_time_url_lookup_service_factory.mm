// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/safe_browsing/real_time_url_lookup_service_factory.h"

#include "base/bind.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/realtime/url_lookup_service.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#import "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_service.h"
#import "ios/chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
safe_browsing::RealTimeUrlLookupService*
RealTimeUrlLookupServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<safe_browsing::RealTimeUrlLookupService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
RealTimeUrlLookupServiceFactory*
RealTimeUrlLookupServiceFactory::GetInstance() {
  static base::NoDestructor<RealTimeUrlLookupServiceFactory> instance;
  return instance.get();
}

RealTimeUrlLookupServiceFactory::RealTimeUrlLookupServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "RealTimeUrlLookupService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(VerdictCacheManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
RealTimeUrlLookupServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  return std::make_unique<safe_browsing::RealTimeUrlLookupService>(
      safe_browsing_service->GetURLLoaderFactory(),
      VerdictCacheManagerFactory::GetForBrowserState(chrome_browser_state),
      base::BindRepeating(
          &safe_browsing::SyncUtils::IsHistorySyncEnabled,
          ProfileSyncServiceFactory::GetForBrowserState(chrome_browser_state)),
      chrome_browser_state->GetPrefs(),
      std::make_unique<safe_browsing::SafeBrowsingPrimaryAccountTokenFetcher>(
          IdentityManagerFactory::GetForBrowserState(chrome_browser_state)),
      base::BindRepeating(
          &safe_browsing::SyncUtils::
              AreSigninAndSyncSetUpForSafeBrowsingTokenFetches,
          ProfileSyncServiceFactory::GetForBrowserState(chrome_browser_state),
          IdentityManagerFactory::GetForBrowserState(chrome_browser_state)),
      safe_browsing::ChromeUserPopulation::NOT_MANAGED,
      /*is_under_advanced_protection=*/false,
      chrome_browser_state->IsOffTheRecord(),
      GetApplicationContext()->GetVariationsService());
}
