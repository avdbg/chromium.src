// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/external_web_app_utils.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/gfx/codec/png_codec.h"

namespace web_app {

namespace {

// kAppUrl is a required string specifying a URL inside the scope of the web
// app that contains a link to the app manifest.
constexpr char kAppUrl[] = "app_url";

// kHideFromUser is an optional boolean which controls whether we add
// a shortcut to the relevant OS surface i.e. Application folder on macOS, Start
// Menu on Windows and Linux, and launcher on Chrome OS. Defaults to false if
// missing. If true, we also don't show the app in search or in app management
// on Chrome OS.
constexpr char kHideFromUser[] = "hide_from_user";

// kOnlyForNewUsers is an optional boolean. If set and true we will not install
// the app for users that have already run Chrome before.
constexpr char kOnlyForNewUsers[] = "only_for_new_users";

// kUserType is an allowlist of user types to install this app for. This must be
// populated otherwise the app won't ever be installed.
// Example: "user_type": ["unmanaged", "managed", "child"]
// See apps::DetermineUserType() for relevant string constants.
constexpr char kUserType[] = "user_type";

// kCreateShortcuts is an optional boolean which controls whether OS
// level shortcuts are created. On Chrome OS this controls whether the app is
// pinned to the shelf.
// The default value of kCreateShortcuts if false.
constexpr char kCreateShortcuts[] = "create_shortcuts";

// kFeatureName is an optional string parameter specifying a feature
// associated with this app. The feature must be present in
// |kExternalAppInstallFeatures| to be applicable.
// If specified:
//  - if the feature is enabled, the app will be installed
//  - if the feature is not enabled, the app will be removed.
constexpr char kFeatureName[] = "feature_name";

// kDisableIfArcSupported is an optional bool which specifies whether to skip
// install of the app if the device supports Arc (Chrome OS only).
// Defaults to false.
constexpr char kDisableIfArcSupported[] = "disable_if_arc_supported";

// kDisableIfTabletFormFactor is an optional bool which specifies whether to
// skip install of the app if the device is a tablet form factor.
// This is only for Chrome OS tablets, Android does not use any of this code.
// Defaults to false.
constexpr char kDisableIfTabletFormFactor[] = "disable_if_tablet_form_factor";

// kLaunchContainer is a required string which can be "window" or "tab"
// and controls what sort of container the web app is launched in.
constexpr char kLaunchContainer[] = "launch_container";
constexpr char kLaunchContainerTab[] = "tab";
constexpr char kLaunchContainerWindow[] = "window";

// kLaunchQueryParams is an optional string which specifies query parameters to
// add to the start_url when launching the app. If the provided params are a
// substring of start_url's existing params then it will not be added a second
// time.
// Note that substring matches include "param=a" matching in "some_param=abc".
// Extend the implementation in AppRegistrar::GetAppLaunchUrl() if this edge
// case needs to be handled differently.
constexpr char kLaunchQueryParams[] = "launch_query_params";

// kLoadAndAwaitServiceWorkerRegistration is an optional bool that specifies
// whether to fetch the |kServiceWorkerRegistrationUrl| after installation to
// allow time for the app to register its service worker. This is done as a
// second pass after install in order to not block the installation of other
// background installed apps. No fetch is made if the service worker has already
// been registered by the |kAppUrl|.
// Defaults to true.
constexpr char kLoadAndAwaitServiceWorkerRegistration[] =
    "load_and_await_service_worker_registration";

// kServiceWorkerRegistrationUrl is an optional string specifying the URL to use
// for the above |kLoadAndAwaitServiceWorkerRegistration|.
// Defaults to the |kAppUrl|.
constexpr char kServiceWorkerRegistrationUrl[] =
    "service_worker_registration_url";

// kUninstallAndReplace is an optional array of strings which specifies App IDs
// which the app is replacing. This will transfer OS attributes (e.g the source
// app's shelf and app list positions on ChromeOS) and then uninstall the source
// app.
constexpr char kUninstallAndReplace[] = "uninstall_and_replace";

// kOnlyUseOfflineManifest is an optional bool.
// If set to true then no network install will be attempted and the app will be
// installed using |kOfflineManifest| data. |kOfflineManifest| must be specified
// in this case.
// If set to false and |kOfflineManifest| is set then it will be used as a
// fallback manifest if the network install fails.
// Defaults to false.
constexpr char kOnlyUseOfflineManifest[] = "only_use_offline_manifest";

// kOfflineManifest is a dictionary of manifest field values to use as an
// install to avoid the expense of fetching the install URL to download the
// app's true manifest. Next time the user visits the app it will undergo a
// manifest update check and correct any differences from the site (except for
// name and start_url).
//
// Why not use blink::ManifestParser?
// blink::ManifestParser depends on substantial sections of the CSS parser which
// is infeasible to run outside of the renderer process.
constexpr char kOfflineManifest[] = "offline_manifest";

// "name" manifest value to use for offline install. Cannot be updated.
// TODO(crbug.com/1119699): Allow updating of name.
constexpr char kOfflineManifestName[] = "name";

// "start_url" manifest value to use for offline install. Cannot be updated.
// TODO(crbug.com/1119699): Allow updating of start_url.
constexpr char kOfflineManifestStartUrl[] = "start_url";

// "scope" manifest value to use for offline install.
constexpr char kOfflineManifestScope[] = "scope";

// "display" manifest value to use for offline install.
constexpr char kOfflineManifestDisplay[] = "display";

// List of PNG files in the default web app config directory to use as the
// icons for offline install. Will be installed with purpose "any".
constexpr char kOfflineManifestIconAnyPngs[] = "icon_any_pngs";

// Optional 8 value ARGB hex code to use as the "theme_color" manifest value.
// Example:
//   "theme_color_argb_hex": "FFFF0000"
// is equivalent to
//   "theme_color": "red"
constexpr char kOfflineManifestThemeColorArgbHex[] = "theme_color_argb_hex";

// Contains numeric milestone number M like 89 (the Chrome version). The app
// gets updated if browser's binary milestone number goes from <M to >=M.
constexpr char kForceReinstallForMilestone[] = "force_reinstall_for_milestone";

}  // namespace

OptionsOrError ParseConfig(FileUtilsWrapper& file_utils,
                           const base::FilePath& dir,
                           const base::FilePath& file,
                           const base::Value& app_config) {
  ExternalInstallOptions options(GURL(), DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalDefault);
  options.require_manifest = true;

  if (app_config.type() != base::Value::Type::DICTIONARY) {
    return base::StrCat(
        {file.AsUTF8Unsafe(), " was not a dictionary as the top level"});
  }

  // user_type
  const base::Value* value = app_config.FindListKey(kUserType);
  if (!value) {
    return base::StrCat({file.AsUTF8Unsafe(), " missing ", kUserType});
  }
  for (const auto& item : value->GetList()) {
    if (!item.is_string()) {
      return base::StrCat({file.AsUTF8Unsafe(), " has invalid ", kUserType,
                           item.DebugString()});
    }
    options.user_type_allowlist.push_back(item.GetString());
  }
  if (options.user_type_allowlist.empty()) {
    return base::StrCat({file.AsUTF8Unsafe(), " has empty ", kUserType});
  }

  // feature_name
  const std::string* feature_name = app_config.FindStringKey(kFeatureName);
  if (feature_name)
    options.gate_on_feature = *feature_name;

  // app_url
  value = app_config.FindKeyOfType(kAppUrl, base::Value::Type::STRING);
  if (!value) {
    return base::StrCat({file.AsUTF8Unsafe(), " had a missing ", kAppUrl});
  }
  options.install_url = GURL(value->GetString());
  if (!options.install_url.is_valid()) {
    return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ", kAppUrl});
  }

  // only_for_new_users
  value = app_config.FindKey(kOnlyForNewUsers);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kOnlyForNewUsers});
    }
    options.only_for_new_users = value->GetBool();
  }

  // hide_from_user
  bool hide_from_user = false;
  value = app_config.FindKey(kHideFromUser);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kHideFromUser});
    }
    hide_from_user = value->GetBool();
  }
  options.add_to_applications_menu = !hide_from_user;
  options.add_to_search = !hide_from_user;
  options.add_to_management = !hide_from_user;

  // create_shortcuts
  bool create_shortcuts = false;
  value = app_config.FindKey(kCreateShortcuts);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kCreateShortcuts});
    }
    create_shortcuts = value->GetBool();
  }
  options.add_to_desktop = create_shortcuts;
  options.add_to_quick_launch_bar = create_shortcuts;

  // It doesn't make sense to hide the app and also create shortcuts for it.
  DCHECK(!(hide_from_user && create_shortcuts));

  // disable_if_arc_supported
  value = app_config.FindKey(kDisableIfArcSupported);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kDisableIfArcSupported});
    }
    options.disable_if_arc_supported = value->GetBool();
  }

  // disable_if_tablet_form_factor
  value = app_config.FindKey(kDisableIfTabletFormFactor);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kDisableIfTabletFormFactor});
    }
    options.disable_if_tablet_form_factor = value->GetBool();
  }

  // launch_container
  value = app_config.FindKeyOfType(kLaunchContainer, base::Value::Type::STRING);
  if (!value) {
    return base::StrCat(
        {file.AsUTF8Unsafe(), " had an invalid ", kLaunchContainer});
  }
  std::string launch_container_str = value->GetString();
  if (launch_container_str == kLaunchContainerTab) {
    options.user_display_mode = DisplayMode::kBrowser;
  } else if (launch_container_str == kLaunchContainerWindow) {
    options.user_display_mode = DisplayMode::kStandalone;
  } else {
    return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                         kLaunchContainer, ": ", launch_container_str});
  }

  // launch_query_params
  value = app_config.FindKey(kLaunchQueryParams);
  if (value) {
    if (!value->is_string()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kLaunchQueryParams});
    }
    options.launch_query_params = value->GetString();
  }

  // load_and_await_service_worker_registration
  value = app_config.FindKey(kLoadAndAwaitServiceWorkerRegistration);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kLoadAndAwaitServiceWorkerRegistration});
    }
    options.load_and_await_service_worker_registration = value->GetBool();
  }

  // service_worker_registration_url
  value = app_config.FindKey(kServiceWorkerRegistrationUrl);
  if (value) {
    if (!options.load_and_await_service_worker_registration) {
      return base::StrCat({file.AsUTF8Unsafe(), " should not specify a ",
                           kServiceWorkerRegistrationUrl, " while ",
                           kLoadAndAwaitServiceWorkerRegistration,
                           " is disabled"});
    }
    if (!value->is_string()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kServiceWorkerRegistrationUrl});
    }
    options.service_worker_registration_url.emplace(value->GetString());
    if (!options.service_worker_registration_url->is_valid()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kServiceWorkerRegistrationUrl});
    }
  }

  // uninstall_and_replace
  value = app_config.FindKey(kUninstallAndReplace);
  if (value) {
    if (!value->is_list()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kUninstallAndReplace});
    }
    base::Value::ConstListView uninstall_and_replace_values = value->GetList();

    for (const auto& app_id_value : uninstall_and_replace_values) {
      if (!app_id_value.is_string()) {
        return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                             kUninstallAndReplace, " entry"});
      }
      options.uninstall_and_replace.push_back(app_id_value.GetString());
    }
  }

  // only_use_offline_manifest
  value = app_config.FindKey(kOnlyUseOfflineManifest);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kOnlyUseOfflineManifest});
    }
    options.only_use_app_info_factory = value->GetBool();
  }

  // offline_manifest
  value = app_config.FindDictKey(kOfflineManifest);
  if (value) {
    WebApplicationInfoFactoryOrError offline_manifest_result =
        ParseOfflineManifest(file_utils, dir, file, *value);
    if (std::string* error =
            absl::get_if<std::string>(&offline_manifest_result)) {
      return std::move(*error);
    }
    options.app_info_factory = std::move(
        absl::get<WebApplicationInfoFactory>(offline_manifest_result));
  }

  if (options.only_use_app_info_factory && !options.app_info_factory) {
    return base::StrCat({file.AsUTF8Unsafe(), kOnlyUseOfflineManifest,
                         " set with no ", kOfflineManifest, " available"});
  }

  // force_reinstall_for_milestone
  value = app_config.FindKey(kForceReinstallForMilestone);
  if (value) {
    if (!value->is_int()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kForceReinstallForMilestone});
    }
    options.force_reinstall_for_milestone = value->GetInt();
  }

  return options;
}

WebApplicationInfoFactoryOrError ParseOfflineManifest(
    FileUtilsWrapper& file_utils,
    const base::FilePath& dir,
    const base::FilePath& file,
    const base::Value& offline_manifest) {
  WebApplicationInfo app_info;

  // name
  const std::string* name_string =
      offline_manifest.FindStringKey(kOfflineManifestName);
  if (!name_string) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestName, " missing or invalid."});
  }
  if (!base::UTF8ToUTF16(name_string->data(), name_string->size(),
                         &app_info.title) ||
      app_info.title.empty()) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestName, " invalid: ", *name_string});
  }

  // start_url
  const std::string* start_url_string =
      offline_manifest.FindStringKey(kOfflineManifestStartUrl);
  if (!start_url_string) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestStartUrl, " missing or invalid."});
  }
  app_info.start_url = GURL(*start_url_string);
  if (!app_info.start_url.is_valid()) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestStartUrl,
                         " invalid: ", *start_url_string});
  }

  // scope
  const std::string* scope_string =
      offline_manifest.FindStringKey(kOfflineManifestScope);
  if (!scope_string) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestScope, " missing or invalid."});
  }
  app_info.scope = GURL(*scope_string);
  if (!app_info.scope.is_valid()) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestScope, " invalid: ", *scope_string});
  }
  if (!base::StartsWith(app_info.start_url.path(), app_info.scope.path(),
                        base::CompareCase::SENSITIVE)) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestScope, " (", app_info.start_url.spec(),
                         ") not within ", kOfflineManifestScope, " (",
                         app_info.scope.spec(), ")."});
  }

  // display
  const std::string* display_string =
      offline_manifest.FindStringKey(kOfflineManifestDisplay);
  if (!display_string) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestDisplay, " missing or invalid."});
  }
  DisplayMode display = blink::DisplayModeFromString(*display_string);
  if (display == DisplayMode::kUndefined) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestDisplay,
                         " invalid: ", *display_string});
  }
  app_info.display_mode = display;

  // icon_any_pngs
  const base::Value* icon_files =
      offline_manifest.FindListKey(kOfflineManifestIconAnyPngs);
  if (!icon_files || icon_files->GetList().empty()) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestIconAnyPngs,
                         " missing, empty or invalid."});
  }
  for (const base::Value& icon_file : icon_files->GetList()) {
    if (!icon_file.is_string()) {
      return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                           kOfflineManifestIconAnyPngs, " ",
                           icon_file.DebugString(), " invalid."});
    }

    base::FilePath icon_path = dir.AppendASCII(icon_file.GetString());
    std::string icon_data;
    if (!file_utils.ReadFileToString(icon_path, &icon_data)) {
      return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                           kOfflineManifestIconAnyPngs, " ",
                           icon_file.DebugString(), " failed to read."});
    }

    SkBitmap bitmap;
    if (!gfx::PNGCodec::Decode(
            reinterpret_cast<const unsigned char*>(icon_data.c_str()),
            icon_data.size(), &bitmap)) {
      return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                           kOfflineManifestIconAnyPngs, " ",
                           icon_file.DebugString(), " failed to decode."});
    }

    if (bitmap.width() != bitmap.height()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
           kOfflineManifestIconAnyPngs, " ", icon_file.DebugString(),
           " must be square: ", base::NumberToString(bitmap.width()), "x",
           base::NumberToString(bitmap.height())});
    }

    app_info.icon_bitmaps_any[bitmap.width()] = std::move(bitmap);
  }
  DCHECK(!app_info.icon_bitmaps_any.empty());

  // theme_color_argb_hex (optional)
  const base::Value* theme_color_value =
      offline_manifest.FindKey(kOfflineManifestThemeColorArgbHex);
  if (theme_color_value) {
    const std::string* theme_color_argb_hex =
        theme_color_value->is_string() ? &theme_color_value->GetString()
                                       : nullptr;
    SkColor theme_color;
    if (!theme_color_argb_hex ||
        !base::HexStringToUInt(*theme_color_argb_hex, &theme_color)) {
      return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                           kOfflineManifestThemeColorArgbHex,
                           " invalid: ", theme_color_value->DebugString()});
    }
    app_info.theme_color = SkColorSetA(theme_color, SK_AlphaOPAQUE);
  }

  return base::BindRepeating(
      &std::make_unique<WebApplicationInfo, const WebApplicationInfo&>,
      std::move(app_info));
}

bool IsReinstallPastMilestoneNeeded(
    base::StringPiece last_preinstall_synchronize_milestone_str,
    base::StringPiece current_milestone_str,
    int force_reinstall_for_milestone) {
  int last_preinstall_synchronize_milestone = 0;
  if (!base::StringToInt(last_preinstall_synchronize_milestone_str,
                         &last_preinstall_synchronize_milestone)) {
    return false;
  }

  int current_milestone = 0;
  if (!base::StringToInt(current_milestone_str, &current_milestone))
    return false;

  return last_preinstall_synchronize_milestone <
             force_reinstall_for_milestone &&
         current_milestone >= force_reinstall_for_milestone;
}

bool WasAppMigratedToWebApp(Profile* profile, const std::string& app_id) {
  const base::ListValue* migrated_apps =
      profile->GetPrefs()->GetList(prefs::kWebAppsMigratedDefaultApps);
  if (!migrated_apps)
    return false;

  for (const auto& val : migrated_apps->GetList()) {
    if (val.is_string() && val.GetString() == app_id)
      return true;
  }

  return false;
}

void MarkAppAsMigratedToWebApp(Profile* profile,
                               const std::string& app_id,
                               bool was_migrated) {
  ListPrefUpdate update(profile->GetPrefs(),
                        prefs::kWebAppsMigratedDefaultApps);
  if (was_migrated)
    update->Append(app_id);
  else
    update->EraseListValue(base::Value(app_id));
}

bool WasMigrationRun(Profile* profile, base::StringPiece feature_name) {
  const base::ListValue* migrated_features =
      profile->GetPrefs()->GetList(prefs::kWebAppsDidMigrateDefaultChromeApps);
  if (!migrated_features)
    return false;

  for (const auto& val : migrated_features->GetList()) {
    if (val.is_string() && val.GetString() == feature_name)
      return true;
  }

  return false;
}

void SetMigrationRun(Profile* profile,
                     base::StringPiece feature_name,
                     bool was_migrated) {
  ListPrefUpdate update(profile->GetPrefs(),
                        prefs::kWebAppsDidMigrateDefaultChromeApps);
  if (was_migrated)
    update->Append(feature_name);
  else
    update->EraseListValue(base::Value(feature_name));
}

bool WasDefaultAppUninstalled(Profile* profile, const std::string& app_id) {
  const base::ListValue* uninstalled_apps =
      profile->GetPrefs()->GetList(prefs::kWebAppsUninstalledDefaultChromeApps);
  if (!uninstalled_apps)
    return false;

  for (const auto& val : uninstalled_apps->GetList()) {
    if (val.is_string() && val.GetString() == app_id)
      return true;
  }

  return false;
}

void MarkDefaultAppAsUninstalled(Profile* profile, const std::string& app_id) {
  if (WasDefaultAppUninstalled(profile, app_id))
    return;
  ListPrefUpdate update(profile->GetPrefs(),
                        prefs::kWebAppsUninstalledDefaultChromeApps);
  update->Append(app_id);
}
}  // namespace web_app
