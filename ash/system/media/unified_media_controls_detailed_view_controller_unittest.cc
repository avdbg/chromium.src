// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_detailed_view_controller.h"

#include "ash/public/cpp/media_notification_provider.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace ash {

namespace {

class MockMediaNotificationProvider : MediaNotificationProvider {
 public:
  MockMediaNotificationProvider() {
    MediaNotificationProvider::Set(this);

    ON_CALL(*this, GetMediaNotificationListView(_)).WillByDefault([](auto) {
      return std::make_unique<views::View>();
    });
  }

  ~MockMediaNotificationProvider() override {
    MediaNotificationProvider::Set(nullptr);
  }

  // MediaNotificationProvider implementations.
  MOCK_METHOD1(GetMediaNotificationListView, std::unique_ptr<views::View>(int));
  MOCK_METHOD0(OnBubbleClosing, void());
  std::unique_ptr<views::View> GetActiveMediaNotificationView() override {
    return std::make_unique<views::View>();
  }
  void AddObserver(MediaNotificationProviderObserver* observer) override {}
  void RemoveObserver(MediaNotificationProviderObserver* observer) override {}
  bool HasActiveNotifications() override { return true; }
  bool HasFrozenNotifications() override { return true; }
  void SetColorTheme(
      const media_message_center::NotificationTheme& color_theme) override {}
};

}  // namespace

class UnifiedMediaControlsDetailedViewControllerTest : public AshTestBase {
 public:
  UnifiedMediaControlsDetailedViewControllerTest() = default;
  ~UnifiedMediaControlsDetailedViewControllerTest() override = default;

  void SetUp() override {
    provider_ = std::make_unique<MockMediaNotificationProvider>();
    AshTestBase::SetUp();

    // Ensure media tray is not pinned to shelf so that media controls
    // show up in quick settings.
    MediaTray::SetPinnedToShelf(false);

    StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->unified_system_tray()
        ->ShowBubble(false /* show_by_click */);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    provider_.reset();
  }

  void SimulateTransitionToMainMenu() {
    UnifiedMediaControlsDetailedViewController* controller =
        static_cast<UnifiedMediaControlsDetailedViewController*>(
            system_tray_controller()->detailed_view_controller());
    controller->detailed_view_delegate_->TransitionToMainView(
        true /* restore_focus */);
  }

  UnifiedSystemTrayController* system_tray_controller() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->unified_system_tray()
        ->bubble()
        ->controller_for_test();
  }

  MockMediaNotificationProvider* provider() { return provider_.get(); }

 private:
  std::unique_ptr<MockMediaNotificationProvider> provider_;
};

TEST_F(UnifiedMediaControlsDetailedViewControllerTest,
       EnterAndExitDetailedView) {
  // UnifiedSystemTrayController should have no DetailedViewController
  // initially.
  EXPECT_EQ(system_tray_controller()->detailed_view_controller(), nullptr);

  // We should get a MediaNotificationProvider::GetMediaNotificationListView
  // call when creating the detailed view.
  EXPECT_CALL(*provider(), GetMediaNotificationListView);
  system_tray_controller()->OnMediaControlsViewClicked();
  EXPECT_NE(system_tray_controller()->detailed_view_controller(), nullptr);

  // Should notify provider when transition to main menu.
  EXPECT_CALL(*provider(), OnBubbleClosing);
  SimulateTransitionToMainMenu();
  EXPECT_EQ(system_tray_controller()->detailed_view_controller(), nullptr);
}

}  // namespace ash
