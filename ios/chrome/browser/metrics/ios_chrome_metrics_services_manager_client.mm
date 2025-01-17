// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_metrics_services_manager_client.h"

#include <string>

#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/strings/string16.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"
#include "ios/chrome/browser/variations/ios_chrome_variations_service_client.h"
#include "ios/chrome/browser/variations/ios_ui_string_overrider_factory.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

void PostStoreMetricsClientInfo(const metrics::ClientInfo& client_info) {}

std::unique_ptr<metrics::ClientInfo> LoadMetricsClientInfo() {
  return std::unique_ptr<metrics::ClientInfo>();
}

}  // namespace

class IOSChromeMetricsServicesManagerClient::IOSChromeEnabledStateProvider
    : public metrics::EnabledStateProvider {
 public:
  IOSChromeEnabledStateProvider() {}
  ~IOSChromeEnabledStateProvider() override {}

  bool IsConsentGiven() const override {
    return IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  }

  DISALLOW_COPY_AND_ASSIGN(IOSChromeEnabledStateProvider);
};

IOSChromeMetricsServicesManagerClient::IOSChromeMetricsServicesManagerClient(
    PrefService* local_state)
    : enabled_state_provider_(
          std::make_unique<IOSChromeEnabledStateProvider>()),
      local_state_(local_state) {
  DCHECK(local_state);
}

IOSChromeMetricsServicesManagerClient::
    ~IOSChromeMetricsServicesManagerClient() = default;

std::unique_ptr<variations::VariationsService>
IOSChromeMetricsServicesManagerClient::CreateVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // NOTE: On iOS, disabling background networking is not supported, so pass in
  // a dummy value for the name of the switch that disables background
  // networking.
  return variations::VariationsService::Create(
      std::make_unique<IOSChromeVariationsServiceClient>(), local_state_,
      GetMetricsStateManager(), "dummy-disable-background-switch",
      ::CreateUIStringOverrider(),
      base::BindOnce(&ApplicationContext::GetNetworkConnectionTracker,
                     base::Unretained(GetApplicationContext())));
}

std::unique_ptr<metrics::MetricsServiceClient>
IOSChromeMetricsServicesManagerClient::CreateMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return IOSChromeMetricsServiceClient::Create(GetMetricsStateManager());
}

metrics::MetricsStateManager*
IOSChromeMetricsServicesManagerClient::GetMetricsStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_state_manager_) {
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_, enabled_state_provider_.get(), std::wstring(),
        base::BindRepeating(&PostStoreMetricsClientInfo),
        base::BindRepeating(&LoadMetricsClientInfo));
  }
  return metrics_state_manager_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSChromeMetricsServicesManagerClient::GetURLLoaderFactory() {
  return GetApplicationContext()->GetSharedURLLoaderFactory();
}

bool IOSChromeMetricsServicesManagerClient::IsMetricsReportingEnabled() {
  return enabled_state_provider_->IsReportingEnabled();
}

bool IOSChromeMetricsServicesManagerClient::IsMetricsConsentGiven() {
  return enabled_state_provider_->IsConsentGiven();
}

bool IOSChromeMetricsServicesManagerClient::IsOffTheRecordSessionActive() {
  return AreIncognitoTabsPresent();
}

// static
bool IOSChromeMetricsServicesManagerClient::AreIncognitoTabsPresent() {
  std::vector<ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();

  for (ChromeBrowserState* browser_state : browser_states) {
    BrowserList* browser_list =
        BrowserListFactory::GetForBrowserState(browser_state);
    for (Browser* browser : browser_list->AllIncognitoBrowsers()) {
      if (!browser->GetWebStateList()->empty()) {
        return true;
      }
    }
  }
  return false;
}
