// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_run_on_os_login.h"

#include <memory>

#include "base/files/file_util.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_win.h"

namespace web_app {

namespace internals {

bool RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info) {
  base::FilePath shortcut_data_dir = GetShortcutDataDir(shortcut_info);

  ShortcutLocations locations;
  locations.in_startup = true;

  return CreatePlatformShortcuts(shortcut_data_dir, locations,
                                 SHORTCUT_CREATION_BY_USER, shortcut_info);
}

bool UnregisterRunOnOsLogin(const std::string& app_id,
                            const base::FilePath& profile_path,
                            const base::string16& shortcut_title) {
  web_app::ShortcutLocations all_shortcut_locations;
  all_shortcut_locations.in_startup = true;
  std::vector<base::FilePath> all_paths =
      GetShortcutPaths(all_shortcut_locations);
  bool result = true;
  // Only Startup folder is the expected path to be returned in all_paths.
  for (const auto& path : all_paths) {
    // Find all app's shortcuts in Startup folder to delete.
    std::vector<base::FilePath> shortcut_files =
        FindAppShortcutsByProfileAndTitle(path, profile_path, shortcut_title);
    for (const auto& shortcut_file : shortcut_files) {
      if (!base::DeleteFile(shortcut_file))
        result = false;
    }
  }
  return result;
}

}  // namespace internals

}  // namespace web_app
