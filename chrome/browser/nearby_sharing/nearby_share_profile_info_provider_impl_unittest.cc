// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/nearby_sharing/nearby_share_profile_info_provider_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFakeGivenName[] = "Barack";
const char kFakeProfileUserName[] = "test@gmail.com";

}  // namespace

class NearbyShareProfileInfoProviderImplTest : public ::testing::Test {
 protected:
  NearbyShareProfileInfoProviderImplTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        user_manager_(new user_manager::FakeUserManager()),
        enabler_(base::WrapUnique(user_manager_)) {}
  ~NearbyShareProfileInfoProviderImplTest() override = default;

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  Profile* CreateProfile(const std::string& profile_user_name) {
    account_id_ = AccountId::FromUserEmail(profile_user_name);
    return profile_manager_.CreateTestingProfile(profile_user_name);
  }

  void AddUser() { user_manager_->AddUser(account_id_); }

  void SetUserGivenName(const std::string& name) {
    user_manager_->UpdateUserAccountData(
        account_id_, user_manager::UserManager::UserAccountData(
                         /*display_name=*/base::string16(),
                         /*given_name=*/base::UTF8ToUTF16(name),
                         /*locale=*/std::string()));
  }

  content::BrowserTaskEnvironment task_environment;
  TestingProfileManager profile_manager_;
  user_manager::FakeUserManager* user_manager_ = nullptr;
  user_manager::ScopedUserManager enabler_;
  AccountId account_id_;
};

TEST_F(NearbyShareProfileInfoProviderImplTest, GivenName) {
  Profile* profile = CreateProfile(kFakeProfileUserName);
  NearbyShareProfileInfoProviderImpl profile_info_provider(profile);

  // If no user, return base::nullopt.
  EXPECT_FALSE(profile_info_provider.GetGivenName());

  // If given name is empty, return base::nullopt.
  AddUser();
  SetUserGivenName(std::string());
  EXPECT_FALSE(profile_info_provider.GetGivenName());

  SetUserGivenName(kFakeGivenName);
  EXPECT_EQ(base::UTF8ToUTF16(kFakeGivenName),
            profile_info_provider.GetGivenName());
}

TEST_F(NearbyShareProfileInfoProviderImplTest, ProfileUserName) {
  {
    // If profile user name is empty, return base::nullopt.
    Profile* profile = CreateProfile(std::string());
    NearbyShareProfileInfoProviderImpl profile_info_provider(profile);
    EXPECT_FALSE(profile_info_provider.GetProfileUserName());
  }
  {
    Profile* profile = CreateProfile(kFakeProfileUserName);
    NearbyShareProfileInfoProviderImpl profile_info_provider(profile);
    EXPECT_EQ(kFakeProfileUserName, profile_info_provider.GetProfileUserName());
  }
}
