// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_features.h"
#import "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/ui/activity_services/activity_params.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/gestures/view_controller_trait_collection_observer.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/history/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_mediator.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_helper.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#include "ios/chrome/browser/ui/recent_tabs/synced_sessions.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_handler.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_coordinator.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridCoordinator () <HistoryPresentationDelegate,
                                  RecentTabsContextMenuDelegate,
                                  RecentTabsPresentationDelegate,
                                  TabGridMediatorDelegate,
                                  TabPresentationDelegate,
                                  TabGridViewControllerDelegate,
                                  ViewControllerTraitCollectionObserver> {
  // Use an explicit ivar instead of synthesizing as the setter isn't using the
  // ivar.
  Browser* _incognitoBrowser;
}

@property(nonatomic, assign, readonly) Browser* regularBrowser;
// Superclass property specialized for the class that this coordinator uses.
@property(nonatomic, weak) TabGridViewController* baseViewController;
// Commad dispatcher used while this coordinator's view controller is active.
@property(nonatomic, strong) CommandDispatcher* dispatcher;
// Container view controller for the BVC to live in; this class's view
// controller will present this.
@property(nonatomic, strong) BVCContainerViewController* bvcContainer;
// Handler for the transitions between the TabGrid and the Browser.
@property(nonatomic, strong) TabGridTransitionHandler* transitionHandler;
// Mediator for regular Tabs.
@property(nonatomic, strong) TabGridMediator* regularTabsMediator;
// Mediator for incognito Tabs.
@property(nonatomic, strong) TabGridMediator* incognitoTabsMediator;
// Mediator for incognito reauth.
@property(nonatomic, strong) IncognitoReauthMediator* incognitoAuthMediator;
// Mediator for remote Tabs.
@property(nonatomic, strong) RecentTabsMediator* remoteTabsMediator;
// Coordinator for history, which can be started from recent tabs.
@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;
// Coordinator for the thumb strip.
@property(nonatomic, strong) ThumbStripCoordinator* thumbStripCoordinator;
// YES if the TabViewController has never been shown yet.
@property(nonatomic, assign) BOOL firstPresentation;
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;
@property(nonatomic, strong)
    RecentTabsContextMenuHelper* recentTabsContextMenuHelper;
// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;
// The timestamp of the user entering the tab grid.
@property(nonatomic, assign) base::TimeTicks tabGridEnterTime;
// The timestamp of the user exiting the tab grid.
@property(nonatomic, assign) base::TimeTicks tabGridExitTime;

// The page configuration used when create the tab grid view controller;
@property(nonatomic, assign) TabGridPageConfiguration pageConfiguration;

@end

@implementation TabGridCoordinator
// Superclass property.
@synthesize baseViewController = _baseViewController;
// Ivars are not auto-synthesized when both accessor and mutator are overridden.
@synthesize regularBrowser = _regularBrowser;

- (instancetype)initWithWindow:(nullable UIWindow*)window
     applicationCommandEndpoint:
         (id<ApplicationCommands>)applicationCommandEndpoint
    browsingDataCommandEndpoint:
        (id<BrowsingDataCommands>)browsingDataCommandEndpoint
                 regularBrowser:(Browser*)regularBrowser
               incognitoBrowser:(Browser*)incognitoBrowser {
  if ((self = [super initWithWindow:window])) {
    _dispatcher = [[CommandDispatcher alloc] init];
    [_dispatcher startDispatchingToTarget:applicationCommandEndpoint
                              forProtocol:@protocol(ApplicationCommands)];
    // -startDispatchingToTarget:forProtocol: doesn't pick up protocols the
    // passed protocol conforms to, so ApplicationSettingsCommands and
    // BrowsingDataCommands are explicitly dispatched to the endpoint as well.
    [_dispatcher
        startDispatchingToTarget:applicationCommandEndpoint
                     forProtocol:@protocol(ApplicationSettingsCommands)];
    [_dispatcher startDispatchingToTarget:browsingDataCommandEndpoint
                              forProtocol:@protocol(BrowsingDataCommands)];
    _regularBrowser = regularBrowser;
    _incognitoBrowser = incognitoBrowser;

    if (IsIncognitoModeDisabled(
            _regularBrowser->GetBrowserState()->GetPrefs())) {
      _pageConfiguration = TabGridPageConfiguration::kIncognitoPageDisabled;
    } else if (IsIncognitoModeForced(
                   _incognitoBrowser->GetBrowserState()->GetPrefs())) {
      _pageConfiguration = TabGridPageConfiguration::kIncognitoPageOnly;
    } else {
      _pageConfiguration = TabGridPageConfiguration::kAllPagesEnabled;
    }
  }
  return self;
}

#pragma mark - Public

- (Browser*)regularBrowser {
  // Ensure browser which is actually used by the mediator is returned, as it
  // may have been updated.
  return self.regularTabsMediator ? self.regularTabsMediator.browser
                                  : _regularBrowser;
}

- (Browser*)incognitoBrowser {
  // Ensure browser which is actually used by the mediator is returned, as it
  // may have been updated.
  return self.incognitoTabsMediator ? self.incognitoTabsMediator.browser
                                    : _incognitoBrowser;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  DCHECK(self.incognitoTabsMediator);
  self.incognitoTabsMediator.browser = incognitoBrowser;
  self.thumbStripCoordinator.incognitoBrowser = incognitoBrowser;

  if ([self isThumbStripEnabled]) {
    // Update the incognito popup menu handler. This is only used in Thumb
    // Strip mode.
    if (incognitoBrowser) {
      self.baseViewController.incognitoPopupMenuHandler = HandlerForProtocol(
          incognitoBrowser->GetCommandDispatcher(), PopupMenuCommands);
    } else {
      self.baseViewController.incognitoPopupMenuHandler = nil;
    }
    // If the tab grid is currently on the
    // incognito page, make sure to update the shown state as it would be
    // visible onscreen at this point.
    if (self.baseViewController.activePage == TabGridPageIncognitoTabs) {
      if (incognitoBrowser) {
        [self showActiveTabInPage:TabGridPageIncognitoTabs
                     focusOmnibox:NO
                     closeTabGrid:NO];
      } else {
        [self showTabViewController:nil shouldCloseTabGrid:NO completion:nil];
      }
    }
  }
}

- (void)setIncognitoThumbStripSupporting:
    (id<ThumbStripSupporting>)incognitoThumbStripSupporting {
  _incognitoThumbStripSupporting = incognitoThumbStripSupporting;
  if (self.isThumbStripEnabled) {
    [self.incognitoThumbStripSupporting
        thumbStripEnabledWithPanHandler:self.thumbStripCoordinator.panHandler];
  }
}

- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion {
  // Recent tabs context menu may be presented on top of the tab grid.
  [self.baseViewController.remoteTabsViewController dismissModals];
  [self.actionSheetCoordinator stop];
  // History may be presented on top of the tab grid.
  if (self.historyCoordinator) {
    [self.historyCoordinator stopWithCompletion:completion];
  } else if (completion) {
    completion();
  }
}

- (void)setActivePage:(TabGridPage)page {
  DCHECK(page != TabGridPageRemoteTabs);
  self.baseViewController.activePage = page;
}

- (UIViewController*)activeViewController {
  if (self.bvcContainer) {
    // When installing the thumb strip while the tab grid is opened, there is no
    // |currentBVC|.
    DCHECK(self.bvcContainer.currentBVC || [self isThumbStripEnabled]);
    return self.bvcContainer.currentBVC ?: self.bvcContainer;
  }
  return self.baseViewController;
}

- (BOOL)isTabGridActive {
  if (self.isThumbStripEnabled) {
    return self.thumbStripCoordinator.panHandler.currentState ==
           ViewRevealState::Revealed;
  }
  return self.bvcContainer == nil && !self.firstPresentation;
}

- (void)prepareToShowTabGrid {
  // No-op if the BVC isn't being presented.
  if (!self.bvcContainer)
    return;
  [base::mac::ObjCCast<TabGridViewController>(self.baseViewController)
      prepareForAppearance];
}

- (void)showTabGrid {
  BOOL animated = !self.animationsDisabledForTesting;

  if (ShowThumbStripInTraitCollection(
          self.baseViewController.traitCollection)) {
    [self.thumbStripCoordinator.panHandler
        setNextState:ViewRevealState::Revealed
            animated:animated];
    [self.baseViewController contentWillAppearAnimated:animated];
    return;
  }

  // If a BVC is currently being presented, dismiss it.  This will trigger any
  // necessary animations.
  if (self.bvcContainer) {
    [self.baseViewController contentWillAppearAnimated:animated];
    // This is done with a dispatch to make sure that the view isn't added to
    // the view hierarchy right away, as it is not the expectations of the
    // API.
    // Store the currentActivePage at this point in code, to be used during
    // execution of the dispatched block to get the transition from Browser to
    // Tab Grid. That is because in some instances the active page might change
    // before the block gets executed, for example when closing the last tab in
    // incognito (crbug.com/1136882).
    TabGridPage currentActivePage = self.baseViewController.activePage;
    dispatch_async(dispatch_get_main_queue(), ^{
      self.baseViewController.childViewControllerForStatusBarStyle = nil;

      self.transitionHandler = [[TabGridTransitionHandler alloc]
          initWithLayoutProvider:self.baseViewController];
      self.transitionHandler.animationDisabled = !animated;
      [self.transitionHandler
          transitionFromBrowser:self.bvcContainer
                      toTabGrid:self.baseViewController
                     activePage:currentActivePage
                 withCompletion:^{
                   self.bvcContainer = nil;
                   [self.baseViewController contentDidAppear];
                 }];
    });
  }
  self.tabGridEnterTime = base::TimeTicks::Now();

  // Record when the tab switcher is presented.
  base::RecordAction(base::UserMetricsAction("MobileTabGridEntered"));
}

- (void)reportTabGridUsageTime {
  base::TimeDelta duration = self.tabGridExitTime - self.tabGridEnterTime;
  base::UmaHistogramLongTimes("IOS.TabSwitcher.TimeSpent", duration);
  self.tabGridEnterTime = base::TimeTicks();
  self.tabGridExitTime = base::TimeTicks();
}

- (void)showTabViewController:(UIViewController*)viewController
           shouldCloseTabGrid:(BOOL)shouldCloseTabGrid
                   completion:(ProceduralBlock)completion {
  bool thumbStripEnabled = self.isThumbStripEnabled;
  DCHECK(viewController || (thumbStripEnabled && self.bvcContainer));

  if (shouldCloseTabGrid) {
    self.tabGridExitTime = base::TimeTicks::Now();

    // Record when the tab switcher is dismissed.
    base::RecordAction(base::UserMetricsAction("MobileTabGridExited"));
    [self reportTabGridUsageTime];
  }

  if (thumbStripEnabled) {
    self.bvcContainer.currentBVC = viewController;
    self.baseViewController.childViewControllerForStatusBarStyle =
        viewController;
    [self.baseViewController setNeedsStatusBarAppearanceUpdate];
    if (shouldCloseTabGrid) {
      [self.baseViewController contentWillDisappearAnimated:YES];
      [self.thumbStripCoordinator.panHandler
          setNextState:ViewRevealState::Hidden
              animated:YES];
    }

    if (completion) {
      completion();
    }
    self.firstPresentation = NO;

    return;
  }

  // If another BVC is already being presented, swap this one into the
  // container.
  if (self.bvcContainer) {
    self.bvcContainer.currentBVC = viewController;
    self.baseViewController.childViewControllerForStatusBarStyle =
        viewController;
    [self.baseViewController setNeedsStatusBarAppearanceUpdate];
    if (completion) {
      completion();
    }
    return;
  }

  self.bvcContainer = [[BVCContainerViewController alloc] init];
  self.bvcContainer.currentBVC = viewController;

  BOOL animated = !self.animationsDisabledForTesting;
  // Never animate the first time.
  if (self.firstPresentation)
    animated = NO;

  // Extened |completion| to signal the tab switcher delegate
  // that the animated "tab switcher dismissal" (that is, presenting something
  // on top of the tab switcher) transition has completed.
  // Finally, the launch mask view should be removed.
  ProceduralBlock extendedCompletion = ^{
    [self.delegate tabGridDismissTransitionDidEnd:self];
    if (!GetFirstResponder()) {
      // It is possible to already have a first responder (for example the
      // omnibox). In that case, we don't want to mark BVC as first responder.
      [self.bvcContainer.currentBVC becomeFirstResponder];
    }
    if (completion) {
      completion();
    }
    self.firstPresentation = NO;
  };

  self.baseViewController.childViewControllerForStatusBarStyle =
      self.bvcContainer.currentBVC;

  [self.baseViewController contentWillDisappearAnimated:animated];

  self.transitionHandler = [[TabGridTransitionHandler alloc]
      initWithLayoutProvider:self.baseViewController];
  self.transitionHandler.animationDisabled = !animated;
  [self.transitionHandler
      transitionFromTabGrid:self.baseViewController
                  toBrowser:self.bvcContainer
                 activePage:self.baseViewController.activePage
             withCompletion:^{
               extendedCompletion();
             }];
}

#pragma mark - Private (Thumb Strip)

// Whether the thumb strip is enabled.
- (BOOL)isThumbStripEnabled {
  return self.thumbStripCoordinator != nil;
}

// Installs the thumb strip and informs this object dependencies.
- (void)installThumbStrip {
  DCHECK(!self.isThumbStripEnabled);
  ViewRevealState initialState = self.isTabGridActive
                                     ? ViewRevealState::Revealed
                                     : ViewRevealState::Hidden;
  self.thumbStripCoordinator = [[ThumbStripCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                    initialState:initialState];
  ThumbStripCoordinator* thumbStripCoordinator = self.thumbStripCoordinator;
  thumbStripCoordinator.regularBrowser = self.regularBrowser;
  thumbStripCoordinator.incognitoBrowser = self.incognitoBrowser;
  [thumbStripCoordinator start];

  ViewRevealingVerticalPanHandler* panHandler =
      thumbStripCoordinator.panHandler;
  DCHECK(panHandler);
  panHandler.layoutSwitcherProvider = self.baseViewController;

  // Create a BVC add it to this view controller if not present. The thumb strip
  // always needs a BVC container on screen.
  self.bvcContainer =
      self.bvcContainer ?: [[BVCContainerViewController alloc] init];
  if (!self.bvcContainer.view.superview) {
    [self.baseViewController addChildViewController:self.bvcContainer];
    self.bvcContainer.view.frame = self.baseViewController.view.bounds;
    [self.baseViewController.view addSubview:self.bvcContainer.view];
    self.bvcContainer.view.accessibilityViewIsModal = YES;
    [self.bvcContainer didMoveToParentViewController:self.baseViewController];
  }

  DCHECK(self.incognitoThumbStripSupporting);
  DCHECK(self.regularThumbStripSupporting);
  // Enable first on BVCContainer, so it is ready to show another BVC.
  [self.bvcContainer thumbStripEnabledWithPanHandler:panHandler];
  [self.baseViewController thumbStripEnabledWithPanHandler:panHandler];
  [self.incognitoThumbStripSupporting
      thumbStripEnabledWithPanHandler:panHandler];
  [self.regularThumbStripSupporting thumbStripEnabledWithPanHandler:panHandler];

  self.baseViewController.regularPopupMenuHandler = HandlerForProtocol(
      self.regularBrowser->GetCommandDispatcher(), PopupMenuCommands);
  self.baseViewController.incognitoPopupMenuHandler = HandlerForProtocol(
      self.incognitoBrowser->GetCommandDispatcher(), PopupMenuCommands);

  [self.baseViewController setNeedsStatusBarAppearanceUpdate];
}

// Uninstalls the thumb strip and informs this object dependencies.
- (void)uninstallThumbStrip {
  DCHECK(self.isThumbStripEnabled);

  BOOL showGridAfterUninstall = self.isTabGridActive;

  [self.regularThumbStripSupporting thumbStripDisabled];
  [self.incognitoThumbStripSupporting thumbStripDisabled];
  [self.bvcContainer thumbStripDisabled];
  [self.baseViewController thumbStripDisabled];

  self.thumbStripCoordinator.panHandler.layoutSwitcherProvider = nil;
  [self.thumbStripCoordinator stop];
  self.thumbStripCoordinator = nil;

  if (showGridAfterUninstall) {
    [self.bvcContainer willMoveToParentViewController:nil];
    [self.bvcContainer.view removeFromSuperview];
    [self.bvcContainer removeFromParentViewController];
    self.bvcContainer = nil;
  }
  [self.baseViewController setNeedsStatusBarAppearanceUpdate];
}

#pragma mark - ChromeCoordinator

- (void)start {
  IncognitoReauthSceneAgent* reauthAgent = [IncognitoReauthSceneAgent
      agentFromScene:SceneStateBrowserAgent::FromBrowser(_incognitoBrowser)
                         ->GetSceneState()];

  [self.dispatcher startDispatchingToTarget:reauthAgent
                                forProtocol:@protocol(IncognitoReauthCommands)];

  TabGridViewController* baseViewController;
  baseViewController = [[TabGridViewController alloc]
      initWithPageConfiguration:_pageConfiguration];
  baseViewController.handler =
      HandlerForProtocol(self.dispatcher, ApplicationCommands);
  baseViewController.reauthHandler =
      HandlerForProtocol(self.dispatcher, IncognitoReauthCommands);
  baseViewController.tabPresentationDelegate = self;
  baseViewController.delegate = self;
  _baseViewController = baseViewController;

  self.regularTabsMediator = [[TabGridMediator alloc]
      initWithConsumer:baseViewController.regularTabsConsumer];
  ChromeBrowserState* regularBrowserState =
      _regularBrowser ? _regularBrowser->GetBrowserState() : nullptr;
  WebStateList* regularWebStateList =
      _regularBrowser ? _regularBrowser->GetWebStateList() : nullptr;

  self.regularTabsMediator.browser = _regularBrowser;
  self.regularTabsMediator.delegate = self;
  if (regularBrowserState) {
    self.regularTabsMediator.tabRestoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(
            regularBrowserState);
  }

  self.incognitoTabsMediator = [[TabGridMediator alloc]
      initWithConsumer:baseViewController.incognitoTabsConsumer];
  self.incognitoTabsMediator.browser = _incognitoBrowser;
  self.incognitoTabsMediator.delegate = self;
  baseViewController.regularTabsDelegate = self.regularTabsMediator;
  baseViewController.incognitoTabsDelegate = self.incognitoTabsMediator;
  baseViewController.regularTabsDragDropHandler = self.regularTabsMediator;
  baseViewController.incognitoTabsDragDropHandler = self.incognitoTabsMediator;
  baseViewController.regularTabsImageDataSource = self.regularTabsMediator;
  baseViewController.incognitoTabsImageDataSource = self.incognitoTabsMediator;

  self.incognitoAuthMediator = [[IncognitoReauthMediator alloc]
      initWithConsumer:self.baseViewController.incognitoTabsConsumer
           reauthAgent:reauthAgent];

  if (@available(iOS 13.0, *)) {
    self.recentTabsContextMenuHelper =
        [[RecentTabsContextMenuHelper alloc] initWithBrowser:self.regularBrowser
                              recentTabsPresentationDelegate:self
                               recentTabsContextMenuDelegate:self];
    self.baseViewController.remoteTabsViewController.menuProvider =
        self.recentTabsContextMenuHelper;
  }

  // TODO(crbug.com/845192) : Remove RecentTabsTableViewController dependency on
  // ChromeBrowserState so that we don't need to expose the view controller.
  baseViewController.remoteTabsViewController.browser = self.regularBrowser;
  self.remoteTabsMediator = [[RecentTabsMediator alloc] init];
  self.remoteTabsMediator.browserState = regularBrowserState;
  self.remoteTabsMediator.consumer = baseViewController.remoteTabsConsumer;
  self.remoteTabsMediator.webStateList = regularWebStateList;
  // TODO(crbug.com/845636) : Currently, the image data source must be set
  // before the mediator starts updating its consumer. Fix this so that order of
  // calls does not matter.
  baseViewController.remoteTabsViewController.imageDataSource =
      self.remoteTabsMediator;
  baseViewController.remoteTabsViewController.delegate =
      self.remoteTabsMediator;
  baseViewController.remoteTabsViewController.handler =
      HandlerForProtocol(self.dispatcher, ApplicationCommands);
  baseViewController.remoteTabsViewController.loadStrategy =
      UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB;
  baseViewController.remoteTabsViewController.restoredTabDisposition =
      WindowOpenDisposition::NEW_FOREGROUND_TAB;
  baseViewController.remoteTabsViewController.presentationDelegate = self;

  self.firstPresentation = YES;

  // TODO(crbug.com/850387) : Currently, consumer calls from the mediator
  // prematurely loads the view in |RecentTabsTableViewController|. Fix this so
  // that the view is loaded only by an explicit placement in the view
  // hierarchy. As a workaround, the view controller hierarchy is loaded here
  // before |RecentTabsMediator| updates are started.
  self.window.rootViewController = self.baseViewController;
  if (self.remoteTabsMediator.browserState) {
    [self.remoteTabsMediator initObservers];
    [self.remoteTabsMediator refreshSessionsView];
  }

  baseViewController.traitCollectionObserver = self;
  if (ShowThumbStripInTraitCollection(
          self.baseViewController.traitCollection)) {
    [self installThumbStrip];
  }

  // Once the mediators are set up, stop keeping pointers to the browsers used
  // to initialize them.
  _regularBrowser = nil;
  _incognitoBrowser = nil;
}

- (void)stop {
  if ([self isThumbStripEnabled]) {
    [self uninstallThumbStrip];
  }
  // The TabGridViewController may still message its application commands
  // handler after this coordinator has stopped; make this action a no-op by
  // setting the handler to nil.
  self.baseViewController.handler = nil;
  self.recentTabsContextMenuHelper = nil;
  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;
  [self.dispatcher stopDispatchingForProtocol:@protocol(ApplicationCommands)];
  [self.dispatcher
      stopDispatchingForProtocol:@protocol(ApplicationSettingsCommands)];
  [self.dispatcher stopDispatchingForProtocol:@protocol(BrowsingDataCommands)];

  // Disconnect UI from models they observe.
  self.regularTabsMediator.browser = nil;
  self.incognitoTabsMediator.browser = nil;

  // TODO(crbug.com/845192) : RecentTabsTableViewController behaves like a
  // coordinator and that should be factored out.
  [self.baseViewController.remoteTabsViewController dismissModals];
  [self.remoteTabsMediator disconnect];
  self.remoteTabsMediator = nil;
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

#pragma mark - TabPresentationDelegate

- (void)showActiveTabInPage:(TabGridPage)page
               focusOmnibox:(BOOL)focusOmnibox
               closeTabGrid:(BOOL)closeTabGrid {
  DCHECK(self.regularBrowser && self.incognitoBrowser);
  DCHECK(closeTabGrid || ShowThumbStripInTraitCollection(
                             self.baseViewController.traitCollection));
  Browser* activeBrowser = nullptr;
  switch (page) {
    case TabGridPageIncognitoTabs:
      if (self.incognitoBrowser->GetWebStateList()->count() == 0) {
        DCHECK([self isThumbStripEnabled]);
        [self showTabViewController:nil
                 shouldCloseTabGrid:closeTabGrid
                         completion:nil];
        return;
      }
      activeBrowser = self.incognitoBrowser;
      break;
    case TabGridPageRegularTabs:
      if (self.regularBrowser->GetWebStateList()->count() == 0) {
        DCHECK([self isThumbStripEnabled]);
        [self showTabViewController:nil
                 shouldCloseTabGrid:closeTabGrid
                         completion:nil];
        return;
      }
      activeBrowser = self.regularBrowser;
      break;
    case TabGridPageRemoteTabs:
      if ([self isThumbStripEnabled]) {
        [self showTabViewController:nil
                 shouldCloseTabGrid:closeTabGrid
                         completion:nil];
        return;
      }
      NOTREACHED() << "It is invalid to have an active tab in remote tabs.";
      // This appears to come up in release -- see crbug.com/1069243.
      // Defensively early return instead of continuing.
      return;
  }
  // Trigger the transition through the delegate. This will in turn call back
  // into this coordinator.
  [self.delegate tabGrid:self
      shouldActivateBrowser:activeBrowser
             dismissTabGrid:closeTabGrid
               focusOmnibox:focusOmnibox];
}

- (void)showCloseAllConfirmationActionSheetWitTabGridMediator:
            (TabGridMediator*)tabGridMediator
                                                 numberOfTabs:
                                                     (NSInteger)numberOfTabs
                                                       anchor:(UIBarButtonItem*)
                                                                  buttonAnchor {
  if (tabGridMediator == self.regularTabsMediator) {
    base::RecordAction(base::UserMetricsAction(
        "MobileTabGridCloseAllRegularTabsConfirmationPresented"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "MobileTabGridCloseAllIncognitoTabsConfirmationPresented"));
  }

  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:nil
                         message:nil
                   barButtonItem:buttonAnchor];

  self.actionSheetCoordinator.alertStyle = UIAlertControllerStyleActionSheet;

  __weak TabGridCoordinator* weakSelf = self;

  [self.actionSheetCoordinator
      addItemWithTitle:base::SysUTF16ToNSString(
                           l10n_util::GetPluralStringFUTF16(
                               IDS_IOS_TAB_GRID_CLOSE_ALL_TABS_CONFIRMATION,
                               numberOfTabs))
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "MobileTabGridCloseAllTabsConfirmationConfirmed"));
                  [tabGridMediator closeAllItems];
                  [weakSelf.baseViewController closeAllTabsConfirmationClosed];
                }
                 style:UIAlertActionStyleDestructive];
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "MobileTabGridCloseAllTabsConfirmationCanceled"));
                  [weakSelf.baseViewController closeAllTabsConfirmationClosed];
                }
                 style:UIAlertActionStyleCancel];
  [self.actionSheetCoordinator start];
}

#pragma mark - TabGridViewControllerDelegate

- (TabGridPage)activePageForTabGridViewController:
    (TabGridViewController*)tabGridViewController {
  return [self.delegate activePageForTabGrid:self];
}

- (void)tabGridViewControllerDidDismiss:
    (TabGridViewController*)tabGridViewController {
  [self.delegate tabGridDismissTransitionDidEnd:self];
}

- (void)openLinkWithURL:(const GURL&)URL {
  id<ApplicationCommands> handler =
      HandlerForProtocol(self.dispatcher, ApplicationCommands);
  [handler openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:URL]];
}

#pragma mark - RecentTabsPresentationDelegate

- (void)showHistoryFromRecentTabs {
  // A history coordinator from main_controller won't work properly from the
  // tab grid. Using a local coordinator works better and we need to set
  // |loadStrategy| to YES to ALWAYS_NEW_FOREGROUND_TAB.
  self.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.regularBrowser];
  self.historyCoordinator.loadStrategy =
      UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB;
  self.historyCoordinator.presentationDelegate = self;
  [self.historyCoordinator start];
}

- (void)showActiveRegularTabFromRecentTabs {
  [self.delegate tabGrid:self
      shouldActivateBrowser:self.regularBrowser
             dismissTabGrid:YES
               focusOmnibox:NO];
}

#pragma mark - HistoryPresentationDelegate

- (void)showActiveRegularTabFromHistory {
  [self.delegate tabGrid:self
      shouldActivateBrowser:self.regularBrowser
             dismissTabGrid:YES
               focusOmnibox:NO];
}

- (void)showActiveIncognitoTabFromHistory {
  [self.delegate tabGrid:self
      shouldActivateBrowser:self.incognitoBrowser
             dismissTabGrid:YES
               focusOmnibox:NO];
}

- (void)openAllTabsFromSession:(const synced_sessions::DistantSession*)session {
  base::RecordAction(base::UserMetricsAction(
      "MobileRecentTabManagerOpenAllTabsFromOtherDevice"));
  base::UmaHistogramCounts100(
      "Mobile.RecentTabsManager.TotalTabsFromOtherDevicesOpenAll",
      session->tabs.size());

  for (auto const& tab : session->tabs) {
    UrlLoadParams params = UrlLoadParams::InNewTab(tab->virtual_url);
    params.SetInBackground(YES);
    params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    params.load_strategy =
        self.baseViewController.remoteTabsViewController.loadStrategy;
    params.in_incognito =
        self.regularBrowser->GetBrowserState()->IsOffTheRecord();
    UrlLoadingBrowserAgent::FromBrowser(self.regularBrowser)->Load(params);
  }

  [self showActiveRegularTabFromRecentTabs];
}

#pragma mark - RecentTabsContextMenuDelegate

- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        fromView:(UIView*)view {
  ActivityParams* params =
      [[ActivityParams alloc] initWithURL:URL
                                    title:title
                                 scenario:ActivityScenario::RecentTabsEntry];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                                     .remoteTabsViewController
                         browser:self.regularBrowser
                          params:params
                      originView:view];
  [self.sharingCoordinator start];
}

- (void)removeSessionAtSessionSectionIdentifier:(NSInteger)sectionIdentifier {
  [self.baseViewController.remoteTabsViewController
      removeSessionAtSessionSectionIdentifier:sectionIdentifier];
}

- (synced_sessions::DistantSession const*)sessionForSectionIdentifier:
    (NSInteger)sectionIdentifier {
  return [self.baseViewController.remoteTabsViewController
      sessionForSectionIdentifier:sectionIdentifier];
}

#pragma mark - ViewControllerTraitCollectionObserver

- (void)viewController:(UIViewController*)viewController
    traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  BOOL canShowThumbStrip =
      ShowThumbStripInTraitCollection(viewController.traitCollection);
  if (canShowThumbStrip != [self isThumbStripEnabled]) {
    if (canShowThumbStrip) {
      [self installThumbStrip];
    } else {
      [self uninstallThumbStrip];
    }
  }
}

@end
