// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_connections_manager_impl.h"

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/constants.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "crypto/random.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_change_notifier.h"

namespace {

const char kServiceId[] = "NearbySharing";
const char kFastAdvertisementServiceUuid[] =
    "0000fef3-0000-1000-8000-00805f9b34fb";
const location::nearby::connections::mojom::Strategy kStrategy =
    location::nearby::connections::mojom::Strategy::kP2pPointToPoint;

bool ShouldEnableWebRtc(DataUsage data_usage, PowerLevel power_level) {
  if (!base::FeatureList::IsEnabled(features::kNearbySharingWebRtc))
    return false;

  // We won't use internet if the user requested we don't.
  if (data_usage == DataUsage::kOffline)
    return false;

  // We won't use internet in a low power mode.
  if (power_level == PowerLevel::kLowPower)
    return false;

  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();

  // Verify that this network has an internet connection.
  if (connection_type == net::NetworkChangeNotifier::CONNECTION_NONE) {
    NS_LOG(VERBOSE) << __func__
                    << ": Do not use WebRTC; no internet connection.";
    return false;
  }

  // If the user wants to limit WebRTC, then don't use it on metered networks.
  if (data_usage == DataUsage::kWifiOnly &&
      net::NetworkChangeNotifier::GetConnectionCost() ==
          net::NetworkChangeNotifier::CONNECTION_COST_METERED) {
    NS_LOG(VERBOSE) << __func__ << ": Do not use WebRTC with " << data_usage
                    << " and a metered connection.";
    return false;
  }

  // We're online, the user hasn't disabled WebRTC, let's use it!
  return true;
}

std::string MediumSelectionToString(
    const location::nearby::connections::mojom::MediumSelection& mediums) {
  std::stringstream ss;
  ss << "{";
  if (mediums.bluetooth)
    ss << "bluetooth ";
  if (mediums.ble)
    ss << "ble ";
  if (mediums.web_rtc)
    ss << "webrtc ";
  if (mediums.wifi_lan)
    ss << "wifilan ";
  ss << "}";

  return ss.str();
}

}  // namespace

NearbyConnectionsManagerImpl::NearbyConnectionsManagerImpl(
    chromeos::nearby::NearbyProcessManager* process_manager)
    : process_manager_(process_manager) {
  DCHECK(process_manager_);
}

NearbyConnectionsManagerImpl::~NearbyConnectionsManagerImpl() {
  ClearIncomingPayloads();
}

void NearbyConnectionsManagerImpl::Shutdown() {
  Reset();
}

void NearbyConnectionsManagerImpl::StartAdvertising(
    std::vector<uint8_t> endpoint_info,
    IncomingConnectionListener* listener,
    PowerLevel power_level,
    DataUsage data_usage,
    ConnectionsCallback callback) {
  DCHECK(listener);
  DCHECK(!incoming_connection_listener_);

  location::nearby::connections::mojom::NearbyConnections* nearby_connections =
      GetNearbyConnections();
  if (!nearby_connections) {
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  bool is_high_power = power_level == PowerLevel::kHighPower;
  bool use_ble = !is_high_power;
  auto allowed_mediums = MediumSelection::New(
      /*bluetooth=*/is_high_power, /*ble=*/use_ble,
      ShouldEnableWebRtc(data_usage, power_level),
      /*wifi_lan=*/is_high_power && kIsWifiLanSupported);
  NS_LOG(VERBOSE) << __func__ << ": "
                  << "is_high_power=" << (is_high_power ? "yes" : "no")
                  << ", data_usage=" << data_usage << ", allowed_mediums="
                  << MediumSelectionToString(*allowed_mediums);

  mojo::PendingRemote<ConnectionLifecycleListener> lifecycle_listener;
  connection_lifecycle_listeners_.Add(
      this, lifecycle_listener.InitWithNewPipeAndPassReceiver());

  // Only auto-upgrade bandwidth if advertising at high-visibility.
  // This acts as a privacy safeguard when advertising in the background.
  // Bandwidth upgrades may expose stable identifiers, and so they're
  // only safe to expose after we've verified the sender's identity.
  // Once we have verified their identity, we will manually trigger
  // a bandwidth upgrade. This isn't a concern in the foreground
  // because high-visibility already leaks the device name.
  bool auto_upgrade_bandwidth = is_high_power;

  incoming_connection_listener_ = listener;
  nearby_connections->StartAdvertising(
      kServiceId, endpoint_info,
      AdvertisingOptions::New(
          kStrategy, std::move(allowed_mediums), auto_upgrade_bandwidth,
          /*enforce_topology_constraints=*/true,
          /*enable_bluetooth_listening=*/use_ble,
          /*fast_advertisement_service_uuid=*/
          device::BluetoothUUID(kFastAdvertisementServiceUuid)),
      std::move(lifecycle_listener), std::move(callback));
}

void NearbyConnectionsManagerImpl::StopAdvertising() {
  incoming_connection_listener_ = nullptr;

  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_)
    return;

  process_reference_->GetNearbyConnections()->StopAdvertising(
      kServiceId, base::BindOnce([](ConnectionsStatus status) {
        NS_LOG(VERBOSE) << __func__
                        << ": Stop advertising attempted over Nearby "
                           "Connections with result: "
                        << ConnectionsStatusToString(status);
      }));
}

void NearbyConnectionsManagerImpl::StartDiscovery(
    DiscoveryListener* listener,
    DataUsage data_usage,
    ConnectionsCallback callback) {
  DCHECK(listener);
  DCHECK(!discovery_listener_);

  location::nearby::connections::mojom::NearbyConnections* nearby_connections =
      GetNearbyConnections();
  if (!nearby_connections) {
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  auto allowed_mediums = MediumSelection::New(
      /*bluetooth=*/true,
      /*ble=*/true,
      /*webrtc=*/ShouldEnableWebRtc(data_usage, PowerLevel::kHighPower),
      /*wifi_lan=*/kIsWifiLanSupported);
  NS_LOG(VERBOSE) << __func__ << ": "
                  << "data_usage=" << data_usage << ", allowed_mediums="
                  << MediumSelectionToString(*allowed_mediums);

  discovery_listener_ = listener;
  nearby_connections->StartDiscovery(
      kServiceId,
      DiscoveryOptions::New(
          kStrategy, std::move(allowed_mediums),
          device::BluetoothUUID(kFastAdvertisementServiceUuid),
          /*is_out_of_band_connection=*/false),
      endpoint_discovery_listener_.BindNewPipeAndPassRemote(),
      std::move(callback));
}

void NearbyConnectionsManagerImpl::StopDiscovery() {
  discovered_endpoints_.clear();
  discovery_listener_ = nullptr;
  endpoint_discovery_listener_.reset();

  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_)
    return;

  process_reference_->GetNearbyConnections()->StopDiscovery(
      kServiceId, base::BindOnce([](ConnectionsStatus status) {
        NS_LOG(VERBOSE) << __func__
                        << ": Stop discovery attempted over Nearby "
                           "Connections with result: "
                        << ConnectionsStatusToString(status);
      }));
}

void NearbyConnectionsManagerImpl::Connect(
    std::vector<uint8_t> endpoint_info,
    const std::string& endpoint_id,
    base::Optional<std::vector<uint8_t>> bluetooth_mac_address,
    DataUsage data_usage,
    NearbyConnectionCallback callback) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (bluetooth_mac_address && bluetooth_mac_address->size() != 6)
    bluetooth_mac_address.reset();

  auto allowed_mediums = MediumSelection::New(
      /*bluetooth=*/true,
      /*ble=*/false, ShouldEnableWebRtc(data_usage, PowerLevel::kHighPower),
      /*wifi_lan=*/kIsWifiLanSupported);
  NS_LOG(VERBOSE) << __func__ << ": "
                  << "data_usage=" << data_usage << ", allowed_mediums="
                  << MediumSelectionToString(*allowed_mediums);

  mojo::PendingRemote<ConnectionLifecycleListener> lifecycle_listener;
  connection_lifecycle_listeners_.Add(
      this, lifecycle_listener.InitWithNewPipeAndPassReceiver());

  auto result =
      pending_outgoing_connections_.emplace(endpoint_id, std::move(callback));
  DCHECK(result.second);

  auto timeout_timer = std::make_unique<base::OneShotTimer>();
  timeout_timer->Start(
      FROM_HERE, kInitiateNearbyConnectionTimeout,
      base::BindOnce(&NearbyConnectionsManagerImpl::OnConnectionTimedOut,
                     weak_ptr_factory_.GetWeakPtr(), endpoint_id));
  connect_timeout_timers_.emplace(endpoint_id, std::move(timeout_timer));

  process_reference_->GetNearbyConnections()->RequestConnection(
      kServiceId, endpoint_info, endpoint_id,
      ConnectionOptions::New(std::move(allowed_mediums),
                             std::move(bluetooth_mac_address)),
      std::move(lifecycle_listener),
      base::BindOnce(&NearbyConnectionsManagerImpl::OnConnectionRequested,
                     weak_ptr_factory_.GetWeakPtr(), endpoint_id));
}

void NearbyConnectionsManagerImpl::OnConnectionTimedOut(
    const std::string& endpoint_id) {
  NS_LOG(ERROR) << "Failed to connect to the remote shareTarget: Timed out.";
  Disconnect(endpoint_id);
}

void NearbyConnectionsManagerImpl::OnConnectionRequested(
    const std::string& endpoint_id,
    ConnectionsStatus status) {
  auto it = pending_outgoing_connections_.find(endpoint_id);
  if (it == pending_outgoing_connections_.end())
    return;

  if (status != ConnectionsStatus::kSuccess) {
    NS_LOG(ERROR) << "Failed to connect to the remote shareTarget: "
                  << ConnectionsStatusToString(status);
    Disconnect(endpoint_id);
    return;
  }

  // TODO(crbug/1111458): Support TransferManager.
}

void NearbyConnectionsManagerImpl::Disconnect(const std::string& endpoint_id) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_)
    return;

  process_reference_->GetNearbyConnections()->DisconnectFromEndpoint(
      kServiceId, endpoint_id,
      base::BindOnce(
          [](const std::string& endpoint_id, ConnectionsStatus status) {
            NS_LOG(VERBOSE)
                << __func__ << ": Disconnecting from endpoint " << endpoint_id
                << " attempted over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
          },
          endpoint_id));

  OnDisconnected(endpoint_id);
  NS_LOG(INFO) << "Disconnected from " << endpoint_id;
}

void NearbyConnectionsManagerImpl::Send(const std::string& endpoint_id,
                                        PayloadPtr payload,
                                        PayloadStatusListener* listener) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_)
    return;

  if (listener)
    RegisterPayloadStatusListener(payload->id, listener);

  process_reference_->GetNearbyConnections()->SendPayload(
      kServiceId, {endpoint_id}, std::move(payload),
      base::BindOnce(
          [](const std::string& endpoint_id, ConnectionsStatus status) {
            NS_LOG(VERBOSE)
                << __func__ << ": Sending payload to endpoint " << endpoint_id
                << " attempted over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
          },
          endpoint_id));
}

void NearbyConnectionsManagerImpl::RegisterPayloadStatusListener(
    int64_t payload_id,
    PayloadStatusListener* listener) {
  payload_status_listeners_.insert_or_assign(payload_id, listener);
}

void NearbyConnectionsManagerImpl::RegisterPayloadPath(
    int64_t payload_id,
    const base::FilePath& file_path,
    ConnectionsCallback callback) {
  if (!process_reference_)
    return;

  DCHECK(!file_path.empty());

  file_handler_.CreateFile(
      file_path, base::BindOnce(&NearbyConnectionsManagerImpl::OnFileCreated,
                                weak_ptr_factory_.GetWeakPtr(), payload_id,
                                std::move(callback)));
}

void NearbyConnectionsManagerImpl::OnFileCreated(
    int64_t payload_id,
    ConnectionsCallback callback,
    NearbyFileHandler::CreateFileResult result) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_)
    return;

  process_reference_->GetNearbyConnections()->RegisterPayloadFile(
      kServiceId, payload_id, std::move(result.input_file),
      std::move(result.output_file), std::move(callback));
}

NearbyConnectionsManagerImpl::Payload*
NearbyConnectionsManagerImpl::GetIncomingPayload(int64_t payload_id) {
  auto it = incoming_payloads_.find(payload_id);
  if (it == incoming_payloads_.end())
    return nullptr;

  return it->second.get();
}

void NearbyConnectionsManagerImpl::Cancel(int64_t payload_id) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_)
    return;

  auto it = payload_status_listeners_.find(payload_id);
  if (it != payload_status_listeners_.end()) {
    it->second->OnStatusUpdate(
        PayloadTransferUpdate::New(payload_id, PayloadStatus::kCanceled,
                                   /*total_bytes=*/0,
                                   /*bytes_transferred=*/0),
        /*upgraded_medium=*/base::nullopt);

    // Erase using the payload ID key instead of the iterator. The
    // OnStatusUpdate() call might result in iterator invalidation, for example,
    // if the listener map entry is removed during a resulting payload clean-up.
    payload_status_listeners_.erase(payload_id);
  }
  process_reference_->GetNearbyConnections()->CancelPayload(
      kServiceId, payload_id,
      base::BindOnce(
          [](int64_t payload_id, ConnectionsStatus status) {
            NS_LOG(VERBOSE)
                << __func__ << ": Cancelling payload to id " << payload_id
                << " attempted over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
          },
          payload_id));
  NS_LOG(INFO) << "Cancelling payload: " << payload_id;
}

void NearbyConnectionsManagerImpl::ClearIncomingPayloads() {
  std::vector<PayloadPtr> payloads;
  for (auto& it : incoming_payloads_) {
    payloads.push_back(std::move(it.second));
    payload_status_listeners_.erase(it.first);
  }

  file_handler_.ReleaseFilePayloads(std::move(payloads));
  incoming_payloads_.clear();
}

base::Optional<std::vector<uint8_t>>
NearbyConnectionsManagerImpl::GetRawAuthenticationToken(
    const std::string& endpoint_id) {
  auto it = connection_info_map_.find(endpoint_id);
  if (it == connection_info_map_.end())
    return base::nullopt;

  return it->second->raw_authentication_token;
}

void NearbyConnectionsManagerImpl::UpgradeBandwidth(
    const std::string& endpoint_id) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_)
    return;

  // The only bandwidth upgrade at this point is WebRTC.
  if (!base::FeatureList::IsEnabled(features::kNearbySharingWebRtc))
    return;

  requested_bwu_endpoint_ids_.emplace(endpoint_id);
  process_reference_->GetNearbyConnections()->InitiateBandwidthUpgrade(
      kServiceId, endpoint_id,
      base::BindOnce(
          [](const std::string& endpoint_id, ConnectionsStatus status) {
            NS_LOG(VERBOSE)
                << __func__ << ": Bandwidth upgrade attempted to endpoint "
                << endpoint_id << "over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
            base::UmaHistogramBoolean(
                "Nearby.Share.Medium.InitiateBandwidthUpgradeResult",
                status == ConnectionsStatus::kSuccess);
          },
          endpoint_id));
}

void NearbyConnectionsManagerImpl::OnNearbyProcessStopped(
    chromeos::nearby::NearbyProcessManager::NearbyProcessShutdownReason) {
  NS_LOG(VERBOSE) << __func__;
  Reset();
}

void NearbyConnectionsManagerImpl::OnEndpointFound(
    const std::string& endpoint_id,
    DiscoveredEndpointInfoPtr info) {
  if (!discovery_listener_) {
    NS_LOG(INFO) << "Ignoring discovered endpoint "
                 << base::HexEncode(info->endpoint_info.data(),
                                    info->endpoint_info.size())
                 << " because we're no longer "
                    "in discovery mode";
    return;
  }

  auto result = discovered_endpoints_.insert(endpoint_id);
  if (!result.second) {
    NS_LOG(INFO) << "Ignoring discovered endpoint "
                 << base::HexEncode(info->endpoint_info.data(),
                                    info->endpoint_info.size())
                 << " because we've already "
                    "reported this endpoint";
    return;
  }

  discovery_listener_->OnEndpointDiscovered(endpoint_id, info->endpoint_info);
  NS_LOG(INFO) << "Discovered "
               << base::HexEncode(info->endpoint_info.data(),
                                  info->endpoint_info.size())
               << " over Nearby Connections";
}

void NearbyConnectionsManagerImpl::OnEndpointLost(
    const std::string& endpoint_id) {
  if (!discovered_endpoints_.erase(endpoint_id)) {
    NS_LOG(INFO) << "Ignoring lost endpoint " << endpoint_id
                 << " because we haven't reported this endpoint";
    return;
  }

  if (!discovery_listener_) {
    NS_LOG(INFO) << "Ignoring lost endpoint " << endpoint_id
                 << " because we're no longer in discovery mode";
    return;
  }

  discovery_listener_->OnEndpointLost(endpoint_id);
  NS_LOG(INFO) << "Endpoint " << endpoint_id << " lost over Nearby Connections";
}

void NearbyConnectionsManagerImpl::OnConnectionInitiated(
    const std::string& endpoint_id,
    ConnectionInfoPtr info) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_)
    return;

  auto result = connection_info_map_.emplace(endpoint_id, std::move(info));
  DCHECK(result.second);

  mojo::PendingRemote<PayloadListener> payload_listener;
  payload_listeners_.Add(this,
                         payload_listener.InitWithNewPipeAndPassReceiver());

  process_reference_->GetNearbyConnections()->AcceptConnection(
      kServiceId, endpoint_id, std::move(payload_listener),
      base::BindOnce(
          [](const std::string& endpoint_id, ConnectionsStatus status) {
            NS_LOG(VERBOSE)
                << __func__ << ": Accept connection attempted to endpoint "
                << endpoint_id << " over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
          },
          endpoint_id));
}

void NearbyConnectionsManagerImpl::OnConnectionAccepted(
    const std::string& endpoint_id) {
  auto it = connection_info_map_.find(endpoint_id);
  if (it == connection_info_map_.end())
    return;

  if (it->second->is_incoming_connection) {
    if (!incoming_connection_listener_) {
      // Not in advertising mode.
      Disconnect(endpoint_id);
      return;
    }

    auto result = connections_.emplace(
        endpoint_id, std::make_unique<NearbyConnectionImpl>(this, endpoint_id));
    DCHECK(result.second);
    incoming_connection_listener_->OnIncomingConnection(
        endpoint_id, it->second->endpoint_info, result.first->second.get());
  } else {
    auto it = pending_outgoing_connections_.find(endpoint_id);
    if (it == pending_outgoing_connections_.end()) {
      Disconnect(endpoint_id);
      return;
    }

    auto result = connections_.emplace(
        endpoint_id, std::make_unique<NearbyConnectionImpl>(this, endpoint_id));
    DCHECK(result.second);
    std::move(it->second).Run(result.first->second.get());
    pending_outgoing_connections_.erase(it);
    connect_timeout_timers_.erase(endpoint_id);
  }
}

void NearbyConnectionsManagerImpl::OnConnectionRejected(
    const std::string& endpoint_id,
    Status status) {
  connection_info_map_.erase(endpoint_id);

  auto it = pending_outgoing_connections_.find(endpoint_id);
  if (it != pending_outgoing_connections_.end()) {
    std::move(it->second).Run(nullptr);
    pending_outgoing_connections_.erase(it);
    connect_timeout_timers_.erase(endpoint_id);
  }

  // TODO(crbug/1111458): Support TransferManager.
}

void NearbyConnectionsManagerImpl::OnDisconnected(
    const std::string& endpoint_id) {
  connection_info_map_.erase(endpoint_id);

  auto it = pending_outgoing_connections_.find(endpoint_id);
  if (it != pending_outgoing_connections_.end()) {
    std::move(it->second).Run(nullptr);
    pending_outgoing_connections_.erase(it);
    connect_timeout_timers_.erase(endpoint_id);
  }

  connections_.erase(endpoint_id);

  if (base::Contains(requested_bwu_endpoint_ids_, endpoint_id)) {
    base::UmaHistogramBoolean(
        "Nearby.Share.Medium.RequestedBandwidthUpgradeResult",
        base::Contains(current_upgraded_mediums_, endpoint_id));
  }
  requested_bwu_endpoint_ids_.erase(endpoint_id);
  current_upgraded_mediums_.erase(endpoint_id);

  // TODO(crbug/1111458): Support TransferManager.
}

void NearbyConnectionsManagerImpl::OnBandwidthChanged(
    const std::string& endpoint_id,
    Medium medium) {
  NS_LOG(VERBOSE) << __func__ << ": Changed to medium=" << medium
                  << "; endpoint_id=" << endpoint_id;
  base::UmaHistogramEnumeration("Nearby.Share.Medium.ChangedToMedium", medium);
  current_upgraded_mediums_.insert_or_assign(endpoint_id, medium);
  // TODO(crbug/1111458): Support TransferManager.
}

void NearbyConnectionsManagerImpl::OnPayloadReceived(
    const std::string& endpoint_id,
    PayloadPtr payload) {
  auto result = incoming_payloads_.emplace(payload->id, std::move(payload));
  DCHECK(result.second);
}

void NearbyConnectionsManagerImpl::OnPayloadTransferUpdate(
    const std::string& endpoint_id,
    PayloadTransferUpdatePtr update) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_)
    return;

  // If this is a payload we've registered for, then forward its status to the
  // PayloadStatusListener. We don't need to do anything more with the payload.
  auto listener_it = payload_status_listeners_.find(update->payload_id);
  if (listener_it != payload_status_listeners_.end()) {
    PayloadStatusListener* listener = listener_it->second;
    switch (update->status) {
      case PayloadStatus::kInProgress:
        break;
      case PayloadStatus::kSuccess:
      case PayloadStatus::kCanceled:
      case PayloadStatus::kFailure:
        payload_status_listeners_.erase(listener_it);
        break;
    }
    listener->OnStatusUpdate(std::move(update), GetUpgradedMedium(endpoint_id));
    return;
  }

  // If this is an incoming payload that we have not registered for, then we'll
  // treat it as a control frame (eg. IntroductionFrame) and forward it to the
  // associated NearbyConnection.
  auto payload_it = incoming_payloads_.find(update->payload_id);
  if (payload_it == incoming_payloads_.end())
    return;

  if (!payload_it->second->content->is_bytes()) {
    NS_LOG(WARNING) << "Received unknown payload of file type. Cancelling.";
    process_reference_->GetNearbyConnections()->CancelPayload(
        kServiceId, payload_it->first, base::DoNothing());
    return;
  }

  if (update->status != PayloadStatus::kSuccess)
    return;

  auto connections_it = connections_.find(endpoint_id);
  if (connections_it == connections_.end())
    return;

  NS_LOG(INFO) << "Writing incoming byte message to NearbyConnection.";
  connections_it->second->WriteMessage(
      payload_it->second->content->get_bytes()->bytes);
}

location::nearby::connections::mojom::NearbyConnections*
NearbyConnectionsManagerImpl::GetNearbyConnections() {
  if (!process_reference_) {
    process_reference_ = process_manager_->GetNearbyProcessReference(
        base::BindOnce(&NearbyConnectionsManagerImpl::OnNearbyProcessStopped,
                       base::Unretained(this)));

    if (!process_reference_) {
      NS_LOG(WARNING) << __func__
                      << "Failed to get a reference to the nearby process.";
      return nullptr;
    }
  }

  location::nearby::connections::mojom::NearbyConnections* nearby_connections =
      process_reference_->GetNearbyConnections().get();

  if (!nearby_connections)
    NS_LOG(WARNING)
        << __func__
        << "Failed to get a nearby connections from process reference.";

  return nearby_connections;
}

void NearbyConnectionsManagerImpl::Reset() {
  if (process_reference_) {
    process_reference_->GetNearbyConnections()->StopAllEndpoints(
        kServiceId, base::BindOnce([](ConnectionsStatus status) {
          NS_LOG(VERBOSE) << __func__
                          << ": Stop all endpoints attempted over Nearby "
                             "Connections with result: "
                          << ConnectionsStatusToString(status);
        }));
  }
  process_reference_.reset();
  discovered_endpoints_.clear();
  payload_status_listeners_.clear();
  ClearIncomingPayloads();
  connections_.clear();
  connection_info_map_.clear();
  discovery_listener_ = nullptr;
  incoming_connection_listener_ = nullptr;
  endpoint_discovery_listener_.reset();
  connect_timeout_timers_.clear();
  requested_bwu_endpoint_ids_.clear();
  current_upgraded_mediums_.clear();

  for (auto& entry : pending_outgoing_connections_)
    std::move(entry.second).Run(/*connection=*/nullptr);

  pending_outgoing_connections_.clear();
}

base::Optional<location::nearby::connections::mojom::Medium>
NearbyConnectionsManagerImpl::GetUpgradedMedium(
    const std::string& endpoint_id) const {
  const auto it = current_upgraded_mediums_.find(endpoint_id);
  if (it == current_upgraded_mediums_.end())
    return base::nullopt;

  return it->second;
}
