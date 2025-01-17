// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_

#include <memory>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observation.h"

namespace ash {

class AssistantMainView;
class AssistantViewDelegate;
class ViewShadow;

// The Assistant page for the app list.
class APP_LIST_EXPORT AssistantPageView : public AppListPage,
                                          public AssistantControllerObserver,
                                          public AssistantUiModelObserver {
 public:
  explicit AssistantPageView(AssistantViewDelegate* assistant_view_delegate);
  ~AssistantPageView() override;

  // AppListPage:
  const char* GetClassName() const override;
  gfx::Size GetMinimumSize() const override;
  void OnBoundsChanged(const gfx::Rect& prev_bounds) override;
  void RequestFocus() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void OnWillBeShown() override;
  void OnAnimationStarted(AppListState from_state,
                          AppListState to_state) override;
  gfx::Size GetPreferredSearchBoxSize() const override;
  base::Optional<int> GetSearchBoxTop(
      AppListViewState view_state) const override;
  void UpdatePageOpacityForState(AppListState state,
                                 float search_box_opacity,
                                 bool restore_opacity) override;
  gfx::Rect GetPageBoundsForState(
      AppListState state,
      const gfx::Rect& contents_bounds,
      const gfx::Rect& search_box_bounds) const override;
  views::View* GetFirstFocusableView() override;
  views::View* GetLastFocusableView() override;
  void AnimateYPosition(AppListViewState target_view_state,
                        const TransformAnimator& animator,
                        float default_offset) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

 private:
  void InitLayout();
  void MaybeUpdateAppListState(int child_height);

  AssistantViewDelegate* const assistant_view_delegate_;

  // Owned by the view hierarchy.
  AssistantMainView* assistant_main_view_ = nullptr;

  int min_height_dip_;

  std::unique_ptr<ViewShadow> view_shadow_;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantPageView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
