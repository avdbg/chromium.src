// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_utils.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/plugin_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#endif

// static
void PluginUtils::GetPluginContentSetting(
    const HostContentSettingsMap* host_content_settings_map,
    const content::WebPluginInfo& plugin,
    const url::Origin& main_frame_origin,
    const GURL& plugin_url,
    const std::string& resource,
    ContentSetting* setting,
    bool* is_default,
    bool* is_managed) {
  // Not JS means Flash. Flash is deprecated.
  if (!ShouldUseJavaScriptSettingForPlugin(plugin)) {
    *is_default = true;
    *setting = CONTENT_SETTING_BLOCK;
    return;
  }

  GURL main_frame_url = main_frame_origin.GetURL();
  std::unique_ptr<base::Value> value;
  content_settings::SettingInfo info;
  bool uses_plugin_specific_setting = false;
  value = host_content_settings_map->GetWebsiteSetting(
      main_frame_url, main_frame_url, ContentSettingsType::JAVASCRIPT, &info);

  *setting = content_settings::ValueToContentSetting(value.get());

  bool uses_default_content_setting =
      !uses_plugin_specific_setting &&
      info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      info.secondary_pattern == ContentSettingsPattern::Wildcard();

  if (is_default)
    *is_default = uses_default_content_setting;
  if (is_managed)
    *is_managed = info.source == content_settings::SETTING_SOURCE_POLICY;
}

// static
std::string PluginUtils::GetExtensionIdForMimeType(
    content::BrowserContext* browser_context,
    const std::string& mime_type) {
  auto map = GetMimeTypeToExtensionIdMap(browser_context);
  auto it = map.find(mime_type);
  if (it != map.end())
    return it->second;
  return std::string();
}

base::flat_map<std::string, std::string>
PluginUtils::GetMimeTypeToExtensionIdMap(
    content::BrowserContext* browser_context) {
  base::flat_map<std::string, std::string> mime_type_to_extension_id_map;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const std::vector<std::string>& allowlist =
      MimeTypesHandler::GetMIMETypeAllowlist();
  // Go through the allowed extensions and try to use them to intercept
  // the URL request.
  for (const std::string& extension_id : allowlist) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(browser_context)
            ->enabled_extensions()
            .GetByID(extension_id);
    // The allowed extension may not be installed, so we have to nullptr
    // check |extension|.
    if (!extension ||
        (profile->IsOffTheRecord() && !extensions::util::IsIncognitoEnabled(
                                          extension_id, browser_context))) {
      continue;
    }

    if (extension_id == extension_misc::kPdfExtensionId &&
        profile->GetPrefs()->GetBoolean(
            prefs::kPluginsAlwaysOpenPdfExternally)) {
      continue;
    }

    if (MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension)) {
      for (const auto& supported_mime_type : handler->mime_type_set()) {
        // If multiple are installed, Quickoffice extensions may clobber ones
        // earlier in the allowlist. Silently allow this (logging causes ~100
        // lines of output since this function is invoked 3 times during startup
        // for ~30 mime types).
        mime_type_to_extension_id_map[supported_mime_type] = extension_id;
      }
    }
  }
#endif
  return mime_type_to_extension_id_map;
}
