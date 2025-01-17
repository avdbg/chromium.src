// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_test.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/test_utils.h"

using base::ASCIIToUTF16;
using password_manager::PasswordFormManager;
using testing::Return;
using testing::ReturnRef;

namespace {
constexpr char kTestUsername[] = "test_username";
constexpr char kTestOrigin[] = "https://www.example.com";
}  // namespace

ManagePasswordsTest::ManagePasswordsTest() {
  fetcher_.Fetch();

  password_form_.signon_realm = kTestOrigin;
  password_form_.url = GURL(kTestOrigin);
  password_form_.username_value = ASCIIToUTF16(kTestUsername);
  password_form_.password_value = ASCIIToUTF16("test_password");

  federated_form_.signon_realm =
      "federation://example.com/somelongeroriginurl.com";
  federated_form_.url = GURL(kTestOrigin);
  federated_form_.federation_origin =
      url::Origin::Create(GURL("https://somelongeroriginurl.com/"));
  federated_form_.username_value =
      base::ASCIIToUTF16("test_federation_username");

  // Create a simple sign-in form.
  observed_form_.url = password_form_.url;
  autofill::FormFieldData field;
  field.form_control_type = "text";
  observed_form_.fields.push_back(field);
  field.form_control_type = "password";
  observed_form_.fields.push_back(field);

  submitted_form_ = observed_form_;
  submitted_form_.fields[1].value = ASCIIToUTF16("password");

  // Turn off waiting for server predictions in order to avoid dealing with
  // posted tasks in PasswordFormManager.
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
}

ManagePasswordsTest::~ManagePasswordsTest() = default;

void ManagePasswordsTest::SetUpOnMainThread() {
  AddTabAtIndex(0, GURL(kTestOrigin), ui::PAGE_TRANSITION_TYPED);
}

void ManagePasswordsTest::SetUpInProcessBrowserTestFixture() {
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating([](content::BrowserContext* context) {
                // Overwrite the password store early before it's accessed by
                // safe browsing.
                PasswordStoreFactory::GetInstance()->SetTestingFactory(
                    context,
                    base::BindRepeating(&password_manager::BuildPasswordStore<
                                        content::BrowserContext,
                                        password_manager::TestPasswordStore>));
              }));
}

void ManagePasswordsTest::ExecuteManagePasswordsCommand() {
  // Show the window to ensure that it's active.
  browser()->window()->Show();

  CommandUpdater* updater = browser()->command_controller();
  EXPECT_TRUE(updater->IsCommandEnabled(IDC_MANAGE_PASSWORDS_FOR_PAGE));
  EXPECT_TRUE(updater->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE));
}

void ManagePasswordsTest::SetupManagingPasswords() {
  std::vector<const password_manager::PasswordForm*> forms;
  for (auto* form : {&password_form_, &federated_form_}) {
    forms.push_back(form);
    GetController()->OnPasswordAutofilled(forms, url::Origin::Create(form->url),
                                          nullptr);
  }
}

void ManagePasswordsTest::SetupPendingPassword() {
  GetController()->OnPasswordSubmitted(CreateFormManager());
}

void ManagePasswordsTest::SetupAutomaticPassword() {
  GetController()->OnAutomaticPasswordSave(CreateFormManager());
}

void ManagePasswordsTest::SetupAutoSignin(
    std::vector<std::unique_ptr<password_manager::PasswordForm>>
        local_credentials) {
  ASSERT_FALSE(local_credentials.empty());
  url::Origin origin = url::Origin::Create(local_credentials[0]->url);
  GetController()->OnAutoSignin(std::move(local_credentials), origin);
}

void ManagePasswordsTest::SetupSafeState() {
  browser()->profile()->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToDoubleT());
  SetupPendingPassword();
  GetController()->SavePassword(password_form_.username_value,
                                password_form_.password_value);
  GetController()->OnBubbleHidden();
  PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());

  EXPECT_EQ(GetController()->GetState(),
            password_manager::ui::PASSWORD_UPDATED_SAFE_STATE);
}

void ManagePasswordsTest::SetupMoreToFixState() {
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(browser()->profile(),
                                          ServiceAccessType::IMPLICIT_ACCESS);
  // This is an unrelated insecure credential that should still be fixed.
  password_manager::InsecureCredential credential(
      "https://somesite.com/", ASCIIToUTF16(kTestUsername), base::Time(),
      password_manager::InsecureType::kLeaked,
      password_manager::IsMuted(false));
  password_store->AddInsecureCredential(credential);
  SetupPendingPassword();
  GetController()->SavePassword(password_form_.username_value,
                                password_form_.password_value);
  GetController()->OnBubbleHidden();
  PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());

  EXPECT_EQ(GetController()->GetState(),
            password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX);
}

void ManagePasswordsTest::SetupUnsafeState() {
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(browser()->profile(),
                                          ServiceAccessType::IMPLICIT_ACCESS);
  // This is an unrelated insecure credential that should still be fixed.
  password_manager::InsecureCredential some_credential(
      "https://somesite.com/", ASCIIToUTF16(kTestUsername), base::Time(),
      password_manager::InsecureType::kLeaked,
      password_manager::IsMuted(false));
  password_manager::InsecureCredential current_credential(
      password_form_.signon_realm, password_form_.username_value, base::Time(),
      password_manager::InsecureType::kLeaked,
      password_manager::IsMuted(false));
  password_store->AddInsecureCredential(some_credential);
  password_store->AddInsecureCredential(current_credential);
  SetupPendingPassword();
  GetController()->SavePassword(password_form_.username_value,
                                password_form_.password_value);
  GetController()->OnBubbleHidden();
  PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());

  EXPECT_EQ(GetController()->GetState(),
            password_manager::ui::PASSWORD_UPDATED_UNSAFE_STATE);
}

void ManagePasswordsTest::SetupMovingPasswords() {
  // The move bubble is shown only to signed in users. Make sure there is one.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  AccountInfo info =
      signin::MakePrimaryAccountAvailable(identity_manager, "test@email.com");
  auto form_manager = std::make_unique<
      testing::NiceMock<password_manager::MockPasswordFormManagerForUI>>();
  password_manager::MockPasswordFormManagerForUI* form_manager_ptr =
      form_manager.get();
  std::vector<const password_manager::PasswordForm*> best_matches = {
      test_form()};
  EXPECT_CALL(*form_manager, GetBestMatches).WillOnce(ReturnRef(best_matches));
  ON_CALL(*form_manager, GetPendingCredentials)
      .WillByDefault(ReturnRef(*test_form()));
  ON_CALL(*form_manager, GetFederatedMatches)
      .WillByDefault(
          Return(std::vector<const password_manager::PasswordForm*>{}));
  ON_CALL(*form_manager, GetURL).WillByDefault(ReturnRef(test_form()->url));
  GetController()->OnShowMoveToAccountBubble(std::move(form_manager));
  // Clearing the mock here ensures that |GetBestMatches| won't be called with a
  // reference to |best_matches|.
  testing::Mock::VerifyAndClear(form_manager_ptr);
}

std::unique_ptr<base::HistogramSamples> ManagePasswordsTest::GetSamples(
    const char* histogram) {
  // Ensure that everything has been properly recorded before pulling samples.
  content::RunAllPendingInMessageLoop();
  return histogram_tester_.GetHistogramSamplesSinceCreation(histogram);
}

ManagePasswordsUIController* ManagePasswordsTest::GetController() {
  return ManagePasswordsUIController::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
}

std::unique_ptr<PasswordFormManager> ManagePasswordsTest::CreateFormManager() {
  auto form_manager = std::make_unique<PasswordFormManager>(
      &client_, driver_.AsWeakPtr(), observed_form_, &fetcher_,
      std::make_unique<password_manager::PasswordSaveManagerImpl>(
          base::WrapUnique(new password_manager::StubFormSaver)),
      nullptr /*  metrics_recorder */);

  password_manager::InsecureCredential credential(
      password_form_.signon_realm, password_form_.username_value, base::Time(),
      password_manager::InsecureType::kLeaked,
      password_manager::IsMuted(false));
  fetcher_.set_insecure_credentials({credential});

  fetcher_.NotifyFetchCompleted();

  form_manager->ProvisionallySave(submitted_form_, &driver_,
                                  nullptr /* possible_username */);

  return form_manager;
}
