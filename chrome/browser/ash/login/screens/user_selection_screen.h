// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/saml/password_sync_token_checkers_collection.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/chromeos/login/signin/token_handle_util.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

class AccountId;

namespace chromeos {

class EasyUnlockService;
class UserBoardView;

enum class DisplayedScreen { SIGN_IN_SCREEN, USER_ADDING_SCREEN, LOCK_SCREEN };

// This class represents User Selection screen: user pod-based login screen.
class UserSelectionScreen
    : public proximity_auth::ScreenlockBridge::LockHandler,
      public BaseScreen,
      public session_manager::SessionManagerObserver,
      public PasswordSyncTokenLoginChecker::Observer {
 public:
  explicit UserSelectionScreen(DisplayedScreen display_type);
  ~UserSelectionScreen() override;

  void SetView(UserBoardView* view);

  static const user_manager::UserList PrepareUserListForSending(
      const user_manager::UserList& users,
      const AccountId& owner,
      bool is_signin_to_add);

  virtual void Init(const user_manager::UserList& users);

  void CheckUserStatus(const AccountId& account_id);
  void HandleFocusPod(const AccountId& account_id);
  void HandleNoPodFocused();
  void OnBeforeShow();

  // Methods for easy unlock support.
  void HardLockPod(const AccountId& account_id);
  void AttemptEasyUnlock(const AccountId& account_id);

  void InitEasyUnlock();

  void SetTpmLockedState(bool is_locked, base::TimeDelta time_left);

  // proximity_auth::ScreenlockBridge::LockHandler implementation:
  void ShowBannerMessage(const base::string16& message,
                         bool is_warning) override;
  void ShowUserPodCustomIcon(
      const AccountId& account_id,
      const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions& icon)
      override;
  void HideUserPodCustomIcon(const AccountId& account_id) override;

  void EnableInput() override;
  void SetAuthType(const AccountId& account_id,
                   proximity_auth::mojom::AuthType auth_type,
                   const base::string16& auth_value) override;
  proximity_auth::mojom::AuthType GetAuthType(
      const AccountId& account_id) const override;
  ScreenType GetScreenType() const override;

  void Unlock(const AccountId& account_id) override;
  void AttemptEasySignin(const AccountId& account_id,
                         const std::string& secret,
                         const std::string& key_label) override;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

  // PasswordSyncTokenLoginChecker::Observer
  void OnInvalidSyncToken(const AccountId& account_id) override;

  // Determines if user auth status requires online sign in.
  static bool ShouldForceOnlineSignIn(const user_manager::User* user);

  // Builds a `UserAvatar` instance which contains the current image for `user`.
  static ash::UserAvatar BuildAshUserAvatarForUser(
      const user_manager::User& user);

  std::vector<ash::LoginUserInfo> UpdateAndReturnUserListForAsh();
  void SetUsersLoaded(bool loaded);

  static void SetSkipForceOnlineSigninForTesting(bool skip);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  UserBoardView* view_ = nullptr;

  // Map from public session account IDs to recommended locales set by policy.
  std::map<AccountId, std::vector<std::string>>
      public_session_recommended_locales_;

  // Whether users have been sent to the UI(WebUI or Views).
  bool users_loaded_ = false;

 private:
  class DircryptoMigrationChecker;
  class TpmLockedChecker;

  EasyUnlockService* GetEasyUnlockServiceForUser(
      const AccountId& account_id) const;

  void OnUserStatusChecked(const AccountId& account_id,
                           TokenHandleUtil::TokenHandleStatus status);
  void OnAllowedInputMethodsChanged();

  // Purpose of the screen.
  const DisplayedScreen display_type_;

  // Set of Users that are visible.
  user_manager::UserList users_;

  // Map of account ids to their current authentication type. If a user is not
  // contained in the map, it is using the default authentication type.
  std::map<AccountId, proximity_auth::mojom::AuthType> user_auth_type_map_;

  // Timer for measuring idle state duration before password clear.
  base::OneShotTimer password_clear_timer_;

  // Token handler util for checking user OAuth token status.
  std::unique_ptr<TokenHandleUtil> token_handle_util_;

  // Helper to check whether a user needs dircrypto migration.
  std::unique_ptr<DircryptoMigrationChecker> dircrypto_migration_checker_;

  // Helper to check whether TPM is locked or not.
  std::unique_ptr<TpmLockedChecker> tpm_locked_checker_;

  user_manager::UserList users_to_send_;

  AccountId focused_pod_account_id_;
  base::Optional<system::SystemClock::ScopedHourClockType>
      focused_user_clock_type_;

  // Sometimes we might get focused pod while user session is still active. e.g.
  // while creating lock screen. So postpone any work until after the session
  // state changes.
  base::Optional<AccountId> pending_focused_account_id_;

  // Input Method Engine state used at the user selection screen.
  scoped_refptr<input_method::InputMethodManager::State> ime_state_;

  base::CallbackListSubscription allowed_input_methods_subscription_;

  // Collection of verifiers that check validity of password sync token for SAML
  // users corresponding to visible pods.
  std::unique_ptr<PasswordSyncTokenCheckersCollection> sync_token_checkers_;

  base::WeakPtrFactory<UserSelectionScreen> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserSelectionScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_
