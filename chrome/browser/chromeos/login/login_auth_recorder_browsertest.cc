// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_auth_recorder.h"

#include "ash/public/cpp/test/shell_test_api.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_test.h"

namespace chromeos {
namespace {

constexpr char kAuthMethodUsageAsTabletHistogramName[] =
    "Ash.Login.Lock.AuthMethod.Used.TabletMode";
constexpr char kAuthMethodUsageAsClamShellHistogramName[] =
    "Ash.Login.Lock.AuthMethod.Used.ClamShellMode";
constexpr char kAuthMethodSwitchHistogramName[] =
    "Ash.Login.Lock.AuthMethod.Switched";
constexpr char kFingerprintSuccessHistogramName[] =
    "Fingerprint.Unlock.AuthSuccessful";
constexpr char kFingerprintAttemptsCountBeforeSuccessHistogramName[] =
    "Fingerprint.Unlock.AttemptsCountBeforeSuccess";

}  // namespace

// Test fixture for the LoginAuthRecorder class.
class LoginAuthRecorderTest : public InProcessBrowserTest {
 public:
  LoginAuthRecorderTest() = default;
  ~LoginAuthRecorderTest() override = default;

 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void EnableTabletMode(bool enable) {
    ash::ShellTestApi().SetTabletModeEnabledForTest(enable);
  }

  LoginAuthRecorder* metrics_recorder() {
    return LoginScreenClient::Get()->auth_recorder();
  }

  void SetAuthMethod(LoginAuthRecorder::AuthMethod auth_method) {
    metrics_recorder()->RecordAuthMethod(auth_method);
  }

  void SetFingerprintUnlockResult(
      LoginAuthRecorder::FingerprintUnlockResult result) {
    metrics_recorder()->RecordFingerprintUnlockResult(
        result, result == LoginAuthRecorder::FingerprintUnlockResult::kSuccess
                    ? base::Optional<int>(1)
                    : base::nullopt);
  }

  void ExpectBucketCount(const std::string& name,
                         LoginAuthRecorder::AuthMethod method,
                         int count) {
    histogram_tester_->ExpectBucketCount(name, static_cast<int>(method), count);
  }

  void ExpectBucketCount(const std::string& name,
                         LoginAuthRecorder::AuthMethodSwitchType switch_type,
                         int count) {
    histogram_tester_->ExpectBucketCount(name, static_cast<int>(switch_type),
                                         count);
  }

  // Used to verify recorded data.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginAuthRecorderTest);
};

// Verifies that auth method usage is recorded correctly.
IN_PROC_BROWSER_TEST_F(LoginAuthRecorderTest, AuthMethodUsage) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  EnableTabletMode(false);
  SetAuthMethod(LoginAuthRecorder::AuthMethod::kPassword);
  ExpectBucketCount(kAuthMethodUsageAsClamShellHistogramName,
                    LoginAuthRecorder::AuthMethod::kPassword, 1);
  histogram_tester_->ExpectTotalCount(kAuthMethodUsageAsTabletHistogramName, 0);

  SetAuthMethod(LoginAuthRecorder::AuthMethod::kPin);
  ExpectBucketCount(kAuthMethodUsageAsClamShellHistogramName,
                    LoginAuthRecorder::AuthMethod::kPin, 1);
  histogram_tester_->ExpectTotalCount(kAuthMethodUsageAsTabletHistogramName, 0);

  SetAuthMethod(LoginAuthRecorder::AuthMethod::kSmartlock);
  ExpectBucketCount(kAuthMethodUsageAsClamShellHistogramName,
                    LoginAuthRecorder::AuthMethod::kSmartlock, 1);
  histogram_tester_->ExpectTotalCount(kAuthMethodUsageAsTabletHistogramName, 0);

  SetAuthMethod(LoginAuthRecorder::AuthMethod::kFingerprint);
  ExpectBucketCount(kAuthMethodUsageAsClamShellHistogramName,
                    LoginAuthRecorder::AuthMethod::kFingerprint, 1);
  histogram_tester_->ExpectTotalCount(kAuthMethodUsageAsTabletHistogramName, 0);
  histogram_tester_->ExpectTotalCount(kAuthMethodUsageAsClamShellHistogramName,
                                      4);

  EnableTabletMode(true);
  SetAuthMethod(LoginAuthRecorder::AuthMethod::kPassword);
  ExpectBucketCount(kAuthMethodUsageAsTabletHistogramName,
                    LoginAuthRecorder::AuthMethod::kPassword, 1);
  histogram_tester_->ExpectTotalCount(kAuthMethodUsageAsClamShellHistogramName,
                                      4);

  SetAuthMethod(LoginAuthRecorder::AuthMethod::kPin);
  ExpectBucketCount(kAuthMethodUsageAsTabletHistogramName,
                    LoginAuthRecorder::AuthMethod::kPin, 1);
  histogram_tester_->ExpectTotalCount(kAuthMethodUsageAsClamShellHistogramName,
                                      4);

  SetAuthMethod(LoginAuthRecorder::AuthMethod::kSmartlock);
  ExpectBucketCount(kAuthMethodUsageAsTabletHistogramName,
                    LoginAuthRecorder::AuthMethod::kSmartlock, 1);
  histogram_tester_->ExpectTotalCount(kAuthMethodUsageAsClamShellHistogramName,
                                      4);

  SetAuthMethod(LoginAuthRecorder::AuthMethod::kFingerprint);
  ExpectBucketCount(kAuthMethodUsageAsTabletHistogramName,
                    LoginAuthRecorder::AuthMethod::kFingerprint, 1);
  histogram_tester_->ExpectTotalCount(kAuthMethodUsageAsClamShellHistogramName,
                                      4);
  histogram_tester_->ExpectTotalCount(kAuthMethodUsageAsTabletHistogramName, 4);
}

// Verifies that auth method switching is recorded correctly.
IN_PROC_BROWSER_TEST_F(LoginAuthRecorderTest, AuthMethodSwitch) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  // Switch from nothing to password.
  SetAuthMethod(LoginAuthRecorder::AuthMethod::kPassword);
  ExpectBucketCount(kAuthMethodSwitchHistogramName,
                    LoginAuthRecorder::AuthMethodSwitchType::kNothingToPassword,
                    1);

  // Switch from password to pin.
  SetAuthMethod(LoginAuthRecorder::AuthMethod::kPin);
  ExpectBucketCount(kAuthMethodSwitchHistogramName,
                    LoginAuthRecorder::AuthMethodSwitchType::kPasswordToPin, 1);

  // Switch from pin to smart lock.
  SetAuthMethod(LoginAuthRecorder::AuthMethod::kSmartlock);
  ExpectBucketCount(kAuthMethodSwitchHistogramName,
                    LoginAuthRecorder::AuthMethodSwitchType::kPinToSmartlock,
                    1);

  // Switch from smart lock to fingerprint.
  SetAuthMethod(LoginAuthRecorder::AuthMethod::kFingerprint);
  ExpectBucketCount(
      kAuthMethodSwitchHistogramName,
      LoginAuthRecorder::AuthMethodSwitchType::kSmartlockToFingerprint, 1);

  // Switch from fingerprint to password.
  SetAuthMethod(LoginAuthRecorder::AuthMethod::kPassword);
  ExpectBucketCount(
      kAuthMethodSwitchHistogramName,
      LoginAuthRecorder::AuthMethodSwitchType::kFingerprintToPassword, 1);

  // Switch from password to fingerprint.
  SetAuthMethod(LoginAuthRecorder::AuthMethod::kFingerprint);
  ExpectBucketCount(
      kAuthMethodSwitchHistogramName,
      LoginAuthRecorder::AuthMethodSwitchType::kPasswordToFingerprint, 1);

  // Switch from fingerprint to smart lock.
  SetAuthMethod(LoginAuthRecorder::AuthMethod::kSmartlock);
  ExpectBucketCount(
      kAuthMethodSwitchHistogramName,
      LoginAuthRecorder::AuthMethodSwitchType::kFingerprintToSmartlock, 1);

  // Switch from smart lock to pin.
  SetAuthMethod(LoginAuthRecorder::AuthMethod::kPin);
  ExpectBucketCount(kAuthMethodSwitchHistogramName,
                    LoginAuthRecorder::AuthMethodSwitchType::kSmartlockToPin,
                    1);

  // Switch from pin to password.
  SetAuthMethod(LoginAuthRecorder::AuthMethod::kPassword);
  ExpectBucketCount(kAuthMethodSwitchHistogramName,
                    LoginAuthRecorder::AuthMethodSwitchType::kPinToPassword, 1);
}

// Verifies that fingerprint auth success is recorded correctly.
IN_PROC_BROWSER_TEST_F(LoginAuthRecorderTest, FingerprintAuthSuccess) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
  SetFingerprintUnlockResult(
      LoginAuthRecorder::FingerprintUnlockResult::kSuccess);
  histogram_tester_->ExpectBucketCount(kFingerprintSuccessHistogramName,
                                       1 /*success*/, 1);
  histogram_tester_->ExpectBucketCount(
      "Fingerprint.Unlock.Result",
      static_cast<int>(LoginAuthRecorder::FingerprintUnlockResult::kSuccess),
      1);
  histogram_tester_->ExpectTotalCount(
      kFingerprintAttemptsCountBeforeSuccessHistogramName, 1);

  SetFingerprintUnlockResult(
      LoginAuthRecorder::FingerprintUnlockResult::kMatchFailed);
  histogram_tester_->ExpectBucketCount(kFingerprintSuccessHistogramName,
                                       0 /*success*/, 1);
  histogram_tester_->ExpectBucketCount(
      "Fingerprint.Unlock.Result",
      static_cast<int>(
          LoginAuthRecorder::FingerprintUnlockResult::kMatchFailed),
      1);
  histogram_tester_->ExpectTotalCount(
      kFingerprintAttemptsCountBeforeSuccessHistogramName, 1);
}

}  // namespace chromeos
