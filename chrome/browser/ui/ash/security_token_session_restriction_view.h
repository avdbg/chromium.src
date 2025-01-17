// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SECURITY_TOKEN_SESSION_RESTRICTION_VIEW_H_
#define CHROME_BROWSER_UI_ASH_SECURITY_TOKEN_SESSION_RESTRICTION_VIEW_H_

#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/security_token_session_controller.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_dialog_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

// The dialog informing the user they are about to be logged out or locked
// because they removed their security token (smart card).
class SecurityTokenSessionRestrictionView : public AppDialogView {
 public:
  METADATA_HEADER(SecurityTokenSessionRestrictionView);

  // Creates the dialog.
  // `duration`: Initial countdown time, will be displayed in seconds.
  // `accept_callback`: Callback when the user accepts the dialog.
  // `windows_closing_callback`: Callback when the window closes for any reason.
  // `behavior`: Determines the displayed strings. Needs to be
  // chromeos::login::SecurityTokenSessionController::Behavior::kLogout or
  // chromeos::login::SecurityTokenSessionController::Behavior::kLock.
  // `domain`: The domain the device is enrolled in.
  SecurityTokenSessionRestrictionView(
      base::TimeDelta duration,
      base::OnceClosure accept_callback,
      chromeos::login::SecurityTokenSessionController::Behavior behavior,
      const std::string& domain);
  SecurityTokenSessionRestrictionView(
      const SecurityTokenSessionRestrictionView& other) = delete;
  SecurityTokenSessionRestrictionView& operator=(
      const SecurityTokenSessionRestrictionView& other) = delete;
  ~SecurityTokenSessionRestrictionView() override;

 private:
  void UpdateLabel();

  const chromeos::login::SecurityTokenSessionController::Behavior behavior_;
  const base::TickClock* clock_;
  const std::string domain_;
  base::TimeTicks end_time_;
  base::RepeatingTimer update_timer_;
};

#endif  // CHROME_BROWSER_UI_ASH_SECURITY_TOKEN_SESSION_RESTRICTION_VIEW_H_
