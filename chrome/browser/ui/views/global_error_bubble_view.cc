// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_error_bubble_view.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/elevation_icon_setter.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/base/buildflags.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

// GlobalErrorBubbleViewBase ---------------------------------------------------

// static
GlobalErrorBubbleViewBase* GlobalErrorBubbleViewBase::ShowStandardBubbleView(
    Browser* browser,
    const base::WeakPtr<GlobalErrorWithStandardBubble>& error) {
  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAppMenuButton();
  GlobalErrorBubbleView* bubble_view = new GlobalErrorBubbleView(
      anchor_view, views::BubbleBorder::TOP_RIGHT, browser, error);
  views::BubbleDialogDelegateView::CreateBubble(bubble_view);
  bubble_view->GetWidget()->Show();
  return bubble_view;
}

// GlobalErrorBubbleView -------------------------------------------------------

GlobalErrorBubbleView::GlobalErrorBubbleView(
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    Browser* browser,
    const base::WeakPtr<GlobalErrorWithStandardBubble>& error)
    : BubbleDialogDelegateView(anchor_view, arrow),
      error_(error) {
  // error_ is a WeakPtr, but it's always non-null during construction.
  DCHECK(error_);

  WidgetDelegate::SetTitle(error_->GetBubbleViewTitle());
  WidgetDelegate::SetShowCloseButton(error_->ShouldShowCloseButton());
  WidgetDelegate::RegisterWindowClosingCallback(base::BindOnce(
      &GlobalErrorWithStandardBubble::BubbleViewDidClose, error_, browser));

  SetDefaultButton(error_->GetDefaultDialogButton());
  SetButtons(!error_->GetBubbleViewCancelButtonLabel().empty()
                 ? (ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL)
                 : ui::DIALOG_BUTTON_OK);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 error_->GetBubbleViewAcceptButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 error_->GetBubbleViewCancelButtonLabel());

  // Note that error is already a WeakPtr, so these callbacks will simply do
  // nothing if they are invoked after its destruction.
  SetAcceptCallback(base::BindOnce(
      &GlobalErrorWithStandardBubble::BubbleViewAcceptButtonPressed, error,
      base::Unretained(browser)));
  SetCancelCallback(base::BindOnce(
      &GlobalErrorWithStandardBubble::BubbleViewCancelButtonPressed, error,
      base::Unretained(browser)));

  if (!error_->GetBubbleViewDetailsButtonLabel().empty()) {
    SetExtraView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(
            &GlobalErrorWithStandardBubble::BubbleViewDetailsButtonPressed,
            error_, browser),
        error_->GetBubbleViewDetailsButtonLabel()));
  }

  chrome::RecordDialogCreation(chrome::DialogIdentifier::GLOBAL_ERROR);
}

GlobalErrorBubbleView::~GlobalErrorBubbleView() = default;

void GlobalErrorBubbleView::Init() {
  const int kMaxBubbleViewWidth = 362;
  // |error_| is assumed to be valid, and stay valid, at least until Init()
  // returns.

  std::vector<base::string16> message_strings(error_->GetBubbleViewMessages());
  std::vector<std::unique_ptr<views::Label>> message_labels;
  for (const auto& message_string : message_strings) {
    auto message_label = std::make_unique<views::Label>(message_string);
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message_labels.push_back(std::move(message_label));
  }

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // First row, message labels.
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                views::GridLayout::ColumnSize::kFixed, kMaxBubbleViewWidth, 0);

  for (size_t i = 0; i < message_labels.size(); ++i) {
    layout->StartRow(1.0, 0);
    layout->AddView(std::move(message_labels[i]));
    if (i < message_labels.size() - 1)
      layout->AddPaddingRow(views::GridLayout::kFixedSize,
                            ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_RELATED_CONTROL_VERTICAL));
  }

  // These bubbles show at times where activation is sporadic (like at startup,
  // or a new window opening). Make sure the bubble doesn't disappear before the
  // user sees it, if the bubble needs to be acknowledged.
  set_close_on_deactivate(error_->ShouldCloseOnDeactivate());
}

void GlobalErrorBubbleView::OnDialogInitialized() {
  views::LabelButton* ok_button = GetOkButton();
  if (ok_button && error_ && error_->ShouldAddElevationIconToAcceptButton()) {
    elevation_icon_setter_ = std::make_unique<ElevationIconSetter>(
        ok_button, base::BindOnce(&GlobalErrorBubbleView::SizeToContents,
                                  base::Unretained(this)));
  }
}

void GlobalErrorBubbleView::CloseBubbleView() {
  GetWidget()->Close();
}

BEGIN_METADATA(GlobalErrorBubbleView, views::BubbleDialogDelegateView)
END_METADATA
