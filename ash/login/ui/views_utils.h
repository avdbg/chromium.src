// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_VIEWS_UTILS_H_
#define ASH_LOGIN_UI_VIEWS_UTILS_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/style/ash_color_provider.h"
#include "ui/views/controls/label.h"

namespace views {
class FocusRing;
class View;
class Widget;
}  // namespace views

namespace ash {

namespace login_views_utils {

namespace {
constexpr int kDefaultLineHeight = 20;

// Helper function to get default font list for login/lock screen text label.
// It is slightly different from views::Label::GetDefaultFontList since the
// font size returned is 13 pt instead of 12 pt.
const gfx::FontList GetLoginDefaultFontList() {
  return gfx::FontList(
      {login_constants::kDefaultFontName}, gfx::Font::FontStyle::NORMAL,
      login_constants::kDefaultFontSize, gfx::Font::Weight::NORMAL);
}

}  // namespace

// Wraps view in another view so the original view is sized to it's preferred
// size, regardless of the view's parent's layout manager.
ASH_EXPORT std::unique_ptr<views::View> WrapViewForPreferredSize(
    std::unique_ptr<views::View> view);

// Returns true if landscape constants should be used for UI shown in |widget|.
ASH_EXPORT bool ShouldShowLandscape(const views::Widget* widget);

// Returns true if |view| or any of its descendant views HasFocus.
ASH_EXPORT bool HasFocusInAnyChildView(views::View* view);

// Creates a standard text label for use in the login bubbles.
// If |view_defining_max_width| is set, we allow the label to have multiple
// lines and we set its maximum width to the preferred width of
// |view_defining_max_width|.
views::Label* CreateBubbleLabel(
    const base::string16& message,
    views::View* view_defining_max_width = nullptr,
    SkColor color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary),
    const gfx::FontList& font_list = GetLoginDefaultFontList(),
    int line_height = kDefaultLineHeight);

// Get the bubble container for |view| to place a LoginBaseBubbleView.
views::View* GetBubbleContainer(views::View* view);

ASH_EXPORT gfx::Point CalculateBubblePositionAfterBeforeStrategy(
    gfx::Rect anchor,
    gfx::Size bubble,
    gfx::Rect bounds);

ASH_EXPORT gfx::Point CalculateBubblePositionBeforeAfterStrategy(
    gfx::Rect anchor,
    gfx::Size bubble,
    gfx::Rect bounds);

// Applies a rectangular focus ring to |focus_ring| and round ink drop to
// |view|. |focus_ring| may not be the ring associated with |view|. If |radius|
// is passed the ink drop will be a circle with radius |radius| otherwise its
// radius will be determined by the view's bounds.
void ConfigureRectFocusRingCircleInkDrop(views::View* view,
                                         views::FocusRing* focus_ring,
                                         base::Optional<int> radius);

}  // namespace login_views_utils

}  // namespace ash

#endif  // ASH_LOGIN_UI_VIEWS_UTILS_H_
