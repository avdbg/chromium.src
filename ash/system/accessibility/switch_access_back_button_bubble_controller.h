// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_BACK_BUTTON_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_BACK_BUTTON_BUBBLE_CONTROLLER_H_

#include "ash/system/tray/tray_bubble_view.h"

namespace ash {

class SwitchAccessBackButtonView;

// Manages the Switch Access back button bubble.
class ASH_EXPORT SwitchAccessBackButtonBubbleController
    : public TrayBubbleView::Delegate {
 public:
  // The additional amount of space taken up by the focus rings around the
  // specified rect.
  static constexpr int kFocusRingPaddingDp = 10;

  SwitchAccessBackButtonBubbleController();
  ~SwitchAccessBackButtonBubbleController() override;

  SwitchAccessBackButtonBubbleController(
      const SwitchAccessBackButtonBubbleController&) = delete;
  SwitchAccessBackButtonBubbleController& operator=(
      const SwitchAccessBackButtonBubbleController&) = delete;

  // A different icon is used when showing for the menu.
  void ShowBackButton(const gfx::Rect& anchor,
                      bool show_focus_ring,
                      bool for_menu);
  void HideFocusRing();
  void Hide();

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;

 private:
  friend class SwitchAccessBackButtonBubbleControllerTest;
  friend class SwitchAccessMenuBubbleControllerTest;

  gfx::Rect AdjustAnchorRect(const gfx::Rect& anchor);

  bool for_menu_ = false;

  // Owned by views hierarchy.
  SwitchAccessBackButtonView* back_button_view_ = nullptr;
  TrayBubbleView* bubble_view_ = nullptr;

  views::Widget* widget_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_BACK_BUTTON_BUBBLE_CONTROLLER_H_
