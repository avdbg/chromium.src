// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_H_

#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer_set.h"
#include "chrome/browser/ui/global_media_controls/media_notification_producer.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_observer.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notification.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

class MediaNotificationContainerImpl;
class Profile;

class MediaSessionNotificationProducer
    : public MediaNotificationProducer,
      public media_session::mojom::AudioFocusObserver,
      public MediaNotificationContainerObserver {
 public:
  MediaSessionNotificationProducer(MediaNotificationService* service,
                                   Profile* profile,
                                   bool show_from_all_profiles);
  ~MediaSessionNotificationProducer() override;

  // MediaNotificationProducer:
  base::WeakPtr<media_message_center::MediaNotificationItem>
  GetNotificationItem(const std::string& id) override;
  std::set<std::string> GetActiveControllableNotificationIds() const override;
  void OnItemShown(const std::string& id,
                   MediaNotificationContainerImpl* container) override;

  // media_session::mojom::AudioFocusObserver:
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;

  // MediaNotificationContainerObserver implementation.
  void OnContainerClicked(const std::string& id) override;
  void OnContainerDismissed(const std::string& id) override;
  void OnContainerDraggedOut(const std::string& id, gfx::Rect bounds) override;
  void OnAudioSinkChosen(const std::string& id,
                         const std::string& sink_id) override;

  void HideItem(const std::string& id);
  void RemoveItem(const std::string& id);
  // Puts the item with the given ID on the list of active items. Returns false
  // if we fail to do so because the item is hidden or is an overlay. Requires
  // that the item exists.
  bool ActivateItem(const std::string& id);
  bool HasSession(const std::string& id) const;
  bool IsSessionPlaying(const std::string& id) const;
  // Returns whether there still exists a session for |id|.
  bool OnOverlayNotificationClosed(const std::string& id);
  bool HasFrozenNotifications() const;
  std::unique_ptr<media_router::CastDialogController>
  CreateCastDialogControllerForSession(const std::string& id);
  bool HasSessionForWebContents(content::WebContents* web_contents) const;
  void LogMediaSessionActionButtonPressed(
      const std::string& id,
      media_session::mojom::MediaSessionAction action);

  // Used by a |MediaNotificationDeviceSelectorView| to query the system
  // for connected audio output devices.
  base::CallbackListSubscription RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallback callback);

  // Used by a |MediaNotificationAudioDeviceSelectorView| to become notified of
  // audio device switching capabilities. The callback will be immediately run
  // with the current availability.
  base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      const std::string& id,
      base::RepeatingCallback<void(bool)> callback);

  void set_device_provider_for_testing(
      std::unique_ptr<MediaNotificationDeviceProvider> device_provider);

 private:
  friend class MediaNotificationService::Session;
  friend class MediaNotificationServiceTest;
  friend class MediaToolbarButtonControllerTest;

  // Looks up a Session object by its ID. Returns null if not found.
  MediaNotificationService::Session* GetSession(const std::string& id);
  // Called by a |MediaNotificationService::Session| when it becomes active.
  void OnSessionBecameActive(const std::string& id);
  // Called by a |MediaNotificationService::Session| when it becomes inactive.
  void OnSessionBecameInactive(const std::string& id);
  void HideMediaDialog();
  void OnReceivedAudioFocusRequests(
      std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions);
  void OnItemUnfrozen(const std::string& id);

  // Used to track whether there are any active controllable sessions.
  std::set<std::string> active_controllable_session_ids_;

  // Tracks the sessions that are currently frozen. If there are only frozen
  // sessions, we will disable the toolbar icon and wait to hide it.
  std::set<std::string> frozen_session_ids_;

  // Tracks the sessions that are currently inactive. Sessions become inactive
  // after a period of time of being paused with no user interaction. Inactive
  // sessions are hidden from the dialog until the user interacts with them
  // again (e.g. by playing the session).
  std::set<std::string> inactive_session_ids_;

  // Tracks the sessions that are currently dragged out of the dialog. These
  // should not be shown in the dialog and will be ignored for showing the
  // toolbar icon.
  std::set<std::string> dragged_out_session_ids_;

  // Connections with the media session service to listen for audio focus
  // updates and control media sessions.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote_;
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};

  MediaNotificationService* const service_;

  // Keeps track of all the containers we're currently observing.
  MediaNotificationContainerObserverSet container_observer_set_;

  OverlayMediaNotificationsManagerImpl overlay_media_notifications_manager_;

  // Stores a Session for each media session keyed by its |request_id| in string
  // format.
  std::map<std::string, MediaNotificationService::Session> sessions_;

  // Tracks the number of times we have recorded an action for a specific
  // source. We use this to cap the number of UKM recordings per site.
  std::map<ukm::SourceId, int> actions_recorded_to_ukm_;

  std::unique_ptr<MediaNotificationDeviceProvider> device_provider_;

  base::WeakPtrFactory<MediaSessionNotificationProducer> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_H_
