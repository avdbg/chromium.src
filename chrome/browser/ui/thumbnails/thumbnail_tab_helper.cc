// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"

#include <stdint.h>
#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/background_thumbnail_video_capturer.h"
#include "chrome/browser/ui/thumbnails/thumbnail_capture_driver.h"
#include "chrome/browser/ui/thumbnails/thumbnail_readiness_tracker.h"
#include "chrome/browser/ui/thumbnails/thumbnail_scheduler.h"
#include "chrome/browser/ui/thumbnails/thumbnail_scheduler_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme.h"

namespace {

// Minimum scale factor to capture thumbnail images at. At 1.0x we want to
// slightly over-sample the image so that it looks good for multiple uses and
// cropped to different dimensions.
constexpr float kMinThumbnailScaleFactor = 1.5f;

gfx::Size GetMinimumThumbnailSize() {
  // Minimum thumbnail dimension (in DIP) for tablet tabstrip previews.
  constexpr int kMinThumbnailDimensionForTablet = 175;

  // Compute minimum sizes for multiple uses of the thumbnail - currently,
  // tablet tabstrip previews and tab hover card preview images.
  gfx::Size min_target_size = TabStyle::GetPreviewImageSize();
  min_target_size.SetToMax(
      {kMinThumbnailDimensionForTablet, kMinThumbnailDimensionForTablet});

  return min_target_size;
}

// Manages increment/decrement of video capture state on a WebContents.
// Acquires (if possible) on construction, releases (if acquired) on
// destruction.
class ScopedThumbnailCapture {
 public:
  explicit ScopedThumbnailCapture(
      content::WebContentsObserver* web_contents_observer)
      : web_contents_observer_(web_contents_observer) {
    auto* const contents = web_contents_observer->web_contents();
    if (contents) {
      contents->IncrementCapturerCount(gfx::Size(), /* stay_hidden */ true);
      captured_ = true;
    }
  }

  ~ScopedThumbnailCapture() {
    auto* const contents = web_contents_observer_->web_contents();
    if (captured_ && contents)
      contents->DecrementCapturerCount(/* stay_hidden */ true);
  }

 private:
  // We track a web contents observer because it's an easy way to see if the
  // web contents has disappeared without having to add another observer.
  content::WebContentsObserver* const web_contents_observer_;
  bool captured_ = false;
};

}  // anonymous namespace

// ThumbnailTabHelper::CaptureType ---------------------------------------

enum class ThumbnailTabHelper::CaptureType {
  // The image was copied directly from a visible RenderWidgetHostView.
  kCopyFromView = 0,
  // The image is a frame from a background tab video capturer.
  kVideoFrame = 1,

  kMaxValue = kVideoFrame,
};

// ThumbnailTabHelper::TabStateTracker ---------------------------

// Stores information about the state of the current WebContents and renderer.
class ThumbnailTabHelper::TabStateTracker
    : public content::WebContentsObserver,
      public ThumbnailCaptureDriver::Client,
      public ThumbnailImage::Delegate {
 public:
  TabStateTracker(ThumbnailTabHelper* thumbnail_tab_helper,
                  content::WebContents* contents)
      : content::WebContentsObserver(contents),
        thumbnail_tab_helper_(thumbnail_tab_helper),
        readiness_tracker_(
            contents,
            base::BindRepeating(&TabStateTracker::PageReadinessChanged,
                                base::Unretained(this))) {
    visible_ =
        (web_contents()->GetVisibility() == content::Visibility::VISIBLE);
  }
  ~TabStateTracker() override = default;

  // Returns the host view associated with the current web contents, or null if
  // none.
  content::RenderWidgetHostView* GetView() {
    auto* const contents = web_contents();
    return contents ? contents->GetMainFrame()
                          ->GetRenderViewHost()
                          ->GetWidget()
                          ->GetView()
                    : nullptr;
  }

  // Returns true if we are capturing thumbnails from a tab and should continue
  // to do so, false if we should stop.
  bool ShouldContinueVideoCapture() const { return scoped_capture_ != nullptr; }

  // Tells our scheduling logic that a frame was received.
  void OnFrameCaptured(CaptureType capture_type) {
    if (capture_type == CaptureType::kVideoFrame)
      capture_driver_.GotFrame();
  }

 private:
  using PageReadiness = ThumbnailReadinessTracker::Readiness;

  // ThumbnailCaptureDriver::Client:
  void RequestCapture() override {
    if (!scoped_capture_)
      scoped_capture_ = std::make_unique<ScopedThumbnailCapture>(this);
  }

  void StartCapture() override {
    DCHECK(scoped_capture_);
    thumbnail_tab_helper_->StartVideoCapture();
  }

  void StopCapture() override {
    thumbnail_tab_helper_->StopVideoCapture();
    scoped_capture_.reset();
  }

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override {
    const bool new_visible = (visibility == content::Visibility::VISIBLE);
    if (new_visible == visible_)
      return;

    visible_ = new_visible;
    capture_driver_.UpdatePageVisibility(visible_);
    if (!visible_ && page_readiness_ != PageReadiness::kNotReady)
      thumbnail_tab_helper_->CaptureThumbnailOnTabHidden();
  }

  void RenderViewReady() override { capture_driver_.SetCanCapture(true); }

  void RenderProcessGone(base::TerminationStatus status) override {
    // TODO(crbug.com/1073141): determine if there are other ways to
    // lose the view.
    capture_driver_.SetCanCapture(false);
  }

  // ThumbnailImage::Delegate:
  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override {
    capture_driver_.UpdateThumbnailVisibility(is_being_observed);
    if (is_being_observed)
      web_contents()->GetController().LoadIfNecessary();
  }

  void PageReadinessChanged(PageReadiness readiness) {
    if (page_readiness_ == readiness)
      return;
    // If we transition back to a kNotReady state, clear any existing thumbnail,
    // as it will contain an old snapshot, possibly from a different domain.
    if (readiness == PageReadiness::kNotReady)
      thumbnail_tab_helper_->ClearData();
    page_readiness_ = readiness;
    capture_driver_.UpdatePageReadiness(readiness);
  }

  ThumbnailTabHelper* const thumbnail_tab_helper_;

  ThumbnailCaptureDriver capture_driver_{
      this, &thumbnail_tab_helper_->GetScheduler()};
  ThumbnailReadinessTracker readiness_tracker_;

  // The last known visibility WebContents visibility.
  bool visible_ = false;

  // Where we are in the page lifecycle.
  PageReadiness page_readiness_ = PageReadiness::kNotReady;

  // Scoped request for video capture. Ensures we always decrement the counter
  // once per increment.
  std::unique_ptr<ScopedThumbnailCapture> scoped_capture_;
};

// ThumbnailTabHelper ----------------------------------------------------

ThumbnailTabHelper::ThumbnailTabHelper(content::WebContents* contents)
    : state_(std::make_unique<TabStateTracker>(this, contents)),
      background_capturer_(std::make_unique<BackgroundThumbnailVideoCapturer>(
          contents,
          base::BindRepeating(
              &ThumbnailTabHelper::StoreThumbnailForBackgroundCapture,
              base::Unretained(this)))),
      thumbnail_(base::MakeRefCounted<ThumbnailImage>(state_.get())) {}

ThumbnailTabHelper::~ThumbnailTabHelper() {
  StopVideoCapture();
}

// Called when a thumbnail is published to observers. Records what
// method was used to capture the thumbnail.
//
// static
void ThumbnailTabHelper::RecordCaptureType(CaptureType type) {
  UMA_HISTOGRAM_ENUMERATION("Tab.Preview.CaptureType", type);
}

// static
ThumbnailScheduler& ThumbnailTabHelper::GetScheduler() {
  static base::NoDestructor<ThumbnailSchedulerImpl> instance;
  return *instance.get();
}

void ThumbnailTabHelper::CaptureThumbnailOnTabHidden() {
  const base::TimeTicks time_of_call = base::TimeTicks::Now();

  // Ignore previous requests to capture a thumbnail on tab switch.
  weak_factory_for_thumbnail_on_tab_hidden_.InvalidateWeakPtrs();

  // Get the WebContents' main view. Note that during shutdown there may not be
  // a view to capture.
  content::RenderWidgetHostView* const source_view = state_->GetView();
  if (!source_view)
    return;

  // Note: this is the size in pixels on-screen, not the size in DIPs.
  gfx::Size source_size = source_view->GetViewBounds().size();
  if (source_size.IsEmpty())
    return;

  const float scale_factor = source_view->GetDeviceScaleFactor();
  ThumbnailCaptureInfo copy_info = GetInitialCaptureInfo(
      source_size, scale_factor, /* include_scrollbars_in_capture */ false);

  source_view->CopyFromSurface(
      copy_info.copy_rect, copy_info.target_size,
      base::BindOnce(&ThumbnailTabHelper::StoreThumbnailForTabSwitch,
                     weak_factory_for_thumbnail_on_tab_hidden_.GetWeakPtr(),
                     time_of_call));
}

void ThumbnailTabHelper::StoreThumbnailForTabSwitch(base::TimeTicks start_time,
                                                    const SkBitmap& bitmap) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Tab.Preview.TimeToStoreAfterTabSwitch",
                             base::TimeTicks::Now() - start_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromSeconds(1), 50);
  StoreThumbnail(CaptureType::kCopyFromView, bitmap, base::nullopt);
}

void ThumbnailTabHelper::StoreThumbnailForBackgroundCapture(
    const SkBitmap& bitmap,
    uint64_t frame_id) {
  StoreThumbnail(CaptureType::kVideoFrame, bitmap, frame_id);
}

void ThumbnailTabHelper::StoreThumbnail(CaptureType type,
                                        const SkBitmap& bitmap,
                                        base::Optional<uint64_t> frame_id) {
  // Failed requests will return an empty bitmap. In tests this can be triggered
  // on threads other than the UI thread.
  if (bitmap.drawsNothing())
    return;

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  RecordCaptureType(type);
  state_->OnFrameCaptured(type);
  thumbnail_->AssignSkBitmap(bitmap, frame_id);
}

void ThumbnailTabHelper::ClearData() {
  thumbnail_->ClearData();
}

void ThumbnailTabHelper::StartVideoCapture() {
  content::RenderWidgetHostView* const source_view = state_->GetView();
  if (!source_view)
    return;

  const float scale_factor = source_view->GetDeviceScaleFactor();
  const gfx::Size source_size = source_view->GetViewBounds().size();
  if (source_size.IsEmpty())
    return;

  last_frame_capture_info_ = GetInitialCaptureInfo(
      source_size, scale_factor, /* include_scrollbars_in_capture */ true);
  background_capturer_->Start(last_frame_capture_info_);
}

void ThumbnailTabHelper::StopVideoCapture() {
  background_capturer_->Stop();
}

// static
ThumbnailCaptureInfo ThumbnailTabHelper::GetInitialCaptureInfo(
    const gfx::Size& source_size,
    float scale_factor,
    bool include_scrollbars_in_capture) {
  ThumbnailCaptureInfo capture_info;
  capture_info.source_size = source_size;

  scale_factor = std::max(scale_factor, kMinThumbnailScaleFactor);

  // Minimum thumbnail dimension (in DIP) for tablet tabstrip previews.
  const gfx::Size smallest_thumbnail = GetMinimumThumbnailSize();
  const int smallest_dimension =
      scale_factor *
      std::min(smallest_thumbnail.width(), smallest_thumbnail.height());

  // Clip the pixels that will commonly hold a scrollbar, which looks bad in
  // thumbnails - but only if that wouldn't make the thumbnail too small. We
  // can't just use gfx::scrollbar_size() because that reports default system
  // scrollbar width which is different from the width used in web rendering.
  const int scrollbar_size_dip =
      ui::NativeTheme::GetInstanceForWeb()
          ->GetPartSize(ui::NativeTheme::Part::kScrollbarVerticalTrack,
                        ui::NativeTheme::State::kNormal,
                        ui::NativeTheme::ExtraParams())
          .width();
  // Round up to make sure any scrollbar pixls are eliminated. It's better to
  // lose a single pixel of content than having a single pixel of scrollbar.
  const int scrollbar_size = std::ceil(scale_factor * scrollbar_size_dip);
  if (source_size.width() - scrollbar_size > smallest_dimension)
    capture_info.scrollbar_insets.set_right(scrollbar_size);
  if (source_size.height() - scrollbar_size > smallest_dimension)
    capture_info.scrollbar_insets.set_bottom(scrollbar_size);

  // Calculate the region to copy from.
  capture_info.copy_rect = gfx::Rect(source_size);
  if (!include_scrollbars_in_capture)
    capture_info.copy_rect.Inset(capture_info.scrollbar_insets);

  // Compute minimum sizes for multiple uses of the thumbnail - currently,
  // tablet tabstrip previews and tab hover card preview images.
  const gfx::Size min_target_size =
      gfx::ScaleToFlooredSize(smallest_thumbnail, scale_factor);

  // Calculate the target size to be the smallest size which meets the minimum
  // requirements but has the same aspect ratio as the source (with or without
  // scrollbars).
  const float width_ratio =
      float{capture_info.copy_rect.width()} / min_target_size.width();
  const float height_ratio =
      float{capture_info.copy_rect.height()} / min_target_size.height();
  const float scale_ratio = std::min(width_ratio, height_ratio);
  capture_info.target_size =
      scale_ratio <= 1.0f
          ? capture_info.copy_rect.size()
          : gfx::ScaleToCeiledSize(capture_info.copy_rect.size(),
                                   1.0f / scale_ratio);

  return capture_info;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ThumbnailTabHelper)
