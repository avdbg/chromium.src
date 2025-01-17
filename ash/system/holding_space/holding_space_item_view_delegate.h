// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_DELEGATE_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/callback_list.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"

namespace ui {
class GestureEvent;
class KeyEvent;
class MouseEvent;
}  // namespace ui

namespace views {
class MenuRunner;
}  // namespace views

namespace ash {

class HoldingSpaceItemView;
class HoldingSpaceTrayBubble;

// A delegate for `HoldingSpaceItemView`s which implements context menu,
// drag-and-drop, and selection functionality. In order to support multiple
// selections at a time, all `HoldingSpaceItemView`s must share the same
// `HoldingSpaceItemViewDelegate` instance.
class ASH_EXPORT HoldingSpaceItemViewDelegate
    : public views::ContextMenuController,
      public views::DragController,
      public ui::SimpleMenuModel::Delegate,
      public TabletModeObserver {
 public:
  // A class which caches the current selection of holding space item views on
  // creation and restores that selection on destruction.
  class ScopedSelectionRestore {
   public:
    explicit ScopedSelectionRestore(HoldingSpaceItemViewDelegate* delegate);
    ScopedSelectionRestore(const ScopedSelectionRestore&) = delete;
    ScopedSelectionRestore& operator=(const ScopedSelectionRestore&) = delete;
    ~ScopedSelectionRestore();

   private:
    HoldingSpaceItemViewDelegate* const delegate_;
    std::vector<std::string> selected_item_ids_;
    base::Optional<std::string> selected_range_start_item_id_;
    base::Optional<std::string> selected_range_end_item_id_;
  };

  explicit HoldingSpaceItemViewDelegate(HoldingSpaceTrayBubble* bubble);
  HoldingSpaceItemViewDelegate(const HoldingSpaceItemViewDelegate&) = delete;
  HoldingSpaceItemViewDelegate& operator=(const HoldingSpaceItemViewDelegate&) =
      delete;
  ~HoldingSpaceItemViewDelegate() override;

  // Invoked when `view` has been created.
  void OnHoldingSpaceItemViewCreated(HoldingSpaceItemView* view);

  // Invoked when `view` is being destroyed.
  void OnHoldingSpaceItemViewDestroying(HoldingSpaceItemView* view);

  // Invoked when `view` should perform an accessible action. Returns true if
  // the action is handled, otherwise false.
  bool OnHoldingSpaceItemViewAccessibleAction(
      HoldingSpaceItemView* view,
      const ui::AXActionData& action_data);

  // Invoked when `view` receives the specified gesture `event`.
  void OnHoldingSpaceItemViewGestureEvent(HoldingSpaceItemView* view,
                                          const ui::GestureEvent& event);

  // Invoked when `view` receives the specified key pressed `event`.
  bool OnHoldingSpaceItemViewKeyPressed(HoldingSpaceItemView* view,
                                        const ui::KeyEvent& event);

  // Invoked when `view` receives the specified mouse pressed `event`.
  bool OnHoldingSpaceItemViewMousePressed(HoldingSpaceItemView* view,
                                          const ui::MouseEvent& event);

  // Invoked when `view` receives the specified mouse released `event`.
  void OnHoldingSpaceItemViewMouseReleased(HoldingSpaceItemView* view,
                                           const ui::MouseEvent& event);

  // Invoked when `view` has changed selected state.
  void OnHoldingSpaceItemViewSelectedChanged(HoldingSpaceItemView* view);

  // Invoked when the tray bubble receives the specified key pressed `event`.
  bool OnHoldingSpaceTrayBubbleKeyPressed(const ui::KeyEvent& event);

  // Invoked when a tray child bubble receives the specified gesture `event`.
  void OnHoldingSpaceTrayChildBubbleGestureEvent(const ui::GestureEvent& event);

  // Invoked when a tray child bubble receives the given mouse pressed `event`.
  void OnHoldingSpaceTrayChildBubbleMousePressed(const ui::MouseEvent& event);

  // Enumeration of possible selection UI's.
  enum class SelectionUi {
    kSingleSelect,  // UI should reflect single selection.
    kMultiSelect,   // UI should reflect multiple selection.
  };

  // Returns the current `selection_ui_` which dictates how UI should represent
  // holding space item views' selected states to the user.
  SelectionUi selection_ui() const { return selection_ui_; }

  // Registers a `callback` to be notified of changes to `selection_ui_`. To
  // unregister, destroy the returned subscription.
  base::RepeatingClosureList::Subscription AddSelectionUiChangedCallback(
      base::RepeatingClosureList::CallbackType callback);

 private:
  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // views::DragController:
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& current_pt) override;
  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& press_pt) override;
  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;

  // SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Builds and returns a raw pointer to `context_menu_model_`.
  ui::SimpleMenuModel* BuildMenuModel();

  // Returns the subset of views which are currently selected. Views are
  // returned in top-to-bottom, left-to-right order (or mirrored for RTL).
  std::vector<const HoldingSpaceItemView*> GetSelection();

  // Marks all views as unselected.
  void ClearSelection();

  // Marks `view` as selected. All other views are marked unselected.
  void SetSelection(HoldingSpaceItemView* view);

  // Marks any views whose associated holding space items are contained in
  // `item_ids` as selected. All other views are marked unselected.
  void SetSelection(const std::vector<std::string>& item_ids);

  // Marks any views between the specified `start` and `end` points (inclusive)
  // as selected. Any views in a previously selected range, as tracked by
  // `selected_range_start_` and `selected_range_end_`, will be marked as
  // unselected. Any views outside of these ranges will not be affected.
  void SetSelectedRange(HoldingSpaceItemView* start, HoldingSpaceItemView* end);

  // Updates `selection_ui_` based on device state and `selection_size_`.
  void UpdateSelectionUi();

  HoldingSpaceTrayBubble* const bubble_;

  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  // Caches a view for which mouse released events should be temporarily
  // ignored. This is to prevent us from selecting a view on mouse pressed but
  // then unselecting that same view on mouse released.
  HoldingSpaceItemView* ignore_mouse_released_ = nullptr;

  // Caches views from which range-based selections should start and end. This
  // is used when determining the range for selection performed via shift-click.
  HoldingSpaceItemView* selected_range_start_ = nullptr;
  HoldingSpaceItemView* selected_range_end_ = nullptr;

  // Dictates how UI should represent holding space item views' selected states
  // to the user based on device state and `selection_size_`.
  SelectionUi selection_ui_;

  // List of callbacks to be run on changes to `selection_ui_`.
  base::RepeatingClosureList selection_ui_changed_callbacks_;

  // Cached size of the selection of holding space item views.
  size_t selection_size_ = 0u;

  base::ScopedObservation<TabletMode, TabletModeObserver> tablet_mode_observer_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_DELEGATE_H_
