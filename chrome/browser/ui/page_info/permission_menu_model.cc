// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/permission_menu_model.h"

#include <tuple>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "components/permissions/features.h"
#include "components/strings/grit/components_strings.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr int kCommandIdOneTimeFlag = (1 << 5);
constexpr int kCommandIdContentSettingMask = kCommandIdOneTimeFlag - 1;

static_assert((CONTENT_SETTING_NUM_SETTINGS - 1) <=
                  kCommandIdContentSettingMask,
              "Content settings do not fit in the mask.");

int EncodeCommandId(ContentSetting setting, bool is_one_time) {
  if (is_one_time)
    return setting | kCommandIdOneTimeFlag;
  return setting;
}

std::tuple<ContentSetting, bool> DecodeCommandId(int encoded_command_id) {
  const ContentSetting decoded_setting = static_cast<ContentSetting>(
      encoded_command_id & kCommandIdContentSettingMask);
  const bool is_one_time =
      ((encoded_command_id & kCommandIdOneTimeFlag) == kCommandIdOneTimeFlag);
  return std::make_tuple(decoded_setting, is_one_time);
}
}  // namespace

PermissionMenuModel::PermissionMenuModel(Profile* profile,
                                         const GURL& url,
                                         const PageInfo::PermissionInfo& info,
                                         ChangeCallback callback)
    : ui::SimpleMenuModel(this),
      host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(profile)),
      permission_(info),
      callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
  base::string16 label;

  DCHECK_NE(permission_.default_setting, CONTENT_SETTING_NUM_SETTINGS);

  // The Material UI for site settings uses comboboxes instead of menubuttons,
  // which means the elements of the menu themselves have to be shorter, instead
  // of simply setting a shorter label on the menubutton.
  auto delegate = ChromePageInfoUiDelegate(profile);
  label = PageInfoUI::PermissionActionToUIString(
      &delegate, permission_.type, CONTENT_SETTING_DEFAULT,
      permission_.default_setting, permission_.source, /*is_one_time=*/false);

  AddCheckItem(EncodeCommandId(CONTENT_SETTING_DEFAULT, /*is_one_time=*/false),
               label);

  // Retrieve the string to show for allowing the permission.
  if (ShouldShowAllow(url)) {
    label = PageInfoUI::PermissionActionToUIString(
        &delegate, permission_.type, CONTENT_SETTING_ALLOW,
        permission_.default_setting, permission_.source, /*is_one_time=*/false);
    AddCheckItem(EncodeCommandId(CONTENT_SETTING_ALLOW, /*is_one_time=*/false),
                 label);
  }

  if (base::FeatureList::IsEnabled(
          permissions::features::kOneTimeGeolocationPermission) &&
      info.type == ContentSettingsType::GEOLOCATION) {
    label = PageInfoUI::PermissionActionToUIString(
        &delegate, permission_.type, CONTENT_SETTING_ALLOW,
        permission_.default_setting, permission_.source, /*is_one_time=*/true);
    AddCheckItem(EncodeCommandId(CONTENT_SETTING_ALLOW, /*is_one_time=*/true),
                 label);
  }

  // Retrieve the string to show for blocking the permission.
  label = PageInfoUI::PermissionActionToUIString(
      &delegate, info.type, CONTENT_SETTING_BLOCK, permission_.default_setting,
      info.source, /*is_one_time=*/false);
  AddCheckItem(EncodeCommandId(CONTENT_SETTING_BLOCK, /*is_one_time=*/false),
               label);

  // Retrieve the string to show for allowing the user to be asked about the
  // permission.
  if (ShouldShowAsk(url)) {
    label = PageInfoUI::PermissionActionToUIString(
        &delegate, info.type, CONTENT_SETTING_ASK, permission_.default_setting,
        info.source, /*is_one_time=*/false);
    AddCheckItem(EncodeCommandId(CONTENT_SETTING_ASK, /*is_one_time=*/false),
                 label);
  }
}

PermissionMenuModel::~PermissionMenuModel() {}

bool PermissionMenuModel::IsCommandIdChecked(int encoded_command_id) const {
  return DecodeCommandId(encoded_command_id) ==
         std::tie(permission_.setting, permission_.is_one_time);
}

bool PermissionMenuModel::IsCommandIdEnabled(int encoded_command_id) const {
  return true;
}

void PermissionMenuModel::ExecuteCommand(int encoded_command_id,
                                         int event_flags) {
  std::tie(permission_.setting, permission_.is_one_time) =
      DecodeCommandId(encoded_command_id);
  callback_.Run(permission_);
}

bool PermissionMenuModel::ShouldShowAllow(const GURL& url) {
  switch (permission_.type) {
    // Notifications and idle detection do not support CONTENT_SETTING_ALLOW in
    // incognito.
    case ContentSettingsType::NOTIFICATIONS:
    case ContentSettingsType::IDLE_DETECTION:
      return !permission_.is_incognito;
    // Media only supports CONTENT_SETTING_ALLOW for secure origins.
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return network::IsUrlPotentiallyTrustworthy(url);
    // Chooser permissions do not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::SERIAL_GUARD:
    case ContentSettingsType::USB_GUARD:
    case ContentSettingsType::BLUETOOTH_GUARD:
    case ContentSettingsType::HID_GUARD:
    // Bluetooth scanning does not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::BLUETOOTH_SCANNING:
    // File system write does not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      return false;
    default:
      return true;
  }
}

bool PermissionMenuModel::ShouldShowAsk(const GURL& url) {
  switch (permission_.type) {
    case ContentSettingsType::USB_GUARD:
    case ContentSettingsType::SERIAL_GUARD:
    case ContentSettingsType::BLUETOOTH_GUARD:
    case ContentSettingsType::BLUETOOTH_SCANNING:
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
    case ContentSettingsType::HID_GUARD:
      return true;
    default:
      return false;
  }
}
