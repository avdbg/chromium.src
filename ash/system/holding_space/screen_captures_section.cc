// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/screen_captures_section.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_item_screen_capture_view.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

// Appearance.
constexpr int kChildSpacing = 8;

ScreenCapturesSection::ScreenCapturesSection(
    HoldingSpaceItemViewDelegate* delegate)
    : HoldingSpaceItemViewsSection(delegate,
                                   /*supported_types=*/
                                   {HoldingSpaceItem::Type::kScreenshot,
                                    HoldingSpaceItem::Type::kScreenRecording},
                                   /*max_count=*/kMaxScreenCaptures) {}

ScreenCapturesSection::~ScreenCapturesSection() = default;

const char* ScreenCapturesSection::GetClassName() const {
  return "ScreenCapturesSection";
}

std::unique_ptr<views::View> ScreenCapturesSection::CreateHeader() {
  auto header = holding_space_util::CreateLabel(
      holding_space_util::LabelStyle::kHeader,
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SCREEN_CAPTURES_TITLE));
  header->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  header->SetPaintToLayer();
  header->layer()->SetFillsBoundsOpaquely(false);
  return header;
}

std::unique_ptr<views::View> ScreenCapturesSection::CreateContainer() {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets(/*top=*/0, /*left=*/0, /*bottom=*/0,
                              /*right=*/kChildSpacing));
  return container;
}

std::unique_ptr<HoldingSpaceItemView> ScreenCapturesSection::CreateView(
    const HoldingSpaceItem* item) {
  return std::make_unique<HoldingSpaceItemScreenCaptureView>(delegate(), item);
}

}  // namespace ash
