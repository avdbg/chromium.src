// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/webui_login_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_screen.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/login/ui/login_display_webui.h"
#include "chrome/browser/chromeos/login/ui/web_contents_forced_title.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/view_type_utils.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

using chromeos::AutoEnrollmentController;
using content::NativeWebKeyboardEvent;
using content::RenderViewHost;
using content::WebContents;
using web_modal::WebContentsModalDialogManager;

namespace {

// A class to change arrow key traversal behavior when it's alive.
class ScopedArrowKeyTraversal {
 public:
  explicit ScopedArrowKeyTraversal(bool new_arrow_key_tranversal_enabled)
      : previous_arrow_key_traversal_enabled_(
            views::FocusManager::arrow_key_traversal_enabled()) {
    views::FocusManager::set_arrow_key_traversal_enabled(
        new_arrow_key_tranversal_enabled);
  }
  ~ScopedArrowKeyTraversal() {
    views::FocusManager::set_arrow_key_traversal_enabled(
        previous_arrow_key_traversal_enabled_);
  }

 private:
  const bool previous_arrow_key_traversal_enabled_;
  DISALLOW_COPY_AND_ASSIGN(ScopedArrowKeyTraversal);
};

}  // namespace

namespace chromeos {

// WebUILoginView public: ------------------------------------------------------

WebUILoginView::WebUILoginView(const WebViewSettings& settings,
                               base::WeakPtr<LoginDisplayHostWebUI> controller)
    : settings_(settings), controller_(controller) {
  ChromeKeyboardControllerClient::Get()->AddObserver(this);

  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  for (size_t i = 0; i < ash::kLoginAcceleratorDataLength; ++i) {
    ui::Accelerator accelerator(ash::kLoginAcceleratorData[i].keycode,
                                ash::kLoginAcceleratorData[i].modifiers);
    // Show reset conflicts with rotate screen when --ash-dev-shortcuts is
    // passed. Favor --ash-dev-shortcuts since that is explicitly added.
    if (ash::kLoginAcceleratorData[i].action ==
            ash::LoginAcceleratorAction::kEnableConsumerKiosk &&
        !KioskAppManager::IsConsumerKioskEnabled()) {
      continue;
    }

    accel_map_[accelerator] = ash::kLoginAcceleratorData[i].action;
  }

  for (AccelMap::iterator i(accel_map_.begin()); i != accel_map_.end(); ++i) {
    AddAccelerator(i->first);
  }

  if (LoginScreenClient::HasInstance()) {
    LoginScreenClient::Get()->AddSystemTrayFocusObserver(this);
    observing_system_tray_focus_ = true;
  }
}

WebUILoginView::~WebUILoginView() {
  for (auto& observer : observer_list_)
    observer.OnHostDestroying();

  if (observing_system_tray_focus_)
    LoginScreenClient::Get()->RemoveSystemTrayFocusObserver(this);
  ChromeKeyboardControllerClient::Get()->RemoveObserver(this);

  // Clear any delegates we have set on the WebView.
  WebContents* web_contents = web_view_->GetWebContents();
  WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(nullptr);
  web_contents->SetDelegate(nullptr);
}

// static
void WebUILoginView::InitializeWebView(views::WebView* web_view,
                                       const base::string16& title) {
  WebContents* web_contents = web_view->GetWebContents();

  if (!title.empty())
    WebContentsForcedTitle::CreateForWebContentsWithTitle(web_contents, title);

  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_contents, SK_ColorTRANSPARENT);

  // Ensure that the login UI has a tab ID, which will allow the GAIA auth
  // extension's background script to tell it apart from a captive portal window
  // that may be opened on top of this UI.
  CreateSessionServiceTabHelper(web_contents);

  // Create the password manager that is needed for the proxy.
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents));

  // LoginHandlerViews uses a constrained window for the password manager view.
  WebContentsModalDialogManager::CreateForWebContents(web_contents);

  extensions::SetViewType(web_contents, extensions::VIEW_TYPE_COMPONENT);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
  blink::RendererPreferences* prefs = web_contents->GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(
      prefs, ProfileHelper::GetSigninProfile());
}

void WebUILoginView::Init() {
  // Init() should only be called once.
  DCHECK(!web_view_);
  auto web_view =
      std::make_unique<views::WebView>(ProfileHelper::GetSigninProfile());
  WebContents* web_contents = web_view->GetWebContents();

  InitializeWebView(web_view.get(), settings_.web_view_title);
  web_view->set_allow_accelerators(true);

  web_view_ = AddChildView(std::move(web_view));

  WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(this);
  web_contents->SetDelegate(this);
}

void WebUILoginView::RequestFocus() {
  web_view_->RequestFocus();
}

web_modal::WebContentsModalDialogHost*
WebUILoginView::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView WebUILoginView::GetHostView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point WebUILoginView::GetDialogPosition(const gfx::Size& size) {
  // Center the widget.
  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  return gfx::Point(widget_size.width() / 2 - size.width() / 2,
                    widget_size.height() / 2 - size.height() / 2);
}

gfx::Size WebUILoginView::GetMaximumDialogSize() {
  return GetWidget()->GetWindowBoundsInScreen().size();
}

void WebUILoginView::AddObserver(web_modal::ModalDialogHostObserver* observer) {
  if (observer && !observer_list_.HasObserver(observer))
    observer_list_.AddObserver(observer);
}

void WebUILoginView::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool WebUILoginView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  AccelMap::const_iterator entry = accel_map_.find(accelerator);
  if (entry == accel_map_.end())
    return false;
  if (controller_)
    return controller_->HandleAccelerator(entry->second);
  return false;
}

gfx::NativeWindow WebUILoginView::GetNativeWindow() const {
  return GetWidget()->GetNativeWindow();
}

void WebUILoginView::LoadURL(const GURL& url) {
  web_view_->LoadInitialURL(url);
  web_view_->RequestFocus();
}

content::WebUI* WebUILoginView::GetWebUI() {
  return web_view_->web_contents()->GetWebUI();
}

content::WebContents* WebUILoginView::GetWebContents() {
  return web_view_->web_contents();
}

OobeUI* WebUILoginView::GetOobeUI() {
  if (!GetWebUI())
    return nullptr;

  return static_cast<OobeUI*>(GetWebUI()->GetController());
}

void WebUILoginView::OnPostponedShow() {
  set_is_hidden(false);
  OnLoginPromptVisible();
}

void WebUILoginView::SetStatusAreaVisible(bool visible) {
  SystemTrayClient::Get()->SetPrimaryTrayVisible(visible);
}

void WebUILoginView::SetUIEnabled(bool enabled) {
  forward_keyboard_event_ = enabled;

  SystemTrayClient::Get()->SetPrimaryTrayEnabled(enabled);
}

// WebUILoginView protected: ---------------------------------------------------

void WebUILoginView::Layout() {
  DCHECK(web_view_);
  web_view_->SetBoundsRect(bounds());

  for (auto& observer : observer_list_)
    observer.OnPositionRequiresUpdate();
}

void WebUILoginView::ChildPreferredSizeChanged(View* child) {
  Layout();
  SchedulePaint();
}

void WebUILoginView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  // Return the focus to the web contents.
  web_view_->web_contents()->FocusThroughTabTraversal(reverse);
  GetWidget()->Activate();
  web_view_->web_contents()->Focus();

  if (!GetOobeUI())
    return;
  CoreOobeView* view = GetOobeUI()->GetCoreOobeView();
  if (view)
    view->FocusReturned(reverse);
}

void WebUILoginView::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE:
    case chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN: {
      OnLoginPromptVisible();
      registrar_.Remove(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                        content::NotificationService::AllSources());
      registrar_.Remove(this, chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
                        content::NotificationService::AllSources());
      break;
    }
    case chrome::NOTIFICATION_APP_TERMINATING: {
      // In some tests, WebUILoginView remains after LoginScreenClient gets
      // deleted on shutdown. It should unregister itself before the deletion
      // happens.
      if (observing_system_tray_focus_) {
        LoginScreenClient::Get()->RemoveSystemTrayFocusObserver(this);
        observing_system_tray_focus_ = false;
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ChromeKeyboardControllerClient::Observer

void WebUILoginView::OnKeyboardVisibilityChanged(bool visible) {
  if (!GetOobeUI())
    return;
  CoreOobeView* view = GetOobeUI()->GetCoreOobeView();
  if (view)
    view->SetVirtualKeyboardShown(visible);
}

// WebUILoginView private: -----------------------------------------------------

bool WebUILoginView::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
#ifndef NDEBUG
  // Do not show the context menu.
  return false;
#else
  return true;
#endif
}

bool WebUILoginView::HandleKeyboardEvent(content::WebContents* source,
                                         const NativeWebKeyboardEvent& event) {
  bool handled = false;
  if (forward_keyboard_event_) {
    // Disable arrow key traversal because arrow keys are handled via
    // accelerator when this view has focus.
    ScopedArrowKeyTraversal arrow_key_traversal(false);

    handled = unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, GetFocusManager());
  }

  // Make sure error bubble is cleared on keyboard event. This is needed
  // when the focus is inside an iframe. Only clear on KeyDown to prevent hiding
  // an immediate authentication error (See crbug.com/103643).
  if (GetOobeUI() && event.GetType() == blink::WebInputEvent::Type::kKeyDown) {
    CoreOobeView* view = GetOobeUI()->GetCoreOobeView();
    if (view)
      view->ClearErrors();
  }
  return handled;
}

bool WebUILoginView::TakeFocus(content::WebContents* source, bool reverse) {
  // In case of blocked UI (ex.: sign in is in progress)
  // we should not process focus change events.
  if (!forward_keyboard_event_)
    return false;

  // FocusLoginShelf focuses either system tray or login shelf buttons.
  // Only do this if the login shelf is enabled.
  if (shelf_enabled_)
    ash::LoginScreen::Get()->FocusLoginShelf(reverse);
  return shelf_enabled_;
}

void WebUILoginView::RequestMediaAccessPermission(
    WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // Note: This is needed for taking photos when selecting new user images
  // and SAML logins. Must work for all user types (including supervised).
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr /* extension */);
}

bool WebUILoginView::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
}

bool WebUILoginView::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

void WebUILoginView::OnFocusLeavingSystemTray(bool reverse) {
  AboutToRequestFocusFromTabTraversal(reverse);
}

void WebUILoginView::OnLoginPromptVisible() {
  if (!observing_system_tray_focus_ && LoginScreenClient::HasInstance()) {
    LoginScreenClient::Get()->AddSystemTrayFocusObserver(this);
    observing_system_tray_focus_ = true;
  }
  // If we're hidden than will generate this signal once we're shown.
  if (is_hidden_ || webui_visible_) {
    VLOG(1) << "Login WebUI >> not emitting signal, hidden: " << is_hidden_;
    return;
  }
  TRACE_EVENT0("chromeos", "WebUILoginView::OnLoginPromptVisible");
  if (should_emit_login_prompt_visible_) {
    VLOG(1) << "Login WebUI >> login-prompt-visible";
    SessionManagerClient::Get()->EmitLoginPromptVisible();
  }

  webui_visible_ = true;
}

BEGIN_METADATA(WebUILoginView, views::View)
END_METADATA

}  // namespace chromeos
