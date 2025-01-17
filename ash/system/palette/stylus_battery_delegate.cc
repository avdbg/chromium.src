// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/stylus_battery_delegate.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/power/peripheral_battery_listener.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/tray_constants.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {
namespace {
// Battery percentage threshold used to label the battery level as Low.
constexpr int kStylusLowBatteryThreshold = 24;
constexpr base::TimeDelta kStylusBatteryStatusStaleThreshold =
    base::TimeDelta::FromDays(14);
}  // namespace

StylusBatteryDelegate::StylusBatteryDelegate() {
  if (Shell::Get()->peripheral_battery_listener())
    battery_observation_.Observe(Shell::Get()->peripheral_battery_listener());
}

StylusBatteryDelegate::~StylusBatteryDelegate() = default;

SkColor StylusBatteryDelegate::GetColorForBatteryLevel() const {
  if (battery_level_ <= kStylusLowBatteryThreshold && !IsBatteryCharging()) {
    return AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorAlert);
  }
  return AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
}

gfx::ImageSkia StylusBatteryDelegate::GetBatteryImage() const {
  PowerStatus::BatteryImageInfo info;
  info.charge_percent = battery_level_.value_or(0);

  if (IsBatteryCharging())
    info.icon_badge = &kUnifiedMenuBatteryBoltIcon;

  const SkColor icon_fg_color = GetColorForBatteryLevel();
  const SkColor icon_bg_color = AshColorProvider::Get()->GetBackgroundColor();

  return PowerStatus::GetBatteryImage(info, kUnifiedTrayIconSize, icon_bg_color,
                                      icon_fg_color);
}

gfx::ImageSkia StylusBatteryDelegate::GetBatteryStatusUnknownImage() const {
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);

  return gfx::CreateVectorIcon(kStylusBatteryStatusUnknownIcon, icon_color);
}

void StylusBatteryDelegate::SetBatteryUpdateCallback(
    Callback battery_update_callback) {
  battery_update_callback_ = std::move(battery_update_callback);
}

bool StylusBatteryDelegate::IsBatteryCharging() const {
  return battery_charge_status_ ==
             PeripheralBatteryListener::BatteryInfo::ChargeStatus::kCharging ||
         battery_charge_status_ ==
             PeripheralBatteryListener::BatteryInfo::ChargeStatus::kFull;
}

bool StylusBatteryDelegate::IsBatteryLevelLow() const {
  return battery_level_ <= kStylusLowBatteryThreshold;
}

bool StylusBatteryDelegate::ShouldShowBatteryStatus() const {
  return last_update_timestamp_.has_value();
}

bool StylusBatteryDelegate::IsBatteryStatusStale() const {
  if (!last_update_timestamp_.has_value())
    return false;

  return (base::TimeTicks::Now() - last_update_timestamp_.value()) >
         kStylusBatteryStatusStaleThreshold;
}

bool StylusBatteryDelegate::IsBatteryInfoValid(
    const PeripheralBatteryListener::BatteryInfo& battery) const {
  if (battery.type != PeripheralBatteryListener::BatteryInfo::PeripheralType::
                          kStylusViaCharger &&
      battery.type != PeripheralBatteryListener::BatteryInfo::PeripheralType::
                          kStylusViaScreen) {
    return false;
  }

  if (!battery.last_active_update_timestamp.has_value() ||
      !battery.level.has_value()) {
    return false;
  }

  if (last_update_timestamp_.has_value() &&
      battery.last_active_update_timestamp < last_update_timestamp_) {
    return false;
  }

  return true;
}

void StylusBatteryDelegate::OnAddingBattery(
    const PeripheralBatteryListener::BatteryInfo& battery) {}

void StylusBatteryDelegate::OnRemovingBattery(
    const PeripheralBatteryListener::BatteryInfo& battery) {}

void StylusBatteryDelegate::OnUpdatedBatteryLevel(
    const PeripheralBatteryListener::BatteryInfo& battery) {
  if (!IsBatteryInfoValid(battery))
    return;

  battery_level_ = battery.level;
  battery_charge_status_ = battery.charge_status;
  last_update_timestamp_ = battery.last_active_update_timestamp;
  if (battery_update_callback_)
    battery_update_callback_.Run();
}

}  // namespace ash
