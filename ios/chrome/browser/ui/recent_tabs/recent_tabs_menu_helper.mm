// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_helper.h"

#import "base/ios/ios_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_provider.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#include "ios/chrome/browser/ui/recent_tabs/synced_sessions.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RecentTabsContextMenuHelper () <RecentTabsMenuProvider>

@property(nonatomic, assign) Browser* browser;

@property(nonatomic, weak) id<RecentTabsPresentationDelegate>
    recentTabsPresentationDelegate;

@property(nonatomic, weak) id<RecentTabsContextMenuDelegate>
    recentTabsContextMenuDelegate;

@end

@implementation RecentTabsContextMenuHelper

- (instancetype)initWithBrowser:(Browser*)browser
    recentTabsPresentationDelegate:
        (id<RecentTabsPresentationDelegate>)recentTabsPresentationDelegate
     recentTabsContextMenuDelegate:
         (id<RecentTabsContextMenuDelegate>)recentTabsContextMenuDelegate {
  self = [super init];
  if (self) {
    _browser = browser;
    _recentTabsPresentationDelegate = recentTabsPresentationDelegate;
    _recentTabsContextMenuDelegate = recentTabsContextMenuDelegate;
  }
  return self;
}

#pragma mark - RecentTabsMenuProvider

- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (TableViewURLItem*)item
                                                      fromView:(UIView*)view
    API_AVAILABLE(ios(13.0)) {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider = ^(
      NSArray<UIMenuElement*>* suggestedActions) {
    if (!weakSelf) {
      // Return an empty menu.
      return [UIMenu menuWithTitle:@"" children:@[]];
    }

    RecentTabsContextMenuHelper* strongSelf = weakSelf;

    // Record that this context menu was shown to the user.
    RecordMenuShown(MenuScenario::kRecentTabsEntry);

    ActionFactory* actionFactory =
        [[ActionFactory alloc] initWithBrowser:strongSelf.browser
                                      scenario:MenuScenario::kRecentTabsEntry];

    NSMutableArray<UIMenuElement*>* menuElements =
        [[NSMutableArray alloc] init];

    [menuElements
        addObject:
            [actionFactory
                actionToOpenInNewTabWithURL:item.URL
                                 completion:^{
                                   [weakSelf.recentTabsPresentationDelegate
                                           showActiveRegularTabFromRecentTabs];
                                 }]];

    if (base::ios::IsMultipleScenesSupported()) {
      [menuElements
          addObject:[actionFactory
                        actionToOpenInNewWindowWithURL:item.URL
                                        activityOrigin:
                                            WindowActivityRecentTabsOrigin]];
    }

    [menuElements addObject:[actionFactory actionToCopyURL:item.URL]];

    [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                    [weakSelf.recentTabsContextMenuDelegate shareURL:item.URL
                                                               title:item.title
                                                            fromView:view];
                  }]];

    return [UIMenu menuWithTitle:@"" children:menuElements];
  };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (UIContextMenuConfiguration*)
    contextMenuConfigurationForHeaderWithSectionIdentifier:
        (NSInteger)sectionIdentifier API_AVAILABLE(ios(13.0)) {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        if (!weakSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        RecentTabsContextMenuHelper* strongSelf = weakSelf;

        // Record that this context menu was shown to the user.
        RecordMenuShown(MenuScenario::kRecentTabsHeader);

        ActionFactory* actionFactory = [[ActionFactory alloc]
            initWithBrowser:strongSelf.browser
                   scenario:MenuScenario::kRecentTabsHeader];

        NSMutableArray<UIMenuElement*>* menuElements =
            [[NSMutableArray alloc] init];

        synced_sessions::DistantSession const* session =
            [weakSelf.recentTabsContextMenuDelegate
                sessionForSectionIdentifier:sectionIdentifier];

        if (!session->tabs.empty()) {
          [menuElements addObject:[actionFactory actionToOpenAllTabsWithBlock:^{
                          [strongSelf.recentTabsPresentationDelegate
                              openAllTabsFromSession:session];
                        }]];
        }

        [menuElements
            addObject:[actionFactory actionToHideWithBlock:^{
              [strongSelf.recentTabsContextMenuDelegate
                  removeSessionAtSessionSectionIdentifier:sectionIdentifier];
            }]];

        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

@end
