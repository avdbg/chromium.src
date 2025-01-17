// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_APP_UNINSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_APP_UNINSTALLER_H_

#include <string>

#include "base/callback_helpers.h"

class Profile;

namespace borealis {

// Helper class responsible for uninstalling borealis' apps.
class BorealisAppUninstaller {
 public:
  enum class UninstallResult {
    kSuccess,
    kError,
  };

  using OnUninstalledCallback = base::OnceCallback<void(UninstallResult)>;

  explicit BorealisAppUninstaller(Profile* profile);

  // Uninstall the given |app_id|'s associated application. Uninstalling the
  // parent borealis app itself will result in removing it and all of the child
  // apps, whereas uninstalling individual child apps will only remove that
  // specific app (using its own uninstallation flow).
  void Uninstall(std::string app_id, OnUninstalledCallback callback);

 private:
  Profile* const profile_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_APP_UNINSTALLER_H_
