// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in/open_in_coordinator.h"

#import "ios/chrome/browser/ui/open_in/open_in_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OpenInCoordinator ()

// The mediator used to configure and display the OpenInController.
@property(nonatomic, strong) OpenInMediator* openInMediator;

@end

@implementation OpenInCoordinator

- (void)start {
  self.openInMediator =
      [[OpenInMediator alloc] initWithBaseViewController:self.baseViewController
                                                 browser:self.browser];
}

- (void)stop {
  self.openInMediator = nil;
}

#pragma mark - Public

- (void)disableAll {
  [self.openInMediator disableAll];
}

@end
