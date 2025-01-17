// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hermes/fake_hermes_euicc_client.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

// Cellular Service EID property.
// TODO(crbug.com/1093185): Use dbus-constants when property is added in shill.
const char kCellularEidProperty[] = "Cellular.EID";

const char* kDefaultMccMnc = "310999";
const char* kFakeActivationCodePrefix = "1$SMDP.GSMA.COM$00000-00000-00000-000";
const char* kFakeProfilePathPrefix = "/org/chromium/Hermes/Profile/";
const char* kFakeIccidPrefix = "10000000000000000";
const char* kFakeProfileNamePrefix = "FakeCellularNetwork_";
const char* kFakeServiceProvider = "Fake Wireless";
const char* kFakeNetworkServicePathPrefix = "/service/cellular1";

bool PopPendingProfile(HermesEuiccClient::Properties* properties,
                       dbus::ObjectPath carrier_profile_path) {
  std::vector<dbus::ObjectPath> pending_profiles =
      properties->pending_carrier_profiles().value();
  auto it = std::find(pending_profiles.begin(), pending_profiles.end(),
                      carrier_profile_path);
  if (it == pending_profiles.end()) {
    return false;
  }

  pending_profiles.erase(it);
  properties->pending_carrier_profiles().ReplaceValue(pending_profiles);
  return true;
}

dbus::ObjectPath PopPendingProfileWithActivationCode(
    HermesEuiccClient::Properties* euicc_properties,
    const std::string& activation_code) {
  std::vector<dbus::ObjectPath> pending_profiles =
      euicc_properties->pending_carrier_profiles().value();
  for (auto it = pending_profiles.begin(); it != pending_profiles.end(); it++) {
    dbus::ObjectPath carrier_profile_path = *it;
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(carrier_profile_path);
    if (profile_properties->activation_code().value() == activation_code) {
      pending_profiles.erase(it);
      euicc_properties->pending_carrier_profiles().ReplaceValue(
          pending_profiles);
      return carrier_profile_path;
    }
  }
  return dbus::ObjectPath();
}

}  // namespace

FakeHermesEuiccClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : HermesEuiccClient::Properties(nullptr, callback) {}

FakeHermesEuiccClient::Properties::~Properties() = default;

void FakeHermesEuiccClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  DVLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeHermesEuiccClient::Properties::GetAll() {
  DVLOG(1) << "GetAll";
}

void FakeHermesEuiccClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  DVLOG(1) << "Set " << property->name();
  std::move(callback).Run(false);
}

FakeHermesEuiccClient::FakeHermesEuiccClient() = default;
FakeHermesEuiccClient::~FakeHermesEuiccClient() = default;

void FakeHermesEuiccClient::ClearEuicc(const dbus::ObjectPath& euicc_path) {
  PropertiesMap::iterator it = properties_map_.find(euicc_path);
  if (it == properties_map_.end())
    return;
  auto* profile_test = HermesProfileClient::Get()->GetTestInterface();
  HermesEuiccClient::Properties* properties = it->second.get();
  for (const auto& path : properties->installed_carrier_profiles().value()) {
    profile_test->ClearProfile(path);
  }
  for (const auto& path : properties->pending_carrier_profiles().value()) {
    profile_test->ClearProfile(path);
  }
  properties_map_.erase(it);
}

void FakeHermesEuiccClient::ResetPendingEventsRequested() {
  pending_event_requested_ = false;
}

dbus::ObjectPath FakeHermesEuiccClient::AddFakeCarrierProfile(
    const dbus::ObjectPath& euicc_path,
    hermes::profile::State state,
    const std::string& activation_code,
    bool service_only) {
  int index = fake_profile_counter_++;
  dbus::ObjectPath carrier_profile_path(
      base::StringPrintf("%s%02d", kFakeProfilePathPrefix, index));

  AddCarrierProfile(
      carrier_profile_path, euicc_path,
      base::StringPrintf("%s%02d", kFakeIccidPrefix, index),
      base::StringPrintf("%s%02d", kFakeProfileNamePrefix, index),
      kFakeServiceProvider,
      activation_code.empty()
          ? base::StringPrintf("%s%02d", kFakeActivationCodePrefix, index)
          : activation_code,
      base::StringPrintf("%s%02d", kFakeNetworkServicePathPrefix, index), state,
      service_only);
  return carrier_profile_path;
}

void FakeHermesEuiccClient::AddCarrierProfile(
    const dbus::ObjectPath& path,
    const dbus::ObjectPath& euicc_path,
    const std::string& iccid,
    const std::string& name,
    const std::string& service_provider,
    const std::string& activation_code,
    const std::string& network_service_path,
    hermes::profile::State state,
    bool service_only) {
  DVLOG(1) << "Adding new profile path=" << path.value() << ", name=" << name
           << ", state=" << state;
  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(path);
  profile_properties->iccid().ReplaceValue(iccid);
  profile_properties->service_provider().ReplaceValue(service_provider);
  profile_properties->mcc_mnc().ReplaceValue(kDefaultMccMnc);
  profile_properties->activation_code().ReplaceValue(activation_code);
  profile_properties->name().ReplaceValue(name);
  profile_properties->nick_name().ReplaceValue(name);
  profile_properties->state().ReplaceValue(state);
  profile_service_path_map_[path] = network_service_path;

  Properties* euicc_properties = GetProperties(euicc_path);
  if (state == hermes::profile::State::kPending) {
    std::vector<dbus::ObjectPath> pending_profiles =
        euicc_properties->pending_carrier_profiles().value();
    pending_profiles.push_back(path);
    euicc_properties->pending_carrier_profiles().ReplaceValue(pending_profiles);
    return;
  }

  CreateCellularService(euicc_path, path);
  if (service_only) {
    QueueInstalledProfile(euicc_path, path);
    return;
  }
  std::vector<dbus::ObjectPath> installed_profiles =
      euicc_properties->installed_carrier_profiles().value();
  installed_profiles.push_back(path);
  euicc_properties->installed_carrier_profiles().ReplaceValue(
      installed_profiles);
}

void FakeHermesEuiccClient::QueueHermesErrorStatus(
    HermesResponseStatus status) {
  error_status_queue_.push(status);
}

void FakeHermesEuiccClient::SetInteractiveDelay(base::TimeDelta delay) {
  interactive_delay_ = delay;
}

void FakeHermesEuiccClient::InstallProfileFromActivationCode(
    const dbus::ObjectPath& euicc_path,
    const std::string& activation_code,
    const std::string& confirmation_code,
    InstallCarrierProfileCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoInstallProfileFromActivationCode,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     activation_code, confirmation_code, std::move(callback)),
      interactive_delay_);
}

void FakeHermesEuiccClient::InstallPendingProfile(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& carrier_profile_path,
    const std::string& confirmation_code,
    HermesResponseCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoInstallPendingProfile,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     carrier_profile_path, confirmation_code,
                     std::move(callback)),
      interactive_delay_);
}

void FakeHermesEuiccClient::RequestInstalledProfiles(
    const dbus::ObjectPath& euicc_path,
    HermesResponseCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoRequestInstalledProfiles,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     std::move(callback)),
      interactive_delay_);
}

void FakeHermesEuiccClient::RequestPendingProfiles(
    const dbus::ObjectPath& euicc_path,
    const std::string& root_smds,
    HermesResponseCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoRequestPendingProfiles,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     std::move(callback)),
      interactive_delay_);
}

void FakeHermesEuiccClient::UninstallProfile(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& carrier_profile_path,
    HermesResponseCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoUninstallProfile,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     carrier_profile_path, std::move(callback)),
      interactive_delay_);
}

FakeHermesEuiccClient::Properties* FakeHermesEuiccClient::GetProperties(
    const dbus::ObjectPath& euicc_path) {
  auto it = properties_map_.find(euicc_path);
  if (it != properties_map_.end()) {
    return it->second.get();
  }

  DVLOG(1) << "Creating new Fake Euicc object path =" << euicc_path.value();
  std::unique_ptr<Properties> properties(new Properties(
      base::BindRepeating(&FakeHermesEuiccClient::CallNotifyPropertyChanged,
                          weak_ptr_factory_.GetWeakPtr(), euicc_path)));
  properties_map_[euicc_path] = std::move(properties);
  return properties_map_[euicc_path].get();
}

HermesEuiccClient::TestInterface* FakeHermesEuiccClient::GetTestInterface() {
  return this;
}

void FakeHermesEuiccClient::DoInstallProfileFromActivationCode(
    const dbus::ObjectPath& euicc_path,
    const std::string& activation_code,
    const std::string& confirmation_code,
    InstallCarrierProfileCallback callback) {
  DVLOG(1) << "Installing profile from activation code: code="
           << activation_code << ", confirmation_code=" << confirmation_code;
  if (!error_status_queue_.empty()) {
    std::move(callback).Run(error_status_queue_.front(), nullptr);
    error_status_queue_.pop();
    return;
  }

  if (!base::StartsWith(activation_code, kFakeActivationCodePrefix,
                        base::CompareCase::SENSITIVE)) {
    std::move(callback).Run(HermesResponseStatus::kErrorInvalidActivationCode,
                            nullptr);
    return;
  }

  Properties* euicc_properties = GetProperties(euicc_path);

  dbus::ObjectPath profile_path =
      PopPendingProfileWithActivationCode(euicc_properties, activation_code);
  if (profile_path.IsValid()) {
    // Move pending profile to installed.
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(profile_path);
    profile_properties->state().ReplaceValue(hermes::profile::State::kInactive);

    std::vector<dbus::ObjectPath> installed_profiles =
        euicc_properties->installed_carrier_profiles().value();
    installed_profiles.push_back(profile_path);
    euicc_properties->installed_carrier_profiles().ReplaceValue(
        installed_profiles);
  } else {
    // Create a new installed profile with given activation code.
    profile_path =
        AddFakeCarrierProfile(euicc_path, hermes::profile::State::kInactive,
                              activation_code, /*service_only=*/false);
  }
  CreateCellularService(euicc_path, profile_path);

  std::move(callback).Run(HermesResponseStatus::kSuccess, &profile_path);
}

void FakeHermesEuiccClient::DoInstallPendingProfile(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& carrier_profile_path,
    const std::string& confirmation_code,
    HermesResponseCallback callback) {
  DVLOG(1) << "Installing pending profile: path="
           << carrier_profile_path.value()
           << ", confirmation_code=" << confirmation_code;
  if (!error_status_queue_.empty()) {
    std::move(callback).Run(error_status_queue_.front());
    error_status_queue_.pop();
    return;
  }

  Properties* euicc_properties = GetProperties(euicc_path);
  if (!PopPendingProfile(euicc_properties, carrier_profile_path)) {
    std::move(callback).Run(HermesResponseStatus::kErrorUnknown);
    return;
  }

  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(carrier_profile_path);
  profile_properties->state().ReplaceValue(hermes::profile::State::kInactive);

  std::vector<dbus::ObjectPath> installed_profiles =
      euicc_properties->installed_carrier_profiles().value();
  installed_profiles.push_back(carrier_profile_path);
  euicc_properties->installed_carrier_profiles().ReplaceValue(
      installed_profiles);
  CreateCellularService(euicc_path, carrier_profile_path);

  std::move(callback).Run(HermesResponseStatus::kSuccess);
}

void FakeHermesEuiccClient::DoRequestInstalledProfiles(
    const dbus::ObjectPath& euicc_path,
    HermesResponseCallback callback) {
  DVLOG(1) << "Installed Profiles Requested";
  if (!error_status_queue_.empty()) {
    std::move(callback).Run(error_status_queue_.front());
    error_status_queue_.pop();
    return;
  }

  auto iter = installed_profile_queue_map_.find(euicc_path);
  if (iter != installed_profile_queue_map_.end() && !iter->second->empty()) {
    InstalledProfileQueue* installed_profile_queue = iter->second.get();
    Properties* euicc_properties = GetProperties(euicc_path);
    std::vector<dbus::ObjectPath> installed_profiles =
        euicc_properties->installed_carrier_profiles().value();
    while (!installed_profile_queue->empty()) {
      installed_profiles.push_back(installed_profile_queue->front());
      installed_profile_queue->pop();
    }
    euicc_properties->installed_carrier_profiles().ReplaceValue(
        installed_profiles);
  }
  std::move(callback).Run(HermesResponseStatus::kSuccess);
}

void FakeHermesEuiccClient::DoRequestPendingProfiles(
    const dbus::ObjectPath& euicc_path,
    HermesResponseCallback callback) {
  DVLOG(1) << "Pending Profiles Requested";
  if (!error_status_queue_.empty()) {
    std::move(callback).Run(error_status_queue_.front());
    error_status_queue_.pop();
    return;
  }

  if (!pending_event_requested_) {
    AddFakeCarrierProfile(euicc_path, hermes::profile::State::kPending, "",
                          /*service_only=*/false);
    pending_event_requested_ = true;
  }
  std::move(callback).Run(HermesResponseStatus::kSuccess);
}

void FakeHermesEuiccClient::DoUninstallProfile(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& carrier_profile_path,
    HermesResponseCallback callback) {
  if (!error_status_queue_.empty()) {
    std::move(callback).Run(error_status_queue_.front());
    error_status_queue_.pop();
    return;
  }

  Properties* euicc_properties = GetProperties(euicc_path);
  std::vector<dbus::ObjectPath> installed_profiles =
      euicc_properties->installed_carrier_profiles().value();
  auto it = std::find(installed_profiles.begin(), installed_profiles.end(),
                      carrier_profile_path);
  if (it == installed_profiles.end()) {
    std::move(callback).Run(HermesResponseStatus::kErrorUnknown);
    return;
  }

  installed_profiles.erase(it);
  euicc_properties->installed_carrier_profiles().ReplaceValue(
      installed_profiles);
  std::move(callback).Run(HermesResponseStatus::kSuccess);
}

// Creates cellular service in shill for the given carrier profile path.
// This simulates the expected hermes - shill interaction when a new carrier
// profile is installed on the device through Hermes. Shill will be notified and
// it then creates cellular services with matching ICCID for this profile.
void FakeHermesEuiccClient::CreateCellularService(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& carrier_profile_path) {
  const std::string& service_path =
      profile_service_path_map_[carrier_profile_path];
  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(carrier_profile_path);
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path);
  ShillServiceClient::TestInterface* service_test =
      ShillServiceClient::Get()->GetTestInterface();
  service_test->AddService(service_path,
                           "esim_guid" + properties->iccid().value(),
                           properties->name().value(), shill::kTypeCellular,
                           shill::kStateIdle, true);
  service_test->SetServiceProperty(
      service_path, kCellularEidProperty,
      base::Value(euicc_properties->eid().value()));
  service_test->SetServiceProperty(service_path, shill::kIccidProperty,
                                   base::Value(properties->iccid().value()));
  service_test->SetServiceProperty(
      service_path, shill::kImsiProperty,
      base::Value(properties->iccid().value() + "-IMSI"));
  service_test->SetServiceProperty(
      service_path, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  service_test->SetServiceProperty(service_path, shill::kConnectableProperty,
                                   base::Value(false));
  service_test->SetServiceProperty(service_path, shill::kVisibleProperty,
                                   base::Value(true));

  ShillProfileClient::TestInterface* profile_test =
      ShillProfileClient::Get()->GetTestInterface();
  profile_test->AddService(ShillProfileClient::GetSharedProfilePath(),
                           service_path);
}

void FakeHermesEuiccClient::CallNotifyPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::NotifyPropertyChanged,
                     base::Unretained(this), object_path, property_name));
}

void FakeHermesEuiccClient::NotifyPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  DVLOG(1) << "Property changed path=" << object_path.value()
           << ", property=" << property_name;
  for (auto& observer : observers()) {
    observer.OnEuiccPropertyChanged(object_path, property_name);
  }
}

void FakeHermesEuiccClient::QueueInstalledProfile(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& profile_path) {
  auto iter = installed_profile_queue_map_.find(euicc_path);
  if (iter != installed_profile_queue_map_.end()) {
    iter->second->push(profile_path);
    return;
  }

  std::unique_ptr<InstalledProfileQueue> installed_profile_queue =
      std::make_unique<InstalledProfileQueue>();
  installed_profile_queue->push(profile_path);
  installed_profile_queue_map_[euicc_path] = std::move(installed_profile_queue);
}

}  // namespace chromeos
