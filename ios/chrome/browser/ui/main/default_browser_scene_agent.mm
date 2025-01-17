// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/default_browser_scene_agent.h"

#include "base/ios/ios_util.h"
#include "base/version.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/whats_new_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DefaultBrowserSceneAgent ()

// Command Dispatcher.
@property(nonatomic, weak) CommandDispatcher* dispatcher;

@end

@implementation DefaultBrowserSceneAgent

- (instancetype)initWithCommandDispatcher:(CommandDispatcher*)dispatcher {
  if ([super init])
    _dispatcher = dispatcher;
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // Don't show Default Browser promo for users not on the stable 14.0.1 iOS
  // version yet.
  if (!base::ios::IsRunningOnOrLater(14, 0, 1)) {
    return;
  }
  AppState* appState = self.sceneState.appState;
  // Can only present UI when activation level is
  // SceneActivationLevelForegroundActive. Show the UI if user has met the
  // qualifications to be shown the promo.
  if (level == SceneActivationLevelForegroundActive &&
      appState.shouldShowDefaultBrowserPromo && !appState.currentUIBlocker) {
    [HandlerForProtocol(self.dispatcher, WhatsNewCommands)
        showDefaultBrowserFullscreenPromo];
    appState.shouldShowDefaultBrowserPromo = NO;
  }
}

@end
