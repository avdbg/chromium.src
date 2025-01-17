// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"

#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "media/audio/audio_device_description.h"
#include "media/mojo/mojom/display_media_information.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

media::mojom::DisplayMediaInformationPtr
DesktopMediaIDToDisplayMediaInformation(
    const content::DesktopMediaID& media_id) {
  media::mojom::DisplayCaptureSurfaceType display_surface =
      media::mojom::DisplayCaptureSurfaceType::MONITOR;
  bool logical_surface = true;
  media::mojom::CursorCaptureType cursor =
      media::mojom::CursorCaptureType::NEVER;
#if defined(USE_AURA)
  const bool uses_aura =
      media_id.window_id != content::DesktopMediaID::kNullId ? true : false;
#else
  const bool uses_aura = false;
#endif  // defined(USE_AURA)
  switch (media_id.type) {
    case content::DesktopMediaID::TYPE_SCREEN:
      display_surface = media::mojom::DisplayCaptureSurfaceType::MONITOR;
      cursor = uses_aura ? media::mojom::CursorCaptureType::MOTION
                         : media::mojom::CursorCaptureType::ALWAYS;
      break;
    case content::DesktopMediaID::TYPE_WINDOW:
      display_surface = media::mojom::DisplayCaptureSurfaceType::WINDOW;
      cursor = uses_aura ? media::mojom::CursorCaptureType::MOTION
                         : media::mojom::CursorCaptureType::ALWAYS;
      break;
    case content::DesktopMediaID::TYPE_WEB_CONTENTS:
      display_surface = media::mojom::DisplayCaptureSurfaceType::BROWSER;
      cursor = media::mojom::CursorCaptureType::MOTION;
      break;
    case content::DesktopMediaID::TYPE_NONE:
      break;
  }

  return media::mojom::DisplayMediaInformation::New(display_surface,
                                                    logical_surface, cursor);
}

#if 0
base::string16 GetStopSharingUIString(
    const base::string16& application_title,
    const base::string16& registered_extension_name,
    bool capture_audio,
    content::DesktopMediaID::Type capture_type) {
  if (!capture_audio) {
    if (application_title == registered_extension_name) {
      switch (capture_type) {
        case content::DesktopMediaID::TYPE_SCREEN:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT, application_title);
        case content::DesktopMediaID::TYPE_WINDOW:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_WINDOW_CAPTURE_NOTIFICATION_TEXT, application_title);
        case content::DesktopMediaID::TYPE_WEB_CONTENTS:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_TAB_CAPTURE_NOTIFICATION_TEXT, application_title);
        case content::DesktopMediaID::TYPE_NONE:
          NOTREACHED();
      }
    } else {
      switch (capture_type) {
        case content::DesktopMediaID::TYPE_SCREEN:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT_DELEGATED,
              registered_extension_name, application_title);
        case content::DesktopMediaID::TYPE_WINDOW:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_WINDOW_CAPTURE_NOTIFICATION_TEXT_DELEGATED,
              registered_extension_name, application_title);
        case content::DesktopMediaID::TYPE_WEB_CONTENTS:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_TAB_CAPTURE_NOTIFICATION_TEXT_DELEGATED,
              registered_extension_name, application_title);
        case content::DesktopMediaID::TYPE_NONE:
          NOTREACHED();
      }
    }
  } else {  // The case with audio
    if (application_title == registered_extension_name) {
      switch (capture_type) {
        case content::DesktopMediaID::TYPE_SCREEN:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_SCREEN_CAPTURE_WITH_AUDIO_NOTIFICATION_TEXT,
              application_title);
        case content::DesktopMediaID::TYPE_WEB_CONTENTS:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_TAB_CAPTURE_WITH_AUDIO_NOTIFICATION_TEXT,
              application_title);
        case content::DesktopMediaID::TYPE_NONE:
        case content::DesktopMediaID::TYPE_WINDOW:
          NOTREACHED();
      }
    } else {
      switch (capture_type) {
        case content::DesktopMediaID::TYPE_SCREEN:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_SCREEN_CAPTURE_WITH_AUDIO_NOTIFICATION_TEXT_DELEGATED,
              registered_extension_name, application_title);
        case content::DesktopMediaID::TYPE_WEB_CONTENTS:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_TAB_CAPTURE_WITH_AUDIO_NOTIFICATION_TEXT_DELEGATED,
              registered_extension_name, application_title);
        case content::DesktopMediaID::TYPE_NONE:
        case content::DesktopMediaID::TYPE_WINDOW:
          NOTREACHED();
      }
    }
  }
  return base::string16();
}
#endif

std::string DeviceNamePrefix(
    content::WebContents* web_contents,
    blink::mojom::MediaStreamType requested_stream_type,
    const content::DesktopMediaID& media_id) {
  if (!web_contents ||
      requested_stream_type !=
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB) {
    return std::string();
  }

  // Note that all of these must still be checked, as the explicit-selection
  // dialog for |getCurrentBrowsingContextMedia| could still return something
  // other than the current tab - be it a screen, window, or another tab.
  if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS &&
      web_contents->GetMainFrame()->GetProcess()->GetID() ==
          media_id.web_contents_id.render_process_id &&
      web_contents->GetMainFrame()->GetRoutingID() ==
          media_id.web_contents_id.main_render_frame_id) {
    return "current-";
  }

  return std::string();
}

}  // namespace

std::unique_ptr<content::MediaStreamUI> GetDevicesForDesktopCapture(
    content::WebContents* web_contents,
    blink::MediaStreamDevices* devices,
    const content::DesktopMediaID& media_id,
    blink::mojom::MediaStreamType devices_video_type,
    blink::mojom::MediaStreamType devices_audio_type,
    bool capture_audio,
    bool disable_local_echo,
    bool display_notification,
    const base::string16& application_title,
    const base::string16& registered_extension_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(2) << __func__ << ": media_id " << media_id.ToString()
           << ", capture_audio " << capture_audio << ", disable_local_echo "
           << disable_local_echo << ", display_notification "
           << display_notification << ", application_title "
           << application_title << ", extension_name "
           << registered_extension_name;

  // Add selected desktop source to the list.
  const std::string device_id = media_id.ToString();
  const std::string device_name =
      DeviceNamePrefix(web_contents, devices_video_type, media_id) + device_id;
  auto device =
      blink::MediaStreamDevice(devices_video_type, device_id, device_name);
  device.display_media_info = DesktopMediaIDToDisplayMediaInformation(media_id);
  devices->push_back(device);
  if (capture_audio) {
    if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) {
      content::WebContentsMediaCaptureId web_id = media_id.web_contents_id;
      web_id.disable_local_echo = disable_local_echo;
      devices->push_back(blink::MediaStreamDevice(
          devices_audio_type, web_id.ToString(), "Tab audio"));
    } else if (disable_local_echo) {
      // Use the special loopback device ID for system audio capture.
      devices->push_back(blink::MediaStreamDevice(
          devices_audio_type,
          media::AudioDeviceDescription::kLoopbackWithMuteDeviceId,
          "System Audio"));
    } else {
      // Use the special loopback device ID for system audio capture.
      devices->push_back(blink::MediaStreamDevice(
          devices_audio_type,
          media::AudioDeviceDescription::kLoopbackInputDeviceId,
          "System Audio"));
    }
  }

#if 0
  // If required, register to display the notification for stream capture.
  std::unique_ptr<MediaStreamUI> notification_ui;
  if (display_notification) {
    if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS &&
        base::FeatureList::IsEnabled(
            features::kDesktopCaptureTabSharingInfobar)) {
      notification_ui = TabSharingUI::Create(media_id, application_title);
    } else {
      notification_ui = ScreenCaptureNotificationUI::Create(
          GetStopSharingUIString(application_title, registered_extension_name,
                                 capture_audio, media_id.type));
    }
  }

  return MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->RegisterMediaStream(web_contents, *devices, std::move(notification_ui),
                            application_title);
#endif
  std::unique_ptr<content::MediaStreamUI> ui;
  return ui;
}
