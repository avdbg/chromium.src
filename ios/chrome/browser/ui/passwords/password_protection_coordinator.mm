// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_protection_coordinator.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/safe_browsing/core/password_protection/metrics_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/passwords/password_protection_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordProtectionCoordinator () <ConfirmationAlertActionHandler>

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordProtectionViewController* viewController;

// The warning text to display.
@property(nonatomic, copy) NSString* warningText;

// The completion block.
@property(nonatomic, copy) void (^completion)(safe_browsing::WarningAction);

@end

@implementation PasswordProtectionCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               warningText:(NSString*)warningText {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _warningText = warningText;
  }
  return self;
}

- (void)startWithCompletion:(void (^)(safe_browsing::WarningAction))completion {
  DCHECK(completion);
  self.completion = completion;
  self.viewController = [[PasswordProtectionViewController alloc] init];
  self.viewController.subtitleString = self.warningText;
  self.viewController.actionHandler = self;
  self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  if (@available(iOS 13, *)) {
    self.viewController.modalInPresentation = YES;
  }
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDismissAction {
  self.completion(safe_browsing::WarningAction::CLOSE);
  [self stop];
}

- (void)confirmationAlertPrimaryAction {
  self.completion(safe_browsing::WarningAction::CHANGE_PASSWORD);
  // Opening Password page will stop the presentation. No need to send |stop|.
  [self startPasswordCheck];
}

- (void)confirmationAlertSecondaryAction {
  NOTREACHED();
}

- (void)confirmationAlertLearnMoreAction {
  NOTREACHED();
}

#pragma mark - Private

- (void)startPasswordCheck {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler showSavedPasswordsSettingsAndStartPasswordCheckFromViewController:
               self.baseViewController];
}

@end
