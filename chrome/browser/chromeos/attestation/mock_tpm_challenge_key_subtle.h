// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_SUBTLE_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_SUBTLE_H_

#include <string>

#include "chrome/browser/chromeos/attestation/tpm_challenge_key_subtle.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace attestation {

class MockTpmChallengeKeySubtle : public TpmChallengeKeySubtle {
 public:
  MockTpmChallengeKeySubtle();
  MockTpmChallengeKeySubtle(const MockTpmChallengeKeySubtle&) = delete;
  MockTpmChallengeKeySubtle& operator=(const MockTpmChallengeKeySubtle&) =
      delete;
  ~MockTpmChallengeKeySubtle() override;

  MOCK_METHOD(void,
              StartPrepareKeyStep,
              (AttestationKeyType key_type,
               bool will_register_key,
               const std::string& key_name,
               Profile* profile,
               TpmChallengeKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              StartSignChallengeStep,
              (const std::string& challenge, TpmChallengeKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              StartRegisterKeyStep,
              (TpmChallengeKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              RestorePreparedKeyState,
              (AttestationKeyType key_type,
               bool will_register_key,
               const std::string& key_name,
               const std::string& public_key,
               Profile* profile),
              (override));
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_SUBTLE_H_
