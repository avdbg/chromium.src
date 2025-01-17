// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/idle_action_warning_dialog_view.h"

#include <algorithm>

#include "base/location.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

const int kCountdownUpdateIntervalMs = 1000;  // 1 second.

}  // namespace

IdleActionWarningDialogView::IdleActionWarningDialogView(
    base::TimeTicks idle_action_time)
    : idle_action_time_(idle_action_time) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);

  SetModalType(ui::MODAL_TYPE_SYSTEM);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetBorder(views::CreateEmptyBorder(
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(views::TEXT,
                                                                 views::TEXT)));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_IDLE_WARNING_LOGOUT_WARNING));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label);

  // Shown on the root window for new windows.
  views::DialogDelegate::CreateDialogWidget(this, nullptr /* context */,
                                            nullptr /* parent */)
      ->Show();

  update_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kCountdownUpdateIntervalMs),
      this, &IdleActionWarningDialogView::UpdateTitle);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::IDLE_ACTION_WARNING);
}

void IdleActionWarningDialogView::CloseDialog() {
  GetWidget()->Close();
}

void IdleActionWarningDialogView::Update(base::TimeTicks idle_action_time) {
  idle_action_time_ = idle_action_time;
  UpdateTitle();
}

base::string16 IdleActionWarningDialogView::GetWindowTitle() const {
  const base::TimeDelta time_until_idle_action =
      std::max(idle_action_time_ - base::TimeTicks::Now(), base::TimeDelta());
  return l10n_util::GetStringFUTF16(
      IDS_IDLE_WARNING_TITLE,
      ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_LONG, 10,
                               time_until_idle_action));
}

IdleActionWarningDialogView::~IdleActionWarningDialogView() = default;

void IdleActionWarningDialogView::UpdateTitle() {
  GetWidget()->UpdateWindowTitle();
}

BEGIN_METADATA(IdleActionWarningDialogView, views::DialogDelegateView)
END_METADATA

}  // namespace chromeos
