// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

class UserContext;

// TODO(nkostylev): Extract interface, create a BaseLoginDisplay class.
// An abstract class that defines login UI implementation.
class LoginDisplay {
 public:
  class Delegate {
   public:
    // Sign in using `username` and `password` specified.
    // Used for known users only.
    virtual void Login(const UserContext& user_context,
                       const SigninSpecifics& specifics) = 0;

    // Returns true if sign in is in progress.
    virtual bool IsSigninInProgress() const = 0;

    // Notify the delegate when the sign-in UI is finished loading.
    virtual void OnSigninScreenReady() = 0;

    // Called when the user requests enterprise enrollment.
    virtual void OnStartEnterpriseEnrollment() = 0;

    // Called when the user requests kiosk enable screen.
    virtual void OnStartKioskEnableScreen() = 0;

    // Called when the owner permission for kiosk app auto launch is requested.
    virtual void OnStartKioskAutolaunchScreen() = 0;

    // Returns name of the currently connected network, for error message,
    virtual base::string16 GetConnectedNetworkName() = 0;

    // Restarts the auto-login timer if it is running.
    virtual void ResetAutoLoginTimer() = 0;

   protected:
    virtual ~Delegate();
  };

  LoginDisplay();
  virtual ~LoginDisplay();

  // Clears and enables fields on user pod or GAIA frame.
  virtual void ClearAndEnablePassword() = 0;

  // Initializes login UI with the user pods based on list of known users and
  // guest, new user pods if those are enabled.
  virtual void Init(const user_manager::UserList& users,
                    bool show_guest,
                    bool show_users,
                    bool show_new_user) = 0;

  // Notifies the login UI that the preferences defining how to visualize it to
  // the user have changed and it needs to refresh.
  virtual void OnPreferencesChanged() = 0;

  // Changes enabled state of the UI.
  virtual void SetUIEnabled(bool is_enabled) = 0;

  // Displays simple error bubble with `error_msg_id` specified.
  // `login_attempts` shows number of login attempts made by current user.
  // `help_topic_id` is additional help topic that is presented as link.
  virtual void ShowError(int error_msg_id,
                         int login_attempts,
                         HelpAppLauncher::HelpTopic help_topic_id) = 0;

  // Show allowlist check failed error. Happens after user completes online
  // signin but allowlist check fails.
  virtual void ShowAllowlistCheckFailedError() = 0;

  Delegate* delegate() { return delegate_; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  bool is_signin_completed() const { return is_signin_completed_; }
  void set_signin_completed(bool value) { is_signin_completed_ = value; }

 protected:
  // Login UI delegate (controller).
  Delegate* delegate_ = nullptr;

  // True if signin for user has completed.
  // TODO(nkostylev): Find a better place to store this state
  // in redesigned login stack.
  // Login stack (and this object) will be recreated for next user sign in.
  bool is_signin_completed_ = false;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplay);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_H_
