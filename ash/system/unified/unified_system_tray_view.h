// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

// namespace views {
// class FocusSearch;
// }

namespace ash {

class FeaturePodButton;
class FeaturePodsContainerView;
class TopShortcutsView;
class UnifiedMediaControlsContainer;
class NotificationHiddenView;
class PageIndicatorView;
class UnifiedSystemInfoView;
class UnifiedSystemTrayController;

// Container view of slider views. If SetExpandedAmount() is called with 1.0,
// the behavior is same as vertiacal BoxLayout, but otherwise it shows
// intermediate state during animation.
class UnifiedSlidersContainerView : public views::View {
 public:
  explicit UnifiedSlidersContainerView(bool initially_expanded);
  ~UnifiedSlidersContainerView() override;

  // Change the expanded state. 0.0 if collapsed, and 1.0 if expanded.
  // Otherwise, it shows intermediate state.
  void SetExpandedAmount(double expanded_amount);

  // Get height of the view when |expanded_amount| is set to 1.0.
  int GetExpandedHeight() const;

  // Update opacity of each child slider views based on |expanded_amount_|.
  void UpdateOpacity();

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;

 private:
  double expanded_amount_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSlidersContainerView);
};

// View class of the main bubble in UnifiedSystemTray.
//
// The UnifiedSystemTray contains two sub components:
//    1. MessageCenter: contains the list of notifications
//    2. SystemTray: contains quick settings controls
// Note that the term "UnifiedSystemTray" refers to entire bubble containing
// both (1) and (2).
class ASH_EXPORT UnifiedSystemTrayView : public views::View,
                                         public views::FocusTraversable,
                                         public views::FocusChangeListener {
 public:
  UnifiedSystemTrayView(UnifiedSystemTrayController* controller,
                        bool initially_expanded);
  ~UnifiedSystemTrayView() override;

  // Set the maximum height that the view can take.
  void SetMaxHeight(int max_height);

  // Add feature pod button to |feature_pods_|.
  void AddFeaturePodButton(FeaturePodButton* button);

  // Add slider view.
  void AddSliderView(views::View* slider_view);

  // Add media controls view to |media_controls_container_|;
  void AddMediaControlsView(views::View* media_controls);

  // Hide the main view and show the given |detailed_view|.
  void SetDetailedView(views::View* detailed_view);

  // Remove the detailed view set by SetDetailedView, and show the main view.
  // It deletes |detailed_view| and children.
  void ResetDetailedView();

  // Save and restore keyboard focus of the currently focused element. Called
  // before transitioning into a detailed view.
  void SaveFocus();
  void RestoreFocus();

  // Set the first child view to be focused when focus is acquired.
  // This is the first visible child unless reverse is true, in which case
  // it is the last visible child.
  void FocusEntered(bool reverse);

  // Change the expanded state. 0.0 if collapsed, and 1.0 if expanded.
  // Otherwise, it shows intermediate state.
  void SetExpandedAmount(double expanded_amount);

  // Get height of the system tray (excluding the message center) when
  // |expanded_amount| is set to 1.0.
  //
  // Note that this function is used to calculate the transform-based
  // collapse/expand animation, which is currently only enabled when there are
  // no notifications.
  int GetExpandedSystemTrayHeight() const;

  // Get height of the system menu (excluding the message center) when
  // |expanded_amount| is set to 0.0.
  int GetCollapsedSystemTrayHeight() const;

  // Get current height of the view (including the message center).
  int GetCurrentHeight() const;

  // Returns the number of visible feature pods.
  int GetVisibleFeaturePodCount() const;

  // Get the accessible name for the currently shown detailed view.
  base::string16 GetDetailedViewAccessibleName() const;

  // Returns true if a detailed view is being shown in the tray. (e.g Bluetooth
  // Settings).
  bool IsDetailedViewShown() const;

  // Show media controls view.
  void ShowMediaControls();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  const char* GetClassName() const override;
  views::FocusTraversable* GetFocusTraversable() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::FocusTraversable:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  FeaturePodsContainerView* feature_pods_container() {
    return feature_pods_container_;
  }

  NotificationHiddenView* notification_hidden_view_for_testing() {
    return notification_hidden_view_;
  }

  View* detailed_view() { return detailed_view_container_; }
  View* detailed_view_for_testing() { return detailed_view_container_; }
  PageIndicatorView* page_indicator_view_for_test() {
    return page_indicator_view_;
  }
  UnifiedMediaControlsContainer* media_controls_container_for_testing() {
    return media_controls_container_;
  }

 private:
  class SystemTrayContainer;
  friend class UnifiedMessageCenterBubbleTest;

  // Get first and last focusable child views. These functions are used to
  // figure out if we need to focus out or to set the correct focused view
  // when focus is acquired from another widget.
  View* GetFirstFocusableChild();
  View* GetLastFocusableChild();

  double expanded_amount_;

  // Unowned.
  UnifiedSystemTrayController* const controller_;

  // Owned by views hierarchy.
  NotificationHiddenView* const notification_hidden_view_;
  TopShortcutsView* const top_shortcuts_view_;
  FeaturePodsContainerView* const feature_pods_container_;
  PageIndicatorView* const page_indicator_view_;
  UnifiedSlidersContainerView* const sliders_container_;
  UnifiedSystemInfoView* const system_info_view_;
  SystemTrayContainer* const system_tray_container_;
  views::View* const detailed_view_container_;

  // Null if media::kGlobalMediaControlsForChromeOS is disabled.
  UnifiedMediaControlsContainer* media_controls_container_ = nullptr;

  // The maximum height available to the view.
  int max_height_ = 0;

  // The view that is saved by calling SaveFocus().
  views::View* saved_focused_view_ = nullptr;

  const std::unique_ptr<views::FocusSearch> focus_search_;

  views::FocusManager* focus_manager_ = nullptr;

  const std::unique_ptr<ui::EventHandler> interacted_by_tap_recorder_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_VIEW_H_
