// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_
#define CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_

#include <string>

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/startup/startup_types.h"

#if defined(OS_ANDROID)
#error "Not used on Android"
#endif

class Profile;

namespace base { class FilePath; }

namespace profiles {

// Returns the path of the profile connected to the given email.  If no profile
// is found an empty file path is returned.
base::FilePath GetPathOfProfileWithEmail(ProfileManager* profile_manager,
                                         const std::string& email);

// Activates a window for |profile| on the desktop specified by
// |desktop_type|. If no such window yet exists, or if |always_create| is
// true, this first creates a new window, then activates
// that. If activating an exiting window and multiple windows exists then the
// window that was most recently active is activated. This is used for
// creation of a window from the multi-profile dropdown menu.
void FindOrCreateNewWindowForProfile(
    Profile* profile,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    bool always_create);

// Opens a Browser for |profile|.
// If |always_create| is true a window is created even if one already exists.
// If |is_new_profile| is true a first run window is created.
// If |unblock_extensions| is true, all extensions are unblocked.
// When the browser is opened, |callback| will be run if it isn't null.
void OpenBrowserWindowForProfile(ProfileManager::CreateCallback callback,
                                 bool always_create,
                                 bool is_new_profile,
                                 bool unblock_extensions,
                                 Profile* profile,
                                 Profile::CreateStatus status);

// Loads the specified profile given by |path| asynchronously. Once profile is
// loaded and initialized it runs |callback| if it isn't null.
void LoadProfileAsync(const base::FilePath& path,
                      ProfileManager::CreateCallback callback);

// Opens a Browser with the specified profile given by |path|.
// If |always_create| is true then a new window is created
// even if a window for that profile already exists. When the browser is
// opened, |callback| will be run if it isn't null.
void SwitchToProfile(const base::FilePath& path,
                     bool always_create,
                     ProfileManager::CreateCallback callback);

// Opens a Browser for the guest profile and runs |callback| if it isn't null.
void SwitchToGuestProfile(ProfileManager::CreateCallback callback);

// Returns true if |profile| has potential profile switch targets, ie there's at
// least one other profile available to switch to, not counting guest. This is
// the case when there are more than 1 profiles available or when there's only
// one and the current window is a guest window.
bool HasProfileSwitchTargets(Profile* profile);

// Closes and locks all the browser windows for |profile| and opens the user
// manager.
void LockProfile(Profile* profile);

// Close all the browser windows for |profile|.
void CloseProfileWindows(Profile* profile);

// Returns whether lock is available to this profile.
bool IsLockAvailable(Profile* profile);

// Converts from modes in the avatar menu to modes understood by
// ProfileMenuView.
void BubbleViewModeFromAvatarBubbleMode(BrowserWindow::AvatarBubbleMode mode,
                                        Profile* profile,
                                        BubbleViewMode* bubble_view_mode);

// Handles running a callback when a new Browser for the given profile
// has been completely created.  This object deletes itself once the browser
// is created and the callback is executed.
class BrowserAddedForProfileObserver : public BrowserListObserver {
 public:
  BrowserAddedForProfileObserver(Profile* profile,
                                 ProfileManager::CreateCallback callback);
  BrowserAddedForProfileObserver(const BrowserAddedForProfileObserver&) =
      delete;
  BrowserAddedForProfileObserver& operator=(
      const BrowserAddedForProfileObserver&) = delete;
  ~BrowserAddedForProfileObserver() override;

 private:
  // Overridden from BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

  // Profile for which the browser should be opened.
  Profile* profile_;
  ProfileManager::CreateCallback callback_;
};

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_
