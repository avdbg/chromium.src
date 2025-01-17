// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_auth_recorder.h"

#include "ash/public/cpp/tablet_mode.h"
#include "base/metrics/histogram_functions.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

using AuthMethod = LoginAuthRecorder::AuthMethod;
using AuthMethodSwitchType = LoginAuthRecorder::AuthMethodSwitchType;

namespace {

base::Optional<AuthMethodSwitchType> SwitchFromPasswordTo(AuthMethod current) {
  DCHECK_NE(AuthMethod::kPassword, current);
  switch (current) {
    case AuthMethod::kPin:
      return AuthMethodSwitchType::kPasswordToPin;
    case AuthMethod::kSmartlock:
      return AuthMethodSwitchType::kPasswordToSmartlock;
    case AuthMethod::kFingerprint:
      return AuthMethodSwitchType::kPasswordToFingerprint;
    case AuthMethod::kChallengeResponse:
      return AuthMethodSwitchType::kPasswordToChallengeResponse;
    case AuthMethod::kPassword:
    case AuthMethod::kNothing:
      NOTREACHED();
      return base::nullopt;
  }
}

base::Optional<AuthMethodSwitchType> SwitchFromPinTo(AuthMethod current) {
  DCHECK_NE(AuthMethod::kPin, current);
  switch (current) {
    case AuthMethod::kPassword:
      return AuthMethodSwitchType::kPinToPassword;
    case AuthMethod::kSmartlock:
      return AuthMethodSwitchType::kPinToSmartlock;
    case AuthMethod::kFingerprint:
      return AuthMethodSwitchType::kPinToFingerprint;
    case AuthMethod::kPin:
    case AuthMethod::kChallengeResponse:
    case AuthMethod::kNothing:
      NOTREACHED();
      return base::nullopt;
  }
}

base::Optional<AuthMethodSwitchType> SwitchFromSmartlockTo(AuthMethod current) {
  DCHECK_NE(AuthMethod::kSmartlock, current);
  switch (current) {
    case AuthMethod::kPassword:
      return AuthMethodSwitchType::kSmartlockToPassword;
    case AuthMethod::kPin:
      return AuthMethodSwitchType::kSmartlockToPin;
    case AuthMethod::kFingerprint:
      return AuthMethodSwitchType::kSmartlockToFingerprint;
    case AuthMethod::kSmartlock:
    case AuthMethod::kChallengeResponse:
    case AuthMethod::kNothing:
      NOTREACHED();
      return base::nullopt;
  }
}

base::Optional<AuthMethodSwitchType> SwitchFromFingerprintTo(
    AuthMethod current) {
  DCHECK_NE(AuthMethod::kFingerprint, current);
  switch (current) {
    case AuthMethod::kPassword:
      return AuthMethodSwitchType::kFingerprintToPassword;
    case AuthMethod::kSmartlock:
      return AuthMethodSwitchType::kFingerprintToSmartlock;
    case AuthMethod::kPin:
      return AuthMethodSwitchType::kFingerprintToPin;
    case AuthMethod::kFingerprint:
    case AuthMethod::kChallengeResponse:
    case AuthMethod::kNothing:
      NOTREACHED();
      return base::nullopt;
  }
}

base::Optional<AuthMethodSwitchType> SwitchFromNothingTo(AuthMethod current) {
  DCHECK_NE(AuthMethod::kNothing, current);
  switch (current) {
    case AuthMethod::kPassword:
      return AuthMethodSwitchType::kNothingToPassword;
    case AuthMethod::kPin:
      return AuthMethodSwitchType::kNothingToPin;
    case AuthMethod::kSmartlock:
      return AuthMethodSwitchType::kNothingToSmartlock;
    case AuthMethod::kFingerprint:
      return AuthMethodSwitchType::kNothingToFingerprint;
    case AuthMethod::kChallengeResponse:
      return AuthMethodSwitchType::kNothingToChallengeResponse;
    case AuthMethod::kNothing:
      NOTREACHED();
      return base::nullopt;
  }
}

base::Optional<AuthMethodSwitchType> FindSwitchType(AuthMethod previous,
                                                    AuthMethod current) {
  DCHECK_NE(previous, current);
  switch (previous) {
    case AuthMethod::kPassword:
      return SwitchFromPasswordTo(current);
    case AuthMethod::kPin:
      return SwitchFromPinTo(current);
    case AuthMethod::kSmartlock:
      return SwitchFromSmartlockTo(current);
    case AuthMethod::kFingerprint:
      return SwitchFromFingerprintTo(current);
    case AuthMethod::kNothing:
      return SwitchFromNothingTo(current);
    case AuthMethod::kChallengeResponse:
      NOTREACHED();
      return base::nullopt;
  }
}

}  // namespace

LoginAuthRecorder::LoginAuthRecorder() {
  session_manager::SessionManager::Get()->AddObserver(this);
}

LoginAuthRecorder::~LoginAuthRecorder() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void LoginAuthRecorder::RecordAuthMethod(AuthMethod method) {
  DCHECK_NE(method, AuthMethod::kNothing);

  bool is_locked;
  switch (session_manager::SessionManager::Get()->session_state()) {
    case session_manager::SessionState::LOGIN_PRIMARY:
    case session_manager::SessionState::LOGIN_SECONDARY:
      is_locked = false;
      break;
    case session_manager::SessionState::LOCKED:
      is_locked = true;
      break;
    default:
      return;
  }
  const std::string prefix =
      is_locked ? "Ash.Login.Lock.AuthMethod." : "Ash.Login.Login.AuthMethod.";

  // Record usage of the authentication method in login/lock screen.
  const bool is_tablet_mode = ash::TabletMode::Get()->InTabletMode();
  std::string used_metric_name;
  if (is_tablet_mode) {
    base::UmaHistogramEnumeration(prefix + "Used.TabletMode", method);
  } else {
    base::UmaHistogramEnumeration(prefix + "Used.ClamShellMode", method);
  }

  if (last_auth_method_ != method) {
    // Record switching between unlock methods.
    const base::Optional<AuthMethodSwitchType> switch_type =
        FindSwitchType(last_auth_method_, method);
    if (switch_type) {
      base::UmaHistogramEnumeration(prefix + "Switched", *switch_type);
    }

    last_auth_method_ = method;
  }
}

void LoginAuthRecorder::RecordFingerprintUnlockResult(
    FingerprintUnlockResult result,
    const base::Optional<int>& num_attempts) {
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOCKED) {
    return;
  }
  base::UmaHistogramEnumeration("Fingerprint.Unlock.Result", result);

  const bool success = (result == FingerprintUnlockResult::kSuccess);
  base::UmaHistogramBoolean("Fingerprint.Unlock.AuthSuccessful", success);
  if (success) {
    DCHECK(num_attempts);
    base::UmaHistogramCounts100("Fingerprint.Unlock.AttemptsCountBeforeSuccess",
                                num_attempts.value());
  }
}

void LoginAuthRecorder::OnSessionStateChanged() {
  // Reset local state.
  last_auth_method_ = AuthMethod::kNothing;
}

}  // namespace chromeos
