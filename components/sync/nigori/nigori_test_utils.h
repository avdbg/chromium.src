// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_TEST_UTILS_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "components/sync/engine/nigori/nigori.h"

namespace sync_pb {

class BookmarkSpecifics;
class NigoriSpecifics;
class EntitySpecifics;

}  // namespace sync_pb

namespace syncer {

class Cryptographer;

struct KeyParamsForTesting {
  KeyDerivationParams derivation_params;
  std::string password;
};

// Creates KeyParamsForTesting, where |derivation_params| is Pbkdf2
// KeyDerivationParams and |password| is base64 encoded |raw_key|.
KeyParamsForTesting Pbkdf2KeyParamsForTesting(
    const std::vector<uint8_t>& raw_key);

// Builds NigoriSpecifics with following fields:
// 1. encryption_keybag contains all keys derived from |keybag_keys_params|
// and encrypted with a key derived from |keybag_decryptor_params|.
// 2. keystore_decryptor_token contains the key derived from
// |keybag_decryptor_params| and encrypted with a key derived from
// |keystore_key_params|.
// 3. passphrase_type is KEYSTORE_PASSHPRASE.
// 4. Other fields are default.
// |keybag_keys_params| must be non-empty.
sync_pb::NigoriSpecifics BuildKeystoreNigoriSpecifics(
    const std::vector<KeyParamsForTesting>& keybag_keys_params,
    const KeyParamsForTesting& keystore_decryptor_params,
    const KeyParamsForTesting& keystore_key_params);

// Builds NigoriSpecifics with following fields:
// 1. encryption_keybag contains keys derived from |trusted_vault_keys| and
// encrypted with key derived from last of them.
// 2. passphrase_type is TRUSTED_VAULT_PASSPHRASE.
// 3. keybag_is_frozen set to true.
sync_pb::NigoriSpecifics BuildTrustedVaultNigoriSpecifics(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys);

// Creates a NigoriSpecifics that describes encryption using a custom
// passphrase with the given |passphrase_key_params|. If |old_key_params| is
// presented, |encryption_keybag| will also contain keys derived from it.
sync_pb::NigoriSpecifics CreateCustomPassphraseNigori(
    const KeyParamsForTesting& passphrase_key_params,
    const base::Optional<KeyParamsForTesting>& old_key_params = base::nullopt);

// Given a |nigori| with CUSTOM_PASSPHRASE passphrase type, initializes the
// given |cryptographer| with the key described in it. Since the key inside the
// Nigori is encrypted (by design), the provided |passphrase| will be used to
// decrypt it. This function will fail the test (using ASSERT) if the Nigori is
// not a custom passphrase one, or if the key cannot be decrypted.
std::unique_ptr<Cryptographer> InitCustomPassphraseCryptographerFromNigori(
    const sync_pb::NigoriSpecifics& nigori,
    const std::string& passphrase);

// Returns an EntitySpecifics containing encrypted data corresponding to the
// provided BookmarkSpecifics and encrypted using the given |key_params|.
sync_pb::EntitySpecifics GetEncryptedBookmarkEntitySpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const KeyParamsForTesting& key_params);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_TEST_UTILS_H_
