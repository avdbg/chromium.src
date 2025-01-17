// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/nearby/nearby_process_manager_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/nearby/nearby_connections_dependencies_provider.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/sharing/webrtc/sharing_mojo_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {
namespace nearby {
namespace {

NearbyProcessManagerImpl::Factory* g_test_factory = nullptr;

constexpr base::TimeDelta kProcessCleanupTimeout =
    base::TimeDelta::FromSeconds(5);

void OnSharingShutDownComplete(
    mojo::Remote<sharing::mojom::Sharing> sharing,
    mojo::SharedRemote<location::nearby::connections::mojom::NearbyConnections>
        connections,
    mojo::SharedRemote<sharing::mojom::NearbySharingDecoder> decoder) {
  NS_LOG(INFO) << "Asynchronous process shutdown complete.";
  // Note: Let the parameters go out of scope, which will disconnect them.
}

}  // namespace

// static
std::unique_ptr<NearbyProcessManager> NearbyProcessManagerImpl::Factory::Create(
    NearbyConnectionsDependenciesProvider*
        nearby_connections_dependencies_provider,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (g_test_factory) {
    return g_test_factory->BuildInstance(
        nearby_connections_dependencies_provider, std::move(timer));
  }

  return base::WrapUnique(new NearbyProcessManagerImpl(
      nearby_connections_dependencies_provider, std::move(timer),
      base::BindRepeating(&sharing::LaunchSharing)));
}

// static
void NearbyProcessManagerImpl::Factory::SetFactoryForTesting(Factory* factory) {
  g_test_factory = factory;
}

NearbyProcessManagerImpl::NearbyReferenceImpl::NearbyReferenceImpl(
    const mojo::SharedRemote<
        location::nearby::connections::mojom::NearbyConnections>& connections,
    const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>& decoder,
    base::OnceClosure destructor_callback)
    : connections_(connections),
      decoder_(decoder),
      destructor_callback_(std::move(destructor_callback)) {}

NearbyProcessManagerImpl::NearbyReferenceImpl::~NearbyReferenceImpl() {
  // Reset the SharedRemotes before the destructor callback is run to ensure
  // that all connections to the utility process are destroyed before we attempt
  // to tear the process down.
  connections_.reset();
  decoder_.reset();

  std::move(destructor_callback_).Run();
}

const mojo::SharedRemote<
    location::nearby::connections::mojom::NearbyConnections>&
NearbyProcessManagerImpl::NearbyReferenceImpl::GetNearbyConnections() const {
  return connections_;
}

const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>&
NearbyProcessManagerImpl::NearbyReferenceImpl::GetNearbySharingDecoder() const {
  return decoder_;
}

NearbyProcessManagerImpl::NearbyProcessManagerImpl(
    NearbyConnectionsDependenciesProvider*
        nearby_connections_dependencies_provider,
    std::unique_ptr<base::OneShotTimer> timer,
    const base::RepeatingCallback<
        mojo::PendingRemote<sharing::mojom::Sharing>()>& sharing_binder)
    : nearby_connections_dependencies_provider_(
          nearby_connections_dependencies_provider),
      shutdown_debounce_timer_(std::move(timer)),
      sharing_binder_(sharing_binder) {}

NearbyProcessManagerImpl::~NearbyProcessManagerImpl() = default;

std::unique_ptr<NearbyProcessManager::NearbyProcessReference>
NearbyProcessManagerImpl::GetNearbyProcessReference(
    NearbyProcessStoppedCallback on_process_stopped_callback) {
  if (shut_down_)
    return nullptr;

  if (!sharing_ || !connections_ || !decoder_) {
    if (!AttemptToBindToUtilityProcess()) {
      NS_LOG(WARNING) << "Could not connect to Nearby utility process; this "
                      << "likely means that the attempt was during shutdown.";
      return nullptr;
    }
  }

  NS_LOG(VERBOSE) << "New Nearby process reference requested.";
  auto reference_id = base::UnguessableToken::Create();
  id_to_process_stopped_callback_map_.emplace(
      reference_id, std::move(on_process_stopped_callback));

  // If we were waiting to shut down the process but a new client was added,
  // stop the timer since the process needs to be kept alive for this new
  // client.
  shutdown_debounce_timer_->Stop();

  return std::make_unique<NearbyReferenceImpl>(
      connections_, decoder_,
      base::BindOnce(&NearbyProcessManagerImpl::OnReferenceDeleted,
                     weak_ptr_factory_.GetWeakPtr(), reference_id));
}

void NearbyProcessManagerImpl::Shutdown() {
  if (shut_down_)
    return;

  shut_down_ = true;

  NearbyProcessShutdownReason shutdown_reason =
      NearbyProcessShutdownReason::kNormal;

  ShutDownProcess(shutdown_reason);
  NotifyProcessStopped(shutdown_reason);
}

bool NearbyProcessManagerImpl::AttemptToBindToUtilityProcess() {
  DCHECK(!sharing_ && !connections_ && !decoder_);

  location::nearby::connections::mojom::NearbyConnectionsDependenciesPtr deps =
      nearby_connections_dependencies_provider_->GetDependencies();

  if (!deps)
    return false;

  NS_LOG(INFO) << "Starting up Nearby utility process.";

  // Bind to the Sharing interface, which launches the process.
  sharing_.Bind(sharing_binder_.Run());
  sharing_.set_disconnect_handler(
      base::BindOnce(&NearbyProcessManagerImpl::OnSharingProcessCrash,
                     weak_ptr_factory_.GetWeakPtr()));

  // Remotes for NearbyConnections and NearbySharingDecoder are bound on the
  // calling sequence by providing a null |bind_task_runner|.
  mojo::PendingRemote<location::nearby::connections::mojom::NearbyConnections>
      connections;
  mojo::PendingReceiver<location::nearby::connections::mojom::NearbyConnections>
      connections_receiver = connections.InitWithNewPipeAndPassReceiver();
  connections_.Bind(std::move(connections), /*bind_task_runner=*/nullptr);
  connections_.set_disconnect_handler(
      base::BindOnce(&NearbyProcessManagerImpl::OnMojoPipeDisconnect,
                     weak_ptr_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  mojo::PendingRemote<sharing::mojom::NearbySharingDecoder> decoder;
  mojo::PendingReceiver<sharing::mojom::NearbySharingDecoder> decoder_receiver =
      decoder.InitWithNewPipeAndPassReceiver();
  decoder_.Bind(std::move(decoder), /*bind_task_runner=*/nullptr);
  decoder_.set_disconnect_handler(
      base::BindOnce(&NearbyProcessManagerImpl::OnMojoPipeDisconnect,
                     weak_ptr_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  // Pass these references to Connect() to start up the process.
  sharing_->Connect(std::move(deps), std::move(connections_receiver),
                    std::move(decoder_receiver));

  return true;
}

void NearbyProcessManagerImpl::OnSharingProcessCrash() {
  NS_LOG(ERROR) << "The utility process has crashed.";

  NearbyProcessShutdownReason shutdown_reason =
      NearbyProcessShutdownReason::kCrash;

  ShutDownProcess(shutdown_reason);
  NotifyProcessStopped(shutdown_reason);
}

void NearbyProcessManagerImpl::OnMojoPipeDisconnect() {
  NS_LOG(ERROR) << "A utility process Mojo pipe has disconnected.";

  NearbyProcessShutdownReason shutdown_reason =
      NearbyProcessShutdownReason::kMojoPipeDisconnection;

  ShutDownProcess(shutdown_reason);
  NotifyProcessStopped(shutdown_reason);
}

void NearbyProcessManagerImpl::OnReferenceDeleted(
    const base::UnguessableToken& reference_id) {
  auto it = id_to_process_stopped_callback_map_.find(reference_id);
  DCHECK(it != id_to_process_stopped_callback_map_.end());

  // Do not call the callback because its owner has already explicitly deleted
  // its reference.
  id_to_process_stopped_callback_map_.erase(it);

  // If there are still active references, the process should be kept alive, so
  // return early.
  if (!id_to_process_stopped_callback_map_.empty())
    return;

  NS_LOG(VERBOSE) << "All Nearby references have been released; will shut down "
                  << "process in " << kProcessCleanupTimeout << " unless a new "
                  << "reference is obtained.";

  // Stop the process, but wait |kProcessCleanupTimeout| before doing so. Adding
  // this additional timeout works around issues during Nearby shutdown
  // (see https://crbug.com/1152609).
  // TODO(https://crbug.com/1152892): Remove this timeout.
  shutdown_debounce_timer_->Start(
      FROM_HERE, kProcessCleanupTimeout,
      base::BindOnce(&NearbyProcessManagerImpl::ShutDownProcess,
                     weak_ptr_factory_.GetWeakPtr(),
                     NearbyProcessShutdownReason::kNormal));
}

void NearbyProcessManagerImpl::ShutDownProcess(
    NearbyProcessShutdownReason shutdown_reason) {
  if (!sharing_ && !connections_ && !decoder_)
    return;

  // Ensure that we don't try to stop the process again.
  shutdown_debounce_timer_->Stop();

  NS_LOG(INFO) << "Shutting down Nearby utility process.";

  base::UmaHistogramEnumeration(
      "Nearby.Connections.UtilityProcessShutdownReason", shutdown_reason);

  // Prevent the Remotes' disconnect handler callbacks from firing.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (sharing_) {
    // Start the asynchronous shutdown flow, and pass ownership of the existing
    // Remote and SharedRemotes to the callback. These instance fields will stay
    // alive until ShutDown() is complete, at which time they will go out of
    // scope and become disconnected in OnSharingShutDownComplete().
    sharing::mojom::Sharing* sharing = sharing_.get();
    sharing->ShutDown(
        base::BindOnce(&OnSharingShutDownComplete, std::move(sharing_),
                       std::move(connections_), std::move(decoder_)));
  }

  sharing_.reset();
  connections_.reset();
  decoder_.reset();
}

void NearbyProcessManagerImpl::NotifyProcessStopped(
    NearbyProcessShutdownReason shutdown_reason) {
  // We are notifying clients that references are no longer active, so
  // invalidate WeakPtrs so that OnReferenceDeleted() is not invoked when
  // clients respond to the callback by releasing their references.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Move the map to a local variable to ensure that the instance field is
  // empty before any callbacks are made.
  auto old_map = std::move(id_to_process_stopped_callback_map_);
  id_to_process_stopped_callback_map_.clear();

  // Invoke the "process stopped" callback for each client.
  for (auto& it : old_map)
    std::move(it.second).Run(shutdown_reason);
}

}  // namespace nearby
}  // namespace chromeos
