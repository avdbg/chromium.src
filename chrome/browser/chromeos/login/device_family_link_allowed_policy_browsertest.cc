// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/login/auth/stub_authenticator_builder.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kFamilyLinkUser[] = "fl@gmail.com";
const char kFamilyLinkGaiaID[] = "111111";
const char kRegularUser[] = "regular@gmail.com";
const char kRegularGaiaID[] = "222222";
const char kSchoolUser[] = "student@edu.com";
const char kSchoolGaiaID[] = "333333";
const char kSchoolAllowlist[] = "*@edu.com";

}  // namespace

// Integration test for DeviceFamilyLinkAllowedPolicy changes.
class DeviceFamilyLinkAllowedPolicyTest : public LoginManagerTest {
 protected:
  DeviceFamilyLinkAllowedPolicyTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFamilyLinkOnSchoolDevice);
  }
  DeviceFamilyLinkAllowedPolicyTest(const DeviceFamilyLinkAllowedPolicyTest&) =
      delete;
  DeviceFamilyLinkAllowedPolicyTest& operator=(
      const DeviceFamilyLinkAllowedPolicyTest&) = delete;
  ~DeviceFamilyLinkAllowedPolicyTest() override = default;

  void AddUserToAllowlist(const std::string& user_id) {
    policy_helper_.device_policy()
        ->payload()
        .mutable_user_allowlist()
        ->add_user_allowlist(user_id);
    policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {kAccountsPrefUsers});
  }

  void SetDeviceAllowNewUsersPolicy(bool enabled) {
    policy_helper_.device_policy()
        ->payload()
        .mutable_allow_new_users()
        ->set_allow_new_users(enabled);
    policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {kAccountsPrefAllowNewUser});
  }

  void SetDeviceFamilyLinkAccountsAllowedPolicy(bool enabled) {
    policy_helper_.device_policy()
        ->payload()
        .mutable_family_link_accounts_allowed()
        ->set_family_link_accounts_allowed(enabled);
    policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {kAccountsPrefFamilyLinkAccountsAllowed});
  }

  void LoginFamilyLinkUser() {
    WizardController::SkipPostLoginScreensForTesting();
    UserContext user_context =
        LoginManagerMixin::CreateDefaultUserContext(family_link_user_);
    user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
    fake_gaia_.SetupFakeGaiaForChildUser(
        family_link_user_.account_id.GetUserEmail(),
        family_link_user_.account_id.GetGaiaId(),
        FakeGaiaMixin::kFakeRefreshToken, false /*issue_any_scope_token*/);
    login_manager_.AttemptLoginUsingAuthenticator(
        user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));
  }

 private:
  const LoginManagerMixin::TestUserInfo school_user_{
      AccountId::FromUserEmailGaiaId(kSchoolUser, kSchoolGaiaID)};
  const LoginManagerMixin::TestUserInfo regular_user_{
      AccountId::FromUserEmailGaiaId(kRegularUser, kRegularGaiaID)};
  const LoginManagerMixin::TestUserInfo family_link_user_{
      AccountId::FromUserEmailGaiaId(kFamilyLinkUser, kFamilyLinkGaiaID),
      user_manager::USER_TYPE_CHILD};

  policy::DevicePolicyCrosTestHelper policy_helper_;
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  UserPolicyMixin user_policy_mixin_{&mixin_host_,
                                     family_link_user_.account_id};
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  LoginManagerMixin login_manager_ = {
      &mixin_host_,
      {school_user_, family_link_user_, regular_user_},
      &fake_gaia_};

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that pods on login screen get updated with policy value changes.
IN_PROC_BROWSER_TEST_F(DeviceFamilyLinkAllowedPolicyTest, LoginScreenUpdates) {
  // No policy restrictions, all users available.
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 3);

  // User allowlist on - only school domain account available.
  AddUserToAllowlist(kSchoolAllowlist);
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 1);

  // Family Link allowed - school and Family Link accounts available.
  SetDeviceFamilyLinkAccountsAllowedPolicy(true);
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 2);

  // Family link off - Family Link account should disappear.
  SetDeviceFamilyLinkAccountsAllowedPolicy(false);
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 1);

  // Allow all new users.
  SetDeviceAllowNewUsersPolicy(true);
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 3);
}

// Tests that the user is signed out when policy value changes.
IN_PROC_BROWSER_TEST_F(DeviceFamilyLinkAllowedPolicyTest, InSessionUpdate) {
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  // Family Link allowed - school and Family Link accounts available.
  AddUserToAllowlist(kSchoolAllowlist);
  SetDeviceFamilyLinkAccountsAllowedPolicy(true);
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 2);

  LoginFamilyLinkUser();
  SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();

  content::WindowedNotificationObserver termination_waiter(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());

  // Family link off - Family Link user session should be terminated.
  SetDeviceFamilyLinkAccountsAllowedPolicy(false);
  EXPECT_TRUE(chrome::IsAttemptingShutdown());
  termination_waiter.Wait();
}

}  // namespace chromeos
