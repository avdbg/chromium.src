// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"
#include "ui/views/view.h"

namespace sharesheet {

SharesheetServiceDelegate::SharesheetServiceDelegate(
    gfx::NativeWindow native_window,
    SharesheetService* sharesheet_service)
    : native_window_(native_window),
      sharesheet_bubble_view_(
          std::make_unique<SharesheetBubbleView>(native_window, this)),
      sharesheet_service_(sharesheet_service) {}

SharesheetServiceDelegate::~SharesheetServiceDelegate() = default;

void SharesheetServiceDelegate::ShowBubble(
    std::vector<TargetInfo> targets,
    apps::mojom::IntentPtr intent,
    sharesheet::CloseCallback close_callback) {
  if (is_bubble_open_) {
    // TODO(melzhang@) Update by adding SharesheetAlreadyOpenCancel.
    if (close_callback) {
      std::move(close_callback).Run(sharesheet::SharesheetResult::kCancel);
    }
    return;
  }
  sharesheet_bubble_view_->ShowBubble(std::move(targets), std::move(intent),
                                      std::move(close_callback));
  is_bubble_open_ = true;
}

void SharesheetServiceDelegate::OnBubbleClosed(
    const base::string16& active_action) {
  sharesheet_bubble_view_.release();
  sharesheet_service_->OnBubbleClosed(native_window_, active_action);
  // This object is now deleted and nothing can be accessed any more.
  // Therefore there is no need to set is_bubble_open_ to false.
}

void SharesheetServiceDelegate::OnActionLaunched() {
  sharesheet_bubble_view_->ShowActionView();
}

void SharesheetServiceDelegate::OnTargetSelected(
    const base::string16& target_name,
    const TargetType type,
    apps::mojom::IntentPtr intent,
    views::View* share_action_view) {
  sharesheet_service_->OnTargetSelected(native_window_, target_name, type,
                                        std::move(intent), share_action_view);
}

gfx::NativeWindow SharesheetServiceDelegate::GetNativeWindow() {
  return native_window_;
}

Profile* SharesheetServiceDelegate::GetProfile() {
  return sharesheet_service_->GetProfile();
}

void SharesheetServiceDelegate::SetSharesheetSize(const int& width,
                                                  const int& height) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  sharesheet_bubble_view_->ResizeBubble(width, height);
}

void SharesheetServiceDelegate::CloseSharesheet() {
  sharesheet_bubble_view_->CloseBubble();
}

const gfx::VectorIcon* SharesheetServiceDelegate::GetVectorIcon(
    const base::string16& display_name) {
  return sharesheet_service_->GetVectorIcon(display_name);
}

}  // namespace sharesheet
