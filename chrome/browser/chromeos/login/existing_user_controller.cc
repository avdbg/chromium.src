// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/login/auth/chrome_login_performer.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_cryptohome.h"
#include "chrome/browser/ash/login/screens/encryption_migration_mode.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/authpolicy/authpolicy_helper.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/enterprise_user_session_metrics.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_token_initializer.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"
#include "chrome/browser/chromeos/policy/powerwash_requirements_checker.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_enable_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/tpm_error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/session/session_termination_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_util.h"
#include "components/arc/enterprise/arc_data_snapshotd_manager.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "components/vector_icons/vector_icons.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/widget/widget.h"

using RebootOnSignOutPolicy =
    enterprise_management::DeviceRebootOnUserSignoutProto;

namespace chromeos {

namespace {

const char kAutoLaunchNotificationId[] =
    "chrome://managed_guest_session/auto_launch";

const char kAutoLaunchNotifierId[] = "ash.managed_guest_session-auto_launch";

// Enum types for Login.PasswordChangeFlow.
// Don't change the existing values and update LoginPasswordChangeFlow in
// histogram.xml when making changes here.
enum LoginPasswordChangeFlow {
  // User is sent to the password changed flow. This is the normal case.
  LOGIN_PASSWORD_CHANGE_FLOW_PASSWORD_CHANGED = 0,
  // User is sent to the unrecoverable cryptohome failure flow. This is the
  // case when http://crbug.com/547857 happens.
  LOGIN_PASSWORD_CHANGE_FLOW_CRYPTOHOME_FAILURE = 1,

  LOGIN_PASSWORD_CHANGE_FLOW_COUNT,  // Must be the last entry.
};

// Delay for transferring the auth cache to the system profile.
const long int kAuthCacheTransferDelayMs = 2000;

// Delay for restarting the ui if safe-mode login has failed.
const long int kSafeModeRestartUiDelayMs = 30000;

// Makes a call to the policy subsystem to reload the policy when we detect
// authentication change.
void RefreshPoliciesOnUIThread() {
  if (g_browser_process->policy_service())
    g_browser_process->policy_service()->RefreshPolicies(base::OnceClosure());
}

void OnTranferredHttpAuthCaches() {
  VLOG(1) << "Main request context populated with authentication data.";
  // Last but not least tell the policy subsystem to refresh now as it might
  // have been stuck until now too.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RefreshPoliciesOnUIThread));
}

void TransferHttpAuthCacheToSystemNetworkContext(
    base::RepeatingClosure completion_callback,
    const base::UnguessableToken& cache_key) {
  network::mojom::NetworkContext* system_network_context =
      g_browser_process->system_network_context_manager()->GetContext();
  system_network_context->LoadHttpAuthCacheProxyEntries(cache_key,
                                                        completion_callback);
}

// Copies any authentication details that were entered in the login profile to
// the main profile to make sure all subsystems of Chrome can access the network
// with the provided authentication which are possibly for a proxy server.
void TransferHttpAuthCaches() {
  content::StoragePartition* webview_storage_partition =
      login::GetSigninPartition();
  base::RepeatingClosure completion_callback =
      base::BarrierClosure(webview_storage_partition ? 2 : 1,
                           base::BindOnce(&OnTranferredHttpAuthCaches));
  if (webview_storage_partition) {
    webview_storage_partition->GetNetworkContext()
        ->SaveHttpAuthCacheProxyEntries(base::BindOnce(
            &TransferHttpAuthCacheToSystemNetworkContext, completion_callback));
  }

  network::mojom::NetworkContext* default_network_context =
      content::BrowserContext::GetDefaultStoragePartition(
          ProfileHelper::GetSigninProfile())
          ->GetNetworkContext();
  default_network_context->SaveHttpAuthCacheProxyEntries(base::BindOnce(
      &TransferHttpAuthCacheToSystemNetworkContext, completion_callback));
}

// Record UMA for password login of regular user when Signin with Smart Lock is
// enabled. Excludes signins in the multi-signin context; only records for the
// signin screen context.
void RecordPasswordLoginEvent(const UserContext& user_context) {
  // If a user is already logged in, this is a multi-signin attempt. Disregard.
  if (session_manager::SessionManager::Get()->IsInSecondaryLoginScreen())
    return;

  EasyUnlockService* easy_unlock_service =
      EasyUnlockService::Get(ProfileHelper::GetSigninProfile());
  if (user_context.GetUserType() == user_manager::USER_TYPE_REGULAR &&
      user_context.GetAuthFlow() == UserContext::AUTH_FLOW_OFFLINE &&
      easy_unlock_service) {
    easy_unlock_service->RecordPasswordLoginEvent(user_context.GetAccountId());
  }
}

bool IsUpdateRequiredDeadlineReached() {
  policy::MinimumVersionPolicyHandler* policy_handler =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetMinimumVersionPolicyHandler();
  return policy_handler && policy_handler->DeadlineReached();
}

void RecordPasswordChangeFlow(LoginPasswordChangeFlow flow) {
  UMA_HISTOGRAM_ENUMERATION("Login.PasswordChangeFlow", flow,
                            LOGIN_PASSWORD_CHANGE_FLOW_COUNT);
}

bool IsTestingMigrationUI() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kTestEncryptionMigrationUI);
}

bool ShouldForceDircrypto(const AccountId& account_id) {
  if (IsTestingMigrationUI())
    return true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableEncryptionMigration)) {
    return false;
  }

  // If the device is not officially supported to run ARC, we don't need to
  // force Ext4 dircrypto.
  if (!arc::IsArcAvailable())
    return false;

  // When a user is signing in as a secondary user, we don't need to force Ext4
  // dircrypto since the user can not run ARC.
  if (UserAddingScreen::Get()->IsRunning())
    return false;

  return true;
}

// Returns true if the device is enrolled to an Active Directory domain
// according to InstallAttributes (proxied through BrowserPolicyConnector).
bool IsActiveDirectoryManaged() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->IsActiveDirectoryManaged();
}

LoginDisplayHost* GetLoginDisplayHost() {
  return LoginDisplayHost::default_host();
}

LoginDisplay* GetLoginDisplay() {
  return GetLoginDisplayHost()->GetLoginDisplay();
}

void AllowOfflineLoginOnErrorScreen(bool allowed) {
  if (!GetLoginDisplayHost()->GetOobeUI())
    return;
  GetLoginDisplayHost()->GetOobeUI()->GetErrorScreen()->AllowOfflineLogin(
      allowed);
}

void SetLoginExtensionApiLaunchExtensionIdPref(const AccountId& account_id,
                                               const std::string extension_id) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  prefs->SetString(prefs::kLoginExtensionApiLaunchExtensionId, extension_id);
  prefs->CommitPendingWrite();
}

base::Optional<EncryptionMigrationMode> GetEncryptionMigrationMode(
    const UserContext& user_context,
    bool has_incomplete_migration) {
  if (has_incomplete_migration) {
    // If migration was incomplete, continue migration automatically.
    return EncryptionMigrationMode::RESUME_MIGRATION;
  }

  if (user_context.GetUserType() == user_manager::USER_TYPE_CHILD) {
    // TODO(https://crbug.com/1147009): Remove child user special case or
    // implement finch experiment for child user migration mode.
    return base::nullopt;
  }

  const bool profile_has_policy =
      user_manager::known_user::GetProfileRequiresPolicy(
          user_context.GetAccountId()) ==
          user_manager::known_user::ProfileRequiresPolicy::kPolicyRequired ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kProfileRequiresPolicy);

  // Force-migrate all home directories if the user is known to have enterprise
  // policy, otherwise ask the user.
  return profile_has_policy ? EncryptionMigrationMode::START_MIGRATION
                            : EncryptionMigrationMode::ASK_USER;
}

// Returns account ID of a public session account if it is unique, otherwise
// returns invalid account ID.
AccountId GetArcDataSnapshotAutoLoginAccountId(
    const std::vector<policy::DeviceLocalAccount>& device_local_accounts) {
  AccountId auto_login_account_id = EmptyAccountId();
  for (std::vector<policy::DeviceLocalAccount>::const_iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->type == policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION) {
      // Do not perform ARC data snapshot auto-login if more than one public
      // session account is configured.
      if (auto_login_account_id.is_valid())
        return EmptyAccountId();
      auto_login_account_id = AccountId::FromUserEmail(it->user_id);
      VLOG(2) << "PublicSession autologin found: " << it->user_id;
    }
  }
  return auto_login_account_id;
}

// Returns account ID if a corresponding to `auto_login_account_id` device local
// account exists, otherwise returns invalid account ID.
AccountId GetPublicSessionAutoLoginAccountId(
    const std::vector<policy::DeviceLocalAccount>& device_local_accounts,
    const std::string& auto_login_account_id) {
  for (std::vector<policy::DeviceLocalAccount>::const_iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->account_id == auto_login_account_id) {
      VLOG(2) << "PublicSession autologin found: " << it->user_id;
      return AccountId::FromUserEmail(it->user_id);
    }
  }
  return EmptyAccountId();
}

}  // namespace

// Utility class used to wait for a Public Session policy store load if public
// session login is requested before the associated policy store is loaded.
// When the store gets loaded, it will run the callback passed to the
// constructor.
class ExistingUserController::PolicyStoreLoadWaiter
    : public policy::CloudPolicyStore::Observer {
 public:
  PolicyStoreLoadWaiter(policy::CloudPolicyStore* store,
                        base::OnceClosure callback)
      : callback_(std::move(callback)) {
    DCHECK(!store->is_initialized());
    scoped_observer_.Add(store);
  }
  ~PolicyStoreLoadWaiter() override = default;

  PolicyStoreLoadWaiter(const PolicyStoreLoadWaiter& other) = delete;
  PolicyStoreLoadWaiter& operator=(const PolicyStoreLoadWaiter& other) = delete;

  // policy::CloudPolicyStore::Observer:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override {
    scoped_observer_.RemoveAll();
    std::move(callback_).Run();
  }
  void OnStoreError(policy::CloudPolicyStore* store) override {
    // If store load fails, run the callback to unblock public session login
    // attempt, which will likely fail.
    scoped_observer_.RemoveAll();
    std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
  ScopedObserver<policy::CloudPolicyStore, policy::CloudPolicyStore::Observer>
      scoped_observer_{this};
};

// static
ExistingUserController* ExistingUserController::current_controller() {
  auto* host = LoginDisplayHost::default_host();
  return host ? host->GetExistingUserController() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, public:

ExistingUserController::ExistingUserController()
    : cros_settings_(CrosSettings::Get()),
      screen_refresh_timer_(std::make_unique<base::OneShotTimer>()),
      network_state_helper_(new login::NetworkStateHelper) {
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  show_user_names_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefShowUserNamesOnSignIn,
      base::BindRepeating(&ExistingUserController::DeviceSettingsChanged,
                          base::Unretained(this)));
  allow_new_user_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefAllowNewUser,
      base::BindRepeating(&ExistingUserController::DeviceSettingsChanged,
                          base::Unretained(this)));
  allow_guest_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefAllowGuest,
      base::BindRepeating(&ExistingUserController::DeviceSettingsChanged,
                          base::Unretained(this)));
  users_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefUsers,
      base::BindRepeating(&ExistingUserController::DeviceSettingsChanged,
                          base::Unretained(this)));
  local_account_auto_login_id_subscription_ =
      cros_settings_->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          base::BindRepeating(&ExistingUserController::ConfigureAutoLogin,
                              base::Unretained(this)));
  local_account_auto_login_delay_subscription_ =
      cros_settings_->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay,
          base::BindRepeating(&ExistingUserController::ConfigureAutoLogin,
                              base::Unretained(this)));
  family_link_allowed_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefFamilyLinkAccountsAllowed,
      base::BindRepeating(&ExistingUserController::DeviceSettingsChanged,
                          base::Unretained(this)));

  observed_user_manager_.Add(user_manager::UserManager::Get());
}

void ExistingUserController::Init(const user_manager::UserList& users) {
  timer_init_ = std::make_unique<base::ElapsedTimer>();
  UpdateLoginDisplay(users);
  ConfigureAutoLogin();
}

void ExistingUserController::UpdateLoginDisplay(
    const user_manager::UserList& users) {
  int reboot_on_signout_policy = -1;
  cros_settings_->GetInteger(kDeviceRebootOnUserSignout,
                             &reboot_on_signout_policy);
  if (reboot_on_signout_policy != -1 &&
      reboot_on_signout_policy !=
          RebootOnSignOutPolicy::REBOOT_ON_SIGNOUT_MODE_UNSPECIFIED &&
      reboot_on_signout_policy != RebootOnSignOutPolicy::NEVER) {
    SessionTerminationManager::Get()->RebootIfNecessary();
    // Initialize PowerwashRequirementsChecker so its instances will be able to
    // use stored cryptohome powerwash state later
    policy::PowerwashRequirementsChecker::Initialize();
  }
  bool show_users_on_signin;
  user_manager::UserList saml_users_for_password_sync;

  cros_settings_->GetBoolean(kAccountsPrefShowUserNamesOnSignIn,
                             &show_users_on_signin);
  user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  // By default disable offline login from the error screen.
  AllowOfflineLoginOnErrorScreen(false /* allowed */);
  for (auto* user : users) {
    // Skip kiosk apps for login screen user list. Kiosk apps as pods (aka new
    // kiosk UI) is currently disabled and it gets the apps directly from
    // KioskAppManager, ArcKioskAppManager and WebKioskAppManager.
    if (user->IsKioskType())
      continue;
    // TODO(xiyuan): Clean user profile whose email is not in allowlist.
    if (user->GetType() == user_manager::USER_TYPE_SUPERVISED_DEPRECATED)
      continue;
    // Allow offline login from the error screen if user of one of these types
    // has already logged in.
    if (user->GetType() == user_manager::USER_TYPE_REGULAR ||
        user->GetType() == user_manager::USER_TYPE_CHILD ||
        user->GetType() == user_manager::USER_TYPE_ACTIVE_DIRECTORY) {
      AllowOfflineLoginOnErrorScreen(true /* allowed */);
    }
    const bool meets_allowlist_requirements =
        !user->HasGaiaAccount() ||
        user_manager::UserManager::Get()->IsGaiaUserAllowed(*user);
    if (meets_allowlist_requirements && user->using_saml())
      saml_users_for_password_sync.push_back(user);
  }

  auto login_users = ExtractLoginUsers(users);
  ForceOnlineFlagChanged(login_users);

  // ExistingUserController owns PasswordSyncTokenLoginCheckers only if user
  // pods are hidden.
  if (!show_users_on_signin && !saml_users_for_password_sync.empty()) {
    sync_token_checkers_ =
        std::make_unique<PasswordSyncTokenCheckersCollection>();
    sync_token_checkers_->StartPasswordSyncCheckers(
        saml_users_for_password_sync,
        /*observer*/ nullptr);
  } else {
    sync_token_checkers_.reset();
  }
  // If no user pods are visible, fallback to single new user pod which will
  // have guest session link.
  bool show_guest = user_manager->IsGuestSessionAllowed();
  show_users_on_signin |= !login_users.empty();
  bool allow_new_user = true;
  cros_settings_->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  GetLoginDisplay()->Init(login_users, show_guest, show_users_on_signin,
                          allow_new_user);
  GetLoginDisplayHost()->OnPreferencesChanged();
}

// Check GAIA with/without SAML offline time limits for `users` and
// schedules next check if needed and returns true if any of user's force
// online sign-in flag is changed.
bool ExistingUserController::ForceOnlineFlagChanged(
    const user_manager::UserList& users) {
  bool force_online_flag_changed = false;
  base::TimeDelta min_delta = base::TimeDelta::Max();
  for (auto* user : users) {
    const base::Optional<base::TimeDelta> offline_signin_limit =
        user_manager::known_user::GetOfflineSigninLimit(user->GetAccountId());
    if (!offline_signin_limit) {
      continue;
    }

    const base::Time last_online_signin =
        user_manager::known_user::GetLastOnlineSignin(user->GetAccountId());
    base::TimeDelta time_to_next_online_signin = login::TimeToOnlineSignIn(
        last_online_signin, offline_signin_limit.value());
    if (time_to_next_online_signin > base::TimeDelta() &&
        time_to_next_online_signin < min_delta) {
      min_delta = time_to_next_online_signin;
    }
    if (time_to_next_online_signin < base::TimeDelta() &&
        !user->force_online_signin()) {
      user_manager::UserManager::Get()->SaveForceOnlineSignin(
          user->GetAccountId(), true);
      force_online_flag_changed = true;
    }
  }
  if (min_delta < base::TimeDelta::Max()) {
    // Schedule update task. If the timer was already running (which is possible
    // when device settings get updated and we are re-running
    // UpdateLoginDisplay) we will restart screen_refresh_timer_ with a new
    // timeout value.
    screen_refresh_timer_->Start(
        FROM_HERE, min_delta,
        base::BindOnce(&ExistingUserController::
                           CheckSamlOfflineTimeLimitAndUpdateLoginDisplay,
                       weak_factory_.GetWeakPtr(), users));
  }
  return force_online_flag_changed;
}

// Calls ForceOnlineFlagChanged and schedules the next call.
void ExistingUserController::CheckSamlOfflineTimeLimitAndUpdateLoginDisplay(
    const user_manager::UserList& users) {
  if (ForceOnlineFlagChanged(users)) {
    ash::LoginScreen::Get()->ShowLoginScreen();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, content::NotificationObserver implementation:
//

void ExistingUserController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_AUTH_SUPPLIED);

  // Don't transfer http auth cache on NOTIFICATION_AUTH_SUPPLIED after user
  // session starts.
  if (session_manager::SessionManager::Get()->IsSessionStarted())
    return;

  // Possibly the user has authenticated against a proxy server and we might
  // need the credentials for enrollment and other system requests from the
  // main `g_browser_process` request context (see bug
  // http://crosbug.com/24861). So we transfer any credentials to the global
  // request context here.
  // The issue we have here is that the NOTIFICATION_AUTH_SUPPLIED is sent
  // just after the UI is closed but before the new credentials were stored
  // in the profile. Therefore we have to give it some time to make sure it
  // has been updated before we copy it.
  // TODO(pmarko): Find a better way to do this, see https://crbug.com/796512.
  VLOG(1) << "Authentication was entered manually, possibly for proxyauth.";
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TransferHttpAuthCaches),
      base::TimeDelta::FromMilliseconds(kAuthCacheTransferDelayMs));
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

ExistingUserController::~ExistingUserController() {
  UserSessionManager::GetInstance()->DelegateDeleted(this);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginDisplay::Delegate implementation:
//
void ExistingUserController::CompleteLogin(const UserContext& user_context) {
  if (!GetLoginDisplayHost()) {
    // Complete login event was generated already from UI. Ignore notification.
    return;
  }

  if (is_login_in_progress_)
    return;

  is_login_in_progress_ = true;

  ContinueLoginIfDeviceNotDisabled(
      base::BindOnce(&ExistingUserController::DoCompleteLogin,
                     weak_factory_.GetWeakPtr(), user_context));
}

base::string16 ExistingUserController::GetConnectedNetworkName() {
  return network_state_helper_->GetCurrentNetworkName();
}

bool ExistingUserController::IsSigninInProgress() const {
  return is_login_in_progress_;
}

void ExistingUserController::Login(const UserContext& user_context,
                                   const SigninSpecifics& specifics) {
  if (is_login_in_progress_) {
    // If there is another login in progress, bail out. Do not re-enable
    // clicking on other windows and the status area. Do not start the
    // auto-login timer.
    return;
  }

  is_login_in_progress_ = true;

  if (user_context.GetUserType() != user_manager::USER_TYPE_REGULAR &&
      user_manager::UserManager::Get()->IsUserLoggedIn()) {
    // Multi-login is only allowed for regular users. If we are attempting to
    // do multi-login as another type of user somehow, bail out. Do not
    // re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer.
    return;
  }

  ContinueLoginIfDeviceNotDisabled(
      base::BindOnce(&ExistingUserController::DoLogin,
                     weak_factory_.GetWeakPtr(), user_context, specifics));
}

void ExistingUserController::PerformLogin(
    const UserContext& user_context,
    LoginPerformer::AuthorizationMode auth_mode) {
  VLOG(1) << "Setting flow from PerformLogin";

  BootTimesRecorder::Get()->RecordLoginAttempted();

  // Use the same LoginPerformer for subsequent login as it has state
  // such as Authenticator instance.
  if (!login_performer_.get() || num_login_attempts_ <= 1) {
    // Only one instance of LoginPerformer should exist at a time.
    login_performer_.reset(nullptr);
    login_performer_.reset(new ChromeLoginPerformer(this));
  }
  if (IsActiveDirectoryManaged() &&
      user_context.GetUserType() != user_manager::USER_TYPE_ACTIVE_DIRECTORY) {
    PerformLoginFinishedActions(false /* don't start auto login timer */);
    ShowError(IDS_LOGIN_ERROR_GOOGLE_ACCOUNT_NOT_ALLOWED,
              "Google accounts are not allowed on this device");
    return;
  }
  if (user_context.GetAccountId().GetAccountType() ==
          AccountType::ACTIVE_DIRECTORY &&
      user_context.GetAuthFlow() == UserContext::AUTH_FLOW_OFFLINE &&
      user_context.GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    // Try to get kerberos TGT while we have user's password typed on the pod
    // screen. Failure to get TGT here is OK - that could mean e.g. Active
    // Directory server is not reachable. We don't want to have user wait for
    // the Active Directory Authentication on the pod screen.
    // AuthPolicyCredentialsManager will be created inside the user session
    // which would get status about last authentication and handle possible
    // failures.
    AuthPolicyHelper::TryAuthenticateUser(
        user_context.GetAccountId().GetUserEmail(),
        user_context.GetAccountId().GetObjGuid(),
        user_context.GetKey()->GetSecret());
  }

  // If plain text password is available, computes its salt, hash, and length,
  // and saves them in `user_context`. They will be saved to prefs when user
  // profile is ready.
  UserContext new_user_context = user_context;
  if (user_context.GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    base::string16 password(
        base::UTF8ToUTF16(new_user_context.GetKey()->GetSecret()));
    new_user_context.SetSyncPasswordData(password_manager::PasswordHashData(
        user_context.GetAccountId().GetUserEmail(), password,
        auth_mode == LoginPerformer::AuthorizationMode::kExternal));
  }

  if (new_user_context.IsUsingPin()) {
    base::Optional<Key> key = quick_unlock::PinStorageCryptohome::TransformKey(
        new_user_context.GetAccountId(), *new_user_context.GetKey());
    if (key) {
      new_user_context.SetKey(*key);
    } else {
      new_user_context.SetIsUsingPin(false);
    }
  }

  // If a regular user log in to a device which supports ARC, we should make
  // sure that the user's cryptohome is encrypted in ext4 dircrypto to run the
  // latest Android runtime.
  new_user_context.SetIsForcingDircrypto(
      ShouldForceDircrypto(new_user_context.GetAccountId()));
  login_performer_->PerformLogin(new_user_context, auth_mode);
  RecordPasswordLoginEvent(new_user_context);
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNING_IN));
  if (timer_init_) {
    base::UmaHistogramMediumTimes("Login.PromptToLoginTime",
                                  timer_init_->Elapsed());
    timer_init_.reset();
  }
  // Stop screen refresh timer - will be restarted on login screen again
  screen_refresh_timer_->Stop();
}

void ExistingUserController::ContinuePerformLogin(
    LoginPerformer::AuthorizationMode auth_mode,
    const UserContext& user_context) {
  login_performer_->PerformLogin(user_context, auth_mode);
}

void ExistingUserController::ContinuePerformLoginWithoutMigration(
    LoginPerformer::AuthorizationMode auth_mode,
    const UserContext& user_context) {
  UserContext user_context_ecryptfs = user_context;
  user_context_ecryptfs.SetIsForcingDircrypto(false);
  ContinuePerformLogin(auth_mode, user_context_ecryptfs);
}

void ExistingUserController::OnSigninScreenReady() {
  // Used to debug crbug.com/902315. Feel free to remove after that is fixed.
  VLOG(1) << "OnSigninScreenReady";
  StartAutoLoginTimer();
}

void ExistingUserController::OnGaiaScreenReady() {
  // Used to debug crbug.com/902315. Feel free to remove after that is fixed.
  VLOG(1) << "OnGaiaScreenReady";
  StartAutoLoginTimer();
}

void ExistingUserController::OnStartEnterpriseEnrollment() {
  if (KioskAppManager::Get()->IsConsumerKioskDeviceWithAutoLaunch()) {
    LOG(WARNING) << "Enterprise enrollment is not available after kiosk auto "
                    "launch is set.";
    return;
  }

  DeviceSettingsService::Get()->GetOwnershipStatusAsync(base::BindOnce(
      &ExistingUserController::OnEnrollmentOwnershipCheckCompleted,
      weak_factory_.GetWeakPtr()));
}

void ExistingUserController::OnStartKioskEnableScreen() {
  KioskAppManager::Get()->GetConsumerKioskAutoLaunchStatus(base::BindOnce(
      &ExistingUserController::OnConsumerKioskAutoLaunchCheckCompleted,
      weak_factory_.GetWeakPtr()));
}

void ExistingUserController::OnStartKioskAutolaunchScreen() {
  ShowKioskAutolaunchScreen();
}

void ExistingUserController::SetDisplayEmail(const std::string& email) {
  display_email_ = email;
}

void ExistingUserController::SetDisplayAndGivenName(
    const std::string& display_name,
    const std::string& given_name) {
  display_name_ = base::UTF8ToUTF16(display_name);
  given_name_ = base::UTF8ToUTF16(given_name);
}

bool ExistingUserController::IsUserAllowlisted(
    const AccountId& account_id,
    const base::Optional<user_manager::UserType>& user_type) {
  bool wildcard_match = false;
  if (login_performer_.get()) {
    return login_performer_->IsUserAllowlisted(account_id, &wildcard_match,
                                               user_type);
  }

  return cros_settings_->IsUserAllowlisted(account_id.GetUserEmail(),
                                           &wildcard_match, user_type);
}

void ExistingUserController::LocalStateChanged(
    user_manager::UserManager* user_manager) {
  DeviceSettingsChanged();
}

void ExistingUserController::OnConsumerKioskAutoLaunchCheckCompleted(
    KioskAppManager::ConsumerKioskAutoLaunchStatus status) {
  if (status == KioskAppManager::ConsumerKioskAutoLaunchStatus::kConfigurable)
    ShowKioskEnableScreen();
}

void ExistingUserController::OnEnrollmentOwnershipCheckCompleted(
    DeviceSettingsService::OwnershipStatus status) {
  VLOG(1) << "OnEnrollmentOwnershipCheckCompleted status=" << status;
  if (status == DeviceSettingsService::OWNERSHIP_NONE) {
    ShowEnrollmentScreen();
  } else if (status == DeviceSettingsService::OWNERSHIP_TAKEN) {
    // On a device that is already owned we might want to allow users to
    // re-enroll if the policy information is invalid.
    CrosSettingsProvider::TrustedStatus trusted_status =
        CrosSettings::Get()->PrepareTrustedValues(base::BindOnce(
            &ExistingUserController::OnEnrollmentOwnershipCheckCompleted,
            weak_factory_.GetWeakPtr(), status));
    if (trusted_status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
      VLOG(1) << "Showing enrollment because device is PERMANENTLY_UNTRUSTED";
      ShowEnrollmentScreen();
    }
  } else {
    // OwnershipService::GetStatusAsync is supposed to return either
    // OWNERSHIP_NONE or OWNERSHIP_TAKEN.
    NOTREACHED();
  }
}

void ExistingUserController::ShowEnrollmentScreen() {
  GetLoginDisplayHost()->StartWizard(EnrollmentScreenView::kScreenId);
}

void ExistingUserController::ShowKioskEnableScreen() {
  GetLoginDisplayHost()->StartWizard(KioskEnableScreenView::kScreenId);
}

void ExistingUserController::ShowKioskAutolaunchScreen() {
  GetLoginDisplayHost()->StartWizard(KioskAutolaunchScreenView::kScreenId);
}

void ExistingUserController::ShowEncryptionMigrationScreen(
    const UserContext& user_context,
    EncryptionMigrationMode migration_mode) {
  GetLoginDisplayHost()->GetSigninUI()->StartEncryptionMigration(
      user_context, migration_mode,
      base::BindOnce(&ExistingUserController::ContinuePerformLogin,
                     weak_factory_.GetWeakPtr(),
                     login_performer_->auth_mode()));
}

void ExistingUserController::ShowTPMError() {
  GetLoginDisplay()->SetUIEnabled(false);
  GetLoginDisplayHost()->StartWizard(TpmErrorView::kScreenId);
}

void ExistingUserController::ShowPasswordChangedDialog(
    const UserContext& user_context) {
  RecordPasswordChangeFlow(LOGIN_PASSWORD_CHANGE_FLOW_PASSWORD_CHANGED);

  VLOG(1) << "Show password changed dialog"
          << ", count=" << login_performer_->password_changed_callback_count();

  // True if user has already made an attempt to enter old password and failed.
  bool show_invalid_old_password_error =
      login_performer_->password_changed_callback_count() > 1;

  GetLoginDisplayHost()->GetSigninUI()->ShowPasswordChangedDialog(
      user_context.GetAccountId(), show_invalid_old_password_error);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginPerformer::Delegate implementation:
//

void ExistingUserController::OnAuthFailure(const AuthFailure& failure) {
  guest_mode_url_ = GURL::EmptyGURL();
  std::string error = failure.GetErrorString();

  PerformLoginFinishedActions(false /* don't start auto login timer */);

  if (ChromeUserManager::Get()
          ->GetUserFlow(last_login_attempt_account_id_)
          ->HandleLoginFailure(failure)) {
    return;
  }

  const bool is_known_user = user_manager::UserManager::Get()->IsKnownUser(
      last_login_attempt_account_id_);
  if (failure.reason() == AuthFailure::OWNER_REQUIRED) {
    ShowError(IDS_LOGIN_ERROR_OWNER_REQUIRED, error);
    // Using Untretained here is safe because SessionTerminationManager is
    // destroyed after the task runner, in
    // ChromeBrowserMainParts::PostDestroyThreads().
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SessionTerminationManager::StopSession,
                       base::Unretained(SessionTerminationManager::Get()),
                       login_manager::SessionStopReason::OWNER_REQUIRED),
        base::TimeDelta::FromMilliseconds(kSafeModeRestartUiDelayMs));
  } else if (failure.reason() == AuthFailure::TPM_ERROR) {
    ShowTPMError();
  } else if (failure.reason() == AuthFailure::TPM_UPDATE_REQUIRED) {
    ShowError(IDS_LOGIN_ERROR_TPM_UPDATE_REQUIRED, error);
  } else if (last_login_attempt_account_id_ == user_manager::GuestAccountId()) {
    // Show no errors, just re-enable input.
    GetLoginDisplay()->ClearAndEnablePassword();
    StartAutoLoginTimer();
  } else if (is_known_user &&
             failure.reason() == AuthFailure::MISSING_CRYPTOHOME) {
    ForceOnlineLoginForAccountId(last_login_attempt_account_id_);
    RecordReauthReason(last_login_attempt_account_id_,
                       ReauthReason::MISSING_CRYPTOHOME);
  } else if (is_known_user &&
             failure.reason() == AuthFailure::UNRECOVERABLE_CRYPTOHOME) {
    // TODO(chromium:1140868, dlunev): for now we route unrecoverable the same
    // way as missing because it is removed under the hood in cryptohomed when
    // the condition met. We should surface that up and deal with it on the
    // chromium level, including making the decision user-driven.
    ForceOnlineLoginForAccountId(last_login_attempt_account_id_);
    RecordReauthReason(last_login_attempt_account_id_,
                       ReauthReason::UNRECOVERABLE_CRYPTOHOME);
  } else {
    // Check networking after trying to login in case user is
    // cached locally or the local admin account.
    if (!network_state_helper_->IsConnected()) {
      if (is_known_user)
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
      else
        ShowError(IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, error);
    } else {
      // TODO(nkostylev): Cleanup rest of ClientLogin related code.
      if (!is_known_user)
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING_NEW, error);
      else
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
    }
    GetLoginDisplay()->ClearAndEnablePassword();
    StartAutoLoginTimer();
  }

  // Reset user flow to default, so that special flow will not affect next
  // attempt.
  ChromeUserManager::Get()->ResetUserFlow(last_login_attempt_account_id_);

  for (auto& auth_status_consumer : auth_status_consumers_)
    auth_status_consumer.OnAuthFailure(failure);

  ClearActiveDirectoryState();
  ClearRecordedNames();
}

void ExistingUserController::OnAuthSuccess(const UserContext& user_context) {
  is_login_in_progress_ = false;
  GetLoginDisplay()->set_signin_completed(true);

  // Login performer will be gone so cache this value to use
  // once profile is loaded.
  password_changed_ = login_performer_->password_changed();
  auth_mode_ = login_performer_->auth_mode();

  ChromeUserManager::Get()
      ->GetUserFlow(user_context.GetAccountId())
      ->HandleLoginSuccess(user_context);

  StopAutoLoginTimer();

  // Truth table of `has_auth_cookies`:
  //                          Regular        SAML
  //  /ServiceLogin              T            T
  //  /ChromeOsEmbeddedSetup     F            T
  const bool has_auth_cookies =
      login_performer_->auth_mode() ==
          LoginPerformer::AuthorizationMode::kExternal &&
      (user_context.GetAccessToken().empty() ||
       user_context.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  // LoginPerformer instance will delete itself in case of successful auth.
  login_performer_->set_delegate(nullptr);
  ignore_result(login_performer_.release());

  if (user_context.GetAuthFlow() == UserContext::AUTH_FLOW_OFFLINE)
    UMA_HISTOGRAM_COUNTS_100("Login.OfflineSuccess.Attempts",
                             num_login_attempts_);

  const bool is_enterprise_managed = g_browser_process->platform_part()
                                         ->browser_policy_connector_chromeos()
                                         ->IsEnterpriseManaged();

  // Mark device will be consumer owned if the device is not managed and this is
  // the first user on the device.
  if (!is_enterprise_managed &&
      user_manager::UserManager::Get()->GetUsers().empty()) {
    DeviceSettingsService::Get()->MarkWillEstablishConsumerOwnership();
  }

  if (user_context.IsLockableManagedGuestSession()) {
    CHECK(user_context.GetUserType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT);
    user_manager::User* user =
        user_manager::UserManager::Get()->FindUserAndModify(
            user_context.GetAccountId());
    DCHECK(user);
    user->AddProfileCreatedObserver(base::BindOnce(
        &SetLoginExtensionApiLaunchExtensionIdPref, user_context.GetAccountId(),
        user_context.GetManagedGuestSessionLaunchExtensionId()));
  }

  UserSessionManager::StartSessionType start_session_type =
      UserAddingScreen::Get()->IsRunning()
          ? UserSessionManager::SECONDARY_USER_SESSION
          : UserSessionManager::PRIMARY_USER_SESSION;
  UserSessionManager::GetInstance()->StartSession(
      user_context, start_session_type, has_auth_cookies,
      false,  // Start session for user.
      this);

  // Update user's displayed email.
  if (!display_email_.empty()) {
    user_manager::UserManager::Get()->SaveUserDisplayEmail(
        user_context.GetAccountId(), display_email_);
  }
  if (!display_name_.empty() || !given_name_.empty()) {
    user_manager::UserManager::Get()->UpdateUserAccountData(
        user_context.GetAccountId(),
        user_manager::UserManager::UserAccountData(display_name_, given_name_,
                                                   std::string() /* locale */));
  }
  ClearRecordedNames();

  if (public_session_auto_login_account_id_.is_valid() &&
      public_session_auto_login_account_id_ == user_context.GetAccountId() &&
      last_login_attempt_was_auto_login_) {
    const std::string& user_id = user_context.GetAccountId().GetUserEmail();
    policy::DeviceLocalAccountPolicyBroker* broker =
        g_browser_process->platform_part()
            ->browser_policy_connector_chromeos()
            ->GetDeviceLocalAccountPolicyService()
            ->GetBrokerForUser(user_id);
    bool privacy_warnings_enabled =
        g_browser_process->local_state()->GetBoolean(
            ash::prefs::kManagedGuestSessionPrivacyWarningsEnabled);
    if (ChromeUserManager::Get()->IsFullManagementDisclosureNeeded(broker) &&
        privacy_warnings_enabled) {
      ShowAutoLaunchManagedGuestSessionNotification();
    }
  }
  if (is_enterprise_managed) {
    enterprise_user_session_metrics::RecordSignInEvent(
        user_context, last_login_attempt_was_auto_login_);
  }
}

void ExistingUserController::ShowAutoLaunchManagedGuestSessionNotification() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DCHECK(connector->IsEnterpriseManaged());
  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_AUTO_LAUNCH_NOTIFICATION_BUTTON)));
  const base::string16 title =
      l10n_util::GetStringUTF16(IDS_AUTO_LAUNCH_NOTIFICATION_TITLE);
  const base::string16 message = l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_FULL_WARNING,
      base::UTF8ToUTF16(connector->GetEnterpriseDomainManager()));
  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](base::Optional<int> button_index) {
            DCHECK(button_index);
            SystemTrayClient::Get()->ShowEnterpriseInfo();
          }));
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kAutoLaunchNotificationId,
          title, message, base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kAutoLaunchNotifierId),
          data, std::move(delegate), vector_icons::kBusinessIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->SetSystemPriority();
  notification->set_pinned(true);
  SystemNotificationHelper::GetInstance()->Display(*notification);
}

void ExistingUserController::OnProfilePrepared(Profile* profile,
                                               bool browser_launched) {
  // Reenable clicking on other windows and status area.
  GetLoginDisplay()->SetUIEnabled(true);

  profile_prepared_ = true;

  chromeos::UserContext user_context =
      UserContext(*chromeos::ProfileHelper::Get()->GetUserByProfile(profile));
  auto* profile_connector = profile->GetProfilePolicyConnector();
  bool is_enterprise_managed =
      profile_connector->IsManaged() &&
      user_context.GetUserType() != user_manager::USER_TYPE_CHILD;
  user_manager::known_user::SetIsEnterpriseManaged(user_context.GetAccountId(),
                                                   is_enterprise_managed);

  if (is_enterprise_managed) {
    std::string manager = ManagementUIHandler::GetAccountManager(profile);
    if (!manager.empty()) {
      user_manager::known_user::SetAccountManager(user_context.GetAccountId(),
                                                  manager);
    }
  }

  // Inform `auth_status_consumers_` about successful login.
  // TODO(nkostylev): Pass UserContext back crbug.com/424550
  for (auto& auth_status_consumer : auth_status_consumers_)
    auth_status_consumer.OnAuthSuccess(user_context);
}

void ExistingUserController::OnOffTheRecordAuthSuccess() {
  is_login_in_progress_ = false;

  // Mark the device as registered., i.e. the second part of OOBE as completed.
  if (!StartupUtils::IsDeviceRegistered())
    StartupUtils::MarkDeviceRegistered(base::OnceClosure());

  UserSessionManager::GetInstance()->CompleteGuestSessionLogin(guest_mode_url_);

  for (auto& auth_status_consumer : auth_status_consumers_)
    auth_status_consumer.OnOffTheRecordAuthSuccess();
}

void ExistingUserController::OnPasswordChangeDetected(
    const UserContext& user_context) {
  is_login_in_progress_ = false;

  // Must not proceed without signature verification.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
          base::BindOnce(&ExistingUserController::OnPasswordChangeDetected,
                         weak_factory_.GetWeakPtr(), user_context))) {
    // Value of owner email is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }

  for (auto& auth_status_consumer : auth_status_consumers_)
    auth_status_consumer.OnPasswordChangeDetected(user_context);

  ShowPasswordChangedDialog(user_context);
}

void ExistingUserController::OnOldEncryptionDetected(
    const UserContext& user_context,
    bool has_incomplete_migration) {
  base::Optional<EncryptionMigrationMode> encryption_migration_mode =
      GetEncryptionMigrationMode(user_context, has_incomplete_migration);
  if (!encryption_migration_mode.has_value()) {
    ContinuePerformLoginWithoutMigration(login_performer_->auth_mode(),
                                         user_context);
    return;
  }
  ShowEncryptionMigrationScreen(user_context,
                                encryption_migration_mode.value());
}

void ExistingUserController::ForceOnlineLoginForAccountId(
    const AccountId& account_id) {
  // Save the necessity to sign-in online into UserManager in case the user
  // aborts the online flow.
  user_manager::UserManager::Get()->SaveForceOnlineSignin(account_id, true);
  GetLoginDisplayHost()->OnPreferencesChanged();

  // Start online sign-in UI for the user.
  is_login_in_progress_ = false;
  login_performer_.reset();
  GetLoginDisplayHost()->ShowGaiaDialog(account_id);
}

void ExistingUserController::AllowlistCheckFailed(const std::string& email) {
  PerformLoginFinishedActions(true /* start auto login timer */);

  GetLoginDisplay()->ShowAllowlistCheckFailedError();

  for (auto& auth_status_consumer : auth_status_consumers_) {
    auth_status_consumer.OnAuthFailure(
        AuthFailure(AuthFailure::ALLOWLIST_CHECK_FAILED));
  }

  ClearActiveDirectoryState();
  ClearRecordedNames();
}

void ExistingUserController::PolicyLoadFailed() {
  ShowError(IDS_LOGIN_ERROR_OWNER_KEY_LOST, "");

  PerformLoginFinishedActions(false /* don't start auto login timer */);
  ClearActiveDirectoryState();
  ClearRecordedNames();
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

void ExistingUserController::DeviceSettingsChanged() {
  // If login was already completed, we should avoid any signin screen
  // transitions, see http://crbug.com/461604 for example.
  if (!profile_prepared_ && GetLoginDisplay() &&
      !GetLoginDisplay()->is_signin_completed()) {
    // Signed settings or user list changed. Notify views and update them.
    const user_manager::UserList& users =
        UserAddingScreen::Get()->IsRunning()
            ? user_manager::UserManager::Get()->GetUsersAllowedForMultiProfile()
            : user_manager::UserManager::Get()->GetUsers();

    UpdateLoginDisplay(users);
    ConfigureAutoLogin();
  }
}

void ExistingUserController::AddLoginStatusConsumer(
    AuthStatusConsumer* consumer) {
  auth_status_consumers_.AddObserver(consumer);
}

void ExistingUserController::RemoveLoginStatusConsumer(
    const AuthStatusConsumer* consumer) {
  auth_status_consumers_.RemoveObserver(consumer);
}

LoginPerformer::AuthorizationMode ExistingUserController::auth_mode() const {
  if (login_performer_)
    return login_performer_->auth_mode();

  return auth_mode_;
}

bool ExistingUserController::password_changed() const {
  if (login_performer_)
    return login_performer_->password_changed();

  return password_changed_;
}

// static
user_manager::UserList ExistingUserController::ExtractLoginUsers(
    const user_manager::UserList& users) {
  bool show_users_on_signin;
  chromeos::CrosSettings::Get()->GetBoolean(
      chromeos::kAccountsPrefShowUserNamesOnSignIn, &show_users_on_signin);
  user_manager::UserList filtered_users;
  for (auto* user : users) {
    // Skip kiosk apps for login screen user list. Kiosk apps as pods (aka new
    // kiosk UI) is currently disabled and it gets the apps directly from
    // KioskAppManager, ArcKioskAppManager and WebKioskAppManager.
    if (user->IsKioskType())
      continue;
    // TODO(xiyuan): Clean user profile whose email is not in allowlist.
    if (user->GetType() == user_manager::USER_TYPE_SUPERVISED_DEPRECATED)
      continue;
    const bool meets_allowlist_requirements =
        !user->HasGaiaAccount() ||
        user_manager::UserManager::Get()->IsGaiaUserAllowed(*user);
    // Public session accounts are always shown on login screen.
    const bool meets_show_users_requirements =
        show_users_on_signin ||
        user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
    if (meets_allowlist_requirements && meets_show_users_requirements)
      filtered_users.push_back(user);
  }
  return filtered_users;
}

void ExistingUserController::LoginAsGuest() {
  PerformPreLoginActions(UserContext(user_manager::USER_TYPE_GUEST,
                                     user_manager::GuestAccountId()));

  bool allow_guest = user_manager::UserManager::Get()->IsGuestSessionAllowed();
  if (!allow_guest) {
    // Disallowed. The UI should normally not show the guest session button.
    LOG(ERROR) << "Guest login attempt when guest mode is disallowed.";
    PerformLoginFinishedActions(true /* start auto login timer */);
    ClearRecordedNames();
    return;
  }

  // Only one instance of LoginPerformer should exist at a time.
  login_performer_.reset(nullptr);
  login_performer_.reset(new ChromeLoginPerformer(this));
  login_performer_->LoginOffTheRecord();
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_OFFRECORD));
}

void ExistingUserController::LoginAsPublicSession(
    const UserContext& user_context) {
  VLOG(2) << "LoginAsPublicSession";
  PerformPreLoginActions(user_context);

  // If there is no public account with the given user ID, logging in is not
  // possible.
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(user_context.GetAccountId());
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    VLOG(2) << "Public session user not found";
    PerformLoginFinishedActions(true /* start auto login timer */);
    return;
  }

  // Public session login will fail if attempted if the associated policy store
  // is not initialized - wait for the policy store load before starting the
  // auto-login timer.
  policy::CloudPolicyStore* policy_store =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceLocalAccountPolicyService()
          ->GetBrokerForUser(user->GetAccountId().GetUserEmail())
          ->core()
          ->store();

  if (!policy_store->is_initialized()) {
    VLOG(2) << "Public session policy store not yet initialized";
    policy_store_waiter_ = std::make_unique<PolicyStoreLoadWaiter>(
        policy_store,
        base::BindOnce(
            &ExistingUserController::LoginAsPublicSessionWithPolicyStoreReady,
            base::Unretained(this), user_context));

    return;
  }

  LoginAsPublicSessionWithPolicyStoreReady(user_context);
}

void ExistingUserController::LoginAsPublicSessionWithPolicyStoreReady(
    const UserContext& user_context) {
  VLOG(2) << "LoginAsPublicSessionWithPolicyStoreReady";
  policy_store_waiter_.reset();

  UserContext new_user_context = user_context;
  std::string locale = user_context.GetPublicSessionLocale();
  if (locale.empty()) {
    // When performing auto-login, no locale is chosen by the user. Check
    // whether a list of recommended locales was set by policy. If so, use its
    // first entry. Otherwise, `locale` will remain blank, indicating that the
    // public session should use the current UI locale.
    const policy::PolicyMap::Entry* entry =
        g_browser_process->platform_part()
            ->browser_policy_connector_chromeos()
            ->GetDeviceLocalAccountPolicyService()
            ->GetBrokerForUser(user_context.GetAccountId().GetUserEmail())
            ->core()
            ->store()
            ->policy_map()
            .Get(policy::key::kSessionLocales);
    base::ListValue const* list = nullptr;
    if (entry && entry->level == policy::POLICY_LEVEL_RECOMMENDED &&
        entry->value() && entry->value()->GetAsList(&list)) {
      if (list->GetString(0, &locale))
        new_user_context.SetPublicSessionLocale(locale);
    }
  }

  if (!locale.empty() &&
      new_user_context.GetPublicSessionInputMethod().empty()) {
    // When `locale` is set, a suitable keyboard layout should be chosen. In
    // most cases, this will already be the case because the UI shows a list of
    // keyboard layouts suitable for the `locale` and ensures that one of them
    // us selected. However, it is still possible that `locale` is set but no
    // keyboard layout was chosen:
    // * The list of keyboard layouts is updated asynchronously. If the user
    //   enters the public session before the list of keyboard layouts for the
    //   `locale` has been retrieved, the UI will indicate that no keyboard
    //   layout was chosen.
    // * During auto-login, the `locale` is set in this method and a suitable
    //   keyboard layout must be chosen next.
    //
    // The list of suitable keyboard layouts is constructed asynchronously. Once
    // it has been retrieved, `SetPublicSessionKeyboardLayoutAndLogin` will
    // select the first layout from the list and continue login.
    VLOG(2) << "Requesting keyboard layouts for public session";
    GetKeyboardLayoutsForLocale(
        base::BindOnce(
            &ExistingUserController::SetPublicSessionKeyboardLayoutAndLogin,
            weak_factory_.GetWeakPtr(), new_user_context),
        locale);
    return;
  }

  // The user chose a locale and a suitable keyboard layout or left both unset.
  // Login can continue immediately.
  LoginAsPublicSessionInternal(new_user_context);
}

void ExistingUserController::LoginAsKioskApp(KioskAppId kiosk_app_id) {
  GetLoginDisplayHost()->StartKiosk(kiosk_app_id, /*auto_launch*/ false);
}

void ExistingUserController::ConfigureAutoLogin() {
  std::string auto_login_account_id;
  cros_settings_->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                            &auto_login_account_id);
  VLOG(2) << "Autologin account in prefs: " << auto_login_account_id;
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(cros_settings_);
  const bool show_update_required_screen = IsUpdateRequiredDeadlineReached();

  auto* data_snapshotd_manager =
      arc::data_snapshotd::ArcDataSnapshotdManager::Get();
  bool is_arc_data_snapshot_autologin =
      (data_snapshotd_manager &&
       data_snapshotd_manager->IsAutoLoginConfigured());
  if (is_arc_data_snapshot_autologin) {
    public_session_auto_login_account_id_ =
        GetArcDataSnapshotAutoLoginAccountId(device_local_accounts);
  } else {
    public_session_auto_login_account_id_ = GetPublicSessionAutoLoginAccountId(
        device_local_accounts, auto_login_account_id);
  }

  const user_manager::User* public_session_user =
      user_manager::UserManager::Get()->FindUser(
          public_session_auto_login_account_id_);
  if (!public_session_user || public_session_user->GetType() !=
                                  user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    VLOG(2) << "PublicSession autologin user not found";
    public_session_auto_login_account_id_ = EmptyAccountId();
  }

  if (is_arc_data_snapshot_autologin ||
      !cros_settings_->GetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay,
                                  &auto_login_delay_)) {
    auto_login_delay_ = 0;
  }

  // TODO(crbug.com/1105387): Part of initial screen logic.
  if (show_update_required_screen) {
    // Update required screen overrides public session auto login.
    StopAutoLoginTimer();
    GetLoginDisplayHost()->StartWizard(UpdateRequiredView::kScreenId);
  } else if (public_session_auto_login_account_id_.is_valid()) {
    StartAutoLoginTimer();
  } else {
    StopAutoLoginTimer();
  }
}

void ExistingUserController::ResetAutoLoginTimer() {
  // Only restart the auto-login timer if it's already running.
  if (auto_login_timer_ && auto_login_timer_->IsRunning()) {
    StopAutoLoginTimer();
    StartAutoLoginTimer();
  }
}

void ExistingUserController::OnPublicSessionAutoLoginTimerFire() {
  CHECK(public_session_auto_login_account_id_.is_valid());
  VLOG(2) << "Public session autologin fired";
  SigninSpecifics signin_specifics;
  signin_specifics.is_auto_login = true;
  Login(UserContext(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                    public_session_auto_login_account_id_),
        signin_specifics);
}

void ExistingUserController::StopAutoLoginTimer() {
  VLOG(2) << "Stopping autologin timer that is "
          << (auto_login_timer_ ? "" : "not ") << "running";
  if (auto_login_timer_)
    auto_login_timer_->Stop();
}

void ExistingUserController::CancelPasswordChangedFlow() {
  login_performer_.reset(nullptr);
  ClearActiveDirectoryState();
  PerformLoginFinishedActions(true /* start auto login timer */);
}

void ExistingUserController::MigrateUserData(const std::string& old_password) {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get()) {
    VLOG(1) << "Migrate the existing cryptohome to new password.";
    login_performer_->RecoverEncryptedData(old_password);
  }
}

void ExistingUserController::ResyncUserData() {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get()) {
    VLOG(1) << "Create a new cryptohome and resync user data.";
    login_performer_->ResyncEncryptedData();
  }
}

void ExistingUserController::StartAutoLoginTimer() {
  if (is_login_in_progress_ ||
      !public_session_auto_login_account_id_.is_valid()) {
    VLOG(2) << "Not starting autologin timer, because:";
    VLOG_IF(2, is_login_in_progress_) << "* Login is in process;";
    VLOG_IF(2, !public_session_auto_login_account_id_.is_valid())
        << "* No valid autologin account;";
    return;
  }
  VLOG(2) << "Starting autologin timer with delay: " << auto_login_delay_;

  if (auto_login_timer_ && auto_login_timer_->IsRunning()) {
    StopAutoLoginTimer();
  }

  // Block auto-login flow until ArcDataSnapshotdManager is ready to enter an
  // auto-login session.
  // ArcDataSnapshotdManager stores a reset auto-login callback to fire it once
  // it is ready.
  auto* data_snapshotd_manager =
      arc::data_snapshotd::ArcDataSnapshotdManager::Get();
  if (data_snapshotd_manager && !data_snapshotd_manager->IsAutoLoginAllowed() &&
      data_snapshotd_manager->IsAutoLoginConfigured()) {
    data_snapshotd_manager->set_reset_autologin_callback(
        base::BindOnce(&ExistingUserController::ResetAutoLoginTimer,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // Start the auto-login timer.
  if (!auto_login_timer_)
    auto_login_timer_.reset(new base::OneShotTimer);

  VLOG(2) << "Public session autologin will be fired in " << auto_login_delay_
          << "ms";
  auto_login_timer_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(auto_login_delay_),
      base::BindOnce(&ExistingUserController::OnPublicSessionAutoLoginTimerFire,
                     weak_factory_.GetWeakPtr()));
}

void ExistingUserController::ShowError(int error_id,
                                       const std::string& details) {
  VLOG(1) << details;
  GetLoginDisplay()->ShowError(error_id, num_login_attempts_,
                               HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
}

void ExistingUserController::SendAccessibilityAlert(
    const std::string& alert_text) {
  AutomationManagerAura::GetInstance()->HandleAlert(alert_text);
}

void ExistingUserController::SetPublicSessionKeyboardLayoutAndLogin(
    const UserContext& user_context,
    std::unique_ptr<base::ListValue> keyboard_layouts) {
  UserContext new_user_context = user_context;
  std::string keyboard_layout;
  for (size_t i = 0; i < keyboard_layouts->GetSize(); ++i) {
    base::DictionaryValue* entry = nullptr;
    keyboard_layouts->GetDictionary(i, &entry);
    bool selected = false;
    entry->GetBoolean("selected", &selected);
    if (selected) {
      entry->GetString("value", &keyboard_layout);
      break;
    }
  }
  DCHECK(!keyboard_layout.empty());
  new_user_context.SetPublicSessionInputMethod(keyboard_layout);

  LoginAsPublicSessionInternal(new_user_context);
}

void ExistingUserController::LoginAsPublicSessionInternal(
    const UserContext& user_context) {
  // Only one instance of LoginPerformer should exist at a time.
  VLOG(2) << "LoginAsPublicSessionInternal for user: "
          << user_context.GetAccountId();
  login_performer_.reset(nullptr);
  login_performer_.reset(new ChromeLoginPerformer(this));
  login_performer_->LoginAsPublicSession(user_context);
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_PUBLIC_ACCOUNT));
}

void ExistingUserController::PerformPreLoginActions(
    const UserContext& user_context) {
  // Disable clicking on other windows and status tray.
  GetLoginDisplay()->SetUIEnabled(false);

  if (last_login_attempt_account_id_ != user_context.GetAccountId()) {
    last_login_attempt_account_id_ = user_context.GetAccountId();
    num_login_attempts_ = 0;
  }
  num_login_attempts_++;

  // Stop the auto-login timer when attempting login.
  StopAutoLoginTimer();
}

void ExistingUserController::PerformLoginFinishedActions(
    bool start_auto_login_timer) {
  is_login_in_progress_ = false;

  // Reenable clicking on other windows and status area.
  GetLoginDisplay()->SetUIEnabled(true);

  if (start_auto_login_timer)
    StartAutoLoginTimer();
}

void ExistingUserController::ContinueLoginWhenCryptohomeAvailable(
    base::OnceClosure continuation,
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "Cryptohome service is not available";
    OnAuthFailure(AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));
    return;
  }
  std::move(continuation).Run();
}

void ExistingUserController::ContinueLoginIfDeviceNotDisabled(
    base::OnceClosure continuation) {
  // Disable clicking on other windows and status tray.
  GetLoginDisplay()->SetUIEnabled(false);

  // Stop the auto-login timer.
  StopAutoLoginTimer();

  auto split_continuation = base::SplitOnceCallback(std::move(continuation));

  // Wait for the `cros_settings_` to become either trusted or permanently
  // untrusted.
  const CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(base::BindOnce(
          &ExistingUserController::ContinueLoginIfDeviceNotDisabled,
          weak_factory_.GetWeakPtr(), std::move(split_continuation.first)));
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the `cros_settings_` are permanently untrusted, show an error message
    // and refuse to log in.
    GetLoginDisplay()->ShowError(IDS_LOGIN_ERROR_OWNER_KEY_LOST, 1,
                                 HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);

    // Re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer though. Without trusted `cros_settings_`, no auto-login
    // can succeed.
    GetLoginDisplay()->SetUIEnabled(true);
    return;
  }

  if (system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation()) {
    // If the device is disabled, bail out. A device disabled screen will be
    // shown by the DeviceDisablingManager.

    // Re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer though. On a disabled device, no auto-login can succeed.
    GetLoginDisplay()->SetUIEnabled(true);
    return;
  }

  CryptohomeClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &ExistingUserController::ContinueLoginWhenCryptohomeAvailable,
      weak_factory_.GetWeakPtr(), std::move(split_continuation.second)));
}

void ExistingUserController::DoCompleteLogin(
    const UserContext& user_context_wo_device_id) {
  UserContext user_context = user_context_wo_device_id;
  std::string device_id =
      user_manager::known_user::GetDeviceId(user_context.GetAccountId());
  if (device_id.empty()) {
    bool is_ephemeral = ChromeUserManager::Get()->AreEphemeralUsersEnabled() &&
                        user_context.GetAccountId() !=
                            ChromeUserManager::Get()->GetOwnerAccountId();
    device_id = GenerateSigninScopedDeviceId(is_ephemeral);
  }
  user_context.SetDeviceId(device_id);

  const std::string& gaps_cookie = user_context.GetGAPSCookie();
  if (!gaps_cookie.empty()) {
    user_manager::known_user::SetGAPSCookie(user_context.GetAccountId(),
                                            gaps_cookie);
  }

  PerformPreLoginActions(user_context);

  if (timer_init_) {
    base::UmaHistogramMediumTimes("Login.PromptToCompleteLoginTime",
                                  timer_init_->Elapsed());
    timer_init_.reset();
  }

  // Fetch OAuth2 tokens if we have an auth code.
  if (!user_context.GetAuthCode().empty()) {
    oauth2_token_initializer_.reset(new OAuth2TokenInitializer);
    oauth2_token_initializer_->Start(
        user_context,
        base::BindOnce(&ExistingUserController::OnOAuth2TokensFetched,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  PerformLogin(user_context, LoginPerformer::AuthorizationMode::kExternal);
}

void ExistingUserController::DoLogin(const UserContext& user_context,
                                     const SigninSpecifics& specifics) {
  last_login_attempt_was_auto_login_ = specifics.is_auto_login;
  screen_refresh_timer_->Stop();
  VLOG(2) << "DoLogin with a user type: " << user_context.GetUserType();

  if (user_context.GetUserType() == user_manager::USER_TYPE_GUEST) {
    if (!specifics.guest_mode_url.empty()) {
      guest_mode_url_ = GURL(specifics.guest_mode_url);
      if (specifics.guest_mode_url_append_locale)
        guest_mode_url_ = google_util::AppendGoogleLocaleParam(
            guest_mode_url_, g_browser_process->GetApplicationLocale());
    }
    LoginAsGuest();
    return;
  }

  if (user_context.GetUserType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    LoginAsPublicSession(user_context);
    return;
  }

  if (user_context.GetUserType() == user_manager::USER_TYPE_KIOSK_APP) {
    LoginAsKioskApp(
        KioskAppId::ForChromeApp(user_context.GetAccountId().GetUserEmail()));
    return;
  }

  if (user_context.GetUserType() == user_manager::USER_TYPE_ARC_KIOSK_APP) {
    LoginAsKioskApp(KioskAppId::ForArcApp(user_context.GetAccountId()));
    return;
  }

  if (user_context.GetUserType() == user_manager::USER_TYPE_WEB_KIOSK_APP) {
    LoginAsKioskApp(KioskAppId::ForWebApp(user_context.GetAccountId()));
    return;
  }

  // Regular user or supervised user login.

  if (!user_context.HasCredentials()) {
    // If credentials are missing, refuse to log in.

    // Reenable clicking on other windows and status area.
    GetLoginDisplay()->SetUIEnabled(true);
    // Restart the auto-login timer.
    StartAutoLoginTimer();
  }

  PerformPreLoginActions(user_context);
  PerformLogin(user_context, LoginPerformer::AuthorizationMode::kInternal);
}

void ExistingUserController::OnOAuth2TokensFetched(
    bool success,
    const UserContext& user_context) {
  if (!success) {
    LOG(ERROR) << "OAuth2 token fetch failed.";
    OnAuthFailure(AuthFailure(AuthFailure::FAILED_TO_INITIALIZE_TOKEN));
    return;
  }
  PerformLogin(user_context, LoginPerformer::AuthorizationMode::kExternal);
}

void ExistingUserController::ClearRecordedNames() {
  display_email_.clear();
  display_name_.clear();
  given_name_.clear();
}

void ExistingUserController::ClearActiveDirectoryState() {
  if (last_login_attempt_account_id_.GetAccountType() !=
      AccountType::ACTIVE_DIRECTORY) {
    return;
  }
  // Clear authpolicyd state so nothing could leak from one user to another.
  AuthPolicyHelper::Restart();
}

}  // namespace chromeos
