// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing/sharing_app.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "components/sync_device_info/device_info.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/color_tracking_icon_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#endif

namespace {

class HeaderImageView : public NonAccessibleImageView {
 public:
  explicit HeaderImageView(const views::BubbleFrameView* frame_view,
                           const SharingDialogData::HeaderIcons& icons)
      : frame_view_(frame_view), icons_(icons) {
    constexpr gfx::Size kHeaderImageSize(320, 100);
    SetPreferredSize(kHeaderImageSize);
    SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  }

  // NonAccessibleImageView
  void OnThemeChanged() override {
    NonAccessibleImageView::OnThemeChanged();
    const auto* icon = color_utils::IsDark(frame_view_->GetBackgroundColor())
                           ? icons_.dark
                           : icons_.light;
    SetImage(gfx::CreateVectorIcon(*icon, gfx::kPlaceholderColor));
  }

 private:
  const views::BubbleFrameView* frame_view_;
  const SharingDialogData::HeaderIcons icons_;
};

constexpr int kSharingDialogSpacing = 8;

// TODO(himanshujaju): This is almost same as self share, we could unify these
// methods once we unify our architecture and dialog views.
base::string16 GetLastUpdatedTimeInDays(base::Time last_updated_timestamp) {
  int time_in_days = (base::Time::Now() - last_updated_timestamp).InDays();
  return l10n_util::GetPluralStringFUTF16(
      IDS_BROWSER_SHARING_DIALOG_DEVICE_SUBTITLE_LAST_ACTIVE_DAYS,
      time_in_days);
}

bool ShouldShowOrigin(const SharingDialogData& data,
                      content::WebContents* web_contents) {
  return data.initiating_origin &&
         !data.initiating_origin->IsSameOriginWith(
             web_contents->GetMainFrame()->GetLastCommittedOrigin());
}

base::string16 PrepareHelpTextWithoutOrigin(const SharingDialogData& data) {
  DCHECK_NE(0, data.help_text_id);
  return l10n_util::GetStringUTF16(data.help_text_id);
}

base::string16 PrepareHelpTextWithOrigin(const SharingDialogData& data) {
  DCHECK_NE(0, data.help_text_origin_id);
  base::string16 origin = url_formatter::FormatOriginForSecurityDisplay(
      *data.initiating_origin,
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);

  return l10n_util::GetStringFUTF16(data.help_text_origin_id, origin);
}

std::unique_ptr<views::View> CreateOriginView(const SharingDialogData& data) {
  DCHECK(data.initiating_origin);
  DCHECK_NE(0, data.origin_text_id);
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          data.origin_text_id,
          url_formatter::FormatOriginForSecurityDisplay(
              *data.initiating_origin,
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)),
      ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  label->SetMultiLine(true);
  return label;
}

}  // namespace

SharingDialogView::SharingDialogView(views::View* anchor_view,
                                     content::WebContents* web_contents,
                                     SharingDialogData data)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      data_(std::move(data)) {
  SetButtons(ui::DIALOG_BUTTON_NONE);

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  if (data_.type == SharingDialogType::kDialogWithoutDevicesWithApp) {
    SetFootnoteView(CreateHelpText());
  } else if ((data_.type == SharingDialogType::kDialogWithDevicesMaybeApps) &&
             ShouldShowOrigin(data_, web_contents)) {
    SetFootnoteView(CreateOriginView(data_));
  }

  SetCloseOnMainFrameOriginNavigation(true);
}

SharingDialogView::~SharingDialogView() = default;

void SharingDialogView::Hide() {
  CloseBubble();
}

bool SharingDialogView::ShouldShowCloseButton() const {
  return true;
}

base::string16 SharingDialogView::GetWindowTitle() const {
  return data_.title;
}

void SharingDialogView::WindowClosing() {
  if (data_.close_callback)
    std::move(data_.close_callback).Run(this);
}

void SharingDialogView::WebContentsDestroyed() {
  LocationBarBubbleDelegateView::WebContentsDestroyed();
  // Call the close callback here already so we can log metrics for closed
  // dialogs before the controller is destroyed.
  WindowClosing();
}

void SharingDialogView::AddedToWidget() {
  views::BubbleFrameView* frame_view = GetBubbleFrameView();
  if (frame_view && data_.header_icons) {
    frame_view->SetHeaderView(
        std::make_unique<HeaderImageView>(frame_view, *data_.header_icons));
  }
}

SharingDialogType SharingDialogView::GetDialogType() const {
  return data_.type;
}

void SharingDialogView::DeviceButtonPressed(size_t index) {
  DCHECK_LT(index, data_.devices.size());
  LogSharingSelectedIndex(data_.prefix, kSharingUiDialog, index);
  std::move(data_.device_callback).Run(*data_.devices[index]);
  CloseBubble();
}

void SharingDialogView::AppButtonPressed(size_t index) {
  DCHECK_LT(index, data_.apps.size());
  LogSharingSelectedIndex(data_.prefix, kSharingUiDialog, index,
                          SharingIndexType::kApp);
  std::move(data_.app_callback).Run(data_.apps[index]);
  CloseBubble();
}

// static
views::BubbleDialogDelegateView* SharingDialogView::GetAsBubble(
    SharingDialog* dialog) {
  return static_cast<SharingDialogView*>(dialog);
}

// static
views::BubbleDialogDelegateView* SharingDialogView::GetAsBubbleForClickToCall(
    SharingDialog* dialog) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!dialog) {
    auto* bubble = IntentPickerBubbleView::intent_picker_bubble();
    if (bubble && bubble->icon_type() == PageActionIconType::kClickToCall)
      return bubble;
  }
#endif
  return static_cast<SharingDialogView*>(dialog);
}

void SharingDialogView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets insets =
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT);

  SharingDialogType type = GetDialogType();
  LogSharingDialogShown(data_.prefix, type);

  switch (type) {
    case SharingDialogType::kErrorDialog:
      InitErrorView();
      break;
    case SharingDialogType::kEducationalDialog:
      AddChildView(CreateHelpText());
      break;
    case SharingDialogType::kDialogWithoutDevicesWithApp:
    case SharingDialogType::kDialogWithDevicesMaybeApps:
      // Spread buttons across the whole dialog width.
      insets = gfx::Insets(kSharingDialogSpacing, 0, kSharingDialogSpacing, 0);
      InitListView();
      break;
  }

  set_margins(gfx::Insets(insets.top(), 0, insets.bottom(), 0));
  SetBorder(views::CreateEmptyBorder(0, insets.left(), 0, insets.right()));

  if (GetWidget())
    SizeToContents();
}

void SharingDialogView::InitListView() {
  constexpr int kPrimaryIconSize = 20;
  const gfx::Insets device_border =
      gfx::Insets(kSharingDialogSpacing, kSharingDialogSpacing * 2,
                  kSharingDialogSpacing, 0);
  // Apps need more padding at the top and bottom as they only have one line.
  const gfx::Insets app_border = device_border + gfx::Insets(2, 0, 2, 0);

  auto button_list = std::make_unique<views::View>();
  button_list->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Devices:
  LogSharingDevicesToShow(data_.prefix, kSharingUiDialog, data_.devices.size());
  size_t index = 0;
  for (const auto& device : data_.devices) {
    auto icon = std::make_unique<views::ColorTrackingIconView>(
        device->device_type() == sync_pb::SyncEnums::TYPE_TABLET
            ? kTabletIcon
            : kHardwareSmartphoneIcon,
        kPrimaryIconSize);

    auto* dialog_button =
        button_list->AddChildView(std::make_unique<HoverButton>(
            base::BindRepeating(&SharingDialogView::DeviceButtonPressed,
                                base::Unretained(this), index++),
            std::move(icon), base::UTF8ToUTF16(device->client_name()),
            GetLastUpdatedTimeInDays(device->last_updated_timestamp())));
    dialog_button->SetEnabled(true);
    dialog_button->SetBorder(views::CreateEmptyBorder(device_border));
  }

  // Apps:
  LogSharingAppsToShow(data_.prefix, kSharingUiDialog, data_.apps.size());
  index = 0;
  for (const auto& app : data_.apps) {
    std::unique_ptr<views::ImageView> icon;
    if (app.vector_icon) {
      icon = std::make_unique<views::ColorTrackingIconView>(*app.vector_icon,
                                                            kPrimaryIconSize);
    } else {
      icon = std::make_unique<views::ImageView>();
      icon->SetImage(app.image.AsImageSkia());
    }

    auto* dialog_button =
        button_list->AddChildView(std::make_unique<HoverButton>(
            base::BindRepeating(&SharingDialogView::AppButtonPressed,
                                base::Unretained(this), index++),
            std::move(icon), app.name,
            /* subtitle= */ base::string16()));
    dialog_button->SetEnabled(true);
    dialog_button->SetBorder(views::CreateEmptyBorder(app_border));
  }

  // Allow up to 5 buttons in the list and let the rest scroll.
  constexpr size_t kMaxDialogButtons = 5;
  if (button_list->children().size() > kMaxDialogButtons) {
    const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH);

    int max_list_height = 0;
    for (size_t i = 0; i < kMaxDialogButtons; ++i) {
      max_list_height +=
          button_list->children()[i]->GetHeightForWidth(bubble_width);
    }
    DCHECK_GT(max_list_height, 0);

    auto* scroll_view = AddChildView(std::make_unique<views::ScrollView>());
    scroll_view->ClipHeightTo(0, max_list_height);
    button_list_ = scroll_view->SetContents(std::move(button_list));
  } else {
    button_list_ = AddChildView(std::move(button_list));
  }
}

void SharingDialogView::InitErrorView() {
  auto label = std::make_unique<views::Label>(data_.error_text,
                                              views::style::CONTEXT_LABEL,
                                              views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  AddChildView(std::move(label));
}

std::unique_ptr<views::StyledLabel> SharingDialogView::CreateHelpText() {
  auto label = std::make_unique<views::StyledLabel>();

  label->SetText(ShouldShowOrigin(data_, web_contents())
                     ? PrepareHelpTextWithOrigin(data_)
                     : PrepareHelpTextWithoutOrigin(data_));

  return label;
}
