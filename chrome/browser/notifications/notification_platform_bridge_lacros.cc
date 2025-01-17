// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_lacros.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegate.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace {

crosapi::mojom::NotificationType ToMojo(message_center::NotificationType type) {
  switch (type) {
    case message_center::NOTIFICATION_TYPE_SIMPLE:
    case message_center::NOTIFICATION_TYPE_BASE_FORMAT:
      // TYPE_BASE_FORMAT is displayed the same as TYPE_SIMPLE.
      return crosapi::mojom::NotificationType::kSimple;
    case message_center::NOTIFICATION_TYPE_IMAGE:
      return crosapi::mojom::NotificationType::kImage;
    case message_center::NOTIFICATION_TYPE_MULTIPLE:
      return crosapi::mojom::NotificationType::kList;
    case message_center::NOTIFICATION_TYPE_PROGRESS:
      return crosapi::mojom::NotificationType::kProgress;
    case message_center::NOTIFICATION_TYPE_CUSTOM:
      // TYPE_CUSTOM exists only within ash.
      NOTREACHED();
      return crosapi::mojom::NotificationType::kSimple;
  }
}

crosapi::mojom::FullscreenVisibility ToMojo(
    message_center::FullscreenVisibility visibility) {
  switch (visibility) {
    case message_center::FullscreenVisibility::NONE:
      return crosapi::mojom::FullscreenVisibility::kNone;
    case message_center::FullscreenVisibility::OVER_USER:
      return crosapi::mojom::FullscreenVisibility::kOverUser;
  }
}

crosapi::mojom::NotificationPtr ToMojo(
    const message_center::Notification& notification) {
  auto mojo_note = crosapi::mojom::Notification::New();
  mojo_note->type = ToMojo(notification.type());
  mojo_note->id = notification.id();
  mojo_note->title = notification.title();
  mojo_note->message = notification.message();
  mojo_note->display_source = notification.display_source();
  mojo_note->origin_url = notification.origin_url();
  if (!notification.icon().IsEmpty())
    mojo_note->icon = notification.icon().AsImageSkia();
  mojo_note->priority = base::ClampToRange(notification.priority(), -2, 2);
  mojo_note->require_interaction = notification.never_timeout();
  mojo_note->timestamp = notification.timestamp();
  if (!notification.image().IsEmpty())
    mojo_note->image = notification.image().AsImageSkia();
  if (!notification.small_image().IsEmpty())
    mojo_note->badge = notification.small_image().AsImageSkia();
  for (const auto& item : notification.items()) {
    auto mojo_item = crosapi::mojom::NotificationItem::New();
    mojo_item->title = item.title;
    mojo_item->message = item.message;
    mojo_note->items.push_back(std::move(mojo_item));
  }
  mojo_note->progress = base::ClampToRange(notification.progress(), -1, 100);
  mojo_note->progress_status = notification.progress_status();
  for (const auto& button : notification.buttons()) {
    auto mojo_button = crosapi::mojom::ButtonInfo::New();
    mojo_button->title = button.title;
    mojo_note->buttons.push_back(std::move(mojo_button));
  }
  mojo_note->pinned = notification.pinned();
  mojo_note->renotify = notification.renotify();
  mojo_note->silent = notification.silent();
  mojo_note->accessible_name = notification.accessible_name();
  mojo_note->fullscreen_visibility =
      ToMojo(notification.fullscreen_visibility());
  return mojo_note;
}

}  // namespace

// Keeps track of notifications being displayed in the remote message center.
class NotificationPlatformBridgeLacros::RemoteNotificationDelegate
    : public crosapi::mojom::NotificationDelegate {
 public:
  RemoteNotificationDelegate(
      const std::string& notification_id,
      NotificationPlatformBridgeDelegate* bridge_delegate,
      base::WeakPtr<NotificationPlatformBridgeLacros> owner)
      : notification_id_(notification_id),
        bridge_delegate_(bridge_delegate),
        owner_(owner) {
    DCHECK(!notification_id_.empty());
    DCHECK(bridge_delegate_);
    DCHECK(owner_);
  }
  RemoteNotificationDelegate(const RemoteNotificationDelegate&) = delete;
  RemoteNotificationDelegate& operator=(const RemoteNotificationDelegate&) =
      delete;
  ~RemoteNotificationDelegate() override = default;

  mojo::PendingRemote<crosapi::mojom::NotificationDelegate>
  BindNotificationDelegate() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // crosapi::mojom::NotificationDelegate:
  void OnNotificationClosed(bool by_user) override {
    bridge_delegate_->HandleNotificationClosed(notification_id_, by_user);
    if (owner_)
      owner_->OnRemoteNotificationClosed(notification_id_);
    // NOTE: |this| is deleted.
  }

  void OnNotificationClicked() override {
    bridge_delegate_->HandleNotificationClicked(notification_id_);
  }

  void OnNotificationButtonClicked(uint32_t button_index) override {
    // Chrome OS does not support inline reply.
    bridge_delegate_->HandleNotificationButtonClicked(
        notification_id_, base::checked_cast<int>(button_index),
        /*reply=*/base::nullopt);
  }

  void OnNotificationSettingsButtonClicked() override {
    bridge_delegate_->HandleNotificationSettingsButtonClicked(notification_id_);
  }

  void OnNotificationDisabled() override {
    bridge_delegate_->DisableNotification(notification_id_);
  }

 private:
  const std::string notification_id_;
  NotificationPlatformBridgeDelegate* const bridge_delegate_;
  base::WeakPtr<NotificationPlatformBridgeLacros> owner_;
  mojo::Receiver<crosapi::mojom::NotificationDelegate> receiver_{this};
};

NotificationPlatformBridgeLacros::NotificationPlatformBridgeLacros(
    NotificationPlatformBridgeDelegate* delegate,
    mojo::Remote<crosapi::mojom::MessageCenter>* message_center_remote)
    : bridge_delegate_(delegate),
      message_center_remote_(message_center_remote) {
  DCHECK(bridge_delegate_);
}

NotificationPlatformBridgeLacros::~NotificationPlatformBridgeLacros() = default;

void NotificationPlatformBridgeLacros::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  if (!message_center_remote_)
    return;

  // |profile| is ignored because Profile management is handled in
  // NotificationPlatformBridgeChromeOs, which includes a profile ID as part of
  // the notification ID. Lacros does not support Chrome OS multi-signin, so we
  // don't need to handle inactive user notification blockers in ash.

  // Clean up any old notification with the same ID before creating the new one.
  remote_notifications_.erase(notification.id());

  auto pending_notification = std::make_unique<RemoteNotificationDelegate>(
      notification.id(), bridge_delegate_, weak_factory_.GetWeakPtr());
  (*message_center_remote_)
      ->DisplayNotification(ToMojo(notification),
                            pending_notification->BindNotificationDelegate());
  remote_notifications_[notification.id()] = std::move(pending_notification);
}

void NotificationPlatformBridgeLacros::Close(
    Profile* profile,
    const std::string& notification_id) {
  if (!message_center_remote_)
    return;

  (*message_center_remote_)->CloseNotification(notification_id);
  // |remote_notifications_| is cleaned up after the remote notification closes
  // and notifies us via the delegate.
}

void NotificationPlatformBridgeLacros::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  NOTIMPLEMENTED();
  std::move(callback).Run(/*notification_ids=*/{}, /*supports_sync=*/false);
}

void NotificationPlatformBridgeLacros::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(!!message_center_remote_);
}

void NotificationPlatformBridgeLacros::DisplayServiceShutDown(
    Profile* profile) {
  remote_notifications_.clear();
}

void NotificationPlatformBridgeLacros::OnRemoteNotificationClosed(
    const std::string& id) {
  remote_notifications_.erase(id);
}
