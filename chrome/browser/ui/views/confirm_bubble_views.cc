// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_bubble_views.h"

#include <utility>

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

ConfirmBubbleViews::ConfirmBubbleViews(
    std::unique_ptr<ConfirmBubbleModel> model)
    : model_(std::move(model)) {
  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 model_->GetButtonLabel(ui::DIALOG_BUTTON_OK));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 model_->GetButtonLabel(ui::DIALOG_BUTTON_CANCEL));
  SetAcceptCallback(base::BindOnce(&ConfirmBubbleModel::Accept,
                                   base::Unretained(model_.get())));
  SetCancelCallback(base::BindOnce(&ConfirmBubbleModel::Cancel,
                                   base::Unretained(model_.get())));
  views::ImageButton* help_button =
      SetExtraView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(
              [](ConfirmBubbleViews* bubble) {
                bubble->model_->OpenHelpPage();
                bubble->GetWidget()->Close();
              },
              base::Unretained(this)),
          vector_icons::kHelpOutlineIcon));
  help_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Use a fixed maximum message width, so longer messages will wrap.
  const int kMaxMessageWidth = 400;
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kFixed, kMaxMessageWidth, false);

  // Add the message label.
  auto label = std::make_unique<views::Label>(
      model_->GetMessageText(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  DCHECK(!label->GetText().empty());
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  label->SizeToFit(kMaxMessageWidth);
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  label_ = layout->AddView(std::move(label));

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CONFIRM_BUBBLE);
}

ConfirmBubbleViews::~ConfirmBubbleViews() {
}

base::string16 ConfirmBubbleViews::GetWindowTitle() const {
  return model_->GetTitle();
}

bool ConfirmBubbleViews::ShouldShowCloseButton() const {
  return false;
}

void ConfirmBubbleViews::OnDialogInitialized() {
  GetWidget()->GetRootView()->GetViewAccessibility().OverrideDescribedBy(
      label_);
}

BEGIN_METADATA(ConfirmBubbleViews, views::DialogDelegateView)
END_METADATA

namespace chrome {

void ShowConfirmBubble(gfx::NativeWindow window,
                       gfx::NativeView anchor_view,
                       const gfx::Point& origin,
                       std::unique_ptr<ConfirmBubbleModel> model) {
  constrained_window::CreateBrowserModalDialogViews(
      new ConfirmBubbleViews(std::move(model)), window)
      ->Show();
}

}  // namespace chrome
