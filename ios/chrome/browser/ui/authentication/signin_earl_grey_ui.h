// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"

@class FakeChromeIdentity;

typedef NS_ENUM(NSInteger, SignOutConfirmationChoice) {
  SignOutConfirmationChoiceClearData,
  SignOutConfirmationChoiceKeepData,
  SignOutConfirmationChoiceNotSyncing
};

// Test methods that perform sign in actions on Chrome UI.
@interface SigninEarlGreyUI : NSObject

// Calls +[SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES].
+ (void)signinWithFakeIdentity:(FakeChromeIdentity*)fakeIdentity;

// Signs the account for |fakeIdentity| into Chrome through the Settings screen,
// with sync enabled or not according to |enableSync|.
// There will be a GREYAssert if the tools menus is open when calling this
// method or if the account is not successfully signed in.
+ (void)signinWithFakeIdentity:(FakeChromeIdentity*)fakeIdentity
                    enableSync:(BOOL)enableSync;

// Signs the primary account out of Chrome through the accounts list screen.
// Taps the "Sign Out" button to begin flow. Note that managed accounts cannot
// go through this flow. There will be a GREYAssert if the tools menus is open
// when calling this method or if the account is not successfully signed out.
// This method should be used until SimplifySignOutIOS is turned on. Otherwise,
// |signOutWithConfirmationChoice:| should be used.
+ (void)signOut;

// Signs the primary account out of Chrome through the accounts list screen.
// Taps the "Sign out and clear data from this device" button to begin flow.
// There will be a GREYAssert if the tools menus is open when calling this
// method or if the account is not successfully signed out.
// This method should be used until SimplifySignOutIOS is turned on. Otherwise,
// |signOutWithConfirmationChoice:| should be used.
+ (void)signOutAndClearDataFromDevice;

// Signs the primary account out of Chrome through the accounts list screen.
// Taps the "Sign Out" button, and then validated the confirmation dialog
// according to |confirmation|.
// This method should be used only if SimplifySignOutIOS is turned on.
+ (void)signOutWithConfirmationChoice:(SignOutConfirmationChoice)confirmation;

// Selects an identity when the identity chooser dialog is presented. The dialog
// is confirmed, but it doesn't validated the user consent page.
+ (void)selectIdentityWithEmail:(NSString*)userEmail;

// Taps on the settings link in the sign-in view. The sign-in view has to be
// opened before calling this method.
+ (void)tapSettingsLink;

// Taps the sign in confirmation page, scrolls first to make the OK button
// visible on short devices (e.g. iPhone 5s).
+ (void)tapSigninConfirmationDialog;

// Taps on the "ADD ACCOUNT" button in the unified consent, to display the
// SSO dialog.
// This method should only be used with UnifiedConsent flag.
+ (void)tapAddAccountButton;

// Opens the confirmation dialog to remove an account from the device, without
// confirming it.
+ (void)openRemoveAccountConfirmationDialogWithFakeIdentity:
    (FakeChromeIdentity*)fakeIdentity;

// Taps "Remove account from this device" button and follow-up confirmation.
// Assumes the user is on the Settings screen.
+ (void)tapRemoveAccountFromDeviceWithFakeIdentity:
    (FakeChromeIdentity*)fakeIdentity;

// Opens the recent tabs and tap in the primary sign-in button.
+ (void)tapPrimarySignInButtonInRecentTabs;

// Opens the tab switcher and tap in the primary sign-in button.
+ (void)tapPrimarySignInButtonInTabSwitcher;

// Checks that the sign-in promo view (with a close button) is visible using the
// right mode.
+ (void)verifySigninPromoVisibleWithMode:(SigninPromoViewMode)mode;

// Checks that the sign-in promo view is visible using the right mode. If
// |closeButton| is set to YES, the close button in the sign-in promo has to be
// visible.
+ (void)verifySigninPromoVisibleWithMode:(SigninPromoViewMode)mode
                             closeButton:(BOOL)closeButton;

// Checks that the sign-in promo view is not visible.
+ (void)verifySigninPromoNotVisible;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_H_
