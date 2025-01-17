// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_restore_util.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_util.h"
#include "base/auto_reset.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace desks_restore_util {

namespace {

// A key for the dictionaries stored in |kDesksMetricsList|. The entry is an int
// which represents the number of minutes for
// base::Time::FromDeltaSinceWindowsEpoch().
constexpr char kCreationTimeKey[] = "creation_time";

// Keys for the dictionaries stored in |kDesksMetricsList|. The entries are ints
// which represent the number of days for
// base::Time::FromDeltaSinceWindowsEpoch().
constexpr char kFirstDayVisitedKey[] = "first_day";
constexpr char kLastDayVisitedKey[] = "last_day";

// While restore is in progress, changes are being made to the desks and their
// names. Those changes should not trigger an update to the prefs.
bool g_pause_desks_prefs_updates = false;

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

// Check if the desk index is valid against a list of existing desks in
// DesksController.
bool IsValidDeskIndex(int desk_index) {
  return desk_index >= 0 &&
         desk_index < int{DesksController::Get()->desks().size()} &&
         desk_index < int{desks_util::GetMaxNumberOfDesks()};
}

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  constexpr int kDefaultActiveDeskIndex = 0;
  registry->RegisterListPref(prefs::kDesksNamesList);
  registry->RegisterListPref(prefs::kDesksMetricsList);
  if (features::IsBentoEnabled()) {
    registry->RegisterIntegerPref(prefs::kDesksActiveDesk,
                                  kDefaultActiveDeskIndex);
  }
}

void RestorePrimaryUserDesks() {
  base::AutoReset<bool> in_progress(&g_pause_desks_prefs_updates, true);

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  const base::ListValue* desks_names =
      primary_user_prefs->GetList(prefs::kDesksNamesList);
  const base::ListValue* desks_metrics =
      primary_user_prefs->GetList(prefs::kDesksMetricsList);

  // First create the same number of desks.
  const size_t restore_size = desks_names->GetSize();

  // If we don't have any restore data, or the list is corrupt for some reason,
  // abort.
  if (!restore_size || restore_size > desks_util::GetMaxNumberOfDesks())
    return;

  auto* desks_controller = DesksController::Get();
  while (desks_controller->desks().size() < restore_size)
    desks_controller->NewDesk(DesksCreationRemovalSource::kDesksRestore);

  const auto& desks_names_list = desks_names->GetList();
  const auto& desks_metrics_list = desks_metrics->GetList();
  const size_t desks_metrics_list_size = desks_metrics->GetSize();
  const auto now = base::Time::Now();
  for (size_t index = 0; index < restore_size; ++index) {
    const std::string& desk_name = desks_names_list[index].GetString();
    // Empty desks names are just place holders for desks whose names haven't
    // been modified by the user. Those don't need to be restored; they already
    // have the correct default names based on their positions in the desks
    // list.
    if (!desk_name.empty()) {
      desks_controller->RestoreNameOfDeskAtIndex(base::UTF8ToUTF16(desk_name),
                                                 index);
    }

    // Only restore metrics if there is existing data.
    if (index >= desks_metrics_list_size)
      continue;

    const auto& desks_metrics_dict = desks_metrics_list[index];

    // Restore creation time.
    const auto& creation_time_entry =
        desks_metrics_dict.FindIntPath(kCreationTimeKey);
    if (creation_time_entry.has_value()) {
      const auto creation_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMinutes(*creation_time_entry));
      if (!creation_time.is_null() && creation_time < now)
        desks_controller->RestoreCreationTimeOfDeskAtIndex(creation_time,
                                                           index);
    }

    // Restore consecutive daily metrics.
    const auto& first_day_visited_entry =
        desks_metrics_dict.FindIntPath(kFirstDayVisitedKey);
    const int first_day_visited = first_day_visited_entry.value_or(-1);

    const auto& last_day_visited_entry =
        desks_metrics_dict.FindIntPath(kLastDayVisitedKey);
    const int last_day_visited = last_day_visited_entry.value_or(-1);

    if (first_day_visited <= last_day_visited && first_day_visited != -1 &&
        last_day_visited != -1) {
      // Only restore the values if they haven't been corrupted.
      desks_controller->RestoreVisitedMetricsOfDeskAtIndex(
          first_day_visited, last_day_visited, index);
    }
  }

  // Restore an active desk for the primary user.
  if (features::IsBentoEnabled()) {
    const int active_desk_index =
        primary_user_prefs->GetInteger(prefs::kDesksActiveDesk);

    // A crash in between prefs::kDesksNamesList and prefs::kDesksActiveDesk
    // can cause an invalid active desk index.
    if (!IsValidDeskIndex(active_desk_index))
      return;

    desks_controller->RestorePrimaryUserActiveDeskIndex(active_desk_index);
  }
}

void UpdatePrimaryUserDeskNamesPrefs() {
  if (g_pause_desks_prefs_updates)
    return;

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  ListPrefUpdate name_update(primary_user_prefs, prefs::kDesksNamesList);
  base::ListValue* name_pref_data = name_update.Get();
  name_pref_data->Clear();

  const auto& desks = DesksController::Get()->desks();
  for (const auto& desk : desks) {
    // Desks whose names were not changed by the user, are stored as empty
    // strings. They're just place holders to restore the correct desks count.
    // RestorePrimaryUserDesks() restores only non-empty desks names.
    name_pref_data->Append(desk->is_name_set_by_user()
                               ? base::UTF16ToUTF8(desk->name())
                               : std::string());
  }

  DCHECK_EQ(name_pref_data->GetSize(), desks.size());
}

void UpdatePrimaryUserDeskMetricsPrefs() {
  if (g_pause_desks_prefs_updates)
    return;

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  ListPrefUpdate metrics_update(primary_user_prefs, prefs::kDesksMetricsList);
  base::ListValue* metrics_pref_data = metrics_update.Get();
  metrics_pref_data->Clear();

  const auto& desks = DesksController::Get()->desks();
  for (const auto& desk : desks) {
    base::DictionaryValue metrics_dict;
    metrics_dict.SetInteger(
        kCreationTimeKey,
        desk->creation_time().ToDeltaSinceWindowsEpoch().InMinutes());
    metrics_dict.SetInteger(kFirstDayVisitedKey, desk->first_day_visited());
    metrics_dict.SetInteger(kLastDayVisitedKey, desk->last_day_visited());
    metrics_pref_data->Append(std::move(metrics_dict));
  }

  DCHECK_EQ(metrics_pref_data->GetSize(), desks.size());
}

void UpdatePrimaryUserActiveDeskPrefs(int active_desk_index) {
  DCHECK(features::IsBentoEnabled());
  DCHECK(IsValidDeskIndex(active_desk_index));
  if (g_pause_desks_prefs_updates)
    return;

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  primary_user_prefs->SetInteger(prefs::kDesksActiveDesk, active_desk_index);
}

}  // namespace desks_restore_util

}  // namespace ash
