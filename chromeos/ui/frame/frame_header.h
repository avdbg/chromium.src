// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_FRAME_HEADER_H_
#define CHROMEOS_UI_FRAME_FRAME_HEADER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/window/frame_caption_button.h"

namespace ash {
FORWARD_DECLARE_TEST(DefaultFrameHeaderTest, BackButtonAlignment);
FORWARD_DECLARE_TEST(DefaultFrameHeaderTest, TitleIconAlignment);
FORWARD_DECLARE_TEST(DefaultFrameHeaderTest, FrameColors);
class FramePaintWaiter;
}  // namespace ash

namespace gfx {
class Canvas;
class Rect;
}  // namespace gfx

namespace ui {
class Layer;
class LayerTreeOwner;
}  // namespace ui

namespace views {
enum class CaptionButtonLayoutSize;
class View;
class Widget;
}  // namespace views

namespace chromeos {

class CaptionButtonModel;

// Helper class for managing the window header.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) FrameHeader {
 public:
  // An invisible view that drives the frame's animation. This holds the
  // animating layer as a layer beneath this view so that it's behind all other
  // child layers of the window to avoid hiding their contents.
  class FrameAnimatorView : public views::View,
                            public views::ViewObserver,
                            public ui::ImplicitAnimationObserver {
   public:
    METADATA_HEADER(FrameAnimatorView);
    explicit FrameAnimatorView(views::View* parent);
    FrameAnimatorView(const FrameAnimatorView&) = delete;
    FrameAnimatorView& operator=(const FrameAnimatorView&) = delete;
    ~FrameAnimatorView() override;

    void StartAnimation(base::TimeDelta duration);

    // views::Views:
    std::unique_ptr<ui::Layer> RecreateLayer() override;

    // ViewObserver:
    void OnChildViewReordered(views::View* observed_view,
                              views::View* child) override;
    void OnViewBoundsChanged(views::View* observed_view) override;

    // ui::ImplicitAnimationObserver overrides:
    void OnImplicitAnimationsCompleted() override;

   private:
    void StopAnimation();

    views::View* parent_;
    std::unique_ptr<ui::LayerTreeOwner> layer_owner_;
  };

  enum Mode { MODE_ACTIVE, MODE_INACTIVE };

  static FrameHeader* Get(views::Widget* widget);

  virtual ~FrameHeader();

  const base::string16& frame_text_override() const {
    return frame_text_override_;
  }

  // Returns the header's minimum width.
  int GetMinimumHeaderWidth() const;

  // Paints the header.
  void PaintHeader(gfx::Canvas* canvas);

  // Performs layout for the header.
  void LayoutHeader();

  // Get the height of the header.
  int GetHeaderHeight() const;

  // Gets / sets how much of the header is painted. This allows the header to
  // paint under things (like the tabstrip) which have transparent /
  // non-painting sections. This height does not affect LayoutHeader().
  int GetHeaderHeightForPainting() const;
  void SetHeaderHeightForPainting(int height_for_painting);

  // Schedule a re-paint of the entire title.
  void SchedulePaintForTitle();

  // True to instruct the frame header to paint the header as an active
  // state.
  void SetPaintAsActive(bool paint_as_active);

  // Called when frame show state is changed.
  void OnShowStateChanged(ui::WindowShowState show_state);

  void SetLeftHeaderView(views::View* view);
  void SetBackButton(views::FrameCaptionButton* view);
  views::FrameCaptionButton* GetBackButton() const;
  const chromeos::CaptionButtonModel* GetCaptionButtonModel() const;

  // Updates the frame header painting to reflect a change in frame colors.
  virtual void UpdateFrameColors() = 0;

  // Returns window mask for the rounded corner of the frame header.
  virtual SkPath GetWindowMaskForFrameHeader(const gfx::Size& size);

  // Sets text to display in place of the window's title. This will be shown
  // regardless of what ShouldShowWindowTitle() returns.
  void SetFrameTextOverride(const base::string16& frame_text_override);

  void UpdateFrameHeaderKey();

  views::View* view() { return view_; }

  chromeos::FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

 protected:
  FrameHeader(views::Widget* target_widget, views::View* view);

  views::Widget* target_widget() { return target_widget_; }
  const views::Widget* target_widget() const { return target_widget_; }

  // Returns bounds of the region in |view_| which is painted with the header
  // images. The region is assumed to start at the top left corner of |view_|
  // and to have the same width as |view_|.
  gfx::Rect GetPaintedBounds() const;

  void UpdateCaptionButtonColors();

  void PaintTitleBar(gfx::Canvas* canvas);

  void SetCaptionButtonContainer(
      chromeos::FrameCaptionButtonContainerView* caption_button_container);

  Mode mode() const { return mode_; }

  virtual void DoPaintHeader(gfx::Canvas* canvas) = 0;
  virtual views::CaptionButtonLayoutSize GetButtonLayoutSize() const = 0;
  virtual SkColor GetTitleColor() const = 0;
  virtual SkColor GetCurrentFrameColor() const = 0;

  // Starts fade transition animation with given duration.
  void StartTransitionAnimation(base::TimeDelta duration);

 private:
  FRIEND_TEST_ALL_PREFIXES(ash::DefaultFrameHeaderTest, BackButtonAlignment);
  FRIEND_TEST_ALL_PREFIXES(ash::DefaultFrameHeaderTest, TitleIconAlignment);
  FRIEND_TEST_ALL_PREFIXES(ash::DefaultFrameHeaderTest, FrameColors);
  friend class ash::FramePaintWaiter;

  void LayoutHeaderInternal();

  gfx::Rect GetTitleBounds() const;

  // The widget that the caption buttons act on. This can be different from
  // |view_|'s widget.
  views::Widget* target_widget_;

  // The view into which |this| paints.
  views::View* view_;
  views::FrameCaptionButton* back_button_ = nullptr;  // May remain nullptr.
  views::View* left_header_view_ = nullptr;           // May remain nullptr.
  chromeos::FrameCaptionButtonContainerView* caption_button_container_ =
      nullptr;
  FrameAnimatorView* frame_animator_ = nullptr;  // owned by view tree.

  // The height of the header to paint.
  int painted_height_ = 0;

  // Used to skip animation when the frame hasn't painted yet.
  bool painted_ = false;

  // Whether the header should be painted as active.
  Mode mode_ = MODE_INACTIVE;

  base::string16 frame_text_override_;

  DISALLOW_COPY_AND_ASSIGN(FrameHeader);
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_FRAME_HEADER_H_
