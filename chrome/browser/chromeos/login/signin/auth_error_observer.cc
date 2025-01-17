// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/auth_error_observer.h"

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

namespace chromeos {

// static
bool AuthErrorObserver::ShouldObserve(Profile* profile) {
  const user_manager::User* const user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  return user &&
         (user->HasGaiaAccount() ||
          user->GetType() == user_manager::USER_TYPE_SUPERVISED_DEPRECATED);
}

AuthErrorObserver::AuthErrorObserver(Profile* profile) : profile_(profile) {
  DCHECK(ShouldObserve(profile));
}

AuthErrorObserver::~AuthErrorObserver() = default;

void AuthErrorObserver::StartObserving() {
  syncer::SyncService* const sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service)
    sync_service->AddObserver(this);

  SigninErrorController* const error_controller =
      SigninErrorControllerFactory::GetForProfile(profile_);
  if (error_controller) {
    error_controller->AddObserver(this);
    OnErrorChanged();
  }
}

void AuthErrorObserver::Shutdown() {
  syncer::SyncService* const sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service)
    sync_service->RemoveObserver(this);

  SigninErrorController* const error_controller =
      SigninErrorControllerFactory::GetForProfile(profile_);
  if (error_controller)
    error_controller->RemoveObserver(this);
}

void AuthErrorObserver::OnStateChanged(syncer::SyncService* sync) {
  HandleAuthError(sync->GetAuthError());
}

void AuthErrorObserver::OnErrorChanged() {
  // This notification could have come for any account but we are only
  // interested in errors for the Primary Account.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  HandleAuthError(identity_manager->GetErrorStateOfRefreshTokenForAccount(
      identity_manager->GetPrimaryAccountId(
          signin::ConsentLevel::kNotRequired)));
}

void AuthErrorObserver::HandleAuthError(
    const GoogleServiceAuthError& auth_error) {
  const user_manager::User* const user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user->HasGaiaAccount() ||
         user->GetType() == user_manager::USER_TYPE_SUPERVISED_DEPRECATED);

  if (auth_error.IsPersistentError()) {
    // Invalidate OAuth2 refresh token to force Gaia sign-in flow. This is
    // needed because sign-out/sign-in solution is suggested to the user.
    LOG(WARNING) << "Invalidate OAuth token because of an auth error: "
                 << auth_error.ToString();
    const AccountId& account_id = user->GetAccountId();
    DCHECK(account_id.is_valid());

    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        account_id, user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);
    RecordReauthReason(account_id, ReauthReason::SYNC_FAILED);
  } else if (auth_error.state() == GoogleServiceAuthError::NONE) {
    if (user->oauth_token_status() ==
        user_manager::User::OAUTH2_TOKEN_STATUS_INVALID) {
      LOG(ERROR) << "Got an incorrectly invalidated token case, restoring "
                    "token status.";
      user_manager::UserManager::Get()->SaveUserOAuthStatus(
          user->GetAccountId(), user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
    }
  }
}

}  // namespace chromeos
