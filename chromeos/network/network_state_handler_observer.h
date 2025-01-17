// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_OBSERVER_H_
#define CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_OBSERVER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_type_pattern.h"

namespace chromeos {

class DeviceState;

// Observer class for all network state changes, including changes to
// active (connecting or connected) services.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkStateHandlerObserver {
 public:
  NetworkStateHandlerObserver();
  virtual ~NetworkStateHandlerObserver();

  // The list of networks changed.
  virtual void NetworkListChanged();

  // The list of devices changed. Use DevicePropertiesUpdated to be notified
  // when a Device property changes.
  virtual void DeviceListChanged();

  // The default network changed (includes VPNs) or one of its properties
  // changed. This won't be called if the WiFi signal strength property
  // changes. If interested in those events, use NetworkPropertiesUpdated()
  // below.
  // |network| will be null if there is no longer a default network.
  virtual void DefaultNetworkChanged(const NetworkState* network);

  // The portal state or the proxy configuration of the default network changed.
  // Note: |default_network| may be null if there is no default network, in
  // which case |portal_state| will always be kUnknown.
  virtual void PortalStateChanged(const NetworkState* default_network,
                                  NetworkState::PortalState portal_state);

  // The connection state of |network| changed.
  virtual void NetworkConnectionStateChanged(const NetworkState* network);

  // Triggered when the connection state of any current or previously active
  // (connected or connecting) network changes. Includes significant changes to
  // the signal strength. Provides the current list of active networks, which
  // may include a VPN.
  virtual void ActiveNetworksChanged(
      const std::vector<const NetworkState*>& active_networks);

  // One or more properties of |network| have been updated. Note: this will get
  // called in *addition* to NetworkConnectionStateChanged() when the
  // connection state property changes. Use this to track properties like
  // wifi strength.
  virtual void NetworkPropertiesUpdated(const NetworkState* network);

  // One or more properties of |device| have been updated.
  virtual void DevicePropertiesUpdated(const DeviceState* device);

  // A scan for a given network type has been requested.
  virtual void ScanRequested(const NetworkTypePattern& type);

  // A scan for |device| started.
  virtual void ScanStarted(const DeviceState* device);

  // A scan for |device| completed.
  virtual void ScanCompleted(const DeviceState* device);

  // The DHCP Hostname changed.
  virtual void HostnameChanged(const std::string& hostname);

  // Called just before NetworkStateHandler is destroyed so that observers
  // can safely stop observing.
  virtual void OnShuttingDown();

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateHandlerObserver);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::NetworkStateHandlerObserver;
}

#endif  // CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_OBSERVER_H_
