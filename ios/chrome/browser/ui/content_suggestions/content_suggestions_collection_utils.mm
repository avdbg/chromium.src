// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"

#include "base/i18n/rtl.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/components/ui_util/dynamic_type_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Width of search field.
const CGFloat kSearchFieldLarge = 432;
const CGFloat kSearchFieldSmall = 343;
const CGFloat kSearchFieldMinMargin = 8;

// Top margin for the doodle.
const CGFloat kDoodleTopMarginRegularXRegular = 162;
const CGFloat kDoodleTopMarginOther = 48;
// Size of the doodle top margin which is multiplied by the scaled font factor,
// and added to |kDoodleTopMarginOther| on non Regular x Regular form factors.
const CGFloat kDoodleScaledTopMarginOther = 10;

// Top margin for the search field
const CGFloat kSearchFieldTopMargin = 32;

// Bottom margin for the search field.
const CGFloat kNTPSearchFieldBottomPadding = 18;

const CGFloat kTopSpacingMaterial = 24;

// Height for the doodle frame.
const CGFloat kGoogleSearchDoodleHeight = 120;

// Height for the doodle frame when Google is not the default search engine.
const CGFloat kNonGoogleSearchDoodleHeight = 60;
}

namespace content_suggestions {

const int kSearchFieldBackgroundColor = 0xF1F3F4;
const CGFloat kHintTextScale = 0.15;

CGFloat doodleHeight(BOOL logoIsShowing, UITraitCollection* traitCollection) {
  if (!IsRegularXRegularSizeClass(traitCollection) && !logoIsShowing)
    return kNonGoogleSearchDoodleHeight;

  return kGoogleSearchDoodleHeight;
}

CGFloat doodleTopMargin(BOOL toolbarPresent,
                        CGFloat topInset,
                        UITraitCollection* traitCollection) {
  if (IsRegularXRegularSizeClass(traitCollection))
    return kDoodleTopMarginRegularXRegular;
  if (IsCompactHeight(traitCollection))
    return topInset;
  return topInset + kDoodleTopMarginOther +
         AlignValueToPixel(kDoodleScaledTopMarginOther *
                           ui_util::SystemSuggestedFontSizeMultiplier());
}

CGFloat searchFieldTopMargin() {
  return kSearchFieldTopMargin;
}

CGFloat searchFieldWidth(CGFloat superviewWidth,
                         UITraitCollection* traitCollection) {
  if (!IsCompactWidth(traitCollection) && !IsCompactHeight(traitCollection))
    return kSearchFieldLarge;

  // Special case for narrow sizes.
  return MIN(kSearchFieldSmall, superviewWidth - kSearchFieldMinMargin * 2);
}

CGFloat heightForLogoHeader(BOOL logoIsShowing,
                            BOOL promoCanShow,
                            BOOL toolbarPresent,
                            CGFloat topInset,
                            UITraitCollection* traitCollection) {
  CGFloat headerHeight =
      doodleTopMargin(toolbarPresent, topInset, traitCollection) +
      doodleHeight(logoIsShowing, traitCollection) + searchFieldTopMargin() +
      ToolbarExpandedHeight(
          [UIApplication sharedApplication].preferredContentSizeCategory) +
      kNTPSearchFieldBottomPadding;
  if (!IsRegularXRegularSizeClass(traitCollection)) {
    return headerHeight;
  }
  if (!logoIsShowing) {
    // Returns sufficient vertical space for the Identity Disc to be
    // displayed.
    return ntp_home::kIdentityAvatarDimension +
           2 * ntp_home::kIdentityAvatarMargin;
  }
  if (!promoCanShow) {
    headerHeight += kTopSpacingMaterial;
  }

  return headerHeight;
}

void configureSearchHintLabel(UILabel* searchHintLabel,
                              UIView* searchTapTarget) {
  [searchHintLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [searchTapTarget addSubview:searchHintLabel];

  [searchHintLabel setText:l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)];
  if (base::i18n::IsRTL()) {
    [searchHintLabel setTextAlignment:NSTextAlignmentRight];
  }
  searchHintLabel.textColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  searchHintLabel.adjustsFontForContentSizeCategory = YES;
  searchHintLabel.textAlignment = NSTextAlignmentCenter;
}

void configureVoiceSearchButton(UIButton* voiceSearchButton,
                                UIView* searchTapTarget) {
  [voiceSearchButton setTranslatesAutoresizingMaskIntoConstraints:NO];
  [searchTapTarget addSubview:voiceSearchButton];

  [voiceSearchButton setAdjustsImageWhenHighlighted:NO];

  UIImage* micImage = [[UIImage imageNamed:@"location_bar_voice"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [voiceSearchButton setImage:micImage forState:UIControlStateNormal];
  voiceSearchButton.tintColor = [UIColor colorNamed:kGrey500Color];
  [voiceSearchButton setAccessibilityLabel:l10n_util::GetNSString(
                                               IDS_IOS_ACCNAME_VOICE_SEARCH)];
  [voiceSearchButton setAccessibilityIdentifier:@"Voice Search"];

  if (@available(iOS 13.4, *)) {
      voiceSearchButton.pointerInteractionEnabled = YES;
      // Make the pointer shape fit the location bar's semi-circle end shape.
      voiceSearchButton.pointerStyleProvider =
          CreateLiftEffectCirclePointerStyleProvider();
  }
}

UIView* nearestAncestor(UIView* view, Class aClass) {
  if (!view) {
    return nil;
  }
  if ([view isKindOfClass:aClass]) {
    return view;
  }
  return nearestAncestor([view superview], aClass);
}

}  // namespace content_suggestions
