// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_IMPL_H_
#define CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_IMPL_H_

#include "base/component_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

// Implementation of NetworkConnectionHandler.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkConnectionHandlerImpl
    : public NetworkConnectionHandler,
      public NetworkCertLoader::Observer,
      public NetworkStateHandlerObserver,
      public base::SupportsWeakPtr<NetworkConnectionHandlerImpl> {
 public:
  NetworkConnectionHandlerImpl();
  ~NetworkConnectionHandlerImpl() override;

  // NetworkConnectionHandler:
  void ConnectToNetwork(const std::string& service_path,
                        base::OnceClosure success_callback,
                        network_handler::ErrorCallback error_callback,
                        bool check_error_state,
                        ConnectCallbackMode mode) override;
  void DisconnectNetwork(
      const std::string& service_path,
      base::OnceClosure success_callback,
      network_handler::ErrorCallback error_callback) override;

  // NetworkStateHandlerObserver
  void NetworkListChanged() override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;

  // NetworkCertLoader::Observer
  void OnCertificatesLoaded() override;

  void Init(
      NetworkStateHandler* network_state_handler,
      NetworkConfigurationHandler* network_configuration_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      CellularESimConnectionHandler* cellular_esim_connection_handler) override;

 private:
  struct ConnectRequest {
    ConnectRequest(ConnectCallbackMode mode,
                   const std::string& service_path,
                   const std::string& profile_path,
                   base::OnceClosure success_callback,
                   network_handler::ErrorCallback error);
    ~ConnectRequest();
    ConnectRequest(ConnectRequest&&);

    enum ConnectState {
      CONNECT_REQUESTED = 0,
      CONNECT_STARTED = 1,
      CONNECT_CONNECTING = 2
    };

    ConnectCallbackMode mode;
    std::string service_path;
    std::string profile_path;
    ConnectState connect_state;
    base::OnceClosure success_callback;
    network_handler::ErrorCallback error_callback;
  };

  bool HasConnectingNetwork(const std::string& service_path);

  ConnectRequest* GetPendingRequest(const std::string& service_path);

  void OnEnableESimProfileFailure(
      const std::string& service_path,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);

  // Callback from Shill.Service.GetProperties. Parses |properties| to verify
  // whether or not the network appears to be configured. If configured,
  // attempts a connection, otherwise invokes error_callback from
  // pending_requests_[service_path]. |check_error_state| is passed from
  // ConnectToNetwork(), see comment for info.
  void VerifyConfiguredAndConnect(bool check_error_state,
                                  const std::string& service_path,
                                  base::Optional<base::Value> properties);

  // Queues a connect request until certificates have loaded.
  void QueueConnectRequest(const std::string& service_path);

  // Checks to see if certificates have loaded and if not, cancels any queued
  // connect request and notifies the user.
  void CheckCertificatesLoaded();

  // Handles connecting to a queued network after certificates are loaded or
  // handle cert load timeout.
  void ConnectToQueuedNetwork();

  // Calls Shill.Manager.Connect asynchronously.
  void CallShillConnect(const std::string& service_path);

  // Handles failure from ConfigurationHandler calls.
  void HandleConfigurationFailure(
      const std::string& service_path,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);

  // Handles success or failure from Shill.Service.Connect.
  void HandleShillConnectSuccess(const std::string& service_path);
  void HandleShillConnectFailure(const std::string& service_path,
                                 const std::string& error_name,
                                 const std::string& error_message);

  // Note: |service_path| is passed by value here, because in some cases
  // the value may be located in the map and then it can be deleted, producing
  // a reference to invalid memory.
  void CheckPendingRequest(const std::string service_path);

  void CheckAllPendingRequests();
  void ClearPendingRequest(const std::string& service_path);

  // Look up the ConnectRequest for |service_path| and call
  // InvokeConnectErrorCallback.
  void ErrorCallbackForPendingRequest(const std::string& service_path,
                                      const std::string& error_name);

  // Calls Shill.Manager.Disconnect asynchronously.
  void CallShillDisconnect(const std::string& service_path,
                           base::OnceClosure success_callback,
                           network_handler::ErrorCallback error_callback);

  // Handle success from Shill.Service.Disconnect.
  void HandleShillDisconnectSuccess(const std::string& service_path,
                                    base::OnceClosure success_callback);

  // Local references to the associated handler instances.
  NetworkCertLoader* network_cert_loader_ = nullptr;
  NetworkStateHandler* network_state_handler_ = nullptr;
  NetworkConfigurationHandler* configuration_handler_ = nullptr;
  ManagedNetworkConfigurationHandler* managed_configuration_handler_ = nullptr;
  CellularESimConnectionHandler* cellular_esim_connection_handler_ = nullptr;

  // Map of pending connect requests, used to prevent repeated attempts while
  // waiting for Shill and to trigger callbacks on eventual success or failure.
  std::map<std::string, ConnectRequest> pending_requests_;
  std::unique_ptr<ConnectRequest> queued_connect_;

  // Track certificate loading state.
  bool certificates_loaded_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_IMPL_H_
