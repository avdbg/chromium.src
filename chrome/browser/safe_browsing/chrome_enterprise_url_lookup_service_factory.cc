// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

namespace safe_browsing {

// static
ChromeEnterpriseRealTimeUrlLookupService*
ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ChromeEnterpriseRealTimeUrlLookupService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
ChromeEnterpriseRealTimeUrlLookupServiceFactory*
ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetInstance() {
  return base::Singleton<
      ChromeEnterpriseRealTimeUrlLookupServiceFactory>::get();
}

ChromeEnterpriseRealTimeUrlLookupServiceFactory::
    ChromeEnterpriseRealTimeUrlLookupServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeEnterpriseRealTimeUrlLookupService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(VerdictCacheManagerFactory::GetInstance());
  DependsOn(enterprise_connectors::ConnectorsServiceFactory::GetInstance());
}

KeyedService*
ChromeEnterpriseRealTimeUrlLookupServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!g_browser_process->safe_browsing_service()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          g_browser_process->safe_browsing_service()->GetURLLoaderFactory());
  if (base::FeatureList::IsEnabled(kSafeBrowsingRemoveCookies)) {
    url_loader_factory =
        std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
            profile->GetURLLoaderFactory());
  }
  const policy::BrowserPolicyConnector* browser_policy_connector =
      g_browser_process->browser_policy_connector();
  bool is_under_advanced_protection =
      AdvancedProtectionStatusManagerFactory::GetForProfile(profile)
          ->IsUnderAdvancedProtection();
  return new ChromeEnterpriseRealTimeUrlLookupService(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      VerdictCacheManagerFactory::GetForProfile(profile), profile,
      base::BindRepeating(&safe_browsing::SyncUtils::IsHistorySyncEnabled,
                          ProfileSyncServiceFactory::GetForProfile(profile)),
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile),
      profile->GetPrefs(), GetProfileManagementStatus(browser_policy_connector),
      is_under_advanced_protection, profile->IsOffTheRecord());
}

}  // namespace safe_browsing
