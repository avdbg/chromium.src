// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_ARC_APPS_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_ARC_APPS_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

class ArcApps;

class ArcAppsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ArcApps* GetForProfile(Profile* profile);

  static ArcAppsFactory* GetInstance();

  static void ShutDownForTesting(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<ArcAppsFactory>;

  ArcAppsFactory();
  ArcAppsFactory(const ArcAppsFactory&) = delete;
  ArcAppsFactory& operator=(const ArcAppsFactory&) = delete;
  ~ArcAppsFactory() override = default;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_ARC_APPS_FACTORY_H_
