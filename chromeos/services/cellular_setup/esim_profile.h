// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_PROFILE_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_PROFILE_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class CellularESimProfile;

namespace cellular_setup {

class Euicc;
class ESimManager;

// Implementation of mojom::ESimProfile. This class represents an
// eSIM profile installed on an EUICC.
class ESimProfile : public mojom::ESimProfile {
 public:
  ESimProfile(const CellularESimProfile& esim_profile_state,
              Euicc* euicc,
              ESimManager* esim_manager);
  ESimProfile(const ESimProfile&) = delete;
  ESimProfile& operator=(const ESimProfile&) = delete;
  ~ESimProfile() override;

  // mojom::ESimProfile:
  void GetProperties(GetPropertiesCallback callback) override;
  void InstallProfile(const std::string& confirmation_code,
                      InstallProfileCallback callback) override;
  void UninstallProfile(UninstallProfileCallback callback) override;
  void EnableProfile(EnableProfileCallback callback) override;
  void DisableProfile(DisableProfileCallback callback) override;
  void SetProfileNickname(const base::string16& nickname,
                          SetProfileNicknameCallback callback) override;

  // Update properties for this ESimProfile from D-Bus.
  void UpdateProperties(const CellularESimProfile& esim_profile_state,
                        bool notify);

  // Called before profile is removed from the euicc.
  void OnProfileRemove();

  // Returns a new pending remote attached to this instance.
  mojo::PendingRemote<mojom::ESimProfile> CreateRemote();

  const dbus::ObjectPath& path() { return path_; }
  const mojom::ESimProfilePropertiesPtr& properties() { return properties_; }

 private:
  // Type of callback for profile installation methods.
  using ProfileInstallResultCallback =
      base::OnceCallback<void(mojom::ProfileInstallResult)>;

  // Type of callback for other esim manager methods.
  using ESimOperationResultCallback =
      base::OnceCallback<void(mojom::ESimOperationResult)>;

  // Type of callback to be passed to EnsureProfileExists method. The callback
  // receives a boolean indicating request profile succeess status and inhibit
  // lock that was passed to the method.
  using EnsureProfileExistsOnEuiccCallback =
      base::OnceCallback<void(bool,
                              std::unique_ptr<CellularInhibitor::InhibitLock>)>;

  void EnsureProfileExistsOnEuicc(
      EnsureProfileExistsOnEuiccCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnRequestProfiles(
      EnsureProfileExistsOnEuiccCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      HermesResponseStatus status);
  void PerformInstallProfile(
      const std::string& confirmation_code,
      bool request_profile_success,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void PerformSetProfileNickname(
      const base::string16& nickname,
      bool request_profile_success,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnPendingProfileInstallResult(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      HermesResponseStatus status);
  void OnProfileUninstallResult(bool success);
  void OnESimOperationResult(ESimOperationResultCallback callback,
                             HermesResponseStatus status);
  void OnProfileNicknameSet(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      bool success);
  bool ProfileExistsOnEuicc();
  bool IsProfileInstalled();

  // Reference to Euicc that owns this profile.
  Euicc* euicc_;
  // Reference to ESimManager that owns Euicc of this profile.
  ESimManager* esim_manager_;
  UninstallProfileCallback uninstall_callback_;
  SetProfileNicknameCallback set_profile_nickname_callback_;
  InstallProfileCallback install_callback_;
  mojo::ReceiverSet<mojom::ESimProfile> receiver_set_;
  mojom::ESimProfilePropertiesPtr properties_;
  dbus::ObjectPath path_;
  base::WeakPtrFactory<ESimProfile> weak_ptr_factory_{this};
};

}  // namespace cellular_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_MANAGER_H_