// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_STATS_REPORTING_CONTROLLER_H_
#define CHROME_BROWSER_ASH_SETTINGS_STATS_REPORTING_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "components/ownership/owner_settings_service.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace ownership {
class OwnerSettingsService;
}

namespace chromeos {

// An extra layer on top of CrosSettings / OwnerSettingsService that allows for
// writing a setting before ownership is taken, for one setting only:
// kStatsReportingPref, which has the key: "cros.metrics.reportingEnabled".
//
// Ordinarily, the OwnerSettingsService interface is used for writing settings,
// and the CrosSettings interface is used for reading them - but as the OSS
// cannot be used until the device has an owner, this class can be used instead,
// since writing the new value with SetEnabled works even before ownership is
// taken.
//
// If OSS is ready then the new value is written straight away, and if not, then
// a pending write is queued that is completed as soon as the OSS is ready.
// This write will complete even if Chrome is restarted in the meantime.
// The caller need not care whether the write was immediate or pending, as long
// as they also use this class to read the value of kStatsReportingPref.
// IsEnabled will return the pending value until ownership is taken and the
// pending value is written - from then on it will return the signed, stored
// value from CrosSettings.
class StatsReportingController
    : public ownership::OwnerSettingsService::Observer {
 public:
  // Manage singleton instance.
  static void Initialize(PrefService* local_state);
  static bool IsInitialized();
  static void Shutdown();
  static StatsReportingController* Get();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Store the new value of |enabled|. This will happen straight away if
  // |profile| is the owner, and it will cause a pending write to be buffered
  // and written later if the device has no owner yet. It will fail if the
  // device already has an owner, and |profile| is not that owner.
  void SetEnabled(Profile* profile, bool enabled);

  // Returns the latest value of enabled - regardless of whether this has been
  // successfully signed and persisted, or if it is still stored as a pending
  // write.
  bool IsEnabled();

  // Add an observer |callback| for changes to the setting.
  base::CallbackListSubscription AddObserver(
      const base::RepeatingClosure& callback) WARN_UNUSED_RESULT;

  // Called once ownership is taken, |service| is the service of the user taking
  // ownership.
  void OnOwnershipTaken(ownership::OwnerSettingsService* service);

  // ownership::OwnerSettingsService::Observer implementation:
  void OnSignedPolicyStored(bool success) override;

  // Sets the callback which is called once when the |enabled| value is
  // propagated to the device settings. Support only one callback at a time.
  // CHECKs if the second callback is being set.
  // It's different from the |AddObserver| API. Observers are called
  // immediately after |SetEnabled| is called with the different |enabled|
  // setting.
  void SetOnDeviceSettingsStoredCallBack(base::OnceClosure callback);

 private:
  friend class StatsReportingControllerTest;

  explicit StatsReportingController(PrefService* local_state);
  ~StatsReportingController() override;

  // Delegates immediately to SetWithService if |service| is ready, otherwise
  // runs SetWithService asynchronously once |service| is ready.
  void SetWithServiceAsync(ownership::OwnerSettingsService* service,
                           bool enabled);

  // Callback used by SetWithServiceAsync.
  void SetWithServiceCallback(
      const base::WeakPtr<ownership::OwnerSettingsService>& service,
      bool enabled,
      bool is_owner);

  // Uses |service| to write the latest value, as long as |service| belongs
  // to the owner - otherwise just prints a warning.
  void SetWithService(ownership::OwnerSettingsService* service, bool enabled);

  // Notifies observers if the value has changed.
  void NotifyObservers();

  // Gets the current ownership status - owned, unowned, or unknown.
  DeviceSettingsService::OwnershipStatus GetOwnershipStatus();

  // Get the owner-settings service for a particular profile. A variety of
  // different results can be returned, depending on the profile.
  // a) A ready-to-use service that we know belongs to the owner.
  // b) A ready-to-use service that we know does NOT belong to the owner.
  // c) A service that is NOT ready-to-use, which MIGHT belong to the owner.
  // d) nullptr (for instance, if |profile| is a guest).
  ownership::OwnerSettingsService* GetOwnerSettingsService(Profile* profile);

  // Sets |*value| to the value waiting to be written (stored in local_state),
  // if one exists. Returns false if there is no such value.
  bool GetPendingValue(bool* value);

  // Sets |*value| to the value signed and stored in CrosSettings, if one
  // exists. Returns false if there is no such value.
  bool GetSignedStoredValue(bool* value);

  // Clears any value waiting to be written (from storage in local state).
  void ClearPendingValue();

  base::WeakPtr<StatsReportingController> as_weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  PrefService* local_state_;
  bool value_notified_to_observers_;
  base::CallbackList<void(void)> callback_list_;
  base::CallbackListSubscription setting_subscription_;

  // Indicates if the setting value is in the process of being set with the
  // service. There is a small period of time needed between start saving the
  // value and before the value is stored correctly in the service. We should
  // not use the setting value from the service if it is still in the process
  // of being saved.
  bool is_value_being_set_with_service_ = false;

  base::ScopedObservation<ownership::OwnerSettingsService,
                          ownership::OwnerSettingsService::Observer>
      owner_settings_service_observation_{this};

  base::OnceClosure on_device_settings_stored_callback_;

  base::WeakPtrFactory<StatsReportingController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StatsReportingController);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::StatsReportingController;
}

#endif  // CHROME_BROWSER_ASH_SETTINGS_STATS_REPORTING_CONTROLLER_H_
