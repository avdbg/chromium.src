// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_run_on_os_login.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_linux.h"

namespace web_app {

namespace internals {

bool RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath shortcut_data_dir = GetShortcutDataDir(shortcut_info);

  ShortcutLocations locations;
  locations.in_startup = true;

  return CreatePlatformShortcuts(shortcut_data_dir, locations,
                                 SHORTCUT_CREATION_BY_USER, shortcut_info);

#else
  return false;
#endif
}

bool UnregisterRunOnOsLogin(const std::string& app_id,
                            const base::FilePath& profile_path,
                            const base::string16& shortcut_title) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ShortcutLocations locations;
  locations.in_startup = true;

  std::vector<base::FilePath> all_shortcut_files =
      GetShortcutLocations(locations, profile_path, app_id);
  bool result = true;
  for (const auto& shortcut_file : all_shortcut_files) {
    if (!base::DeleteFile(shortcut_file))
      result = false;
  }
  return result;
#else
  return true;
#endif
}

}  // namespace internals

}  // namespace web_app
