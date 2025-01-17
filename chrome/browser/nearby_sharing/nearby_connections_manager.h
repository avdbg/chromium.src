// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_enums.h"
#include "chrome/browser/nearby_sharing/nearby_connection.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections_types.mojom.h"

// A wrapper around the Nearby Connections mojo API.
class NearbyConnectionsManager {
 public:
  using Payload = location::nearby::connections::mojom::Payload;
  using PayloadPtr = location::nearby::connections::mojom::PayloadPtr;
  using ConnectionsStatus = location::nearby::connections::mojom::Status;
  using ConnectionsCallback =
      base::OnceCallback<void(ConnectionsStatus status)>;
  using NearbyConnectionCallback = base::OnceCallback<void(NearbyConnection*)>;

  // A callback for handling incoming connections while advertising.
  class IncomingConnectionListener {
   public:
    virtual ~IncomingConnectionListener() = default;

    // |endpoint_info| is returned from remote devices and should be parsed in
    // utilitiy process.
    virtual void OnIncomingConnection(const std::string& endpoint_id,
                                      const std::vector<uint8_t>& endpoint_info,
                                      NearbyConnection* connection) = 0;
  };

  // A callback for handling discovered devices while discovering.
  class DiscoveryListener {
   public:
    virtual ~DiscoveryListener() = default;

    // |endpoint_info| is returned from remote devices and should be parsed in
    // utilitiy process.
    virtual void OnEndpointDiscovered(
        const std::string& endpoint_id,
        const std::vector<uint8_t>& endpoint_info) = 0;

    virtual void OnEndpointLost(const std::string& endpoint_id) = 0;
  };

  // A callback for tracking the status of a payload (both incoming and
  // outgoing).
  class PayloadStatusListener {
   public:
    using Medium = location::nearby::connections::mojom::Medium;
    using PayloadTransferUpdatePtr =
        location::nearby::connections::mojom::PayloadTransferUpdatePtr;

    virtual ~PayloadStatusListener() = default;

    // Note: |upgraded_medium| is passed in for use in metrics, and it is
    // base::nullopt if the bandwidth has not upgraded yet or if the upgrade
    // status is not known.
    virtual void OnStatusUpdate(PayloadTransferUpdatePtr update,
                                base::Optional<Medium> upgraded_medium) = 0;
  };

  // Converts the status to a logging-friendly string.
  static std::string ConnectionsStatusToString(ConnectionsStatus status);

  virtual ~NearbyConnectionsManager() = default;

  // Disconnects from all endpoints and shut down Nearby Connections.
  // As a side effect of this call, both StopAdvertising and StopDiscovery may
  // be invoked if Nearby Connections is advertising or discovering.
  virtual void Shutdown() = 0;

  // Starts advertising through Nearby Connections. Caller is expected to ensure
  // |listener| remains valid until StopAdvertising is called.
  virtual void StartAdvertising(std::vector<uint8_t> endpoint_info,
                                IncomingConnectionListener* listener,
                                PowerLevel power_level,
                                DataUsage data_usage,
                                ConnectionsCallback callback) = 0;

  // Stops advertising through Nearby Connections.
  virtual void StopAdvertising() = 0;

  // Starts discovery through Nearby Connections. Caller is expected to ensure
  // |listener| remains valid until StopDiscovery is called.
  virtual void StartDiscovery(DiscoveryListener* listener,
                              DataUsage data_usage,
                              ConnectionsCallback callback) = 0;

  // Stops discovery through Nearby Connections.
  virtual void StopDiscovery() = 0;

  // Conntects to remote |endpoint_id| through Nearby Connections.
  virtual void Connect(
      std::vector<uint8_t> endpoint_info,
      const std::string& endpoint_id,
      base::Optional<std::vector<uint8_t>> bluetooth_mac_address,
      DataUsage data_usage,
      NearbyConnectionCallback callback) = 0;

  // Disconnects from remote |endpoint_id| through Nearby Connections.
  virtual void Disconnect(const std::string& endpoint_id) = 0;

  // Sends |payload| through Nearby Connections. Caller is expected to ensure
  // |listener| remains valid until kSuccess/kFailure/kCancelled is invoked with
  // OnStatusUpdate.
  virtual void Send(const std::string& endpoint_id,
                    PayloadPtr payload,
                    PayloadStatusListener* listener) = 0;

  // Register a |listener| with |payload_id|. Caller is expected to ensure
  // |listener| remains valid until kSuccess/kFailure/kCancelled is invoked with
  // OnStatusUpdate.
  virtual void RegisterPayloadStatusListener(
      int64_t payload_id,
      PayloadStatusListener* listener) = 0;

  // Register a |file_path| for receiving incoming payload with |payload_id|.
  virtual void RegisterPayloadPath(int64_t payload_id,
                                   const base::FilePath& file_path,
                                   ConnectionsCallback callback) = 0;

  // Gets the payload associated with |payload_id| if available.
  virtual Payload* GetIncomingPayload(int64_t payload_id) = 0;

  // Cancels a Payload currently in-flight to or from remote endpoints.
  virtual void Cancel(int64_t payload_id) = 0;

  // Clears all incoming payloads.
  virtual void ClearIncomingPayloads() = 0;

  // Gets the raw authentication token for the |endpoint_id|.
  virtual base::Optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      const std::string& endpoint_id) = 0;

  // Initiates bandwidth upgrade for |endpoint_id|.
  virtual void UpgradeBandwidth(const std::string& endpoint_id) = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_H_
