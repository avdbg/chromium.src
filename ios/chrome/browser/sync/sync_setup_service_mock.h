// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_SYNC_SETUP_SERVICE_MOCK_H_
#define IOS_CHROME_BROWSER_SYNC_SYNC_SETUP_SERVICE_MOCK_H_

#include "ios/chrome/browser/sync/sync_setup_service.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace web {
class BrowserState;
}

// Mock for the class that allows configuring sync on iOS.
class SyncSetupServiceMock : public SyncSetupService {
 public:
  static std::unique_ptr<KeyedService> CreateKeyedService(
      web::BrowserState* browser_state);

  SyncSetupServiceMock(syncer::SyncService* sync_service);
  ~SyncSetupServiceMock();
  MOCK_METHOD(bool, IsSyncEnabled, (), (const override));
  MOCK_METHOD(bool, IsSyncingAllDataTypes, (), (const override));
  MOCK_METHOD(SyncServiceState, GetSyncServiceState, (), (override));
  MOCK_METHOD(bool, IsDataTypePreferred, (syncer::ModelType), (const override));
  MOCK_METHOD(bool, IsDataTypeActive, (syncer::ModelType), (const override));
  MOCK_METHOD(bool, HasFinishedInitialSetup, (), (override));
  MOCK_METHOD(void, PrepareForFirstSyncSetup, (), (override));
  MOCK_METHOD(void,
              SetFirstSetupComplete,
              (syncer::SyncFirstSetupCompleteSource),
              (override));

  // Allow the real SyncSetupService::HasFinishedInitialSetup() to be used when
  // mocking HasFinishedInitialSetup().
  bool SyncSetupServiceHasFinishedInitialSetup();
};

#endif  // IOS_CHROME_BROWSER_SYNC_SYNC_SETUP_SERVICE_MOCK_H_
