// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/device_disabling_manager_default_delegate.h"

#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"

namespace ash {
namespace system {

DeviceDisablingManagerDefaultDelegate::DeviceDisablingManagerDefaultDelegate() {
}

void DeviceDisablingManagerDefaultDelegate::RestartToLoginScreen() {
  chrome::AttemptUserExit();
}

void DeviceDisablingManagerDefaultDelegate::ShowDeviceDisabledScreen() {
  if (!LoginDisplayHost::default_host()) {
    // LoginDisplayHost will check if the device is disabled on creation.
    return;
  }
  LoginDisplayHost::default_host()->StartWizard(
      DeviceDisabledScreenView::kScreenId);
}

}  // namespace system
}  // namespace ash
