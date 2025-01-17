// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_tray.h"

#include "ash/public/cpp/media_notification_provider.h"
#include "ash/shelf/shelf.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/event.h"

using ::testing::_;

namespace ash {
namespace {

constexpr gfx::Size kMockTraySize = gfx::Size(48, 48);

class MockMediaNotificationProvider : public MediaNotificationProvider {
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

  // Medianotificationprovider implementations.
  MOCK_METHOD1(GetMediaNotificationListView, std::unique_ptr<views::View>(int));
  MOCK_METHOD0(GetActiveMediaNotificationView, std::unique_ptr<views::View>());
  MOCK_METHOD0(OnBubbleClosing, void());
  void AddObserver(MediaNotificationProviderObserver* observer) override {}
  void RemoveObserver(MediaNotificationProviderObserver* observer) override {}
  bool HasActiveNotifications() override { return has_active_notifications_; }
  bool HasFrozenNotifications() override { return has_frozen_notifications_; }
  void SetColorTheme(
      const media_message_center::NotificationTheme& color_theme) override {}

  void SetHasActiveNotifications(bool has_active_notifications) {
    has_active_notifications_ = has_active_notifications;
  }

  void SetHasFrozenNotifications(bool has_frozen_notifications) {
    has_frozen_notifications_ = has_frozen_notifications;
  }

 private:
  bool has_active_notifications_ = false;
  bool has_frozen_notifications_ = false;
};

// Mock tray button used to test media tray bubble's anchor update.
class MockTrayBackgroundView : public ash::TrayBackgroundView {
 public:
  MockTrayBackgroundView(Shelf* shelf) : TrayBackgroundView(shelf) {
    SetSize(kMockTraySize);
  }

  ~MockTrayBackgroundView() override = default;

  // TrayBackgroundview implementations
  base::string16 GetAccessibleNameForTray() override {
    return base::ASCIIToUTF16("");
  }
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  void ClickedOutsideBubble() override {}
};

}  // namespace

class MediaTrayTest : public AshTestBase {
 public:
  MediaTrayTest() = default;
  ~MediaTrayTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(media::kGlobalMediaControlsForChromeOS);
    provider_ = std::make_unique<MockMediaNotificationProvider>();
    AshTestBase::SetUp();

    media_tray_ = status_area_widget()->media_tray();
    ASSERT_TRUE(MediaTray::IsPinnedToShelf());
  }

  void TearDown() override {
    provider_.reset();
    mock_tray_.reset();
    AshTestBase::TearDown();
  }

  // Insert mock tray to status area widget right before system tray (The last
  // two tray buttons are always system tray and overview button tray).
  void InsertMockTray() {
    mock_tray_ =
        std::make_unique<MockTrayBackgroundView>(status_area_widget()->shelf());
    status_area_widget()->tray_buttons_.insert(
        status_area_widget()->tray_buttons_.end() - 2, mock_tray_.get());
  }

  void SimulateNotificationListChanged() {
    media_tray_->OnNotificationListChanged();
  }

  void SimulateTapOnMediaTray() {
    ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    media_tray_->PerformAction(tap);
  }

  void SimulateTapOnPinButton() {
    ASSERT_TRUE(media_tray_->pin_button_for_testing());
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(media_tray_->pin_button_for_testing()
                               ->GetBoundsInScreen()
                               .CenterPoint());
    generator->ClickLeftButton();
  }

  void SimulateMockTrayVisibilityChanged(bool visible) {
    mock_tray_->SetVisible(visible);
    media_tray_->AnchorUpdated();
  }

  TrayBubbleWrapper* GetBubbleWrapper() {
    return media_tray_->tray_bubble_wrapper_for_testing();
  }

  gfx::Rect GetBubbleBounds() {
    return GetBubbleWrapper()->GetBubbleView()->GetBoundsInScreen();
  }

  StatusAreaWidget* status_area_widget() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  }

  MockMediaNotificationProvider* provider() { return provider_.get(); }

  MediaTray* media_tray() { return media_tray_; }

  views::View* empty_state_view() { return media_tray_->empty_state_view_; }

 private:
  std::unique_ptr<MockMediaNotificationProvider> provider_;
  MediaTray* media_tray_;
  std::unique_ptr<MockTrayBackgroundView> mock_tray_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MediaTrayTest, MediaTrayVisibilityTest) {
  // Media tray should be invisible initially.
  ASSERT_TRUE(media_tray());
  EXPECT_FALSE(media_tray()->GetVisible());

  // Media tray should be visible when there is active notification.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());

  // Media tray should hide itself when no media is playing.
  provider()->SetHasActiveNotifications(false);
  SimulateNotificationListChanged();
  EXPECT_FALSE(media_tray()->GetVisible());

  // Media tray should be visible when there is frozen notification.
  provider()->SetHasFrozenNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());

  // Media tray should be hidden when screen is locked.
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->FlushForTest();
  EXPECT_FALSE(media_tray()->GetVisible());

  // Media tray should be visible again when we unlock the screen.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(media_tray()->GetVisible());

  // Media tray should not be visible if global media controls is
  // not pinned to shelf.
  MediaTray::SetPinnedToShelf(false);
  EXPECT_FALSE(media_tray()->GetVisible());
}

TEST_F(MediaTrayTest, ShowAndHideBubbleTest) {
  // Media tray should be visible when there is active notification.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());

  // Bubble should not exist initially, and media tray should not
  // be active.
  EXPECT_EQ(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->is_active());

  // Tap the media tray should show the bubble, and media tray should
  // be active. GetMediaNotificationlistview also should be called for
  // getting active notifications.
  EXPECT_CALL(*provider(), GetMediaNotificationListView(_));
  SimulateTapOnMediaTray();
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_TRUE(media_tray()->is_active());

  // Tap again should close the bubble and MediaNotificationProvider should
  // be notified.
  EXPECT_CALL(*provider(), OnBubbleClosing());
  SimulateTapOnMediaTray();
  EXPECT_EQ(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->is_active());
}

TEST_F(MediaTrayTest, ShowEmptyStateWhenNoActiveNotification) {
  // Media tray should be visible when there is active notification.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());

  // Bubble should not exist initially, and media tray should not
  // be active.
  EXPECT_EQ(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->is_active());

  // Tap and show bubble.
  SimulateTapOnMediaTray();
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_TRUE(media_tray()->is_active());

  // We should display empty state if no media is playing.
  provider()->SetHasActiveNotifications(false);
  SimulateNotificationListChanged();
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->GetVisible());
  EXPECT_NE(empty_state_view(), nullptr);
  EXPECT_TRUE(empty_state_view()->GetVisible());

  // Empty state should be hidden if a new media starts playing.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_FALSE(empty_state_view()->GetVisible());
}

TEST_F(MediaTrayTest, PinButtonTest) {
  // Media tray should be invisible initially.
  ASSERT_TRUE(media_tray());
  EXPECT_FALSE(media_tray()->GetVisible());

  // Open global media controls dialog.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());
  SimulateTapOnMediaTray();
  EXPECT_NE(GetBubbleWrapper(), nullptr);

  // Tapping the pin button while the media controls dialog is opened
  // should have the media tray hidden.
  SimulateTapOnPinButton();
  EXPECT_NE(GetBubbleWrapper(), nullptr);
  EXPECT_FALSE(media_tray()->GetVisible());
  EXPECT_FALSE(MediaTray::IsPinnedToShelf());

  // Tap pin button again should bring back media tray.
  SimulateTapOnPinButton();
  EXPECT_TRUE(media_tray()->GetVisible());
  EXPECT_TRUE(MediaTray::IsPinnedToShelf());
}

TEST_F(MediaTrayTest, PinToShelfDefaultBehavior) {
  // Media controls should be pinned on shelf by default on a
  // 10 inch display.
  UpdateDisplay("800x530");
  EXPECT_FALSE(MediaTray::IsPinnedToShelf());

  // Media controls should be pinned on shelf by default on a
  // display larger than 10 inches.
  UpdateDisplay("800x600");
  EXPECT_TRUE(MediaTray::IsPinnedToShelf());
}

TEST_F(MediaTrayTest, BubbleGetsFocusWhenOpenWithKeyboard) {
  media_tray()->ShowBubble(false /* show_by_click */);
  EXPECT_TRUE(GetBubbleWrapper()->GetBubbleWidget()->IsActive());
}

TEST_F(MediaTrayTest, DialogAnchor) {
  InsertMockTray();

  // Simulate active notification and tap media tray to show dialog.
  provider()->SetHasActiveNotifications(true);
  SimulateNotificationListChanged();
  EXPECT_TRUE(media_tray()->GetVisible());
  SimulateTapOnMediaTray();
  EXPECT_NE(GetBubbleWrapper(), nullptr);

  EXPECT_TRUE(status_area_widget()->shelf()->IsHorizontalAlignment());
  gfx::Rect initial_bounds = GetBubbleBounds();

  // Simulate mock tray becoming visible, bubble should shift left.
  SimulateMockTrayVisibilityChanged(true);
  EXPECT_EQ(initial_bounds - gfx::Vector2d(kMockTraySize.width(), 0),
            GetBubbleBounds());

  // Simulate mock tray disappearing, bubble should shift back to the
  // original position.
  SimulateMockTrayVisibilityChanged(false);
  EXPECT_EQ(initial_bounds, GetBubbleBounds());

  // Simulate tapping pin button to hide media tray, bubble position
  // should not change.
  SimulateTapOnPinButton();
  EXPECT_FALSE(media_tray()->GetVisible());
  EXPECT_EQ(initial_bounds, GetBubbleBounds());

  // Simlate mock tray appearing and disappearing while the media tray
  // is hidden. Bubble should shift accordingly.
  SimulateMockTrayVisibilityChanged(true);
  EXPECT_EQ(initial_bounds - gfx::Vector2d(kMockTraySize.width(), 0),
            GetBubbleBounds());

  SimulateMockTrayVisibilityChanged(false);
  EXPECT_EQ(initial_bounds, GetBubbleBounds());

  // Tap pin button and bring back media tray, bubble position should
  // stay the same.
  SimulateTapOnPinButton();
  EXPECT_TRUE(media_tray()->GetVisible());
  EXPECT_EQ(initial_bounds, GetBubbleBounds());

  // Hide bubble, change shelf alignment to left (vertical), and open
  // bubble again.
  SimulateTapOnMediaTray();
  status_area_widget()->shelf()->SetAlignment(ShelfAlignment::kLeft);
  SimulateTapOnMediaTray();

  // Get new bounds.
  initial_bounds = GetBubbleBounds();

  // Simulate mock tray appears and disappears while the shelf alignment is
  // vertical. The bubble should shift vertically.
  SimulateMockTrayVisibilityChanged(true);
  EXPECT_EQ(initial_bounds - gfx::Vector2d(0, kMockTraySize.height()),
            GetBubbleBounds());

  SimulateMockTrayVisibilityChanged(false);
  EXPECT_EQ(initial_bounds, GetBubbleBounds());

  // Hide bubble, change shelf alignment back to bottom and switch ui
  // direction to RTL.
  SimulateTapOnMediaTray();
  status_area_widget()->shelf()->SetAlignment(ShelfAlignment::kBottom);
  base::i18n::SetRTLForTesting(true);
  status_area_widget()->UpdateLayout(false);
  SimulateTapOnMediaTray();

  // Get new bounds.
  initial_bounds = GetBubbleBounds();

  // Simulate tray appears and triggers while ui direction is RTL,
  // bubble should shift to the right.
  SimulateMockTrayVisibilityChanged(true);
  EXPECT_EQ(initial_bounds + gfx::Vector2d(kMockTraySize.width(), 0),
            GetBubbleBounds());

  SimulateMockTrayVisibilityChanged(false);
  EXPECT_EQ(initial_bounds, GetBubbleBounds());
}

class MediaTrayPinnedParamTest : public AshTestBase {
 public:
  MediaTrayPinnedParamTest() = default;
  ~MediaTrayPinnedParamTest() override = default;

  void SetUp() override {
    auto& pin_param = media::kCrosGlobalMediaControlsPinParam;
    feature_list_.InitAndEnableFeatureWithParameters(
        media::kGlobalMediaControlsForChromeOS,
        {{pin_param.name,
          pin_param.GetName(media::kCrosGlobalMediaControlsPinOptions::kPin)}});
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MediaTrayPinnedParamTest, PinParamTest) {
  UpdateDisplay("100x100");
  EXPECT_TRUE(MediaTray::IsPinnedToShelf());
}

class MediaTrayNotPinnedParamTest : public AshTestBase {
 public:
  MediaTrayNotPinnedParamTest() = default;
  ~MediaTrayNotPinnedParamTest() override = default;

  void SetUp() override {
    auto& pin_param = media::kCrosGlobalMediaControlsPinParam;
    feature_list_.InitAndEnableFeatureWithParameters(
        media::kGlobalMediaControlsForChromeOS,
        {{pin_param.name,
          pin_param.GetName(
              media::kCrosGlobalMediaControlsPinOptions::kNotPin)}});
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MediaTrayNotPinnedParamTest, PinParamTest) {
  UpdateDisplay("2560x1440");
  EXPECT_FALSE(MediaTray::IsPinnedToShelf());
}

}  // namespace ash
