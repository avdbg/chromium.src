// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILES_STATE_H_
#define CHROME_BROWSER_PROFILES_PROFILES_STATE_H_

#include <string>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include <vector>

#include "chrome/browser/profiles/avatar_menu.h"
#endif

struct AccountInfo;
struct CoreAccountInfo;
class Browser;
class PrefRegistrySimple;
class PrefService;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace profiles {

// Assortment of methods for dealing with profiles.
// TODO(michaelpg): Most of these functions can be inlined or moved to more
// appropriate locations.

// Checks if multiple profiles is enabled.
bool IsMultipleProfilesEnabled();

// Returns the path to the default profile directory, based on the given
// user data directory.
base::FilePath GetDefaultProfileDir(const base::FilePath& user_data_dir);

// Register multi-profile related preferences in Local State.
void RegisterPrefs(PrefRegistrySimple* registry);

// Sets the last used profile pref to |profile_dir|, unless |profile_dir| is the
// System Profile directory, which is an invalid last used profile.
void SetLastUsedProfile(const std::string& profile_dir);

#if !defined(OS_ANDROID)
// Returns the display name of the specified on-the-record profile (or guest),
// specified by |profile_path|, used in the avatar button or user manager. If
// |profile_path| is the guest path, it will return IDS_GUEST_PROFILE_NAME. If
// there is only one local profile present, it will return
// IDS_SINGLE_PROFILE_DISPLAY_NAME, unless the profile has a user entered
// custom name.
base::string16 GetAvatarNameForProfile(const base::FilePath& profile_path);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Returns the string to use in the fast user switcher menu for the specified
// menu item. Adds a supervision indicator to the profile name if appropriate.
base::string16 GetProfileSwitcherTextForItem(const AvatarMenu::Item& item);

// Update the name of |profile| to |new_profile_name|. This updates the profile
// preferences, which triggers an update in the ProfileAttributesStorage. This
// method should be called when the user is explicitely changing the profile
// name, as it will always set |prefs::kProfileUsingDefaultName| to false.
void UpdateProfileName(Profile* profile,
                       const base::string16& new_profile_name);

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Returns whether the |browser|'s profile is not incognito (a regular profile
// or a guest session).
// The distinction is needed because guest profiles and incognito profiles are
// implemented as off-the-record profiles.
bool IsRegularOrGuestSession(Browser* browser);

// Returns true if starting in guest mode is requested at startup (e.g. through
// command line argument). If |show_warning| is true, send a warning if guest
// mode is requested but not allowed by policy.
bool IsGuestModeRequested(const base::CommandLine& command_line,
                          PrefService* local_state,
                          bool show_warning);

// Returns true if sign in is required to browse as this profile.  Call with
// profile->GetPath() if you have a profile pointer.
// TODO(mlerman): Refactor appropriate calls to
// ProfileAttributesStorage::IsSigninRequired to call here instead.
bool IsProfileLocked(const base::FilePath& profile_path);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Starts an update for a new version of the Gaia profile picture and other
// profile info.
void UpdateGaiaProfileInfoIfNeeded(Profile* profile);

// If the current active profile (given by prefs::kProfileLastUsed) is locked,
// changes the active profile to the Guest profile. Returns true if the active
// profile had been Guest before calling or became Guest as a result of this
// method.
bool SetActiveProfileToGuestIfLocked();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// If the profile given by |profile_path| is loaded in the ProfileManager, use
// a BrowsingDataRemover to delete all the Profile's data.
void RemoveBrowsingDataForProfile(const base::FilePath& profile_path);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Returns true if there exists at least one non-supervised or non-child profile
// and they are all locked.
bool AreAllNonChildNonSupervisedProfilesLocked();
#endif

// Returns whether a public session is being run currently.
bool IsPublicSession();

// Returns whether public session restrictions are enabled.
bool ArePublicSessionRestrictionsEnabled();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Returns the default name for a new enterprise profile.
base::string16 GetDefaultNameForNewEnterpriseProfile(
    const std::string& hosted_domain = std::string());

// Returns the default name for a new signed-in profile, based on
// `account_info`.
base::string16 GetDefaultNameForNewSignedInProfile(
    const AccountInfo& account_info);

// The same as above but using incomplete account info.
base::string16 GetDefaultNameForNewSignedInProfileWithIncompleteInfo(
    const CoreAccountInfo& account_info);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // !defined(OS_ANDROID)

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILES_STATE_H_
