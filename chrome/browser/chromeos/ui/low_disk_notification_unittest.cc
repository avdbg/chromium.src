// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/low_disk_notification.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// Copied from low_disk_notification.cc
const uint64_t kMediumNotification = (1 << 30) - 1;
const uint64_t kHighNotification = (512 << 20) - 1;

}  // namespace

namespace chromeos {

class LowDiskNotificationTest : public BrowserWithTestWindowTest {
 public:
  LowDiskNotificationTest() = default;
  ~LowDiskNotificationTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    CryptohomeClient::InitializeFake();

    GetCrosSettingsHelper()->ReplaceDeviceSettingsProviderWithStub();
    GetCrosSettingsHelper()->SetBoolean(
        chromeos::kDeviceShowLowDiskSpaceNotification, true);

    auto user_manager = std::make_unique<user_manager::FakeUserManager>();
    user_manager_ = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);
    tester_->SetNotificationAddedClosure(base::BindRepeating(
        &LowDiskNotificationTest::OnNotificationAdded, base::Unretained(this)));
    low_disk_notification_ = std::make_unique<LowDiskNotification>();
    notification_count_ = 0;
  }

  void TearDown() override {
    low_disk_notification_.reset();
    CryptohomeClient::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  base::Optional<message_center::Notification> GetNotification() {
    return tester_->GetNotification("low_disk");
  }

  void SetNotificationThrottlingInterval(int ms) {
    low_disk_notification_->SetNotificationIntervalForTest(
        base::TimeDelta::FromMilliseconds(ms));
  }

  void OnNotificationAdded() { notification_count_++; }

 protected:
  user_manager::FakeUserManager* user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<LowDiskNotification> low_disk_notification_;
  int notification_count_;
};

TEST_F(LowDiskNotificationTest, MediumLevelNotification) {
  base::string16 expected_title =
      l10n_util::GetStringUTF16(IDS_LOW_DISK_NOTIFICATION_TITLE);
  low_disk_notification_->LowDiskSpace(kMediumNotification);
  auto notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
  EXPECT_EQ(1, notification_count_);
}

TEST_F(LowDiskNotificationTest, HighLevelReplacesMedium) {
  base::string16 expected_title =
      l10n_util::GetStringUTF16(IDS_CRITICALLY_LOW_DISK_NOTIFICATION_TITLE);
  low_disk_notification_->LowDiskSpace(kMediumNotification);
  low_disk_notification_->LowDiskSpace(kHighNotification);
  auto notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
  EXPECT_EQ(2, notification_count_);
}

TEST_F(LowDiskNotificationTest, NotificationsAreThrottled) {
  SetNotificationThrottlingInterval(10000000);
  low_disk_notification_->LowDiskSpace(kHighNotification);
  low_disk_notification_->LowDiskSpace(kHighNotification);
  EXPECT_EQ(1, notification_count_);
}

TEST_F(LowDiskNotificationTest, HighNotificationsAreShownAfterThrottling) {
  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->LowDiskSpace(kHighNotification);
  low_disk_notification_->LowDiskSpace(kHighNotification);
  EXPECT_EQ(2, notification_count_);
}

TEST_F(LowDiskNotificationTest, MediumNotificationsAreNotShownAfterThrottling) {
  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->LowDiskSpace(kMediumNotification);
  low_disk_notification_->LowDiskSpace(kMediumNotification);
  EXPECT_EQ(1, notification_count_);
}

TEST_F(LowDiskNotificationTest, ShowForMultipleUsersWhenEnrolled) {
  user_manager_->AddUser(
      AccountId::FromUserEmailGaiaId("test_user1@example.com", "1234567891"));
  user_manager_->AddUser(
      AccountId::FromUserEmailGaiaId("test_user2@example.com", "1234567892"));

  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->LowDiskSpace(kHighNotification);
  EXPECT_EQ(1, notification_count_);
}

TEST_F(LowDiskNotificationTest, SupressedForMultipleUsersWhenEnrolled) {
  user_manager_->AddUser(
      AccountId::FromUserEmailGaiaId("test_user1@example.com", "1234567891"));
  user_manager_->AddUser(
      AccountId::FromUserEmailGaiaId("test_user2@example.com", "1234567892"));

  GetCrosSettingsHelper()->SetBoolean(
      chromeos::kDeviceShowLowDiskSpaceNotification, false);

  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->LowDiskSpace(kHighNotification);
  EXPECT_EQ(0, notification_count_);
}

}  // namespace chromeos
