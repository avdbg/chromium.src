// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace ash {

class AccessibilityExtensionLoader {
 public:
  AccessibilityExtensionLoader(
      const std::string& extension_id,
      const base::FilePath& extension_path,
      const base::FilePath::CharType* manifest_filename,
      const base::FilePath::CharType* guest_manifest_filename,
      const base::Closure& unload_callback);
  ~AccessibilityExtensionLoader();

  void SetProfile(Profile* profile, const base::Closure& done_callback);
  void Load(Profile* profile, const base::Closure& done_cb);
  void Unload();

  bool loaded() { return loaded_; }

  Profile* profile() { return profile_; }

 private:
  void LoadExtension(Profile* profile, base::Closure done_cb);
  void LoadExtensionImpl(Profile* profile, base::Closure done_cb);
  void ReinstallExtensionForKiosk(Profile* profile, base::Closure done_cb);
  void UnloadExtensionFromProfile(Profile* profile);

  Profile* profile_;
  std::string extension_id_;
  base::FilePath extension_path_;

  const base::FilePath::CharType* manifest_filename_ = nullptr;

  const base::FilePath::CharType* guest_manifest_filename_ = nullptr;

  bool loaded_;

  // Whether this extension was reset for kiosk mode.
  bool was_reset_for_kiosk_ = false;

  base::Closure unload_callback_;

  base::WeakPtrFactory<AccessibilityExtensionLoader> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AccessibilityExtensionLoader);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_
