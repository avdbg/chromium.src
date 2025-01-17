// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/edu_coexistence_login_screen.h"

#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/login/test/wizard_controller_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_context.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos_onboarding.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

namespace {

SystemWebDialogDelegate* GetInlineLoginDialog() {
  return chromeos::SystemWebDialogDelegate::FindInstance(
      SupervisedUserService::GetEduCoexistenceLoginUrl());
}

bool IsInlineLoginDialogShown() {
  return GetInlineLoginDialog() != nullptr;
}

}  // namespace

class EduCoexistenceLoginBrowserTest : public OobeBaseTest {
 public:
  EduCoexistenceLoginBrowserTest();
  ~EduCoexistenceLoginBrowserTest() override = default;

  EduCoexistenceLoginBrowserTest(const EduCoexistenceLoginBrowserTest&) =
      delete;
  EduCoexistenceLoginBrowserTest& operator=(
      const EduCoexistenceLoginBrowserTest&) = delete;

  void SetUpOnMainThread() override;

 protected:
  void WaitForScreenExit();

  EduCoexistenceLoginScreen* GetEduCoexistenceLoginScreen();

  const base::Optional<EduCoexistenceLoginScreen::Result>& result() {
    return result_;
  }

  LoginManagerMixin& login_manager_mixin() { return login_manager_mixin_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  void HandleScreenExit(EduCoexistenceLoginScreen::Result result);

  base::OnceCallback<void()> quit_closure_;

  base::Optional<EduCoexistenceLoginScreen::Result> result_;

  EduCoexistenceLoginScreen::ScreenExitCallback original_callback_;

  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  base::test::ScopedFeatureList feature_list_;

  base::HistogramTester histogram_tester_;

  LoginManagerMixin login_manager_mixin_{&mixin_host_, {}, &fake_gaia_};
};

EduCoexistenceLoginBrowserTest::EduCoexistenceLoginBrowserTest() {
  feature_list_.InitAndEnableFeature(supervised_users::kEduCoexistenceFlowV2);
}

void EduCoexistenceLoginBrowserTest::SetUpOnMainThread() {
  EduCoexistenceLoginScreen* screen = GetEduCoexistenceLoginScreen();
  original_callback_ = screen->get_exit_callback_for_test();
  screen->set_exit_callback_for_test(
      base::BindRepeating(&EduCoexistenceLoginBrowserTest::HandleScreenExit,
                          base::Unretained(this)));
  OobeBaseTest::SetUpOnMainThread();
}

void EduCoexistenceLoginBrowserTest::HandleScreenExit(
    EduCoexistenceLoginScreen::Result result) {
  result_ = result;
  original_callback_.Run(result);
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

void EduCoexistenceLoginBrowserTest::WaitForScreenExit() {
  if (result_.has_value())
    return;
  base::RunLoop run_loop;
  quit_closure_ = base::BindOnce(run_loop.QuitClosure());
  run_loop.Run();
}

EduCoexistenceLoginScreen*
EduCoexistenceLoginBrowserTest::GetEduCoexistenceLoginScreen() {
  return EduCoexistenceLoginScreen::Get(
      WizardController::default_controller()->screen_manager());
}

IN_PROC_BROWSER_TEST_F(EduCoexistenceLoginBrowserTest, RegularUserLogin) {
  login_manager_mixin().LoginAsNewRegularUser();
  WaitForScreenExit();

  // Regular user login shouldn't show the EduCoexistenceLoginScreen.
  EXPECT_EQ(result().value(), EduCoexistenceLoginScreen::Result::SKIPPED);

  histogram_tester().ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Edu-coexistence-login.Done", 0);
}

class EduCoexistenceLoginChildBrowserTest
    : public EduCoexistenceLoginBrowserTest {
 public:
  // Child users require a user policy, set up an empty one so the user can
  // get through login.
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
    EduCoexistenceLoginBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void LoginAsNewChildUser() {
    WizardController::default_controller()
        ->get_wizard_context_for_testing()
        ->sign_in_as_child = true;
    login_manager_mixin().LoginAsNewChildUser();

    WizardControllerExitWaiter(UserCreationView::kScreenId).Wait();
    WizardControllerExitWaiter(LocaleSwitchView::kScreenId).Wait();

    base::RunLoop().RunUntilIdle();
  }

 private:
  LocalPolicyTestServerMixin policy_server_mixin_{&mixin_host_};
  UserPolicyMixin user_policy_mixin_{
      &mixin_host_,
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId),
      &policy_server_mixin_};
};

IN_PROC_BROWSER_TEST_F(EduCoexistenceLoginChildBrowserTest, ChildUserLogin) {
  LoginAsNewChildUser();

  WizardController* wizard = WizardController::default_controller();

  EXPECT_EQ(wizard->current_screen()->screen_id(),
            EduCoexistenceLoginScreen::kScreenId);

  EduCoexistenceLoginScreen* screen = GetEduCoexistenceLoginScreen();

  // Expect that the inline login dialog is shown.
  EXPECT_TRUE(IsInlineLoginDialogShown());
  screen->Hide();
  base::RunLoop().RunUntilIdle();

  // Expect that the inline login dialog is hidden.
  EXPECT_FALSE(IsInlineLoginDialogShown());

  screen->Show(wizard->get_wizard_context_for_testing());

  // Expect that the inline login dialog is shown.
  EXPECT_TRUE(IsInlineLoginDialogShown());

  // Dialog got closed.
  GetInlineLoginDialog()->Close();
  WaitForScreenExit();

  histogram_tester().ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Edu-coexistence-login.Done", 1);
}

}  // namespace chromeos
