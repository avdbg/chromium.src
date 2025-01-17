// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/tether_connection_pending_view.h"

#include <algorithm>
#include <memory>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "base/strings/string16.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::Screen;

TetherConnectionPendingView::TetherConnectionPendingView() {
  SetID(PhoneHubViewID::kTetherConnectionPendingView);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  content_view_ = AddChildView(
      std::make_unique<PhoneHubInterstitialView>(/*show_progress=*/true));

  // TODO(crbug.com/1127996): Replace PNG file with vector icon.
  gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PHONE_HUB_CONNECTING_IMAGE);
  content_view_->SetImage(*image);

  content_view_->SetTitle(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_TETHER_CONNECTION_PENDING_DIALOG_TITLE));
  content_view_->SetDescription(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_PHONE_CONNECTING_DIALOG_DESCRIPTION));

  LogInterstitialScreenEvent(InterstitialScreenEvent::kShown);
}

TetherConnectionPendingView::~TetherConnectionPendingView() = default;

phone_hub_metrics::Screen TetherConnectionPendingView::GetScreenForMetrics()
    const {
  return Screen::kTetherConnectionPending;
}

BEGIN_METADATA(TetherConnectionPendingView, views::View)
END_METADATA

}  // namespace ash
