// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_AUTH_RECORDER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_AUTH_RECORDER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace chromeos {

// A metrics recorder that records login authentication related metrics.
// This keeps track of the last authentication method we used and records
// switching between different authentication methods.
// This is tied to LoginScreenClient lifetime.
class LoginAuthRecorder : public session_manager::SessionManagerObserver {
 public:
  // Authentication method to unlock the screen. This enum is used to back an
  // UMA histogram and new values should be inserted immediately above
  // kMaxValue.
  enum class AuthMethod {
    kPassword = 0,
    kPin = 1,
    kSmartlock = 2,
    kFingerprint = 3,
    kChallengeResponse = 4,
    kNothing = 5,
    kMaxValue = kNothing,
  };

  // The type of switching between auth methods. This enum is used to back an
  // UMA histogram and new values should be inserted immediately above
  // kMaxValue.
  enum class AuthMethodSwitchType {
    kPasswordToPin = 0,
    kPasswordToSmartlock = 1,
    kPinToPassword = 2,
    kPinToSmartlock = 3,
    kSmartlockToPassword = 4,
    kSmartlockToPin = 5,
    kPasswordToFingerprint = 6,
    kPinToFingerprint = 7,
    kSmartlockToFingerprint = 8,
    kFingerprintToPassword = 9,
    kFingerprintToPin = 10,
    kFingerprintToSmartlock = 11,
    kPasswordToChallengeResponse = 12,
    kNothingToPassword = 13,
    kNothingToPin = 14,
    kNothingToSmartlock = 15,
    kNothingToFingerprint = 16,
    kNothingToChallengeResponse = 17,
    kMaxValue = kNothingToChallengeResponse,
  };

  // The result of fingerprint auth attempt on the lock screen. These values are
  // persisted to logs. Entries should not be renumbered and numeric values
  // should never be reused.
  enum class FingerprintUnlockResult {
    kSuccess = 0,
    kFingerprintUnavailable = 1,
    kAuthTemporarilyDisabled = 2,
    kMatchFailed = 3,
    kMatchNotForPrimaryUser = 4,
    kMaxValue = kMatchNotForPrimaryUser,
  };

  LoginAuthRecorder();
  ~LoginAuthRecorder() override;

  // Called when user attempts authentication using AuthMethod `type`.
  void RecordAuthMethod(AuthMethod type);

  // Called after a fingerprint unlock attempt to record the result.
  // `num_attempts`:  Only valid when auth success to record number of attempts.
  void RecordFingerprintUnlockResult(FingerprintUnlockResult result,
                                     const base::Optional<int>& num_attempts);

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

 private:
  AuthMethod last_auth_method_ = AuthMethod::kNothing;

  DISALLOW_COPY_AND_ASSIGN(LoginAuthRecorder);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_AUTH_RECORDER_H_
