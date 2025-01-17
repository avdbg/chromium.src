// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/esim_profile.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/network/cellular_esim_profile.h"
#include "chromeos/network/cellular_esim_uninstall_handler.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/services/cellular_setup/esim_manager.h"
#include "chromeos/services/cellular_setup/esim_mojo_utils.h"
#include "chromeos/services/cellular_setup/euicc.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom-shared.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"

namespace chromeos {
namespace cellular_setup {

namespace {

bool IsESimProfilePropertiesEqualToState(
    const mojom::ESimProfilePropertiesPtr& properties,
    const CellularESimProfile& esim_profile_state) {
  return esim_profile_state.iccid() == properties->iccid &&
         esim_profile_state.name() == properties->name &&
         esim_profile_state.nickname() == properties->nickname &&
         esim_profile_state.service_provider() ==
             properties->service_provider &&
         ProfileStateToMojo(esim_profile_state.state()) == properties->state &&
         esim_profile_state.activation_code() == properties->activation_code;
}

}  // namespace

ESimProfile::ESimProfile(const CellularESimProfile& esim_profile_state,
                         Euicc* euicc,
                         ESimManager* esim_manager)
    : euicc_(euicc),
      esim_manager_(esim_manager),
      properties_(mojom::ESimProfileProperties::New()),
      path_(esim_profile_state.path()) {
  UpdateProperties(esim_profile_state, /*notify=*/false);
  properties_->eid = euicc->properties()->eid;
}

ESimProfile::~ESimProfile() {
  if (install_callback_) {
    NET_LOG(ERROR) << "Profile destroyed with unfulfilled install callback";
  }
  if (uninstall_callback_) {
    NET_LOG(ERROR) << "Profile destroyed with unfulfilled uninstall callbacks";
  }
  if (set_profile_nickname_callback_) {
    NET_LOG(ERROR)
        << "Profile destroyed with unfulfilled set profile nickname callbacks";
  }
}

void ESimProfile::GetProperties(GetPropertiesCallback callback) {
  std::move(callback).Run(properties_->Clone());
}

void ESimProfile::InstallProfile(const std::string& confirmation_code,
                                 InstallProfileCallback callback) {
  if (properties_->state == mojom::ProfileState::kInstalling ||
      properties_->state != mojom::ProfileState::kPending) {
    NET_LOG(ERROR) << "Profile is already installed or in installing state.";
    std::move(callback).Run(mojom::ProfileInstallResult::kFailure);
    return;
  }

  properties_->state = mojom::ProfileState::kInstalling;
  esim_manager_->NotifyESimProfileChanged(this);

  NET_LOG(USER) << "Installing profile with path " << path().value();
  install_callback_ = std::move(callback);
  EnsureProfileExistsOnEuiccCallback perform_install_profile_callback =
      base::BindOnce(&ESimProfile::PerformInstallProfile,
                     weak_ptr_factory_.GetWeakPtr(), confirmation_code);
  esim_manager_->cellular_inhibitor()->InhibitCellularScanning(base::BindOnce(
      &ESimProfile::EnsureProfileExistsOnEuicc, weak_ptr_factory_.GetWeakPtr(),
      std::move(perform_install_profile_callback)));
}

void ESimProfile::UninstallProfile(UninstallProfileCallback callback) {
  if (!IsProfileInstalled()) {
    NET_LOG(ERROR) << "Profile uninstall failed: Profile is not installed.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  NET_LOG(USER) << "Uninstalling profile with path " << path().value();
  uninstall_callback_ = std::move(callback);
  esim_manager_->cellular_esim_uninstall_handler()->UninstallESim(
      properties_->iccid, path_, euicc_->path(),
      base::BindOnce(&ESimProfile::OnProfileUninstallResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ESimProfile::EnableProfile(EnableProfileCallback callback) {
  if (properties_->state == mojom::ProfileState::kActive ||
      properties_->state == mojom::ProfileState::kPending) {
    NET_LOG(ERROR)
        << "Profile enable failed: Profile already enabled or not installed";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  NET_LOG(USER) << "Enabling profile with path " << path().value();
  HermesProfileClient::Get()->EnableCarrierProfile(
      path_,
      base::BindOnce(&ESimProfile::OnESimOperationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ESimProfile::DisableProfile(DisableProfileCallback callback) {
  if (properties_->state == mojom::ProfileState::kInactive ||
      properties_->state == mojom::ProfileState::kPending) {
    NET_LOG(ERROR)
        << "Profile enable failed: Profile already disabled or not installed";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  NET_LOG(USER) << "Disabling profile with path " << path().value();
  HermesProfileClient::Get()->DisableCarrierProfile(
      path_,
      base::BindOnce(&ESimProfile::OnESimOperationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ESimProfile::SetProfileNickname(const base::string16& nickname,
                                     SetProfileNicknameCallback callback) {
  if (set_profile_nickname_callback_) {
    NET_LOG(ERROR) << "Set Profile Nickname already in progress.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  if (properties_->state == mojom::ProfileState::kInstalling ||
      properties_->state == mojom::ProfileState::kPending) {
    NET_LOG(ERROR) << "Set Profile Nickname failed: Profile is not installed.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  NET_LOG(USER) << "Setting profile nickname for path " << path().value();
  set_profile_nickname_callback_ = std::move(callback);
  EnsureProfileExistsOnEuiccCallback perform_set_profile_nickname_callback =
      base::BindOnce(&ESimProfile::PerformSetProfileNickname,
                     weak_ptr_factory_.GetWeakPtr(), nickname);
  esim_manager_->cellular_inhibitor()->InhibitCellularScanning(base::BindOnce(
      &ESimProfile::EnsureProfileExistsOnEuicc, weak_ptr_factory_.GetWeakPtr(),
      std::move(perform_set_profile_nickname_callback)));
}

void ESimProfile::UpdateProperties(
    const CellularESimProfile& esim_profile_state,
    bool notify) {
  if (IsESimProfilePropertiesEqualToState(properties_, esim_profile_state)) {
    return;
  }

  properties_->iccid = esim_profile_state.iccid();
  properties_->name = esim_profile_state.name();
  properties_->nickname = esim_profile_state.nickname();
  properties_->service_provider = esim_profile_state.service_provider();
  properties_->state = ProfileStateToMojo(esim_profile_state.state());
  properties_->activation_code = esim_profile_state.activation_code();
  if (notify) {
    esim_manager_->NotifyESimProfileChanged(this);
  }
}

void ESimProfile::OnProfileRemove() {
  // Run pending callbacks before profile is removed.
  if (uninstall_callback_) {
    // This profile could be removed before UninstallHandler returns. Return a
    // success since the profile will be removed.
    std::move(uninstall_callback_).Run(mojom::ESimOperationResult::kSuccess);
  }

  // Installation or setting nickname could trigger a request for profiles. If
  // this profile gets removed at that point, return the pending call with
  // failure.
  if (install_callback_) {
    std::move(install_callback_).Run(mojom::ProfileInstallResult::kFailure);
  }
  if (set_profile_nickname_callback_) {
    std::move(set_profile_nickname_callback_)
        .Run(mojom::ESimOperationResult::kFailure);
  }
}

mojo::PendingRemote<mojom::ESimProfile> ESimProfile::CreateRemote() {
  mojo::PendingRemote<mojom::ESimProfile> esim_profile_remote;
  receiver_set_.Add(this, esim_profile_remote.InitWithNewPipeAndPassReceiver());
  return esim_profile_remote;
}

void ESimProfile::EnsureProfileExistsOnEuicc(
    EnsureProfileExistsOnEuiccCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error inhibiting cellular device";
    std::move(callback).Run(/*request_profile_success=*/false,
                            /*inhibit_lock=*/nullptr);
    return;
  }

  if (!ProfileExistsOnEuicc()) {
    if (IsProfileInstalled()) {
      HermesEuiccClient::Get()->RequestInstalledProfiles(
          euicc_->path(),
          base::BindOnce(&ESimProfile::OnRequestProfiles,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         std::move(inhibit_lock)));
    } else {
      HermesEuiccClient::Get()->RequestPendingProfiles(
          euicc_->path(), /*root_smds=*/std::string(),
          base::BindOnce(&ESimProfile::OnRequestProfiles,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         std::move(inhibit_lock)));
    }
    return;
  }

  std::move(callback).Run(/*request_profile_success=*/true,
                          std::move(inhibit_lock));
}

void ESimProfile::OnRequestProfiles(
    EnsureProfileExistsOnEuiccCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Error requesting profiles to ensure profile exists on "
                      "Euicc. status="
                   << static_cast<int>(status);
    std::move(callback).Run(/*request_profile_success=*/false,
                            std::move(inhibit_lock));
    return;
  }

  // If profile does not exist on Euicc even after request for profiles then
  // return failure. The profile was removed and this object will get destroyed
  // when CellularESimProfileHandler updates.
  if (!ProfileExistsOnEuicc()) {
    NET_LOG(ERROR) << "Unable to ensure profile exists on Euicc. path="
                   << path_.value();
    std::move(callback).Run(/*request_profile_success=*/false,
                            std::move(inhibit_lock));
    return;
  }

  std::move(callback).Run(/*request_profile_success=*/true,
                          std::move(inhibit_lock));
}

void ESimProfile::PerformInstallProfile(
    const std::string& confirmation_code,
    bool request_profile_success,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!request_profile_success) {
    std::move(install_callback_).Run(mojom::ProfileInstallResult::kFailure);
    return;
  }

  HermesEuiccClient::Get()->InstallPendingProfile(
      euicc_->path(), path_, confirmation_code,
      base::BindOnce(&ESimProfile::OnPendingProfileInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(inhibit_lock)));
}

void ESimProfile::PerformSetProfileNickname(
    const base::string16& nickname,
    bool request_profile_success,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!request_profile_success) {
    std::move(set_profile_nickname_callback_)
        .Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(path_);
  properties->nick_name().Set(
      base::UTF16ToUTF8(nickname),
      base::BindOnce(&ESimProfile::OnProfileNicknameSet,
                     weak_ptr_factory_.GetWeakPtr(), std::move(inhibit_lock)));
}

void ESimProfile::OnPendingProfileInstallResult(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Error Installing pending profile status="
                   << static_cast<int>(status);
    properties_->state = mojom::ProfileState::kPending;
    esim_manager_->NotifyESimProfileChanged(this);
    std::move(install_callback_).Run(InstallResultFromStatus(status));
    return;
  }

  std::move(install_callback_).Run(mojom::ProfileInstallResult::kSuccess);
  // inhibit_lock goes out of scope and will uninhibit automatically.
}

void ESimProfile::OnProfileUninstallResult(bool success) {
  std::move(uninstall_callback_)
      .Run(success ? mojom::ESimOperationResult::kSuccess
                   : mojom::ESimOperationResult::kFailure);
}

void ESimProfile::OnESimOperationResult(ESimOperationResultCallback callback,
                                        HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "ESim operation error status="
                   << static_cast<int>(status);
  }
  std::move(callback).Run(OperationResultFromStatus(status));
}

void ESimProfile::OnProfileNicknameSet(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    bool success) {
  if (!success) {
    NET_LOG(ERROR) << "ESimProfile property set error.";
  }
  std::move(set_profile_nickname_callback_)
      .Run(success ? mojom::ESimOperationResult::kSuccess
                   : mojom::ESimOperationResult::kFailure);
  // inhibit_lock goes out of scope and will uninhibit automatically.
}

bool ESimProfile::ProfileExistsOnEuicc() {
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_->path());
  const std::vector<dbus::ObjectPath>& profile_paths =
      IsProfileInstalled()
          ? euicc_properties->installed_carrier_profiles().value()
          : euicc_properties->pending_carrier_profiles().value();

  auto iter = std::find(profile_paths.begin(), profile_paths.end(), path_);
  return iter != profile_paths.end();
}

bool ESimProfile::IsProfileInstalled() {
  return properties_->state != mojom::ProfileState::kPending &&
         properties_->state != mojom::ProfileState::kInstalling;
}

}  // namespace cellular_setup
}  // namespace chromeos
