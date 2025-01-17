// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cloud_services_dialog_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/media_router/cloud_services_dialog.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace media_router {

void ShowCloudServicesDialog(Browser* browser) {
  views::View* icon_view =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar()->cast_button();
  CloudServicesDialogView::ShowDialog(icon_view, browser);
}

// static
void CloudServicesDialogView::ShowDialog(views::View* anchor_view,
                                         Browser* browser) {
  if (CloudServicesDialogView::IsShowing())
    HideDialog();
  instance_ = new CloudServicesDialogView(anchor_view, browser);
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();
}

// static
void CloudServicesDialogView::HideDialog() {
  if (IsShowing())
    instance_->GetWidget()->Close();
  // We also set |instance_| to nullptr in WindowClosing() which is called
  // asynchronously, because not all paths to close the dialog go through
  // HideDialog(). We set it here because IsShowing() should be false after
  // HideDialog() is called.
  instance_ = nullptr;
}

// static
bool CloudServicesDialogView::IsShowing() {
  return instance_ != nullptr;
}

// static
CloudServicesDialogView* CloudServicesDialogView::GetDialogForTest() {
  return instance_;
}

void CloudServicesDialogView::OnDialogAccepted() {
  PrefService* pref_service = browser_->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kMediaRouterEnableCloudServices, true);
  pref_service->SetBoolean(prefs::kMediaRouterCloudServicesPrefSet, true);
}

CloudServicesDialogView::CloudServicesDialogView(views::View* anchor_view,
                                                 Browser* browser)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      browser_(browser) {
  SetShowCloseButton(true);
  SetTitle(IDS_MEDIA_ROUTER_CLOUD_SERVICES_DIALOG_TITLE);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_CLOUD_SERVICES_DIALOG_ENABLE));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_CLOUD_SERVICES_DIALOG_CANCEL));
  SetAcceptCallback(base::BindOnce(&CloudServicesDialogView::OnDialogAccepted,
                                   base::Unretained(this)));

  set_close_on_deactivate(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

CloudServicesDialogView::~CloudServicesDialogView() = default;

void CloudServicesDialogView::Init() {
  std::vector<base::string16> substrings;
  substrings.push_back(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_CLOUD_SERVICES_DIALOG_BODY));
  substrings.push_back(l10n_util::GetStringUTF16(IDS_LEARN_MORE));

  std::vector<size_t> offsets;

  base::string16 text = base::ReplaceStringPlaceholders(
      base::ASCIIToUTF16("$1 $2"), substrings, &offsets);
  gfx::Range learn_more_range(offsets[1], text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          [](Browser* browser) {
            chrome::AddSelectedTabWithURL(
                browser, GURL(chrome::kCastCloudServicesHelpURL),
                ui::PAGE_TRANSITION_LINK);
          },
          base::Unretained(browser_)));
  link_style.disable_line_wrapping = false;

  views::StyledLabel* body_text =
      AddChildView(std::make_unique<views::StyledLabel>());
  body_text->SetText(text);
  body_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body_text->AddStyleRange(learn_more_range, link_style);
}

void CloudServicesDialogView::WindowClosing() {
  if (instance_ == this)
    instance_ = nullptr;
}

// static
CloudServicesDialogView* CloudServicesDialogView::instance_ = nullptr;

BEGIN_METADATA(CloudServicesDialogView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace media_router
