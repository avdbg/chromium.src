// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/views_screen_locker.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/authpolicy/authpolicy_helper.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/chromeos/login/lock_screen_utils.h"
#include "chrome/browser/chromeos/login/mojo_system_info_dispatcher.h"
#include "chrome/browser/chromeos/login/user_board_view_mojo.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/common/pref_names.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"

namespace chromeos {

ViewsScreenLocker::ViewsScreenLocker(ScreenLocker* screen_locker)
    : screen_locker_(screen_locker),
      system_info_updater_(std::make_unique<MojoSystemInfoDispatcher>()) {
  LoginScreenClient::Get()->SetDelegate(this);
  user_board_view_mojo_ = std::make_unique<UserBoardViewMojo>();
  user_selection_screen_ =
      std::make_unique<ChromeUserSelectionScreen>(DisplayedScreen::LOCK_SCREEN);
  user_selection_screen_->SetView(user_board_view_mojo_.get());
}

ViewsScreenLocker::~ViewsScreenLocker() {
  lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(nullptr);
  LoginScreenClient::Get()->SetDelegate(nullptr);
}

void ViewsScreenLocker::Init() {
  lock_time_ = base::TimeTicks::Now();
  user_selection_screen_->Init(screen_locker_->GetUsersToShow());

  // Reset Caps Lock state when lock screen is shown.
  input_method::InputMethodManager::Get()->GetImeKeyboard()->SetCapsLockEnabled(
      false);

  system_info_updater_->StartRequest();

  ash::LoginScreen::Get()->GetModel()->SetUserList(
      user_selection_screen_->UpdateAndReturnUserListForAsh());
  ash::LoginScreen::Get()->SetAllowLoginAsGuest(false /*show_guest*/);

  if (user_manager::UserManager::IsInitialized()) {
    // Enable pin and challenge-response authentication for any users who can
    // use them.
    for (user_manager::User* user :
         user_manager::UserManager::Get()->GetLoggedInUsers()) {
      UpdatePinKeyboardState(user->GetAccountId());
      UpdateChallengeResponseAuthAvailability(user->GetAccountId());
    }
  }

  user_selection_screen_->InitEasyUnlock();
  UMA_HISTOGRAM_TIMES("LockScreen.LockReady",
                      base::TimeTicks::Now() - lock_time_);
  screen_locker_->ScreenLockReady();
  lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(this);
}

void ViewsScreenLocker::ShowErrorMessage(
    int error_msg_id,
    HelpAppLauncher::HelpTopic help_topic_id) {
  // TODO(xiaoyinh): Complete the implementation here.
  NOTIMPLEMENTED();
}

void ViewsScreenLocker::ClearErrors() {
  NOTIMPLEMENTED();
}

void ViewsScreenLocker::OnAshLockAnimationFinished() {
  SessionControllerClientImpl::Get()->NotifyChromeLockAnimationsComplete();
}

void ViewsScreenLocker::HandleAuthenticateUserWithPasswordOrPin(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_EQ(account_id.GetUserEmail(),
            gaia::SanitizeEmail(account_id.GetUserEmail()));
  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  UserContext user_context(*user);
  user_context.SetKey(
      Key(chromeos::Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), password));
  user_context.SetIsUsingPin(authenticated_by_pin);
  user_context.SetSyncPasswordData(password_manager::PasswordHashData(
      account_id.GetUserEmail(), base::UTF8ToUTF16(password),
      false /*force_update*/));
  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY &&
      (user_context.GetUserType() !=
       user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY)) {
    LOG(FATAL) << "Incorrect Active Directory user type "
               << user_context.GetUserType();
  }
  ScreenLocker::default_screen_locker()->Authenticate(user_context,
                                                      std::move(callback));
  UpdatePinKeyboardState(account_id);
}

void ViewsScreenLocker::HandleAuthenticateUserWithEasyUnlock(
    const AccountId& account_id) {
  user_selection_screen_->AttemptEasyUnlock(account_id);
}

void ViewsScreenLocker::HandleAuthenticateUserWithChallengeResponse(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  ScreenLocker::default_screen_locker()->AuthenticateWithChallengeResponse(
      account_id, std::move(callback));
}

void ViewsScreenLocker::HandleHardlockPod(const AccountId& account_id) {
  user_selection_screen_->HardLockPod(account_id);
}

void ViewsScreenLocker::HandleOnFocusPod(const AccountId& account_id) {
  user_selection_screen_->HandleFocusPod(account_id);

  WallpaperControllerClient::Get()->ShowUserWallpaper(account_id);
}

void ViewsScreenLocker::HandleOnNoPodFocused() {
  user_selection_screen_->HandleNoPodFocused();
}

bool ViewsScreenLocker::HandleFocusLockScreenApps(bool reverse) {
  if (lock_screen_app_focus_handler_.is_null())
    return false;

  lock_screen_app_focus_handler_.Run(reverse);
  return true;
}

void ViewsScreenLocker::HandleFocusOobeDialog() {
  NOTREACHED();
}

void ViewsScreenLocker::HandleLaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  NOTREACHED();
}

void ViewsScreenLocker::SuspendDone(base::TimeDelta sleep_duration) {
  for (user_manager::User* user :
       user_manager::UserManager::Get()->GetUnlockUsers()) {
    UpdatePinKeyboardState(user->GetAccountId());
  }
}

void ViewsScreenLocker::RegisterLockScreenAppFocusHandler(
    const LockScreenAppFocusCallback& focus_handler) {
  lock_screen_app_focus_handler_ = focus_handler;
}

void ViewsScreenLocker::UnregisterLockScreenAppFocusHandler() {
  lock_screen_app_focus_handler_.Reset();
}

void ViewsScreenLocker::HandleLockScreenAppFocusOut(bool reverse) {
  ash::LoginScreen::Get()->GetModel()->HandleFocusLeavingLockScreenApps(
      reverse);
}

void ViewsScreenLocker::UpdatePinKeyboardState(const AccountId& account_id) {
  quick_unlock::PinBackend::GetInstance()->CanAuthenticate(
      account_id, base::BindOnce(&ViewsScreenLocker::OnPinCanAuthenticate,
                                 weak_factory_.GetWeakPtr(), account_id));
}

void ViewsScreenLocker::UpdateChallengeResponseAuthAvailability(
    const AccountId& account_id) {
  const bool enable_challenge_response =
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id);
  ash::LoginScreen::Get()->GetModel()->SetChallengeResponseAuthEnabledForUser(
      account_id, enable_challenge_response);
}

void ViewsScreenLocker::OnPinCanAuthenticate(const AccountId& account_id,
                                             bool can_authenticate) {
  ash::LoginScreen::Get()->GetModel()->SetPinEnabledForUser(account_id,
                                                            can_authenticate);
}

}  // namespace chromeos
