// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/quick_actions_view.h"
#include "ash/system/phonehub/task_continuation_view.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "chromeos/components/phonehub/phone_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

// Padding for tray icon (dp; the button that shows the phone_hub menu).
constexpr int kTrayIconMainAxisInset = 8;
constexpr int kTrayIconCrossAxisInset = 0;

constexpr gfx::Insets kBubblePadding(0, 0, kBubbleBottomPaddingDip, 0);

}  // namespace

PhoneHubTray::PhoneHubTray(Shelf* shelf)
    : TrayBackgroundView(shelf), ui_controller_(new PhoneHubUiController()) {
  observed_phone_hub_ui_controller_.Observe(ui_controller_.get());

  auto icon = std::make_unique<views::ImageView>();
  icon->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME));
  icon->SetImage(CreateVectorIcon(
      kPhoneHubPhoneIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));

  tray_container()->SetMargin(kTrayIconMainAxisInset, kTrayIconCrossAxisInset);
  icon_ = tray_container()->AddChildView(std::move(icon));
}

PhoneHubTray::~PhoneHubTray() {
  if (bubble_)
    bubble_->bubble_view()->ResetDelegate();
}

void PhoneHubTray::SetPhoneHubManager(
    chromeos::phonehub::PhoneHubManager* phone_hub_manager) {
  ui_controller_->SetPhoneHubManager(phone_hub_manager);
}

void PhoneHubTray::ClickedOutsideBubble() {
  CloseBubble();
}

base::string16 PhoneHubTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME);
}

void PhoneHubTray::HandleLocaleChange() {
  icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME));
}

void PhoneHubTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    CloseBubble();
}

base::string16 PhoneHubTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool PhoneHubTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void PhoneHubTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void PhoneHubTray::OnPhoneHubUiStateChanged() {
  UpdateVisibility();

  if (!bubble_)
    return;
  TrayBubbleView* bubble_view = bubble_->bubble_view();

  DCHECK(ui_controller_.get());
  std::unique_ptr<PhoneHubContentView> content_view =
      ui_controller_->CreateContentView(this);
  if (!content_view.get()) {
    CloseBubble();
    return;
  }

  if (content_view_) {
    bubble_view->RemoveChildView(content_view_);
    delete content_view_;
  }

  content_view_ = content_view.get();
  bubble_view->AddChildView(std::move(content_view));

  // Updates bubble to handle possible size change with a different child view.
  bubble_view->UpdateBubble();
}

void PhoneHubTray::AnchorUpdated() {
  if (bubble_)
    bubble_->bubble_view()->UpdateBubble();
}

void PhoneHubTray::Initialize() {
  TrayBackgroundView::Initialize();
  UpdateVisibility();
}

bool PhoneHubTray::PerformAction(const ui::Event& event) {
  if (bubble_)
    CloseBubble();
  else
    ShowBubble(event.IsMouseEvent() || event.IsGestureEvent());
  return true;
}

void PhoneHubTray::ShowBubble(bool show_by_click) {
  if (bubble_)
    return;

  ui_controller_->HandleBubbleOpened();

  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = shelf()->GetSystemTrayAnchorRect();
  init_params.insets = GetTrayBubbleInsets();
  init_params.shelf_alignment = shelf()->alignment();
  init_params.preferred_width = kTrayMenuWidth;
  init_params.close_on_deactivate = true;
  init_params.has_shadow = false;
  init_params.translucent = true;
  init_params.corner_radius = kTrayItemCornerRadius;
  init_params.show_by_click = show_by_click;

  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
  bubble_view->SetBorder(views::CreateEmptyBorder(kBubblePadding));

  // Creates header view on top for displaying phone status and settings icon.
  auto phone_status = ui_controller_->CreateStatusHeaderView(this);
  phone_status_view_ = phone_status.get();
  DCHECK(phone_status_view_);
  bubble_view->AddChildView(std::move(phone_status));

  // Other contents, i.e. the connected view and the interstitial views,
  // will be positioned underneath the phone status view and updated based
  // on the current mode.
  auto content_view = ui_controller_->CreateContentView(this);
  content_view_ = content_view.get();
  DCHECK(content_view_);
  bubble_view->AddChildView(std::move(content_view));

  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view,
                                                false /* is_persistent */);

  SetIsActive(true);

  // Only focus the widget if it's opened by the keyboard.
  if (!show_by_click) {
    views::Widget* widget = bubble_->GetBubbleWidget();
    widget->widget_delegate()->SetCanActivate(true);
    Shell::Get()->focus_cycler()->FocusWidget(widget);
    widget->Activate();
  }

  phone_hub_metrics::LogScreenOnBubbleOpen(
      content_view_->GetScreenForMetrics());
}

TrayBubbleView* PhoneHubTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

const char* PhoneHubTray::GetClassName() const {
  return "PhoneHubTray";
}

bool PhoneHubTray::CanOpenConnectedDeviceSettings() {
  return TrayPopupUtils::CanOpenWebUISettings();
}

void PhoneHubTray::OpenConnectedDevicesSettings() {
  DCHECK(content_view_);
  phone_hub_metrics::LogScreenOnSettingsButtonClicked(
      content_view_->GetScreenForMetrics());

  DCHECK(CanOpenConnectedDeviceSettings());
  Shell::Get()->system_tray_model()->client()->ShowConnectedDevicesSettings();
}

void PhoneHubTray::HideStatusHeaderView() {
  if (!phone_status_view_)
    return;

  phone_status_view_->SetVisible(false);
  bubble_->bubble_view()->UpdateBubble();
}

void PhoneHubTray::CloseBubble() {
  if (!bubble_)
    return;

  DCHECK(content_view_);
  phone_hub_metrics::LogScreenOnBubbleClose(
      content_view_->GetScreenForMetrics());

  content_view_->OnBubbleClose();
  content_view_ = nullptr;
  bubble_.reset();
  SetIsActive(false);
  shelf()->UpdateAutoHideState();
}

void PhoneHubTray::UpdateVisibility() {
  DCHECK(ui_controller_.get());
  auto ui_state = ui_controller_->ui_state();
  SetVisiblePreferred(ui_state != PhoneHubUiController::UiState::kHidden);
}

}  // namespace ash
