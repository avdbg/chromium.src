// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_interactive_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/password_manager/password_manager_signin_intercept_test_helper.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#endif  // ENABLE_DICE_SUPPORT

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Wait until |condition| returns true.
void WaitForCondition(base::RepeatingCallback<bool()> condition) {
  while (!condition.Run()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}
#endif  // ENABLE_DICE_SUPPORT

}  // namespace

namespace password_manager {

class PasswordManagerInteractiveTest
    : public PasswordManagerInteractiveTestBase {
 public:
  PasswordManagerInteractiveTest() {
    // Turn off waiting for server predictions before filing. It makes filling
    // behaviour more deterministic. Filling with server predictions is tested
    // in PasswordFormManager unit tests.
    password_manager::PasswordFormManager::
        set_wait_for_server_predictions_for_filling(false);
  }
  ~PasswordManagerInteractiveTest() override = default;
};

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest, UsernameChanged) {
  // At first let us save a credential to the password store.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.url = embedded_test_server()->base_url();
  signin_form.username_value = base::ASCIIToUTF16("temp");
  signin_form.password_value = base::ASCIIToUTF16("random");
  password_store->AddLogin(signin_form);

  // Load the page to have the saved credentials autofilled.
  NavigateToFile("/password/signup_form.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  WaitForElementValue("username_field", "temp");
  WaitForElementValue("password_field", "random");

  // Change username and submit. This should add the characters "orary" to the
  // already autofilled username.
  FillElementWithValue("username_field", "orary", "temporary");

  NavigationObserver navigation_observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string submit =
      "document.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecuteScript(WebContents(), submit));
  navigation_observer.Wait();
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  WaitForPasswordStore();
  EXPECT_FALSE(password_store->IsEmpty());

  // Verify that there are two saved password, the old password and the new
  // password.
  password_manager::TestPasswordStore::PasswordMap stored_passwords =
      password_store->stored_passwords();
  EXPECT_EQ(1u, stored_passwords.size());
  EXPECT_EQ(2u, stored_passwords.begin()->second.size());
  EXPECT_EQ(base::UTF8ToUTF16("temp"),
            (stored_passwords.begin()->second)[0].username_value);
  EXPECT_EQ(base::UTF8ToUTF16("temporary"),
            (stored_passwords.begin()->second)[1].username_value);
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       ManualFallbackForSaving) {
  NavigateToFile("/password/password_form.html");

  FillElementWithValue("password_field", "123");
  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForFallbackForSaving();

  // The save prompt should be available but shouldn't pop up automatically.
  EXPECT_TRUE(prompt_observer.IsSavePromptAvailable());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());

  // Simulate several navigations.
  NavigateToFile("/password/signup_form.html");
  NavigateToFile("/password/failed.html");
  NavigateToFile("/password/done.html");

  // The save prompt should be still available.
  EXPECT_TRUE(prompt_observer.IsSavePromptAvailable());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  WaitForPasswordStore();
  CheckThatCredentialsStored("", "123");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       ManualFallbackForSaving_HideAfterTimeout) {
  NavigateToFile("/password/password_form.html");
  ManagePasswordsUIController::set_save_fallback_timeout_in_seconds(0);

  FillElementWithValue("password_field", "123");
  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForFallbackForSaving();

  // Since the timeout is changed to zero for testing, the save prompt should be
  // hidden right after show.
  prompt_observer.WaitForInactiveState();
  EXPECT_FALSE(prompt_observer.IsSavePromptAvailable());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       ManualFallbackForSaving_HideIcon) {
  NavigateToFile("/password/password_form.html");

  FillElementWithValue("password_field", "123");
  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForFallbackForSaving();

  // Delete typed content and verify that inactive state is reached.
  SimulateUserDeletingFieldContent("password_field");
  prompt_observer.WaitForInactiveState();
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       ManualFallbackForSaving_GoToManagedState) {
  // At first let us save a credential to the password store.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.url = embedded_test_server()->base_url();
  signin_form.username_value = base::ASCIIToUTF16("temp");
  signin_form.password_value = base::ASCIIToUTF16("random");
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");

  SimulateUserDeletingFieldContent("password_field");
  FillElementWithValue("password_field", "123");
  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForFallbackForSaving();

  // Delete typed content and verify that management state is reached.
  SimulateUserDeletingFieldContent("password_field");
  prompt_observer.WaitForManagementState();
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       PromptForXHRWithoutOnSubmit) {
  NavigateToFile("/password/password_xhr_submit.html");

  // Verify that if XHR navigation occurs and the form is properly filled out,
  // we try and save the password even though onsubmit hasn't been called.
  FillElementWithValue("username_field", "user");
  FillElementWithValue("password_field", "1234");
  NavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(WebContents(), "send_xhr()"));
  observer.Wait();
  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       PromptForXHRWithNewPasswordsWithoutOnSubmit) {
  NavigateToFile("/password/password_xhr_submit.html");

  // Verify that if XHR navigation occurs and the form is properly filled out,
  // we try and save the password even though onsubmit hasn't been called.
  // Specifically verify that the password form saving new passwords is treated
  // the same as a login form.
  FillElementWithValue("signup_username_field", "user");
  FillElementWithValue("signup_password_field", "1234");
  FillElementWithValue("confirmation_password_field", "1234");
  NavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(WebContents(), "send_xhr()"));
  observer.Wait();
  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       PromptForFetchWithoutOnSubmit) {
  NavigateToFile("/password/password_fetch_submit.html");

  // Verify that if Fetch navigation occurs and the form is properly filled out,
  // we try and save the password even though onsubmit hasn't been called.
  FillElementWithValue("username_field", "user");
  FillElementWithValue("password_field", "1234");

  NavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(WebContents(), "send_fetch()"));
  observer.Wait();
  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       PromptForFetchWithNewPasswordsWithoutOnSubmit) {
  NavigateToFile("/password/password_fetch_submit.html");

  // Verify that if Fetch navigation occurs and the form is properly filled out,
  // we try and save the password even though onsubmit hasn't been called.
  // Specifically verify that the password form saving new passwords is treated
  // the same as a login form.
  FillElementWithValue("signup_username_field", "user");
  FillElementWithValue("signup_password_field", "1234");
  FillElementWithValue("confirmation_password_field", "1234");
  NavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(WebContents(), "send_fetch()"));
  observer.Wait();
  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       AutofillPasswordFormWithoutUsernameField) {
  std::string submit = "document.getElementById('submit-button').click();";
  VerifyPasswordIsSavedAndFilled("/password/form_with_only_password_field.html",
                                 std::string(), "password", submit);
}

// Tests that if a site embeds the login and signup forms into one <form>, the
// login form still gets autofilled.
IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       AutofillLoginSignupForm) {
  std::string submit = "document.getElementById('submit').click();";
  VerifyPasswordIsSavedAndFilled("/password/login_signup_form.html", "username",
                                 "password", submit);
}

// Tests that password suggestions still work if the fields have the
// "autocomplete" attribute set to off.
IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       AutofillPasswordFormWithAutocompleteOff) {
  std::string submit = "document.getElementById('submit').click();";
  VerifyPasswordIsSavedAndFilled(
      "/password/password_autocomplete_off_test.html", "username", "password",
      submit);
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       AutofillPasswordNoFormElement) {
  VerifyPasswordIsSavedAndFilled("/password/no_form_element.html",
                                 "username_field", "password_field",
                                 "send_xhr();");
}

// Check that we can fill in cases where <base href> is set and the action of
// the form is not set. Regression test for https://crbug.com/360230.
IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       AutofillBaseTagWithNoActionTest) {
  std::string submit = "document.getElementById('submit_button').click();";
  VerifyPasswordIsSavedAndFilled("/password/password_xhr_submit.html",
                                 "username_field", "password_field", submit);
}

// This fixture enables detecting submission on form clear feature.
class PasswordManagerInteractiveTestSubmissionDetectionOnFormClear
    : public PasswordManagerInteractiveTest {
 public:
  PasswordManagerInteractiveTestSubmissionDetectionOnFormClear() {
    feature_list_.InitAndEnableFeature(
        features::kDetectFormSubmissionOnFormClear);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that submission is detected when change password form is reset.
IN_PROC_BROWSER_TEST_F(
    PasswordManagerInteractiveTestSubmissionDetectionOnFormClear,
    ChangePwdFormCleared) {
  base::HistogramTester histogram_tester;
  // At first let us save credentials to the PasswordManager.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.username_value = base::ASCIIToUTF16("temp");
  signin_form.password_value = base::ASCIIToUTF16("old_pw");
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/cleared_change_password_forms.html");

  // Fill a form and submit through a <input type="submit"> button.
  std::unique_ptr<BubbleObserver> prompt_observer(
      new BubbleObserver(WebContents()));

  FillElementWithValue("chg_new_password_1", "new_pw", "new_pw");
  FillElementWithValue("chg_new_password_2", "new_pw", "new_pw");

  std::string submit = "document.getElementById('chg_clear_button').click();";
  ASSERT_TRUE(content::ExecuteScript(WebContents(), submit));

  EXPECT_TRUE(prompt_observer->IsUpdatePromptShownAutomatically());

  // We emulate that the user clicks "Update" button.
  prompt_observer->AcceptUpdatePrompt();

  // Check that credentials are stored.
  WaitForPasswordStore();
  CheckThatCredentialsStored("temp", "new_pw");

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SuccessfulSubmissionIndicatorEvent",
      autofill::mojom::SubmissionIndicatorEvent::CHANGE_PASSWORD_FORM_CLEARED,
      1);
}

// Tests that submission is detected when all password fields in a change
// password form are cleared and not detected when only some fields are cleared.
IN_PROC_BROWSER_TEST_F(
    PasswordManagerInteractiveTestSubmissionDetectionOnFormClear,
    ChangePwdFormFieldsCleared) {
  // At first let us save credentials to the PasswordManager.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.username_value = base::ASCIIToUTF16("temp");
  signin_form.password_value = base::ASCIIToUTF16("old_pw");
  password_store->AddLogin(signin_form);

  for (bool all_fields_cleared : {false, true}) {
    base::HistogramTester histogram_tester;
    SCOPED_TRACE(testing::Message("#all_fields_cleared = ")
                 << all_fields_cleared);
    NavigateToFile("/password/cleared_change_password_forms.html");

    // Fill a form and submit through a <input type="submit"> button.
    std::unique_ptr<BubbleObserver> prompt_observer(
        new BubbleObserver(WebContents()));

    FillElementWithValue("chg_new_password_1", "new_pw", "new_pw");
    FillElementWithValue("chg_new_password_2", "new_pw", "new_pw");

    std::string submit =
        all_fields_cleared
            ? "document.getElementById('chg_clear_all_fields_button').click();"
            : "document.getElementById('chg_clear_some_fields_button').click()"
              ";";
    ASSERT_TRUE(content::ExecuteScript(WebContents(), submit));

    if (all_fields_cleared)
      EXPECT_TRUE(prompt_observer->IsUpdatePromptShownAutomatically());
    else
      EXPECT_FALSE(prompt_observer->IsUpdatePromptShownAutomatically());

    if (all_fields_cleared) {
      // We emulate that the user clicks "Update" button.
      prompt_observer->AcceptUpdatePrompt();

      // Check that credentials are stored.
      WaitForPasswordStore();
      CheckThatCredentialsStored("temp", "new_pw");
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.SuccessfulSubmissionIndicatorEvent",
          autofill::mojom::SubmissionIndicatorEvent::
              CHANGE_PASSWORD_FORM_CLEARED,
          1);
    }
  }
}

// Tests that submission is detected when the new password field outside the
// form tag is cleared not detected when other password fields are cleared.
IN_PROC_BROWSER_TEST_F(
    PasswordManagerInteractiveTestSubmissionDetectionOnFormClear,
    ChangePwdFormRelevantFormlessFieldsCleared) {
  base::HistogramTester histogram_tester;
  // At first let us save credentials to the PasswordManager.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.username_value = base::ASCIIToUTF16("temp");
  signin_form.password_value = base::ASCIIToUTF16("old_pw");
  password_store->AddLogin(signin_form);

  for (bool relevant_fields_cleared : {false, true}) {
    SCOPED_TRACE(testing::Message("#relevant_fields_cleared = ")
                 << relevant_fields_cleared);
    NavigateToFile("/password/cleared_change_password_forms.html");

    // Fill a form and submit through a <input type="submit"> button.
    std::unique_ptr<BubbleObserver> prompt_observer(
        new BubbleObserver(WebContents()));

    FillElementWithValue("formless_chg_new_password_1", "new_pw", "new_pw");
    FillElementWithValue("formless_chg_new_password_2", "new_pw", "new_pw");

    std::string submit = relevant_fields_cleared
                             ? "document.getElementById('chg_clear_all_"
                               "formless_fields_button').click();"
                             : "document.getElementById('chg_clear_some_"
                               "formless_fields_button').click();";

    ASSERT_TRUE(content::ExecuteScript(WebContents(), submit));

    if (relevant_fields_cleared) {
      prompt_observer->WaitForAutomaticUpdatePrompt();
      EXPECT_TRUE(prompt_observer->IsUpdatePromptShownAutomatically());
    } else {
      EXPECT_FALSE(prompt_observer->IsUpdatePromptShownAutomatically());
    }

    if (relevant_fields_cleared) {
      // We emulate that the user clicks "Update" button.
      prompt_observer->AcceptUpdatePrompt();

      // Check that credentials are stored.
      WaitForPasswordStore();
      CheckThatCredentialsStored("temp", "new_pw");
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.SuccessfulSubmissionIndicatorEvent",
          autofill::mojom::SubmissionIndicatorEvent::
              CHANGE_PASSWORD_FORM_CLEARED,
          1);
    }
  }
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// This test suite only applies to Gaia signin page, and checks that the
// signin interception bubble and the password bubbles never conflict.
class PasswordManagerInteractiveTestWithSigninInterception
    : public PasswordManagerInteractiveTest {
 public:
  PasswordManagerInteractiveTestWithSigninInterception()
      : helper_(&https_test_server()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PasswordManagerInteractiveTest::SetUpCommandLine(command_line);
    helper_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    helper_.SetUpOnMainThread();
    PasswordManagerInteractiveTest::SetUpOnMainThread();
  }

 protected:
  PasswordManagerSigninInterceptTestHelper helper_;
};

// Checks that password update suppresses signin interception.
IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTestWithSigninInterception,
                       InterceptionBubbleSuppressedByPendingPasswordUpdate) {
  Profile* profile = browser()->profile();
  helper_.SetupProfilesForInterception(profile);
  // Prepopulate Gaia credentials to trigger an update bubble.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  helper_.StoreGaiaCredentials(password_store);

  helper_.NavigateToGaiaSigninPage(WebContents());

  // Have user interact with the page
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  // Wait for password to be autofilled.
  WaitForElementValue("password_field", "pw");

  // Change username and submit. This should add the characters "new" to the
  // already autofilled password.
  FillElementWithValue("password_field", "new", "pwnew");

  // Wait until the form change is picked up by the password manager.
  const PasswordManager* password_manager =
      ChromePasswordManagerClient::FromWebContents(WebContents())
          ->GetPasswordManager();
  WaitForCondition(
      base::BindRepeating(&PasswordManager::IsFormManagerPendingPasswordUpdate,
                          base::Unretained(password_manager)));

  // Start the navigation.
  NavigationObserver navigation_observer(WebContents());
  content::ExecuteScriptAsync(
      WebContents(), "document.getElementById('input_submit_button').click()");

  // Complete the Gaia signin before the navigation completes.
  CoreAccountId account_id = helper_.AddGaiaAccountToProfile(
      profile, helper_.gaia_email(), helper_.gaia_id());

  // Check that interception does not happen.
  base::HistogramTester histogram_tester;
  DiceWebSigninInterceptor* signin_interceptor =
      helper_.GetSigninInterceptor(profile);
  signin_interceptor->MaybeInterceptWebSignin(WebContents(), account_id,
                                              /*is_new_account=*/true,
                                              /*is_sync_signin=*/false);
  EXPECT_FALSE(signin_interceptor->is_interception_in_progress());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortPasswordUpdatePending, 1);

  // Complete the navigation. The stored password "pw" was overridden with
  // "pwnew", so update prompt is expected.
  BubbleObserver prompt_observer(WebContents());
  navigation_observer.Wait();
  EXPECT_TRUE(prompt_observer.IsUpdatePromptShownAutomatically());
}
#endif  // ENABLE_DICE_SUPPORT

}  // namespace password_manager
