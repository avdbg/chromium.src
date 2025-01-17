// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_window.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/escape.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#endif  // !defined (OS_ANDROID)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/profile_picker.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

using base::UserMetricsAction;
using content::BrowserThread;

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
void BlockExtensions(Profile* profile) {
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extension_service->BlockAllExtensions();
}

void UnblockExtensions(Profile* profile) {
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extension_service->UnblockAllExtensions();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Called in profiles::LoadProfileAsync once profile is loaded. It runs
// |callback| if it isn't null.
void ProfileLoadedCallback(ProfileManager::CreateCallback callback,
                           Profile* profile,
                           Profile::CreateStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  if (!callback.is_null())
    callback.Run(profile, Profile::CREATE_STATUS_INITIALIZED);
}

}  // namespace

namespace profiles {

base::FilePath GetPathOfProfileWithEmail(ProfileManager* profile_manager,
                                         const std::string& email) {
  base::string16 profile_email = base::UTF8ToUTF16(email);
  std::vector<ProfileAttributesEntry*> entries =
      profile_manager->GetProfileAttributesStorage().GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (entry->GetUserName() == profile_email)
      return entry->GetPath();
  }
  return base::FilePath();
}

void FindOrCreateNewWindowForProfile(
    Profile* profile,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    bool always_create) {
  DCHECK(profile);

  if (!always_create) {
    Browser* browser = chrome::FindTabbedBrowser(profile, false);
    if (browser) {
      browser->window()->Activate();
      return;
    }
  }

  base::RecordAction(UserMetricsAction("NewWindow"));
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  // This is not a browser launch from the user; don't record the launch mode.
  browser_creator.LaunchBrowser(command_line, profile, base::FilePath(),
                                process_startup, is_first_run,
                                /*launch_mode_recorder=*/nullptr);
}

void OpenBrowserWindowForProfile(ProfileManager::CreateCallback callback,
                                 bool always_create,
                                 bool is_new_profile,
                                 bool unblock_extensions,
                                 Profile* profile,
                                 Profile::CreateStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  chrome::startup::IsProcessStartup is_process_startup =
      chrome::startup::IS_NOT_PROCESS_STARTUP;
  chrome::startup::IsFirstRun is_first_run = chrome::startup::IS_NOT_FIRST_RUN;

  // If this is a brand new profile, then start a first run window.
  if (is_new_profile) {
    is_process_startup = chrome::startup::IS_PROCESS_STARTUP;
    is_first_run = chrome::startup::IS_FIRST_RUN;
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!profile->IsGuestSession() && !profile->IsEphemeralGuestProfile()) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath());
    if (entry && entry->IsSigninRequired()) {
      ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileLocked);
      return;
    }
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (unblock_extensions)
    UnblockExtensions(profile);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // If |always_create| is false, and we have a |callback| to run, check
  // whether a browser already exists so that we can run the callback. We don't
  // want to rely on the observer listening to OnBrowserSetLastActive in this
  // case, as you could manually activate an incorrect browser and trigger
  // a false positive.
  if (!always_create) {
    Browser* browser = chrome::FindTabbedBrowser(profile, false);
    if (browser) {
      browser->window()->Activate();
      if (!callback.is_null())
        callback.Run(profile, Profile::CREATE_STATUS_INITIALIZED);
      return;
    }
  }

  // If there is a callback, create an observer to make sure it is only
  // run when the browser has been completely created. This observer will
  // delete itself once that happens. This should not leak, because we are
  // passing |always_create| = true to FindOrCreateNewWindow below, which ends
  // up calling LaunchBrowser and opens a new window. If for whatever reason
  // that fails, either something has crashed, or the observer will be cleaned
  // up when a different browser for this profile is opened.
  if (!callback.is_null())
    new BrowserAddedForProfileObserver(profile, callback);

  // We already dealt with the case when |always_create| was false and a browser
  // existed, which means that here a browser definitely needs to be created.
  // Passing true for |always_create| means we won't duplicate the code that
  // tries to find a browser.
  profiles::FindOrCreateNewWindowForProfile(profile, is_process_startup,
                                            is_first_run, true);
}

#if !defined(OS_ANDROID)

void LoadProfileAsync(const base::FilePath& path,
                      ProfileManager::CreateCallback callback) {
  g_browser_process->profile_manager()->CreateProfileAsync(
      path, base::BindRepeating(&ProfileLoadedCallback, callback),
      base::string16(), std::string());
}

void SwitchToProfile(const base::FilePath& path,
                     bool always_create,
                     ProfileManager::CreateCallback callback) {
  g_browser_process->profile_manager()->CreateProfileAsync(
      path,
      base::BindRepeating(&profiles::OpenBrowserWindowForProfile, callback,
                          always_create, false, false),
      base::string16(), std::string());
}

void SwitchToGuestProfile(ProfileManager::CreateCallback callback) {
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileManager::GetGuestProfilePath(),
      base::BindRepeating(&profiles::OpenBrowserWindowForProfile, callback,
                          false, false, false),
      base::string16(), std::string());
}
#endif

bool HasProfileSwitchTargets(Profile* profile) {
  size_t min_profiles =
      (profile->IsGuestSession() || profile->IsEphemeralGuestProfile()) ? 1 : 2;
  size_t number_of_profiles =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  return number_of_profiles >= min_profiles;
}

void LockBrowserCloseSuccess(const base::FilePath& profile_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  DCHECK(entry);
  entry->SetIsSigninRequired(true);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Profile guaranteed to exist for it to have been locked.
  BlockExtensions(profile_manager->GetProfileByPath(profile_path));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  chrome::HideTaskManager();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileLocked);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

void LockProfile(Profile* profile) {
  DCHECK(profile);
  if (profile) {
    BrowserList::CloseAllBrowsersWithProfile(
        profile, base::BindRepeating(&LockBrowserCloseSuccess),
        BrowserList::CloseCallback(), false);
  }
}

bool IsLockAvailable(Profile* profile) {
  DCHECK(profile);
  if (profile->IsGuestSession() || profile->IsSystemProfile() ||
      profile->IsEphemeralGuestProfile()) {
    return false;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync))
    return false;
  base::Optional<AccountInfo> primary_account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync));
  std::string hosted_domain = primary_account_info.has_value()
                                  ? primary_account_info.value().hosted_domain
                                  : "";

  // TODO(mlerman): Prohibit only users who authenticate using SAML. Until then,
  // prohibited users who use hosted domains (aside from google.com).
  if (hosted_domain != kNoHostedDomainFound && hosted_domain != "google.com") {
    return false;
  }

  // Lock only when there is at least one supervised user on the machine.
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (entry->IsSupervised())
      return true;
  }
  return false;
}

void CloseProfileWindows(Profile* profile) {
  DCHECK(profile);
  BrowserList::CloseAllBrowsersWithProfile(profile,
                                           BrowserList::CloseCallback(),
                                           BrowserList::CloseCallback(), false);
}

void BubbleViewModeFromAvatarBubbleMode(BrowserWindow::AvatarBubbleMode mode,
                                        Profile* profile,
                                        BubbleViewMode* bubble_view_mode) {
  switch (mode) {
    case BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN:
      *bubble_view_mode = BUBBLE_VIEW_MODE_GAIA_SIGNIN;
      return;
    case BrowserWindow::AVATAR_BUBBLE_MODE_ADD_ACCOUNT:
      *bubble_view_mode = BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT;
      return;
    case BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH:
      *bubble_view_mode = BUBBLE_VIEW_MODE_GAIA_REAUTH;
      return;
    case BrowserWindow::AVATAR_BUBBLE_MODE_CONFIRM_SIGNIN:
      *bubble_view_mode = BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
      return;
    case BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT:
      *bubble_view_mode = profile->IsIncognitoProfile()
                              ? profiles::BUBBLE_VIEW_MODE_INCOGNITO
                              : profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
  }
}

BrowserAddedForProfileObserver::BrowserAddedForProfileObserver(
    Profile* profile,
    ProfileManager::CreateCallback callback)
    : profile_(profile), callback_(callback) {
  DCHECK(!callback_.is_null());
  BrowserList::AddObserver(this);
}

BrowserAddedForProfileObserver::~BrowserAddedForProfileObserver() {}

void BrowserAddedForProfileObserver::OnBrowserAdded(Browser* browser) {
  if (browser->profile() == profile_) {
    BrowserList::RemoveObserver(this);
    // By the time the browser is added a tab (or multiple) are about to be
    // added. Post the callback to the message loop so it gets executed after
    // the tabs are created.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback_, profile_,
                                  Profile::CREATE_STATUS_INITIALIZED));
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }
}

}  // namespace profiles
