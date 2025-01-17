// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/display_media_access_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif

// Holds pending request information so that we display one picker UI at a time
// for each content::WebContents.
struct DisplayMediaAccessHandler::PendingAccessRequest {
  PendingAccessRequest(std::unique_ptr<DesktopMediaPicker> picker,
                       const content::MediaStreamRequest& request,
                       content::MediaResponseCallback callback)
      : picker(std::move(picker)),
        request(request),
        callback(std::move(callback)) {}
  ~PendingAccessRequest() = default;

  std::unique_ptr<DesktopMediaPicker> picker;
  content::MediaStreamRequest request;
  content::MediaResponseCallback callback;
};

DisplayMediaAccessHandler::DisplayMediaAccessHandler()
    : picker_factory_(new DesktopMediaPickerFactoryImpl()),
      web_contents_collection_(this) {}

DisplayMediaAccessHandler::DisplayMediaAccessHandler(
    std::unique_ptr<DesktopMediaPickerFactory> picker_factory,
    bool display_notification)
    : display_notification_(display_notification),
      picker_factory_(std::move(picker_factory)),
      web_contents_collection_(this) {}

DisplayMediaAccessHandler::~DisplayMediaAccessHandler() = default;

bool DisplayMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType stream_type,
    const extensions::Extension* extension) {
  return stream_type == blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE ||
         stream_type ==
             blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB;
  // This class handles MEDIA_DISPLAY_AUDIO_CAPTURE as well, but only if it is
  // accompanied by MEDIA_DISPLAY_VIDEO_CAPTURE request as per spec.
  // https://w3c.github.io/mediacapture-screen-share/#mediadevices-additions
  // 5.1 MediaDevices Additions
  // "The user agent MUST reject audio-only requests."
}

bool DisplayMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return false;
}

void DisplayMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile->GetPrefs()->GetBoolean(prefs::kScreenCaptureAllowed)) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, nullptr);
    return;
  }

#if 0
  // SafeBrowsing Delayed Warnings experiment can delay some SafeBrowsing
  // warnings until user interaction. If the current page has a delayed warning,
  // it'll have a user interaction observer attached. Show the warning
  // immediately in that case.
  safe_browsing::SafeBrowsingUserInteractionObserver* observer =
      safe_browsing::SafeBrowsingUserInteractionObserver::FromWebContents(
          web_contents);
  if (observer) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, nullptr);
    observer->OnDesktopCaptureRequest();
    return;
  }
#endif

#if defined(OS_MAC)
  // Do not allow picker UI to be shown on a page that isn't in the foreground
  // in Mac, because the UI implementation in Mac pops a window over any content
  // which might be confusing for the users. See https://crbug.com/1407733 for
  // details.
  // TODO(emircan): Remove this once Mac UI doesn't use a window.
  if (web_contents->GetVisibility() != content::Visibility::VISIBLE) {
    LOG(ERROR) << "Do not allow getDisplayMedia() on a backgrounded page.";
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
    return;
  }
#endif  // defined(OS_MAC)

  if (request.video_type ==
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB) {
    // Repeat the permission test from the render process.
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
        request.render_process_id, request.render_frame_id);
    if (!rfh) {
      std::move(callback).Run(
          blink::MediaStreamDevices(),
          blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
      return;
    }
    if (!rfh->IsFeatureEnabled(
            blink::mojom::FeaturePolicyFeature::kDisplayCapture)) {
      bad_message::ReceivedBadMessage(
          rfh->GetProcess(), bad_message::BadMessageReason::
                                 RFH_DISPLAY_CAPTURE_PERMISSION_MISSING);
      std::move(callback).Run(
          blink::MediaStreamDevices(),
          blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, nullptr);
      return;
    }
  }

  std::unique_ptr<DesktopMediaPicker> picker =
      picker_factory_->CreatePicker(&request);
  if (!picker) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
    return;
  }

  // Ensure we are observing the deletion of |web_contents|.
  web_contents_collection_.StartObserving(web_contents);

  RequestsQueue& queue = pending_requests_[web_contents];

  queue.push_back(std::make_unique<PendingAccessRequest>(
      std::move(picker), request, std::move(callback)));
  // If this is the only request then pop picker UI.
  if (queue.size() == 1)
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DisplayMediaAccessHandler::UpdateMediaRequestState(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state != content::MEDIA_REQUEST_STATE_DONE &&
      state != content::MEDIA_REQUEST_STATE_CLOSING) {
    return;
  }

  if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
    DeletePendingAccessRequest(render_process_id, render_frame_id,
                               page_request_id);
  }
  CaptureAccessHandlerBase::UpdateMediaRequestState(
      render_process_id, render_frame_id, page_request_id, stream_type, state);

  // This method only gets called with the above checked states when all
  // requests are to be canceled. Therefore, we don't need to process the
  // next queued request.
}

void DisplayMediaAccessHandler::ProcessQueuedAccessRequest(
    const RequestsQueue& queue,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const PendingAccessRequest& pending_request = *queue.front();
  UpdateTrusted(pending_request.request, false /* is_trusted */);

  std::vector<DesktopMediaList::Type> media_types;
  if (pending_request.request.video_type ==
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB) {
    media_types = {DesktopMediaList::Type::kCurrentTab,
                   DesktopMediaList::Type::kWebContents,
                   DesktopMediaList::Type::kWindow,
                   DesktopMediaList::Type::kScreen};
  } else {
    media_types = {DesktopMediaList::Type::kScreen,
                   DesktopMediaList::Type::kWindow,
                   DesktopMediaList::Type::kWebContents};
  }

  // Avoid offering window-capture as a separate source, since PipeWire's
  // content-picker will offer both screen and window sources.
  // See crbug.com/1157006.
  if (content::desktop_capture::CanUsePipeWire()) {
    base::Erase(media_types, DesktopMediaList::Type::kWindow);
  }

  auto source_lists =
      picker_factory_->CreateMediaList(media_types, web_contents);

  DesktopMediaPicker::DoneCallback done_callback =
      base::BindOnce(&DisplayMediaAccessHandler::OnPickerDialogResults,
                     base::Unretained(this), web_contents);
  DesktopMediaPicker::Params picker_params;
  picker_params.web_contents = web_contents;
  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();
  picker_params.context = parent_window;
  picker_params.parent = parent_window;
  picker_params.app_name = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(web_contents->GetLastCommittedURL()),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  picker_params.target_name = picker_params.app_name;
  picker_params.request_audio =
      pending_request.request.audio_type ==
      blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;
  // getDisplayMedia's checkbox state defaults to unchecked, but for
  // getCurrentBrowsingContextMedia, we default to checked.
  picker_params.approve_audio_by_default =
      (picker_params.request_audio &&
       pending_request.request.video_type ==
           blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB);
  pending_request.picker->Show(picker_params, std::move(source_lists),
                               std::move(done_callback));
}

void DisplayMediaAccessHandler::OnPickerDialogResults(
    content::WebContents* web_contents,
    content::DesktopMediaID media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  auto it = pending_requests_.find(web_contents);
  if (it == pending_requests_.end())
    return;
  RequestsQueue& queue = it->second;
  if (queue.empty()) {
    // UpdateMediaRequestState() called with MEDIA_REQUEST_STATE_CLOSING. Don't
    // need to do anything.
    return;
  }

  PendingAccessRequest& pending_request = *queue.front();
  blink::MediaStreamDevices devices;
  blink::mojom::MediaStreamRequestResult request_result =
      blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
  std::unique_ptr<content::MediaStreamUI> ui;
  if (media_id.is_null()) {
    request_result = blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
  } else {
    request_result = blink::mojom::MediaStreamRequestResult::OK;
#if defined(OS_MAC)
    // Check screen capture permissions on Mac if necessary.
    if ((media_id.type == content::DesktopMediaID::TYPE_SCREEN ||
         media_id.type == content::DesktopMediaID::TYPE_WINDOW) &&
        system_media_permissions::CheckSystemScreenCapturePermission() !=
            system_media_permissions::SystemPermission::kAllowed) {
      request_result =
          blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED;
    }
#endif
    if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS &&
        !content::WebContents::FromRenderFrameHost(
            content::RenderFrameHost::FromID(
                media_id.web_contents_id.render_process_id,
                media_id.web_contents_id.main_render_frame_id))) {
      request_result =
          blink::mojom::MediaStreamRequestResult::TAB_CAPTURE_FAILURE;
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (request_result == blink::mojom::MediaStreamRequestResult::OK) {
      if (policy::DlpContentManager::Get()->IsScreenCaptureRestricted(
              media_id)) {
        request_result =
            blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
      }
    }
#endif
    if (request_result == blink::mojom::MediaStreamRequestResult::OK) {
      const auto& visible_url = url_formatter::FormatUrlForSecurityDisplay(
          web_contents->GetLastCommittedURL(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
      const bool disable_local_echo =
          (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) &&
          media_id.web_contents_id.disable_local_echo;
      ui = GetDevicesForDesktopCapture(
          web_contents, &devices, media_id, pending_request.request.video_type,
          blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
          media_id.audio_share, disable_local_echo, display_notification_,
          visible_url, visible_url);
    }
  }

  if (request_result == blink::mojom::MediaStreamRequestResult::OK)
    UpdateTarget(pending_request.request, media_id);

  std::move(pending_request.callback)
      .Run(devices, request_result, std::move(ui));
  queue.pop_front();

  if (!queue.empty())
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DisplayMediaAccessHandler::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pending_requests_.erase(web_contents);
}

void DisplayMediaAccessHandler::DeletePendingAccessRequest(
    int render_process_id,
    int render_frame_id,
    int page_request_id) {
  for (auto& queue_it : pending_requests_) {
    RequestsQueue& queue = queue_it.second;
    for (auto it = queue.begin(); it != queue.end(); ++it) {
      const PendingAccessRequest& pending_request = **it;
      if (pending_request.request.render_process_id == render_process_id &&
          pending_request.request.render_frame_id == render_frame_id &&
          pending_request.request.page_request_id == page_request_id) {
        queue.erase(it);
        return;
      }
    }
  }
}
