// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/credential_provider_view_controller.h"

#import <Foundation/Foundation.h>

#include "base/check.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/credential_provider_extension/account_verification_provider.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"
#import "ios/chrome/credential_provider_extension/password_util.h"
#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"
#import "ios/chrome/credential_provider_extension/ui/consent_coordinator.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_coordinator.h"
#import "ios/chrome/credential_provider_extension/ui/stale_credentials_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialProviderViewController () <ConfirmationAlertActionHandler,
                                                SuccessfulReauthTimeAccessor>

// Interface for the persistent credential store.
@property(nonatomic, strong) id<CredentialStore> credentialStore;

// List coordinator that shows the list of passwords when started.
@property(nonatomic, strong) CredentialListCoordinator* listCoordinator;

// Consent coordinator that shows a view requesting device auth in order to
// enable the extension.
@property(nonatomic, strong) ConsentCoordinator* consentCoordinator;

// Date kept for ReauthenticationModule.
@property(nonatomic, strong) NSDate* lastSuccessfulReauthTime;

// Reauthentication Module used for reauthentication.
@property(nonatomic, strong) ReauthenticationModule* reauthenticationModule;

// Interface for |reauthenticationModule|, handling mostly the case when no
// hardware for authentication is available.
@property(nonatomic, strong) ReauthenticationHandler* reauthenticationHandler;

// Interface for verified that accounts are still valid.
@property(nonatomic, strong) AccountVerificationProvider* accountVerificator;

// Loading indicator used for user validation, which APIs can take a long time.
@property(nonatomic, strong) UIActivityIndicatorView* activityIndicatorView;

@end

@implementation CredentialProviderViewController

#pragma mark - ASCredentialProviderViewController

- (void)prepareCredentialListForServiceIdentifiers:
    (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers {
  __weak __typeof__(self) weakSelf = self;
  [self validateUserWithCompletion:^(BOOL userIsValid) {
    if (!userIsValid) {
      [weakSelf showStaleCredentials];
      return;
    }
    [weakSelf reauthenticateIfNeededWithCompletionHandler:^(
                  ReauthenticationResult result) {
      if (result != ReauthenticationResult::kFailure) {
        [weakSelf showCredentialListForServiceIdentifiers:serviceIdentifiers];
      } else {
        [weakSelf exitWithErrorCode:ASExtensionErrorCodeFailed];
      }
    }];
  }];
}

- (void)provideCredentialWithoutUserInteractionForIdentity:
    (ASPasswordCredentialIdentity*)credentialIdentity {
  __weak __typeof__(self) weakSelf = self;
  [self validateUserWithCompletion:^(BOOL userIsValid) {
    // reauthenticationModule can't attempt reauth when no password is set. This
    // means a password shouldn't be retrieved.
    if (!weakSelf.reauthenticationModule.canAttemptReauth || !userIsValid) {
      [weakSelf exitWithErrorCode:ASExtensionErrorCodeUserInteractionRequired];
      return;
    }
    // iOS already gates the password with device auth for
    // -provideCredentialWithoutUserInteractionForIdentity:. Not using
    // reauthenticationModule here to avoid a double authentication request.
    [weakSelf provideCredentialForIdentity:credentialIdentity];
  }];
}

- (void)prepareInterfaceToProvideCredentialForIdentity:
    (ASPasswordCredentialIdentity*)credentialIdentity {
  __weak __typeof__(self) weakSelf = self;
  [self validateUserWithCompletion:^(BOOL userIsValid) {
    if (!userIsValid) {
      [weakSelf showStaleCredentials];
      return;
    }
    [weakSelf reauthenticateIfNeededWithCompletionHandler:^(
                  ReauthenticationResult result) {
      if (result != ReauthenticationResult::kFailure) {
        [weakSelf provideCredentialForIdentity:credentialIdentity];
      } else {
        [weakSelf exitWithErrorCode:ASExtensionErrorCodeUserCanceled];
      }
    }];
  }];
}

- (void)prepareInterfaceForExtensionConfiguration {
  // Reset the consent if the extension was disabled and reenabled.
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  [user_defaults
      removeObjectForKey:kUserDefaultsCredentialProviderConsentVerified];
  self.consentCoordinator = [[ConsentCoordinator alloc]
         initWithBaseViewController:self
                            context:self.extensionContext
            reauthenticationHandler:self.reauthenticationHandler
      isInitialConfigurationRequest:YES];
  [self.consentCoordinator start];
}

#pragma mark - Properties

- (id<CredentialStore>)credentialStore {
  if (!_credentialStore) {
    _credentialStore = [[ArchivableCredentialStore alloc]
        initWithFileURL:CredentialProviderSharedArchivableStoreURL()];
  }
  return _credentialStore;
}

- (ReauthenticationHandler*)reauthenticationHandler {
  if (!_reauthenticationHandler) {
    _reauthenticationHandler = [[ReauthenticationHandler alloc]
        initWithReauthenticationModule:self.reauthenticationModule];
  }
  return _reauthenticationHandler;
}

- (ReauthenticationModule*)reauthenticationModule {
  if (!_reauthenticationModule) {
    _reauthenticationModule = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:self];
  }
  return _reauthenticationModule;
}

- (AccountVerificationProvider*)accountVerificator {
  if (!_accountVerificator) {
    _accountVerificator = [[AccountVerificationProvider alloc] init];
  }
  return _accountVerificator;
}

#pragma mark - Private

- (void)reauthenticateIfNeededWithCompletionHandler:
    (void (^)(ReauthenticationResult))completionHandler {
  [self.reauthenticationHandler
      verifyUserWithCompletionHandler:completionHandler
      presentReminderOnViewController:self];
}

// Completes the extension request providing |ASPasswordCredential| that matches
// the |credentialIdentity| or an error if not found.
- (void)provideCredentialForIdentity:
    (ASPasswordCredentialIdentity*)credentialIdentity {
  NSString* identifier = credentialIdentity.recordIdentifier;
  id<Credential> credential =
      [self.credentialStore credentialWithRecordIdentifier:identifier];
  if (credential) {
    NSString* password =
        PasswordWithKeychainIdentifier(credential.keychainIdentifier);
    if (password) {
      UpdateUMACountForKey(
          app_group::kCredentialExtensionQuickPasswordUseCount);
      ASPasswordCredential* ASCredential =
          [ASPasswordCredential credentialWithUser:credential.user
                                          password:password];
      [self.extensionContext completeRequestWithSelectedCredential:ASCredential
                                                 completionHandler:nil];
      return;
    }
  }
  [self exitWithErrorCode:ASExtensionErrorCodeCredentialIdentityNotFound];
}

// Shows a loading indicator,
- (void)showLoadingIndicator {
  DCHECK(!self.activityIndicatorView);
  self.activityIndicatorView = [[UIActivityIndicatorView alloc] init];
  UIActivityIndicatorView* activityView = self.activityIndicatorView;
  activityView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:activityView];
  [NSLayoutConstraint activateConstraints:@[
    [activityView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [activityView.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
  ]];
  [activityView startAnimating];
  activityView.color = [UIColor colorNamed:kBlueColor];
}

// Hides the loading indicator.
- (void)hideLoadingIndicator {
  [self.activityIndicatorView removeFromSuperview];
  self.activityIndicatorView = nil;
}

// Verifies that the user is still signed in.
// Return NO in the completion when the user is no longer valid. YES otherwise.
- (void)validateUserWithCompletion:(void (^)(BOOL))completion {
  [self showLoadingIndicator];
  auto handler = ^(BOOL isValid) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self hideLoadingIndicator];
      if (completion) {
        completion(isValid);
      }
    });
  };

  NSString* validationID = [app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderManagedUserID()];
  if (validationID) {
    [self.accountVerificator
        validateValidationID:validationID
           completionHandler:^(BOOL isValid, NSError* error) {
             handler(!error && isValid);
           }];
  } else {
    handler(YES);
  }
}

// Presents the stale credentials view controller.
- (void)showStaleCredentials {
  StaleCredentialsViewController* staleCredentialsViewController =
      [[StaleCredentialsViewController alloc] init];
  staleCredentialsViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  staleCredentialsViewController.actionHandler = self;
  [self presentViewController:staleCredentialsViewController
                     animated:YES
                   completion:nil];
}

// Starts the credential list feature.
- (void)showCredentialListForServiceIdentifiers:
    (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers {
  self.listCoordinator = [[CredentialListCoordinator alloc]
      initWithBaseViewController:self
                 credentialStore:self.credentialStore
                         context:self.extensionContext
              serviceIdentifiers:serviceIdentifiers
         reauthenticationHandler:self.reauthenticationHandler];
  [self.listCoordinator start];
  UpdateUMACountForKey(app_group::kCredentialExtensionDisplayCount);
}

// Convenience wrapper for -cancelRequestWithError.
- (void)exitWithErrorCode:(ASExtensionErrorCode)errorCode {
  NSError* error = [[NSError alloc] initWithDomain:ASExtensionErrorDomain
                                              code:errorCode
                                          userInfo:nil];
  [self.extensionContext cancelRequestWithError:error];
}

#pragma mark - SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  self.lastSuccessfulReauthTime = [[NSDate alloc] init];
  UpdateUMACountForKey(app_group::kCredentialExtensionReauthCount);
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDismissAction {
  // Finish the extension. There is no recovery from the stale credentials
  // state.
  [self exitWithErrorCode:ASExtensionErrorCodeFailed];
}

- (void)confirmationAlertPrimaryAction {
  // No-op.
}

- (void)confirmationAlertSecondaryAction {
  // No-op.
}

- (void)confirmationAlertLearnMoreAction {
  // No-op.
}

@end
