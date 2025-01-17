// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/page_content_annotations_service_factory.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/browser_context.h"

// static
optimization_guide::PageContentAnnotationsService*
PageContentAnnotationsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<optimization_guide::PageContentAnnotationsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PageContentAnnotationsServiceFactory*
PageContentAnnotationsServiceFactory::GetInstance() {
  static base::NoDestructor<PageContentAnnotationsServiceFactory> factory;
  return factory.get();
}

PageContentAnnotationsServiceFactory::PageContentAnnotationsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PageContentAnnotationsService",
          BrowserContextDependencyManager::GetInstance()) {}

PageContentAnnotationsServiceFactory::~PageContentAnnotationsServiceFactory() =
    default;

KeyedService* PageContentAnnotationsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // The optimization guide service must be available for the page content
  // annotations service to work.
  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (optimization_guide_keyed_service) {
    return new optimization_guide::PageContentAnnotationsService(
        optimization_guide_keyed_service);
  }
  return nullptr;
}

bool PageContentAnnotationsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return optimization_guide::features::IsPageContentAnnotationEnabled();
}

bool PageContentAnnotationsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
