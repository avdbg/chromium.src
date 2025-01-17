// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/eche_app/eche_app_manager_factory.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chromeos/components/eche_app_ui/eche_app_manager.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "url/gurl.h"

namespace chromeos {
namespace eche_app {

namespace {

void LaunchEcheApp(Profile* profile, int64_t notification_id) {
  std::string url = "chrome://eche-app/?notification_id=";
  url.append(base::NumberToString(notification_id));
  struct web_app::SystemAppLaunchParams params = {.url = GURL(url)};
  web_app::LaunchSystemWebAppAsync(profile, web_app::SystemAppType::ECHE,
                                   params);
}

}  // namespace

// static
EcheAppManager* EcheAppManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<EcheAppManager*>(
      EcheAppManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
EcheAppManagerFactory* EcheAppManagerFactory::GetInstance() {
  return base::Singleton<EcheAppManagerFactory>::get();
}

EcheAppManagerFactory::EcheAppManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "EcheAppManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(phonehub::PhoneHubManagerFactory::GetInstance());
}

EcheAppManagerFactory::~EcheAppManagerFactory() = default;

KeyedService* EcheAppManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!features::IsPhoneHubEnabled() || !features::IsEcheSWAEnabled())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  phonehub::PhoneHubManager* phone_hub_manager =
      phonehub::PhoneHubManagerFactory::GetForProfile(profile);
  if (!phone_hub_manager)
    return nullptr;

  return new EcheAppManager(phone_hub_manager,
                            base::BindRepeating(&LaunchEcheApp, profile));
}

}  // namespace eche_app
}  // namespace chromeos
