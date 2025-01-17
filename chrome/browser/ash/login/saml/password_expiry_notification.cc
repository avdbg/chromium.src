// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_expiry_notification.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/session/session_activation_observer.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::ButtonInfo;
using message_center::HandleNotificationClickDelegate;
using message_center::Notification;
using message_center::NotificationDelegate;
using message_center::NotificationObserver;
using message_center::NotificationType;
using message_center::NotifierId;
using message_center::NotifierType;
using message_center::RichNotificationData;
using message_center::SystemNotificationWarningLevel;
using message_center::ThunkNotificationDelegate;

namespace chromeos {

namespace {

// Unique ID for this notification.
const char kNotificationId[] = "saml.password-expiry-notification";

// Simplest type of notification UI - no progress bars, images etc.
const NotificationType kNotificationType =
    message_center::NOTIFICATION_TYPE_SIMPLE;

// Generic type for notifications that are not from web pages etc.
const NotificationHandler::Type kNotificationHandlerType =
    NotificationHandler::Type::TRANSIENT;

// The icon to use for this notification - looks like an office building.
const gfx::VectorIcon& kIcon = vector_icons::kBusinessIcon;

// Warning level of WARNING makes the title orange.
constexpr SystemNotificationWarningLevel kWarningLevel =
    SystemNotificationWarningLevel::WARNING;

// A time-delta of length one minute.
constexpr base::TimeDelta kOneMinute = base::TimeDelta::FromMinutes(1);

base::string16 GetBodyText() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_EXPIRY_CALL_TO_ACTION);
}

RichNotificationData GetRichNotificationData() {
  RichNotificationData result;
  result.buttons = std::vector<ButtonInfo>{ButtonInfo(
      l10n_util::GetStringUTF16(IDS_PASSWORD_EXPIRY_CHANGE_PASSWORD_BUTTON))};
  return result;
}

// Delegate for handling clicks on the notification.
class PasswordExpiryNotificationDelegate : public NotificationDelegate {
 public:
  PasswordExpiryNotificationDelegate();

 protected:
  ~PasswordExpiryNotificationDelegate() override;

  // message_center::NotificationDelegate:
  void Close(bool by_user) override;
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override;
};

PasswordExpiryNotificationDelegate::PasswordExpiryNotificationDelegate() =
    default;
PasswordExpiryNotificationDelegate::~PasswordExpiryNotificationDelegate() =
    default;

void PasswordExpiryNotificationDelegate::Close(bool by_user) {
  if (by_user) {
    InSessionPasswordChangeManager::Get()
        ->OnExpiryNotificationDismissedByUser();
  }
}

void PasswordExpiryNotificationDelegate::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  bool clicked_on_button = button_index.has_value();
  if (clicked_on_button) {
    InSessionPasswordChangeManager::Get()->StartInSessionPasswordChange();
  }
}

}  // namespace

// static
void PasswordExpiryNotification::Show(Profile* profile,
                                      base::TimeDelta time_until_expiry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // NotifierId for histogram reporting.
  static const base::NoDestructor<NotifierId> kNotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotificationId);

  // Leaving this empty means the notification is attributed to the system -
  // ie "Chromium OS" or similar.
  static const base::NoDestructor<base::string16> kEmptyDisplaySource;

  // No origin URL is needed since the notification comes from the system.
  static const base::NoDestructor<GURL> kEmptyOriginUrl;

  const base::string16 title = GetTitleText(time_until_expiry);
  const base::string16 body = GetBodyText();
  const RichNotificationData rich_notification_data = GetRichNotificationData();
  const scoped_refptr<PasswordExpiryNotificationDelegate> delegate =
      base::MakeRefCounted<PasswordExpiryNotificationDelegate>();

  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      kNotificationType, kNotificationId, title, body, *kEmptyDisplaySource,
      *kEmptyOriginUrl, *kNotifierId, rich_notification_data, delegate, kIcon,
      kWarningLevel);

  NotificationDisplayService* nds =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  // Calling close before display ensures that the notification pops up again
  // even if it is already shown.
  nds->Close(kNotificationHandlerType, kNotificationId);
  nds->Display(kNotificationHandlerType, *notification, /*metadata=*/nullptr);
}

// static
base::string16 PasswordExpiryNotification::GetTitleText(
    base::TimeDelta time_until_expiry) {
  if (time_until_expiry < kOneMinute) {
    // Don't need to count the seconds - just say its overdue.
    return l10n_util::GetStringUTF16(IDS_PASSWORD_CHANGE_OVERDUE_TITLE);
  }
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_EXPIRES_AFTER_TIME_TITLE,
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, time_until_expiry));
}

// static
void PasswordExpiryNotification::Dismiss(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      kNotificationHandlerType, kNotificationId);
}

}  // namespace chromeos
