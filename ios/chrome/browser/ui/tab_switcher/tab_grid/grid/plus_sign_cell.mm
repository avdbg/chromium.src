// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/plus_sign_cell.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/colors/dynamic_color_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PlusSignCell ()
@property(nonatomic, weak) UIView* plusSignView;
@end

@implementation PlusSignCell

// |-dequeueReusableCellWithReuseIdentifier:forIndexPath:| calls this method to
// initialize a cell.
- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.layer.cornerRadius = kGridCellCornerRadius;
    self.layer.masksToBounds = YES;
    UIImageView* plusSignView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"grid_cell_plus_sign"]];
    [self.contentView addSubview:plusSignView];
    plusSignView.translatesAutoresizingMaskIntoConstraints = NO;
    _plusSignView = plusSignView;

    AddSameCenterConstraints(plusSignView, self.contentView);

    if (@available(iOS 13, *)) {
      // TODO(crbug.com/981889): When iOS 12 is dropped, only the next line is
      // needed for styling. Every other check can be sremoved, as well as the
      // incognito specific assets.
      self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
    }
  }
  return self;
}

#pragma mark - UIAccessibility

- (BOOL)isAccessibilityElement {
  // This makes the whole cell tappable in VoiceOver rather than the plus sign.
  return YES;
}

#pragma mark - Public

// Updates the theme to either dark or light. Updating is only done if the
// current theme is not the desired theme.
- (void)setTheme:(GridTheme)theme {
  if (_theme == theme)
    return;

  switch (theme) {
    case GridThemeDark:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);
      break;
    case GridThemeLight:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);
      break;
  }

  self.backgroundView = [[UIView alloc] init];
  self.backgroundView.backgroundColor =
      [UIColor colorNamed:kPlusSignCellBackgroundColor];

  // selectedBackgroundView is used for highlighting as well.
  self.selectedBackgroundView = [[UIView alloc] init];
  UIColor* highlightedBackgroundColor = color::DarkModeDynamicColor(
      [UIColor colorNamed:kTertiaryBackgroundColor], /*forceDark=*/true,
      [UIColor colorNamed:kTertiaryBackgroundDarkColor]);
  self.selectedBackgroundView.backgroundColor = highlightedBackgroundColor;

  _theme = theme;
}

@end
