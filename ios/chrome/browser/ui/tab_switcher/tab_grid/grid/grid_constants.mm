// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Accessibility identifier prefix of a grid cell.
NSString* const kGridCellIdentifierPrefix = @"GridCellIdentifierPrefix";

// Accessibility identifier for the close button in a grid cell.
NSString* const kGridCellCloseButtonIdentifier =
    @"GridCellCloseButtonIdentifier";

// Grid styling.
NSString* const kGridBackgroundColor = @"grid_background_color";

// PlusSignCell styling
NSString* const kPlusSignCellBackgroundColor =
    @"plus_sign_grid_cell_background_color";

// The height of the BVC that remains visible after transitioning from thumb
// strip to tab grid.
const CGFloat kBVCHeightTabGrid = 108.0f;

// Definition of limited width for applicable size classes. The first refers to
// the horizontal size class; the second to the vertical.
const CGFloat kGridLayoutCompactCompactLimitedWidth = 666.0f;
const CGFloat kGridLayoutCompactRegularLimitedWidth = 374.0f;
// Insets for size classes. The first refers to the horizontal size class; the
// second to the vertical.
const UIEdgeInsets kGridLayoutInsetsCompactCompact =
    UIEdgeInsets{20.0f, 20.0f, 20.0f, 20.0f};
const UIEdgeInsets kGridLayoutInsetsCompactCompactLimitedWidth =
    UIEdgeInsets{22.0f, 44.0f, 22.0f, 44.0f};
const UIEdgeInsets kGridLayoutInsetsCompactRegular =
    UIEdgeInsets{13.0f, 13.0f, 13.0f, 13.0f};
const UIEdgeInsets kGridLayoutInsetsCompactRegularLimitedWidth =
    UIEdgeInsets{28.0f, 10.0f, 28.0f, 10.0f};
const UIEdgeInsets kGridLayoutInsetsRegularCompact =
    UIEdgeInsets{32.0f, 32.0f, 32.0f, 32.0f};
const UIEdgeInsets kGridLayoutInsetsRegularRegular =
    UIEdgeInsets{28.0f, 28.0f, 28.0f, 28.0f};
// Minimum line spacing for size classes. The first refers to the horizontal
// size class; the second to the vertical.
const CGFloat kGridLayoutLineSpacingCompactCompact = 17.0f;
const CGFloat kGridLayoutLineSpacingCompactCompactLimitedWidth = 22.0f;
const CGFloat kGridLayoutLineSpacingCompactRegular = 13.0f;
const CGFloat kGridLayoutLineSpacingCompactRegularLimitedWidth = 15.0f;
const CGFloat kGridLayoutLineSpacingRegularCompact = 32.0f;
const CGFloat kGridLayoutLineSpacingRegularRegular = 14.0f;

const CGFloat kReorderingInactiveCellOpacity = 0.80;
const CGFloat kReorderingActiveCellScale = 1.15;

// GridCell styling.
// Dark theme colors.
// Extra dark theme colors until iOS 12 gets removed.
const int kGridDarkThemeCellTitleColor = 0xFFFFFF;
const int kGridDarkThemeCellDetailColor = 0xEBEBF5;
const CGFloat kGridDarkThemeCellDetailAlpha = 0.6;
const int kGridDarkThemeCellTintColor = 0x8AB4F9;
extern const int kGridDarkThemeCellSolidButtonTextColor = 0x202124;

// GridCell dimensions.
const CGSize kGridCellSizeSmall = CGSize{144.0f, 168.0f};
const CGSize kGridCellSizeMedium = CGSize{168.0f, 202.0f};
const CGSize kGridCellSizeLarge = CGSize{228.0f, 256.0f};
const CGSize kGridCellSizeAccessibility = CGSize{288.0f, 336.0f};
const CGFloat kGridCellCornerRadius = 13.0f;
const CGFloat kGridCellIconCornerRadius = 3.0f;
// The cell header contains the icon, title, and close button.
const CGFloat kGridCellHeaderHeight = 32.0f;
const CGFloat kGridCellHeaderAccessibilityHeight = 108.0f;
const CGFloat kGridCellHeaderLeadingInset = 9.0f;
const CGFloat kGridCellCloseTapTargetWidthHeight = 44.0f;
const CGFloat kGridCellCloseButtonContentInset = 8.5f;
const CGFloat kGridCellTitleLabelContentInset = 4.0f;
const CGFloat kGridCellIconDiameter = 16.0f;
const CGFloat kGridCellSelectionRingGapWidth = 2.0f;
const CGFloat kGridCellSelectionRingTintWidth = 5.0f;

// The plus sign image should be at the center of a view with width equal to
// kGridCellSizeSmall.width / 2.
const CGFloat kPlusSignImageTrailingCenterDistance =
    kGridCellSizeSmall.width / 4;
// The threshold is set so that the hide transition starts at the moment the
// plus sign image from the button and the one from the plus sign cell coincide
// in position.
const CGFloat kScrollThresholdForPlusSignButtonHide =
    kPlusSignImageTrailingCenterDistance -
    kGridLayoutLineSpacingCompactCompactLimitedWidth -
    kGridCellSizeSmall.width / 2;
const CGFloat kPlusSignImageYCenterConstant =
    kGridLayoutLineSpacingCompactCompactLimitedWidth +
    kGridCellSelectionRingGapWidth + kGridCellSelectionRingTintWidth +
    kGridCellSizeSmall.height / 2 - 1.5f;
// The width is set so that when the scrollThresholdForPlusSignButtonHide is
// attained by the user scroll, the plus sign button does not overlap with
// second cell from right to left (the first being the plus sign cell)
const CGFloat kPlusSignButtonWidth =
    kGridCellSizeSmall.width / 2 +
    kGridLayoutLineSpacingCompactCompactLimitedWidth +
    kPlusSignImageTrailingCenterDistance;
