// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_
#define CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_

#include <string>
#include <vector>

#include "ash/public/cpp/login_screen.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/test_login_screen_model.h"

namespace ash {
class ScopedGuestButtonBlocker;
enum class SupervisedAction;
}

// Test implementation of ash's mojo LoginScreen interface.
class TestLoginScreen : public ash::LoginScreen {
 public:
  TestLoginScreen();
  ~TestLoginScreen() override;

  // ash::LoginScreen:
  void SetClient(ash::LoginScreenClient* client) override;
  ash::LoginScreenModel* GetModel() override;
  void ShowLockScreen() override;
  void ShowLoginScreen() override;
  void ShowKioskAppError(const std::string& message) override;
  void FocusLoginShelf(bool reverse) override;
  bool IsReadyForPassword() override;
  void EnableAddUserButton(bool enable) override;
  void EnableShutdownButton(bool enable) override;
  void EnableShelfButtons(bool enable) override;
  void SetIsFirstSigninStep(bool is_first) override;
  void ShowParentAccessButton(bool show) override;
  void SetAllowLoginAsGuest(bool allow_guest) override;
  std::unique_ptr<ash::ScopedGuestButtonBlocker> GetScopedGuestButtonBlocker()
      override;
  void RequestSecurityTokenPin(ash::SecurityTokenPinRequest request) override;
  void ClearSecurityTokenPinRequest() override;
  bool SetLoginShelfGestureHandler(const base::string16& nudge_text,
                                   const base::RepeatingClosure& fling_callback,
                                   base::OnceClosure exit_callback) override;
  void ClearLoginShelfGestureHandler() override;

 private:
  TestLoginScreenModel test_screen_model_;

  DISALLOW_COPY_AND_ASSIGN(TestLoginScreen);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_
