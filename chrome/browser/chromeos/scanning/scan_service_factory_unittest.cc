// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scan_service_factory.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/scanning/fake_lorgnette_scanner_manager.h"
#include "chrome/browser/chromeos/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/chromeos/scanning/scan_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// Creates a new LorgnetteScannerManager for the given `context`.
std::unique_ptr<KeyedService> BuildLorgnetteScannerManager(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::FakeLorgnetteScannerManager>();
}

// Creates a new ScanService for the given `context`.
std::unique_ptr<KeyedService> BuildScanService(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(static_cast<KeyedService*>(
      ScanServiceFactory::BuildInstanceFor(context)));
}

// Creates a Profile based on the provided `file_path` and sets the required
// testing factories for that Profile.
std::unique_ptr<Profile> CreateProfile(const std::string& file_path) {
  TestingProfile::Builder builder;
  if (!file_path.empty())
    builder.SetPath(base::FilePath(file_path));

  std::unique_ptr<Profile> profile = builder.Build();

  LorgnetteScannerManagerFactory::GetInstance()->SetTestingFactory(
      profile.get(), base::BindRepeating(&BuildLorgnetteScannerManager));
  ScanServiceFactory::GetInstance()->SetTestingFactory(
      profile.get(), base::BindRepeating(&BuildScanService));

  return profile;
}

}  // namespace

// Test that the ScanService can be created with the original profile.
TEST(ScanServiceFactoryTest, OriginalProfileHasService) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<Profile> profile = CreateProfile("");
  EXPECT_NE(nullptr, ScanServiceFactory::GetForBrowserContext(profile.get()));
}

// Test that the ScanService can be created with an off-the-record profile.
TEST(ScanServiceFactoryTest, OffTheRecordProfileHasService) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<Profile> profile = CreateProfile("");
  EXPECT_NE(nullptr, ScanServiceFactory::GetForBrowserContext(
                         profile->GetPrimaryOTRProfile()));
}

// Test that the ScanService cannot be created with a signin profile.
TEST(ScanServiceFactoryTest, SigninProfileNoService) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<Profile> signin_profile =
      CreateProfile(chrome::kInitialProfile);
  EXPECT_EQ(nullptr,
            ScanServiceFactory::GetForBrowserContext(signin_profile.get()));
}

// Test that the ScanService cannot be created on the lock screen.
TEST(ScanServiceFactoryTest, LockScreenProfileNoService) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<Profile> lockscreen_profile =
      CreateProfile(chrome::kLockScreenAppProfile);
  EXPECT_EQ(nullptr,
            ScanServiceFactory::GetForBrowserContext(lockscreen_profile.get()));
}

}  // namespace chromeos
