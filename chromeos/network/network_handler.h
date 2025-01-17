// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_HANDLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"

class PrefService;

namespace chromeos {

class AutoConnectHandler;
class CellularESimConnectionHandler;
class CellularESimProfileHandler;
class CellularInhibitor;
class CellularESimUninstallHandler;
class CellularMetricsLogger;
class ClientCertResolver;
class GeolocationHandler;
class ManagedNetworkConfigurationHandler;
class ManagedNetworkConfigurationHandlerImpl;
class NetworkActivationHandler;
class NetworkCertMigrator;
class NetworkCertificateHandler;
class NetworkConfigurationHandler;
class NetworkConnectionHandler;
class NetworkDeviceHandler;
class NetworkDeviceHandlerImpl;
class NetworkMetadataStore;
class NetworkProfileHandler;
class NetworkStateHandler;
class NetworkSmsHandler;
class ProhibitedTechnologiesHandler;
class UIProxyConfigService;

// Class for handling initialization and access to chromeos network handlers.
// This class should NOT be used in unit tests. Instead, construct individual
// classes independently.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkHandler {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static NetworkHandler* Get();

  // Returns true if the global instance has been initialized.
  static bool IsInitialized();

  // Called whenever the pref services change, e.g. on login. Initializes
  // services with PrefService dependencies (i.e. ui_proxy_config_service).
  // |logged_in_profile_prefs| is the PrefService associated with the logged
  // in user profile. |device_prefs| is the PrefService associated with the
  // device (e.g. in Chrome, g_browser_process->local_state()).
  void InitializePrefServices(PrefService* logged_in_profile_prefs,
                              PrefService* device_prefs);

  // Must be called before pref services are shut down.
  void ShutdownPrefServices();

  // Global network configuration services.
  static bool HasUiProxyConfigService();
  static UIProxyConfigService* GetUiProxyConfigService();

  // Returns the task runner for posting NetworkHandler calls from other
  // threads.
  base::SingleThreadTaskRunner* task_runner() { return task_runner_.get(); }

  // Do not use these accessors within this module; all dependencies should be
  // explicit so that classes can be constructed explicitly in tests without
  // NetworkHandler.
  AutoConnectHandler* auto_connect_handler();
  CellularESimProfileHandler* cellular_esim_profile_handler();
  CellularESimUninstallHandler* cellular_esim_uninstall_handler();
  CellularInhibitor* cellular_inhibitor();
  NetworkStateHandler* network_state_handler();
  NetworkDeviceHandler* network_device_handler();
  NetworkProfileHandler* network_profile_handler();
  NetworkConfigurationHandler* network_configuration_handler();
  ManagedNetworkConfigurationHandler* managed_network_configuration_handler();
  NetworkActivationHandler* network_activation_handler();
  NetworkCertificateHandler* network_certificate_handler();
  NetworkConnectionHandler* network_connection_handler();
  NetworkMetadataStore* network_metadata_store();
  NetworkSmsHandler* network_sms_handler();
  GeolocationHandler* geolocation_handler();
  ProhibitedTechnologiesHandler* prohibited_technologies_handler();

  void set_is_enterprise_managed(bool is_enterprise_managed) {
    is_enterprise_managed_ = is_enterprise_managed;
  }

 private:
  NetworkHandler();
  virtual ~NetworkHandler();

  void Init();

  // The order of these determines the (inverse) destruction order.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandlerImpl> network_device_handler_;
  std::unique_ptr<CellularESimProfileHandler> cellular_esim_profile_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<CellularESimConnectionHandler>
      cellular_esim_connection_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_network_configuration_handler_;
  std::unique_ptr<NetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimUninstallHandler>
      cellular_esim_uninstall_handler_;
  std::unique_ptr<CellularMetricsLogger> cellular_metrics_logger_;
  std::unique_ptr<NetworkCertMigrator> network_cert_migrator_;
  std::unique_ptr<ClientCertResolver> client_cert_resolver_;
  std::unique_ptr<AutoConnectHandler> auto_connect_handler_;
  std::unique_ptr<NetworkCertificateHandler> network_certificate_handler_;
  std::unique_ptr<NetworkActivationHandler> network_activation_handler_;
  std::unique_ptr<ProhibitedTechnologiesHandler>
      prohibited_technologies_handler_;
  std::unique_ptr<NetworkSmsHandler> network_sms_handler_;
  std::unique_ptr<GeolocationHandler> geolocation_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  std::unique_ptr<NetworkMetadataStore> network_metadata_store_;

  // True when the device is managed by policy.
  bool is_enterprise_managed_ = false;

  DISALLOW_COPY_AND_ASSIGN(NetworkHandler);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to  ash/components/.
namespace ash {
using ::chromeos::NetworkHandler;
}

#endif  // CHROMEOS_NETWORK_NETWORK_HANDLER_H_
