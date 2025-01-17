// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_navigation_button_container.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/back_forward_button.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_utils.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/window/hit_test_utils.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

constexpr int kPaddingBetweenNavigationButtons = 9;

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr int kWebAppFrameLeftMargin = 4;
#else
constexpr int kWebAppFrameLeftMargin = 9;
#endif

template <class BaseClass>
class WebAppToolbarButton : public BaseClass {
 public:
  using BaseClass::BaseClass;
  WebAppToolbarButton(const WebAppToolbarButton&) = delete;
  WebAppToolbarButton& operator=(const WebAppToolbarButton&) = delete;
  ~WebAppToolbarButton() override = default;

#if defined(OS_WIN)
  bool ShouldUseWindowsIconsForMinimalUI() const {
    return base::win::GetVersion() >= base::win::Version::WIN10;
  }
#endif

  void SetIconColor(SkColor icon_color) {
    if (icon_color_ == icon_color)
      return;

    icon_color_ = icon_color;
    UpdateIcon();
  }

  virtual const gfx::VectorIcon* GetAlternativeIcon() const { return nullptr; }

  // ToolbarButton:
  void UpdateIcon() override {
    if (const auto* icon = GetAlternativeIcon()) {
      BaseClass::UpdateIconsWithStandardColors(*icon);
      return;
    }

    BaseClass::UpdateIcon();
  }

 protected:
  // ToolbarButton:
  SkColor GetForegroundColor(views::Button::ButtonState state) const override {
    if (state == views::Button::STATE_DISABLED)
      return SkColorSetA(icon_color_, gfx::kDisabledControlAlpha);

    return icon_color_;
  }

 private:
  SkColor icon_color_ = gfx::kPlaceholderColor;
};

class WebAppToolbarBackButton : public WebAppToolbarButton<BackForwardButton> {
 public:
  METADATA_HEADER(WebAppToolbarBackButton);
  WebAppToolbarBackButton(PressedCallback callback, Browser* browser);
  WebAppToolbarBackButton(const WebAppToolbarBackButton&) = delete;
  WebAppToolbarBackButton& operator=(const WebAppToolbarBackButton&) = delete;
  ~WebAppToolbarBackButton() override = default;

  // WebAppToolbarButton:
  const gfx::VectorIcon* GetAlternativeIcon() const override;
};

WebAppToolbarBackButton::WebAppToolbarBackButton(PressedCallback callback,
                                                 Browser* browser)
    : WebAppToolbarButton<BackForwardButton>(
          BackForwardButton::Direction::kBack,
          std::move(callback),
          browser) {}

const gfx::VectorIcon* WebAppToolbarBackButton::GetAlternativeIcon() const {
#if defined(OS_WIN)
  if (ShouldUseWindowsIconsForMinimalUI()) {
    return ui::TouchUiController::Get()->touch_ui()
               ? &kBackArrowWindowsTouchIcon
               : &kBackArrowWindowsIcon;
  }
#endif
  return nullptr;
}

BEGIN_METADATA(WebAppToolbarBackButton, BackForwardButton)
END_METADATA

class WebAppToolbarReloadButton : public WebAppToolbarButton<ReloadButton> {
 public:
  METADATA_HEADER(WebAppToolbarReloadButton);
  using WebAppToolbarButton<ReloadButton>::WebAppToolbarButton;
  WebAppToolbarReloadButton(const WebAppToolbarReloadButton&) = delete;
  WebAppToolbarReloadButton& operator=(const WebAppToolbarReloadButton&) =
      delete;
  ~WebAppToolbarReloadButton() override = default;

  // WebAppToolbarButton:
  const gfx::VectorIcon* GetAlternativeIcon() const override;
};

const gfx::VectorIcon* WebAppToolbarReloadButton::GetAlternativeIcon() const {
#if defined(OS_WIN)
  if (ShouldUseWindowsIconsForMinimalUI()) {
    const bool is_reload = visible_mode() == ReloadButton::Mode::kReload;
    if (ui::TouchUiController::Get()->touch_ui()) {
      return is_reload ? &kReloadWindowsTouchIcon
                       : &kNavigateStopWindowsTouchIcon;
    }
    return is_reload ? &kReloadWindowsIcon : &kNavigateStopWindowsIcon;
  }
#endif
  return nullptr;
}

BEGIN_METADATA(WebAppToolbarReloadButton, ReloadButton)
END_METADATA

}  // namespace

WebAppNavigationButtonContainer::WebAppNavigationButtonContainer(
    BrowserView* browser_view)
    : browser_(browser_view->browser()) {
  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kWebAppFrameLeftMargin),
          kPaddingBetweenNavigationButtons));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  layout.set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  back_button_ = AddChildView(std::make_unique<WebAppToolbarBackButton>(
      base::BindRepeating(
          [](Browser* browser, const ui::Event& event) {
            chrome::ExecuteCommandWithDisposition(
                browser, IDC_BACK,
                ui::DispositionFromEventFlags(event.flags()));
          },
          browser_),
      browser_));
  back_button_->set_tag(IDC_BACK);
  reload_button_ = AddChildView(std::make_unique<WebAppToolbarReloadButton>(
      browser_->command_controller()));
  reload_button_->set_tag(IDC_RELOAD);

  const bool is_browser_focus_mode = browser_->is_focus_mode();
  SetInsetsForWebAppToolbarButton(back_button_, is_browser_focus_mode);
  SetInsetsForWebAppToolbarButton(reload_button_, is_browser_focus_mode);

  views::SetHitTestComponent(back_button_, static_cast<int>(HTCLIENT));
  views::SetHitTestComponent(reload_button_, static_cast<int>(HTCLIENT));

  chrome::AddCommandObserver(browser_, IDC_BACK, this);
  chrome::AddCommandObserver(browser_, IDC_RELOAD, this);
}

WebAppNavigationButtonContainer::~WebAppNavigationButtonContainer() {
  chrome::RemoveCommandObserver(browser_, IDC_BACK, this);
  chrome::RemoveCommandObserver(browser_, IDC_RELOAD, this);
}

BackForwardButton* WebAppNavigationButtonContainer::back_button() {
  return back_button_;
}

ReloadButton* WebAppNavigationButtonContainer::reload_button() {
  return reload_button_;
}

void WebAppNavigationButtonContainer::SetIconColor(SkColor icon_color) {
  back_button_->SetIconColor(icon_color);
  reload_button_->SetIconColor(icon_color);
}

void WebAppNavigationButtonContainer::EnabledStateChangedForCommand(
    int id,
    bool enabled) {
  switch (id) {
    case IDC_BACK:
      back_button_->SetEnabled(enabled);
      break;
    case IDC_RELOAD:
      reload_button_->SetEnabled(enabled);
      break;
    default:
      NOTREACHED();
  }
}

BEGIN_METADATA(WebAppNavigationButtonContainer, views::View)
END_METADATA
