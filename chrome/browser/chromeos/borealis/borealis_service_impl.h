// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_IMPL_H_

#include "chrome/browser/chromeos/borealis/borealis_service.h"

#include "chrome/browser/chromeos/borealis/borealis_app_launcher.h"
#include "chrome/browser/chromeos/borealis/borealis_app_uninstaller.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager_impl.h"
#include "chrome/browser/chromeos/borealis/borealis_features.h"
#include "chrome/browser/chromeos/borealis/borealis_installer_impl.h"
#include "chrome/browser/chromeos/borealis/borealis_shutdown_monitor.h"
#include "chrome/browser/chromeos/borealis/borealis_window_manager.h"

namespace borealis {

class BorealisServiceImpl : public BorealisService {
 public:
  explicit BorealisServiceImpl(Profile* profile);

  ~BorealisServiceImpl() override;

 private:
  // BorealisService overrides.
  BorealisAppLauncher& AppLauncher() override;
  BorealisAppUninstaller& AppUninstaller() override;
  BorealisContextManager& ContextManager() override;
  BorealisFeatures& Features() override;
  BorealisInstaller& Installer() override;
  BorealisShutdownMonitor& ShutdownMonitor() override;
  BorealisWindowManager& WindowManager() override;

  Profile* const profile_;

  BorealisAppLauncher app_launcher_;
  BorealisAppUninstaller app_uninstaller_;
  BorealisContextManagerImpl context_manager_;
  BorealisFeatures features_;
  BorealisInstallerImpl installer_;
  BorealisShutdownMonitor shutdown_monitor_;
  BorealisWindowManager window_manager_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_IMPL_H_
