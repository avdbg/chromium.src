// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/sync_setup_service_mock.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/web/public/browser_state.h"

SyncSetupServiceMock::SyncSetupServiceMock(syncer::SyncService* sync_service)
    : SyncSetupService(sync_service) {}

SyncSetupServiceMock::~SyncSetupServiceMock() {
}

bool SyncSetupServiceMock::SyncSetupServiceHasFinishedInitialSetup() {
  return SyncSetupService::HasFinishedInitialSetup();
}

std::unique_ptr<KeyedService> SyncSetupServiceMock::CreateKeyedService(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<SyncSetupServiceMock>(
      ProfileSyncServiceFactory::GetForBrowserState(browser_state));
}
