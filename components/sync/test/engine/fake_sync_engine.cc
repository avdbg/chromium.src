// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/fake_sync_engine.h"

#include <utility>

#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace syncer {

constexpr char FakeSyncEngine::kTestBirthday[];

FakeSyncEngine::FakeSyncEngine(
    bool allow_init_completion,
    bool is_first_time_sync_configure,
    const base::RepeatingClosure& sync_transport_data_cleared_cb)
    : allow_init_completion_(allow_init_completion),
      is_first_time_sync_configure_(is_first_time_sync_configure),
      sync_transport_data_cleared_cb_(sync_transport_data_cleared_cb) {}

FakeSyncEngine::~FakeSyncEngine() = default;

void FakeSyncEngine::TriggerInitializationCompletion(bool success) {
  DCHECK(host_) << "Initialize() not called.";
  DCHECK(!initialized_);

  initialized_ = success;

  host_->OnEngineInitialized(ModelTypeSet(), WeakHandle<JsBackend>(),
                             WeakHandle<DataTypeDebugInfoListener>(), success,
                             is_first_time_sync_configure_);
}

void FakeSyncEngine::Initialize(InitParams params) {
  DCHECK(params.host);

  authenticated_account_id_ = params.authenticated_account_info.account_id;
  host_ = params.host;

  if (allow_init_completion_) {
    TriggerInitializationCompletion(/*success=*/true);
  }
}

bool FakeSyncEngine::IsInitialized() const {
  return initialized_;
}

void FakeSyncEngine::TriggerRefresh(const ModelTypeSet& types) {}

void FakeSyncEngine::UpdateCredentials(const SyncCredentials& credentials) {}

void FakeSyncEngine::InvalidateCredentials() {}

std::string FakeSyncEngine::GetCacheGuid() const {
  return "fake_engine_cache_guid";
}

std::string FakeSyncEngine::GetBirthday() const {
  return kTestBirthday;
}

base::Time FakeSyncEngine::GetLastSyncedTimeForDebugging() const {
  return base::Time();
}

void FakeSyncEngine::StartConfiguration() {}

void FakeSyncEngine::StartSyncingWithServer() {}

void FakeSyncEngine::SetEncryptionPassphrase(const std::string& passphrase) {}

void FakeSyncEngine::SetDecryptionPassphrase(const std::string& passphrase) {}

void FakeSyncEngine::SetEncryptionBootstrapToken(const std::string& token) {}

void FakeSyncEngine::SetKeystoreEncryptionBootstrapToken(
    const std::string& token) {}

void FakeSyncEngine::AddTrustedVaultDecryptionKeys(
    const std::vector<std::vector<uint8_t>>& keys,
    base::OnceClosure done_cb) {
  std::move(done_cb).Run();
}

void FakeSyncEngine::StopSyncingForShutdown() {}

void FakeSyncEngine::Shutdown(ShutdownReason reason) {
  if (reason == DISABLE_SYNC) {
    sync_transport_data_cleared_cb_.Run();
  }
}

void FakeSyncEngine::ConfigureDataTypes(ConfigureParams params) {
  std::move(params.ready_task)
      .Run(/*succeeded_configuration_types=*/params.enabled_types,
           /*failed_configuration_types=*/ModelTypeSet());
}

void FakeSyncEngine::ActivateDataType(
    ModelType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {}

void FakeSyncEngine::DeactivateDataType(ModelType type) {}

void FakeSyncEngine::ActivateProxyDataType(ModelType type) {}

void FakeSyncEngine::DeactivateProxyDataType(ModelType type) {}

const SyncStatus& FakeSyncEngine::GetDetailedStatus() const {
  return default_sync_status_;
}

void FakeSyncEngine::HasUnsyncedItemsForTest(
    base::OnceCallback<void(bool)> cb) const {}

void FakeSyncEngine::RequestBufferedProtocolEventsAndEnableForwarding() {}

void FakeSyncEngine::DisableProtocolEventForwarding() {}

void FakeSyncEngine::OnCookieJarChanged(bool account_mismatch,
                                        base::OnceClosure callback) {
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

void FakeSyncEngine::SetInvalidationsForSessionsEnabled(bool enabled) {}

void FakeSyncEngine::GetNigoriNodeForDebugging(AllNodesCallback callback) {}

}  // namespace syncer
