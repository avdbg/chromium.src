// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sad_tab/sad_tab_coordinator.h"

#include "base/metrics/histogram_macros.h"
#include "components/ui_metrics/sadtab_metrics_types.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_observer_bridge.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/fullscreen/chrome_coordinator+fullscreen_disabling.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_view_controller.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SadTabCoordinator () <SadTabViewControllerDelegate,
                                 BrowserObserving> {
  SadTabViewController* _viewController;
  // Observe BrowserObserver to stop fullscreen disabling before the
  // FullscreenController tied to the Browser is destroyed.
  std::unique_ptr<BrowserObserverBridge> _browserObserver;
}
@end

@implementation SadTabCoordinator

- (void)start {
  if (_viewController)
    return;

  if (self.repeatedFailure) {
    UMA_HISTOGRAM_ENUMERATION(ui_metrics::kSadTabFeedbackHistogramKey,
                              ui_metrics::SadTabEvent::DISPLAYED,
                              ui_metrics::SadTabEvent::MAX_SAD_TAB_EVENT);
  } else {
    UMA_HISTOGRAM_ENUMERATION(ui_metrics::kSadTabReloadHistogramKey,
                              ui_metrics::SadTabEvent::DISPLAYED,
                              ui_metrics::SadTabEvent::MAX_SAD_TAB_EVENT);
  }
  _browserObserver = std::make_unique<BrowserObserverBridge>(self);
  self.browser->AddObserver(_browserObserver.get());
  // Creates a fullscreen disabler.
  [self didStartFullscreenDisablingUI];

  _viewController = [[SadTabViewController alloc] init];
  _viewController.delegate = self;
  _viewController.overscrollDelegate = self.overscrollDelegate;
  _viewController.offTheRecord =
      self.browser->GetBrowserState()->IsOffTheRecord();
  _viewController.repeatedFailure = self.repeatedFailure;

  [self.baseViewController addChildViewController:_viewController];
  [self.baseViewController.view addSubview:_viewController.view];
  [_viewController didMoveToParentViewController:self.baseViewController];

  _viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints([NamedGuide guideWithName:kContentAreaGuide
                                          view:self.baseViewController.view],
                     _viewController.view);
}

- (void)stop {
  if (!_viewController)
    return;

  self.browser->RemoveObserver(_browserObserver.get());
  _browserObserver.reset();
  [self didStopFullscreenDisablingUI];

  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

- (void)setOverscrollDelegate:
    (id<OverscrollActionsControllerDelegate>)delegate {
  _viewController.overscrollDelegate = delegate;
  _overscrollDelegate = delegate;
}

#pragma mark - BrowserObserving

- (void)browserDestroyed:(Browser*)browser {
  [self stop];
}

#pragma mark - SadTabViewDelegate

- (void)sadTabViewControllerShowReportAnIssue:
    (SadTabViewController*)sadTabViewController {
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  [static_cast<id<ApplicationCommands>>(self.browser->GetCommandDispatcher())
      showReportAnIssueFromViewController:self.baseViewController
                                   sender:UserFeedbackSender::SadTab];
}

- (void)sadTabViewController:(SadTabViewController*)sadTabViewController
    showSuggestionsPageWithURL:(const GURL&)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  [static_cast<id<ApplicationCommands>>(self.browser->GetCommandDispatcher())
      openURLInNewTab:command];
}

- (void)sadTabViewControllerReload:(SadTabViewController*)sadTabViewController {
  WebNavigationBrowserAgent::FromBrowser(self.browser)->Reload();
}

#pragma mark - SadTabTabHelperDelegate

- (void)sadTabTabHelper:(SadTabTabHelper*)tabHelper
    presentSadTabForWebState:(web::WebState*)webState
             repeatedFailure:(BOOL)repeatedFailure {
  if (!webState->IsVisible())
    return;

  self.repeatedFailure = repeatedFailure;
  [self start];
}

- (void)sadTabTabHelperDismissSadTab:(SadTabTabHelper*)tabHelper {
  [self stop];
}

- (void)sadTabTabHelper:(SadTabTabHelper*)tabHelper
    didShowForRepeatedFailure:(BOOL)repeatedFailure {
  self.repeatedFailure = repeatedFailure;
  [self start];
}

- (void)sadTabTabHelperDidHide:(SadTabTabHelper*)tabHelper {
  [self stop];
}

@end
