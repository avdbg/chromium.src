// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_UTILS_H_

namespace base {
class TimeDelta;
}

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace chromeos {
namespace quick_unlock {

// Enumeration specifying the possible intervals before a strong auth
// (password) is required to use quick unlock. These values correspond to the
// policy items of QuickUnlockTimeout (policy ID 352) in policy_templates.json,
// and should be updated accordingly.
enum class PasswordConfirmationFrequency {
  SIX_HOURS = 0,
  TWELVE_HOURS = 1,
  TWO_DAYS = 2,
  WEEK = 3
};

// Enumeration specifying the possible fingerprint sensor locations.
enum class FingerprintLocation {
  TABLET_POWER_BUTTON = 0,
  KEYBOARD_BOTTOM_LEFT = 1,
  KEYBOARD_BOTTOM_RIGHT = 2,
  KEYBOARD_TOP_RIGHT = 3,
};

base::TimeDelta PasswordConfirmationFrequencyToTimeDelta(
    PasswordConfirmationFrequency frequency);

// Register quick unlock prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if PIN unlock is disabled by policy.
bool IsPinDisabledByPolicy(PrefService* pref_service);

// Returns true if the quick unlock feature flag is present.
bool IsPinEnabled(PrefService* pref_service);

// Returns true if the fingerprint is allowed for specified profile.
bool IsFingerprintEnabled(Profile* profile);

// Returns true if the fingerprint unlock is disabled by policy.
bool IsFingerprintDisabledByPolicy(const PrefService* pref_service);

// Returns fingerprint sensor location depending on the command line switch.
// Is used to display correct UI assets. Returns TABLET_POWER_BUTTON by default.
FingerprintLocation GetFingerprintLocation();

// Enable or Disable quick-unlock modes for testing
void EnabledForTesting(bool state);

// Returns true if EnableForTesting() was previously called.
bool IsEnabledForTesting();

// Forcibly disable PIN for testing purposes.
void DisablePinByPolicyForTesting(bool disable);

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_UTILS_H_
