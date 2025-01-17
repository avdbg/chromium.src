// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace borealis {

class BorealisService;

// Implementation of the factory used to access profile-keyed instances of the
// features service.
class BorealisServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static BorealisService* GetForProfile(Profile* profile);

  static BorealisServiceFactory* GetInstance();

  // Can not be moved or copied.
  BorealisServiceFactory(const BorealisServiceFactory&) = delete;
  BorealisServiceFactory& operator=(const BorealisServiceFactory&) = delete;

 private:
  friend base::NoDestructor<BorealisServiceFactory>;

  BorealisServiceFactory();
  ~BorealisServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_FACTORY_H_
