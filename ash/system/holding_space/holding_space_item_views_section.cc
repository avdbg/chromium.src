// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_views_section.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "base/auto_reset.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/platform_style.h"

namespace ash {

namespace {

using ScrollBarMode = views::ScrollView::ScrollBarMode;

// Animation.
constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(167);

// Helpers ---------------------------------------------------------------------

// Initializes the layer for the specified `view` for animations.
void InitLayerForAnimations(views::View* view) {
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
  view->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

// Returns a callback which deletes the associated animation observer after
// running another `callback`.
using AnimationCompletedCallback =
    base::OnceCallback<void(const ui::CallbackLayerAnimationObserver&)>;
base::RepeatingCallback<bool(const ui::CallbackLayerAnimationObserver&)>
DeleteObserverAfterRunning(AnimationCompletedCallback callback) {
  return base::BindRepeating(
      [](AnimationCompletedCallback callback,
         const ui::CallbackLayerAnimationObserver& observer) {
        // NOTE: It's safe to move `callback` since this code will only run
        // once due to deletion of the associated `observer`. The `observer` is
        // deleted by returning `true`.
        std::move(callback).Run(observer);
        return true;
      },
      base::Passed(std::move(callback)));
}

// HoldingSpaceScrollView ------------------------------------------------------

class HoldingSpaceScrollView : public views::ScrollView,
                               public views::ViewObserver {
 public:
  HoldingSpaceScrollView() {
    // `HoldingSpaceItemView`s draw a focus ring outside of their view bounds.
    // `HoldingSpaceScrollView` needs to paint to a layer to avoid clipping
    // these focus rings.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }

  views::View* SetContents(std::unique_ptr<views::View> view) {
    views::View* contents = views::ScrollView::SetContents(std::move(view));
    view_observer_.Observe(contents);
    return contents;
  }

 private:
  // views::ScrollView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    // The focus ring for `HoldingSpaceItemView`s is painted just outside of
    // their view bounds. The clip rect for this view should be expanded to
    // avoid clipping of these focus rings. Note that a clip rect *does* need to
    // be applied to prevent this view from painting its contents outside of its
    // viewport.
    const float kFocusInsets =
        kHoldingSpaceFocusInsets -
        (views::PlatformStyle::kFocusHaloThickness / 2.f);
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(gfx::Insets(kFocusInsets));
    layer()->SetClipRect(bounds);
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(View* observed_view) override {
    PreferredSizeChanged();
  }

  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override {
    // Sync scroll view visibility with contents visibility.
    if (GetVisible() != observed_view->GetVisible())
      SetVisible(observed_view->GetVisible());
  }

  void OnViewIsDeleting(View* observed_view) override {
    view_observer_.Reset();
  }

  base::ScopedObservation<views::View, views::ViewObserver> view_observer_{
      this};
};

}  // namespace

// HoldingSpaceItemViewsSection ------------------------------------------------

HoldingSpaceItemViewsSection::HoldingSpaceItemViewsSection(
    HoldingSpaceItemViewDelegate* delegate,
    std::set<HoldingSpaceItem::Type> supported_types,
    const base::Optional<size_t>& max_count)
    : delegate_(delegate),
      supported_types_(std::move(supported_types)),
      max_count_(max_count) {}

HoldingSpaceItemViewsSection::~HoldingSpaceItemViewsSection() = default;

void HoldingSpaceItemViewsSection::Init() {
  // Disable propagation of `PreferredSizeChanged()` while initializing this
  // view to reduce the number of layout events bubbling up.
  disable_preferred_size_changed_ = true;

  SetVisible(false);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kHoldingSpaceSectionChildSpacing));

  // Header.
  header_ = AddChildView(CreateHeader());
  InitLayerForAnimations(header_);
  header_->layer()->SetOpacity(0.f);
  header_->SetVisible(false);

  // Container.
  // NOTE: If `max_count_` is not present `container_` does not limit the number
  // of holding space item views visible to the user at one time. In this case
  // `container_` needs to be wrapped in a `views::ScrollView` to allow the user
  // access to all contained item views.
  if (max_count_.has_value()) {
    container_ = AddChildView(CreateContainer());
  } else {
    auto* scroll = AddChildView(std::make_unique<HoldingSpaceScrollView>());
    scroll->SetBackgroundColor(base::nullopt);
    scroll->ClipHeightTo(0, INT_MAX);
    scroll->SetDrawOverflowIndicator(false);
    scroll->SetVerticalScrollBarMode(ScrollBarMode::kHiddenButEnabled);
    layout->SetFlexForView(scroll, 1);
    container_ = scroll->SetContents(CreateContainer());
    scroll_view_ = scroll;
  }

  InitLayerForAnimations(container_);
  container_->SetVisible(false);

  // Placeholder.
  auto placeholder = CreatePlaceholder();
  if (placeholder) {
    placeholder_ = AddChildView(std::move(placeholder));
    InitLayerForAnimations(placeholder_);
    placeholder_->SetVisible(true);
    header_->layer()->SetOpacity(1.f);
    header_->SetVisible(true);
  }

  // Views.
  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  if (model && !model->items().empty()) {
    std::vector<const HoldingSpaceItem*> item_ptrs;
    for (const auto& item : model->items())
      item_ptrs.push_back(item.get());

    // Sections are not animated during initialization as their respective
    // bubbles will be animated in instead.
    base::AutoReset<bool> scoped_disable_animations(&disable_animations_, true);
    OnHoldingSpaceItemsAdded(item_ptrs);
  }

  // Re-enable propagation of `PreferredSizeChanged()` after initializing.
  disable_preferred_size_changed_ = false;
  PreferredSizeChanged();
}

void HoldingSpaceItemViewsSection::Reset() {
  // The holding space item views `delegate_` will be destroyed before this view
  // when asynchronously closing the holding space bubble. To prevent accessing
  // `delegate_` after deletion, prevent animation callbacks from being run.
  weak_factory_.InvalidateWeakPtrs();

  // Propagate `Reset()` to children.
  for (views::View* view : container_->children()) {
    DCHECK(HoldingSpaceItemView::IsInstance(view));
    HoldingSpaceItemView::Cast(view)->Reset();
  }
}

std::vector<HoldingSpaceItemView*>
HoldingSpaceItemViewsSection::GetHoldingSpaceItemViews() {
  std::vector<HoldingSpaceItemView*> views;
  for (views::View* view : container_->children()) {
    DCHECK(HoldingSpaceItemView::IsInstance(view));
    views.push_back(HoldingSpaceItemView::Cast(view));
  }
  return views;
}

void HoldingSpaceItemViewsSection::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void HoldingSpaceItemViewsSection::ChildVisibilityChanged(views::View* child) {
  // This section should be visible iff it has visible children.
  bool visible = false;
  for (const views::View* c : children()) {
    if (c->GetVisible()) {
      visible = true;
      break;
    }
  }

  if (visible != GetVisible())
    SetVisible(visible);

  PreferredSizeChanged();
}

void HoldingSpaceItemViewsSection::PreferredSizeChanged() {
  if (!disable_preferred_size_changed_)
    views::View::PreferredSizeChanged();
}

void HoldingSpaceItemViewsSection::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.parent != container_)
    return;

  // Update visibility when becoming empty or non-empty. Note that in the case
  // of a view being added, `ViewHierarchyChanged()` is called *after* the view
  // has been parented but in the case of a view being removed, it is called
  // *before* the view is un-parented.
  if (container_->children().size() != 1u)
    return;

  // Disable propagation of `PreferredSizeChanged()` while modifying child
  // view visibility to reduce the number of layout events bubbling up.
  disable_preferred_size_changed_ = true;

  header_->SetVisible(placeholder_ || details.is_add);
  container_->SetVisible(details.is_add);

  if (placeholder_)
    placeholder_->SetVisible(!details.is_add);

  // Re-enable propagation of `PreferredSizeChanged()` after modifying child
  // view visibility.
  disable_preferred_size_changed_ = false;
  PreferredSizeChanged();
}

void HoldingSpaceItemViewsSection::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  const bool needs_update = std::any_of(
      items.begin(), items.end(), [this](const HoldingSpaceItem* item) {
        return item->IsFinalized() &&
               base::Contains(supported_types_, item->type());
      });
  if (needs_update)
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsSection::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  const bool needs_update = std::any_of(
      items.begin(), items.end(), [this](const HoldingSpaceItem* item) {
        return base::Contains(views_by_item_id_, item->id());
      });
  if (needs_update)
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsSection::OnHoldingSpaceItemFinalized(
    const HoldingSpaceItem* item) {
  if (base::Contains(supported_types_, item->type()))
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsSection::RemoveAllHoldingSpaceItemViews() {
  // Holding space item views should only be removed when the `container_` is
  // not visible to the user.
  DCHECK(!IsDrawn() || !container_->IsDrawn() ||
         container_->layer()->opacity() == 0.f);
  container_->RemoveAllChildViews(/*delete_children=*/true);
  views_by_item_id_.clear();
}

std::unique_ptr<views::View> HoldingSpaceItemViewsSection::CreatePlaceholder() {
  return nullptr;
}

void HoldingSpaceItemViewsSection::DestroyPlaceholder() {
  if (!placeholder_)
    return;

  RemoveChildViewT(placeholder_);
  placeholder_ = nullptr;

  // In the absence of `placeholder_`, the `header_` should only be visible
  // when `container_` is non-empty.
  if (header_->GetVisible() && container_->children().empty())
    header_->SetVisible(false);
}

void HoldingSpaceItemViewsSection::MaybeAnimateIn() {
  if (animation_state_ & AnimationState::kAnimatingIn)
    return;

  animation_state_ |= AnimationState::kAnimatingIn;

  // NOTE: `animate_in_observer` is deleted after `OnAnimateInCompleted()`.
  ui::CallbackLayerAnimationObserver* animate_in_observer =
      new ui::CallbackLayerAnimationObserver(DeleteObserverAfterRunning(
          base::BindOnce(&HoldingSpaceItemViewsSection::OnAnimateInCompleted,
                         weak_factory_.GetWeakPtr())));

  AnimateIn(animate_in_observer);
  animate_in_observer->SetActive();
}

void HoldingSpaceItemViewsSection::MaybeAnimateOut() {
  if (animation_state_ & AnimationState::kAnimatingOut)
    return;

  animation_state_ |= AnimationState::kAnimatingOut;

  // Don't allow event processing while animating out. The views being animated
  // out may be associated with holding space items that no longer exist and
  // so should not be acted upon by the user during this time.
  SetCanProcessEventsWithinSubtree(false);

  // Hide the vertical scroll bar when swapping out section contents to prevent
  // it from showing as views are being added/removed and while the holding
  // space bubble is animating bounds.
  if (scroll_view_)
    scroll_view_->SetVerticalScrollBarMode(ScrollBarMode::kHiddenButEnabled);

  // NOTE: `animate_out_observer` is deleted after `OnAnimateOutCompleted()`.
  ui::CallbackLayerAnimationObserver* animate_out_observer =
      new ui::CallbackLayerAnimationObserver(DeleteObserverAfterRunning(
          base::BindOnce(&HoldingSpaceItemViewsSection::OnAnimateOutCompleted,
                         weak_factory_.GetWeakPtr())));

  AnimateOut(animate_out_observer);
  animate_out_observer->SetActive();
}

void HoldingSpaceItemViewsSection::AnimateIn(
    ui::LayerAnimationObserver* observer) {
  const base::TimeDelta animation_duration =
      disable_animations_ ? base::TimeDelta() : kAnimationDuration;

  // Delay animations slightly to allow time for bubble layout animations to
  // complete which animate size changes for this view when needed.
  const base::TimeDelta animation_delay =
      disable_animations_ ? base::TimeDelta() : kAnimationDuration;

  // If the `header_` is not opaque, this section was not previously visible
  // to the user so the `header_` needs to be animated in alongside any content.
  const bool animate_in_header = header_->layer()->GetTargetOpacity() != 1.f;
  if (animate_in_header) {
    holding_space_util::AnimateIn(header_, animation_duration, animation_delay,
                                  observer);
  }

  if (views_by_item_id_.empty() && placeholder_) {
    holding_space_util::AnimateIn(placeholder_, animation_duration,
                                  animation_delay, observer);
    return;
  }

  holding_space_util::AnimateIn(container_, animation_duration, animation_delay,
                                observer);
}

void HoldingSpaceItemViewsSection::AnimateOut(
    ui::LayerAnimationObserver* observer) {
  // If this view is not drawn, animating will only cause latency to the user.
  const bool disable_animations = disable_animations_ || !IsDrawn();
  const base::TimeDelta animation_duration =
      disable_animations ? base::TimeDelta() : kAnimationDuration;

  // If this section does not have a `placeholder_` and the model does not
  // contain any associated and finalized items, then this section is becoming
  // invisible to the user and the `header_` needs to be animated out alongside
  // any content.
  bool animate_out_header = !placeholder_;
  if (animate_out_header) {
    HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
    if (model) {
      animate_out_header = std::none_of(
          supported_types_.begin(), supported_types_.end(),
          [&model](HoldingSpaceItem::Type supported_type) {
            return model->ContainsFinalizedItemOfType(supported_type);
          });
    }
  }

  if (animate_out_header)
    holding_space_util::AnimateOut(header_, animation_duration, observer);

  if (placeholder_ && placeholder_->GetVisible()) {
    DCHECK(views_by_item_id_.empty());
    holding_space_util::AnimateOut(placeholder_, animation_duration, observer);
    return;
  }

  holding_space_util::AnimateOut(container_, animation_duration, observer);
}

void HoldingSpaceItemViewsSection::OnAnimateInCompleted(
    const ui::CallbackLayerAnimationObserver& observer) {
  DCHECK(animation_state_ & AnimationState::kAnimatingIn);
  animation_state_ &= ~AnimationState::kAnimatingIn;

  if (observer.aborted_count())
    return;

  DCHECK_EQ(animation_state_, AnimationState::kNotAnimating);

  // Restore event processing that was disabled while animating out. The views
  // that have been animated in should all be associated with holding space
  // items that exist in the model.
  SetCanProcessEventsWithinSubtree(true);

  // Once contents have animated in the holding space bubble should have reached
  // its target bounds and the vertical scroll bar can be re-enabled.
  if (scroll_view_)
    scroll_view_->SetVerticalScrollBarMode(ScrollBarMode::kEnabled);
}

void HoldingSpaceItemViewsSection::OnAnimateOutCompleted(
    const ui::CallbackLayerAnimationObserver& observer) {
  DCHECK(animation_state_ & AnimationState::kAnimatingOut);
  animation_state_ &= ~AnimationState::kAnimatingOut;

  if (observer.aborted_count())
    return;

  DCHECK_EQ(animation_state_, AnimationState::kNotAnimating);

  // All holding space item views are going to be removed after which views will
  // be re-added for those items which still exist. A `ScopedSelectionRestore`
  // will serve to persist the current selection during this modification.
  HoldingSpaceItemViewDelegate::ScopedSelectionRestore scoped_selection_restore(
      delegate_);

  // Disable propagation of `PreferredSizeChanged()` while performing batch
  // child additions/removals to reduce the number of layout events bubbling up.
  disable_preferred_size_changed_ = true;
  base::ScopedClosureRunner scoped_preferred_size_changed(base::BindOnce(
      [](HoldingSpaceItemViewsSection* section) {
        section->disable_preferred_size_changed_ = false;
        section->PreferredSizeChanged();
      },
      base::Unretained(this)));

  RemoveAllHoldingSpaceItemViews();
  DCHECK(views_by_item_id_.empty());

  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  if (!model)
    return;

  for (const auto& item : model->items()) {
    if (item->IsFinalized() && base::Contains(supported_types_, item->type())) {
      DCHECK(!base::Contains(views_by_item_id_, item->id()));

      // Remove the last holding space item view if already at max capacity.
      if (max_count_ && container_->children().size() == max_count_.value()) {
        auto view = container_->RemoveChildViewT(container_->children().back());
        views_by_item_id_.erase(
            HoldingSpaceItemView::Cast(view.get())->item()->id());
      }

      // Add holding space item view to the front in order to sort by recency.
      views_by_item_id_[item->id()] =
          container_->AddChildViewAt(CreateView(item.get()), 0);
    }
  }

  // Only animate this section in if it has content to show.
  if (placeholder_ || !container_->children().empty())
    MaybeAnimateIn();
}

}  // namespace ash
