// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_manager_policy_controller.h"

#include <string>

#include "ash/components/account_manager/account_manager.h"
#include "ash/constants/ash_pref_names.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/account_manager/account_manager_edu_coexistence_controller.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/child_accounts/secondary_account_consent_logger.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "components/prefs/pref_service.h"

namespace ash {

AccountManagerPolicyController::AccountManagerPolicyController(
    Profile* profile,
    AccountManager* account_manager,
    const AccountId& device_account_id)
    : profile_(profile),
      account_manager_(account_manager),
      device_account_id_(device_account_id) {}

AccountManagerPolicyController::~AccountManagerPolicyController() {
  pref_change_registrar_.RemoveAll();
}

void AccountManagerPolicyController::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsAccountManagerAvailable(profile_))
    return;

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      chromeos::prefs::kSecondaryGoogleAccountSigninAllowed,
      base::BindRepeating(&AccountManagerPolicyController::
                              OnSecondaryAccountsSigninAllowedPrefChanged,
                          weak_factory_.GetWeakPtr()));
  // Take any necessary initial action based on the current state of the pref.
  OnSecondaryAccountsSigninAllowedPrefChanged();

  ChildAccountTypeChangedUserData* user_data =
      ChildAccountTypeChangedUserData::GetForProfile(profile_);
  child_account_type_changed_subscription_ =
      user_data->RegisterCallback(base::BindRepeating(
          &AccountManagerPolicyController::OnChildAccountTypeChanged,
          base::Unretained(this)));
  // Take any necessary initial action based on the current value.
  OnChildAccountTypeChanged(user_data->value());

  if (profile_->IsChild()) {
    if (base::FeatureList::IsEnabled(supervised_users::kEduCoexistenceFlowV2)) {
      edu_coexistence_consent_invalidation_controller_ =
          std::make_unique<EduCoexistenceConsentInvalidationController>(
              profile_, account_manager_, device_account_id_);
      edu_coexistence_consent_invalidation_controller_->Init();
    } else {
      // Invalidate secondary accounts if parental consent text version for EDU
      // accounts addition has changed.
      CheckEduCoexistenceSecondaryAccountsInvalidationVersion();
    }
  }
}

void AccountManagerPolicyController::RemoveSecondaryAccounts(
    const std::vector<::account_manager::Account>& accounts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The objective here is to remove all Secondary Accounts in Chrome OS
  // Account Manager. When this policy / pref is applied, all account
  // additions to Chrome OS Account Manager are blocked. Hence, we do not need
  // to take care of the case where accounts are being added to Account
  // Manager, while we are removing them from here. We can simply retrieve the
  // current list of accounts from Account Manager and then issue calls to
  // remove all Secondary Accounts.
  for (const auto& account : accounts) {
    if (account.key.account_type != account_manager::AccountType::kGaia) {
      // |kSecondaryGoogleAccountSigninAllowed| applies only to Gaia accounts.
      // Ignore other types of accounts.
      continue;
    }

    if (device_account_id_.GetAccountType() == AccountType::GOOGLE &&
        account.key.id == device_account_id_.GetGaiaId()) {
      // Do not remove the Device Account.
      continue;
    }

    // This account is a Secondary Gaia account. Remove it.
    account_manager_->RemoveAccount(account.key);
  }
}

void AccountManagerPolicyController::
    OnSecondaryAccountsSigninAllowedPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (profile_->GetPrefs()->GetBoolean(
          chromeos::prefs::kSecondaryGoogleAccountSigninAllowed)) {
    return;
  }

  account_manager_->GetAccounts(
      base::BindOnce(&AccountManagerPolicyController::RemoveSecondaryAccounts,
                     weak_factory_.GetWeakPtr()));
}

void AccountManagerPolicyController::OnChildAccountTypeChanged(
    bool type_changed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!type_changed) {
    return;
  }

  account_manager_->GetAccounts(
      base::BindOnce(&AccountManagerPolicyController::RemoveSecondaryAccounts,
                     weak_factory_.GetWeakPtr()));
}

void AccountManagerPolicyController::
    CheckEduCoexistenceSecondaryAccountsInvalidationVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_->IsChild());

  const std::string stored_version = profile_->GetPrefs()->GetString(
      chromeos::prefs::kEduCoexistenceSecondaryAccountsInvalidationVersion);
  const std::string current_version =
      SecondaryAccountConsentLogger::GetSecondaryAccountsInvalidationVersion();

  if (stored_version == current_version)
    return;

  account_manager_->GetAccounts(
      base::BindOnce(&AccountManagerPolicyController::
                         InvalidateSecondaryAccountsOnEduConsentChange,
                     weak_factory_.GetWeakPtr(), current_version));
}

void AccountManagerPolicyController::
    InvalidateSecondaryAccountsOnEduConsentChange(
        const std::string& new_invalidation_version,
        const std::vector<::account_manager::Account>& accounts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& account : accounts) {
    if (account.key.account_type != account_manager::AccountType::kGaia) {
      continue;
    }

    if (device_account_id_.GetAccountType() == AccountType::GOOGLE &&
        account.key.id == device_account_id_.GetGaiaId()) {
      // Do not invalidate the Device Account.
      continue;
    }

    // This account is a Secondary Gaia account. Invalidate it.
    account_manager_->UpdateToken(account.key, AccountManager::kInvalidToken);
  }

  profile_->GetPrefs()->SetString(
      chromeos::prefs::kEduCoexistenceSecondaryAccountsInvalidationVersion,
      new_invalidation_version);
}

void AccountManagerPolicyController::Shutdown() {
  child_account_type_changed_subscription_ = {};
  edu_coexistence_consent_invalidation_controller_.reset();
}

}  // namespace ash
