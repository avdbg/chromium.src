// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_view_views.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/view_type.h"
#include "ui/events/event.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/base/cursor/cursor.h"
#endif

ExtensionViewViews::ExtensionViewViews(extensions::ExtensionViewHost* host)
    : views::WebView(host->browser() ? host->browser()->profile() : nullptr),
      host_(host) {
  host_->set_view(this);
  SetWebContents(host_->web_contents());
}

ExtensionViewViews::~ExtensionViewViews() {
  if (parent())
    parent()->RemoveChildView(this);
}

void ExtensionViewViews::Init() {
  if (host_->extension_host_type() == extensions::VIEW_TYPE_EXTENSION_POPUP) {
    DCHECK(container_);

    // This will set the max popup bounds for the duration of the popup's
    // lifetime; they won't be readjusted if the window moves. This is usually
    // okay, since moving the window typically (but not always) results in
    // the popup closing.
    EnableSizingFromWebContents(container_->GetMinBounds(),
                                container_->GetMaxBounds());
  }
}

void ExtensionViewViews::VisibilityChanged(View* starting_from,
                                           bool is_visible) {
  views::WebView::VisibilityChanged(starting_from, is_visible);

  if (starting_from == this) {
    // Also tell RenderWidgetHostView the new visibility. Despite its name, it
    // is not part of the View hierarchy and does not know about the change
    // unless we tell it.
    content::RenderWidgetHostView* host_view =
        host_->main_frame_host()->GetView();
    if (host_view) {
      if (is_visible)
        host_view->Show();
      else
        host_view->Hide();
    }
  }
}

gfx::Size ExtensionViewViews::GetMinimumSize() const {
  return minimum_size_.value_or(GetPreferredSize());
}

void ExtensionViewViews::SetMinimumSize(const gfx::Size& minimum_size) {
  if (minimum_size_ && minimum_size_.value() == minimum_size)
    return;
  minimum_size_ = minimum_size;
  OnPropertyChanged(&minimum_size_,
                    views::kPropertyEffectsPreferredSizeChanged);
}

void ExtensionViewViews::SetContainer(
    ExtensionViewViews::Container* container) {
  container_ = container;
  OnPropertyChanged(&container_, views::kPropertyEffectsPreferredSizeChanged);
}

ExtensionViewViews::Container* ExtensionViewViews::GetContainer() const {
  return container_;
}

gfx::NativeView ExtensionViewViews::GetNativeView() {
  return holder()->native_view();
}

void ExtensionViewViews::ResizeDueToAutoResize(
    content::WebContents* web_contents,
    const gfx::Size& new_size) {
  // Don't actually do anything with this information until we have been shown.
  // Size changes will not be honored by lower layers while we are hidden.
  if (!GetVisible()) {
    pending_preferred_size_ = new_size;
    return;
  }

  WebView::ResizeDueToAutoResize(web_contents, new_size);
}

void ExtensionViewViews::RenderFrameCreated(
    content::RenderFrameHost* frame_host) {
  WebView::RenderFrameCreated(frame_host);
}

bool ExtensionViewViews::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

void ExtensionViewViews::OnLoaded() {
  DCHECK(host_->has_loaded_once());

  // ExtensionPopup delegates showing the view to OnLoaded(). ExtensionDialog
  // handles visibility directly.
  if (GetVisible())
    return;

  SetVisible(true);
  ResizeDueToAutoResize(web_contents(), pending_preferred_size_);
}

gfx::NativeCursor ExtensionViewViews::GetCursor(const ui::MouseEvent& event) {
  return gfx::kNullCursor;
}

void ExtensionViewViews::PreferredSizeChanged() {
  View::PreferredSizeChanged();
  if (container_)
    container_->OnExtensionSizeChanged(this);
}

void ExtensionViewViews::OnWebContentsAttached() {
  host_->CreateRendererSoon();
  SetVisible(false);
}

BEGIN_METADATA(ExtensionViewViews, views::WebView)
ADD_PROPERTY_METADATA(gfx::Size, MinimumSize)
ADD_PROPERTY_METADATA(ExtensionViewViews::Container*, Container)
END_METADATA
