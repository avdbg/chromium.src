// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_IMPL_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/connection_status.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/engine/sync_credentials.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/invalidations/invalidations_listener.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/sync_protocol_error.h"

namespace invalidation {
class InvalidationService;
}  // namespace invalidation

namespace syncer {

class ActiveDevicesProvider;
class DataTypeDebugInfoListener;
class JsBackend;
class ModelTypeConnector;
class ProtocolEvent;
class SyncEngineBackend;
class SyncInvalidationsService;
class SyncTransportDataPrefs;

// The only real implementation of the SyncEngine. See that interface's
// definition for documentation of public methods.
class SyncEngineImpl : public SyncEngine,
                       public invalidation::InvalidationHandler,
                       public InvalidationsListener {
 public:
  using Status = SyncStatus;

  SyncEngineImpl(const std::string& name,
                 invalidation::InvalidationService* invalidator,
                 SyncInvalidationsService* sync_invalidations_service,
                 std::unique_ptr<ActiveDevicesProvider> active_devices_provider,
                 std::unique_ptr<SyncTransportDataPrefs> prefs,
                 const base::FilePath& sync_data_folder,
                 scoped_refptr<base::SequencedTaskRunner> sync_task_runner,
                 const base::RepeatingClosure& sync_transport_data_cleared_cb);
  ~SyncEngineImpl() override;

  // SyncEngine implementation.
  void Initialize(InitParams params) override;
  bool IsInitialized() const override;
  void TriggerRefresh(const ModelTypeSet& types) override;
  void UpdateCredentials(const SyncCredentials& credentials) override;
  void InvalidateCredentials() override;
  std::string GetCacheGuid() const override;
  std::string GetBirthday() const override;
  base::Time GetLastSyncedTimeForDebugging() const override;
  void StartConfiguration() override;
  void StartSyncingWithServer() override;
  void SetEncryptionPassphrase(const std::string& passphrase) override;
  void SetDecryptionPassphrase(const std::string& passphrase) override;
  void SetEncryptionBootstrapToken(const std::string& token) override;
  void SetKeystoreEncryptionBootstrapToken(const std::string& token) override;
  void AddTrustedVaultDecryptionKeys(
      const std::vector<std::vector<uint8_t>>& keys,
      base::OnceClosure done_cb) override;
  void StopSyncingForShutdown() override;
  void Shutdown(ShutdownReason reason) override;
  void ConfigureDataTypes(ConfigureParams params) override;
  void ActivateDataType(ModelType type,
                        std::unique_ptr<DataTypeActivationResponse>) override;
  void DeactivateDataType(ModelType type) override;
  void ActivateProxyDataType(ModelType type) override;
  void DeactivateProxyDataType(ModelType type) override;
  const Status& GetDetailedStatus() const override;
  void HasUnsyncedItemsForTest(
      base::OnceCallback<void(bool)> cb) const override;
  void RequestBufferedProtocolEventsAndEnableForwarding() override;
  void DisableProtocolEventForwarding() override;
  void OnCookieJarChanged(bool account_mismatch,
                          base::OnceClosure callback) override;
  void SetInvalidationsForSessionsEnabled(bool enabled) override;
  void GetNigoriNodeForDebugging(AllNodesCallback callback) override;

  // InvalidationHandler implementation.
  void OnInvalidatorStateChange(invalidation::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const invalidation::TopicInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;
  void OnInvalidatorClientIdChange(const std::string& client_id) override;

  // InvalidationsListener implementation.
  void OnInvalidationReceived(const std::string& payload) override;

  static std::string GenerateCacheGUIDForTest();

 private:
  friend class SyncEngineBackend;

  // Called when the syncer has finished performing a configuration.
  void FinishConfigureDataTypesOnFrontendLoop(
      const ModelTypeSet enabled_types,
      const ModelTypeSet succeeded_configuration_types,
      const ModelTypeSet failed_configuration_types,
      base::OnceCallback<void(ModelTypeSet, ModelTypeSet)> ready_task);

  // Reports backend initialization success.  Includes some objects from sync
  // manager initialization to be passed back to the UI thread.
  //
  // |model_type_connector| is our ModelTypeConnector, which is owned because in
  // production it is a proxy object to the real ModelTypeConnector.
  virtual void HandleInitializationSuccessOnFrontendLoop(
      ModelTypeSet initial_types,
      const WeakHandle<JsBackend> js_backend,
      const WeakHandle<DataTypeDebugInfoListener> debug_info_listener,
      std::unique_ptr<ModelTypeConnector> model_type_connector,
      const std::string& birthday,
      const std::string& bag_of_chips);

  // Forwards a ProtocolEvent to the host. Will not be called unless a call to
  // SetForwardProtocolEvents() explicitly requested that we start forwarding
  // these events.
  void HandleProtocolEventOnFrontendLoop(std::unique_ptr<ProtocolEvent> event);

  // Overwrites the kSyncInvalidationVersions preference with the most recent
  // set of invalidation versions for each type.
  void UpdateInvalidationVersions(
      const std::map<ModelType, int64_t>& invalidation_versions);

  void HandleSyncStatusChanged(const SyncStatus& status);

  // Handles backend initialization failure.
  void HandleInitializationFailureOnFrontendLoop();

  // Called from SyncEngineBackend::OnSyncCycleCompleted to handle updating
  // frontend thread components.
  void HandleSyncCycleCompletedOnFrontendLoop(
      const SyncCycleSnapshot& snapshot);

  // Let the front end handle the actionable error event.
  void HandleActionableErrorEventOnFrontendLoop(
      const SyncProtocolError& sync_error);

  // Handle a migration request.
  void HandleMigrationRequestedOnFrontendLoop(const ModelTypeSet types);

  // Dispatched to from OnConnectionStatusChange to handle updating
  // frontend UI components.
  void HandleConnectionStatusChangeOnFrontendLoop(ConnectionStatus status);

  void OnCookieJarChangedDoneOnFrontendLoop(base::OnceClosure callback);

  void SendInterestedTopicsToInvalidator();

  // Called on each device infos change and might be called more than once with
  // the same |active_devices|.
  void OnActiveDevicesChanged();

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  // Helper function that clears SyncTransportDataPrefs and also notifies
  // upper layers via |sync_transport_data_cleared_cb_|.
  void ClearLocalTransportDataAndNotify();

  // The task runner where all the sync engine operations happen.
  scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  // Name used for debugging (set from profile_->GetDebugName()).
  const std::string name_;

  const std::unique_ptr<SyncTransportDataPrefs> prefs_;

  const base::RepeatingClosure sync_transport_data_cleared_cb_;

  // Our backend, which communicates directly to the syncapi. Use refptr instead
  // of WeakHandle because |backend_| is created on UI loop but released on
  // sync loop.
  scoped_refptr<SyncEngineBackend> backend_;

  // A handle referencing the main interface for sync data types. This
  // object is owned because in production code it is a proxy object.
  std::unique_ptr<ModelTypeConnector> model_type_connector_;

  bool initialized_ = false;

  // The host which we serve (and are owned by). Set in Initialize() and nulled
  // out in StopSyncingForShutdown().
  SyncEngineHost* host_ = nullptr;

  invalidation::InvalidationService* invalidator_ = nullptr;
  bool invalidation_handler_registered_ = false;

  // Sync invalidation service, it may be nullptr if sync invalidations are
  // disabled or not supported. It doesn't need to have the same as
  // |invalidation_handler_registered_| flag as the service doesn't have topics
  // to unsibscribe.
  SyncInvalidationsService* sync_invalidations_service_ = nullptr;

  ModelTypeSet last_enabled_types_;
  bool sessions_invalidation_enabled_;

  SyncStatus cached_status_;

  std::unique_ptr<ActiveDevicesProvider> active_devices_provider_;

  // Checks that we're on the same thread this was constructed on (UI thread).
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SyncEngineImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncEngineImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_IMPL_H_
