// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <string>
#include <vector>

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/scoped_observer.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/browser/ui/ash/assistant/test_support/test_util.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/notification_view_md.h"

namespace chromeos {
namespace assistant {

namespace {

using message_center::MessageCenter;
using message_center::MessageCenterObserver;

// Please remember to set auth token when *not* running in |kReplay| mode.
constexpr auto kMode = FakeS3Mode::kReplay;

// Update this when you introduce breaking changes to existing tests.
constexpr int kVersion = 1;

// Macros ----------------------------------------------------------------------

#define EXPECT_VISIBLE_NOTIFICATIONS_BY_PREFIXED_ID(prefix_)                  \
  {                                                                           \
    if (!FindVisibleNotificationsByPrefixedId(prefix_).empty())               \
      return;                                                                 \
                                                                              \
    MockMessageCenterObserver mock;                                           \
    ScopedObserver<MessageCenter, MessageCenterObserver> observer_{&mock};    \
    observer_.Add(MessageCenter::Get());                                      \
                                                                              \
    base::RunLoop run_loop;                                                   \
    EXPECT_CALL(mock, OnNotificationAdded)                                    \
        .WillOnce(                                                            \
            testing::Invoke([&run_loop](const std::string& notification_id) { \
              if (!FindVisibleNotificationsByPrefixedId(prefix_).empty())     \
                run_loop.QuitClosure().Run();                                 \
            }));                                                              \
    run_loop.Run();                                                           \
  }

// Helpers ---------------------------------------------------------------------

// Returns the status area widget.
ash::StatusAreaWidget* FindStatusAreaWidget() {
  return ash::Shelf::ForWindow(ash::Shell::GetRootWindowForNewWindows())
      ->shelf_widget()
      ->status_area_widget();
}

// Returns the set of Assistant notifications (as indicated by application id).
message_center::NotificationList::Notifications FindAssistantNotifications() {
  return MessageCenter::Get()->FindNotificationsByAppId("assistant");
}

// Returns the visible notification specified by |id|.
message_center::Notification* FindVisibleNotificationById(
    const std::string& id) {
  return MessageCenter::Get()->FindVisibleNotificationById(id);
}

// Returns visible notifications having id starting with |prefix|.
std::vector<message_center::Notification*> FindVisibleNotificationsByPrefixedId(
    const std::string& prefix) {
  std::vector<message_center::Notification*> notifications;
  for (auto* notification : MessageCenter::Get()->GetVisibleNotifications()) {
    if (base::StartsWith(notification->id(), prefix,
                         base::CompareCase::SENSITIVE)) {
      notifications.push_back(notification);
    }
  }
  return notifications;
}

// Returns the view for the specified |notification|.
message_center::MessageView* FindViewForNotification(
    const message_center::Notification* notification) {
  ash::UnifiedMessageCenterView* unified_message_center_view =
      FindStatusAreaWidget()
          ->unified_system_tray()
          ->message_center_bubble()
          ->message_center_view();

  std::vector<message_center::MessageView*> message_views;
  FindDescendentsOfClass(unified_message_center_view, &message_views);

  for (message_center::MessageView* message_view : message_views) {
    if (message_view->notification_id() == notification->id())
      return message_view;
  }

  return nullptr;
}

// Returns the action buttons for the specified |notification|.
std::vector<message_center::NotificationMdTextButton*>
FindActionButtonsForNotification(
    const message_center::Notification* notification) {
  auto* notification_view = FindViewForNotification(notification);

  std::vector<message_center::NotificationMdTextButton*> action_buttons;
  FindDescendentsOfClass(notification_view, &action_buttons);

  return action_buttons;
}

// Returns the label for the specified |notification| title.
// NOTE: This method assumes that the title string is unique from other strings
// displayed in the notification. This should be safe since we only use this API
// under controlled circumstances.
views::Label* FindTitleLabelForNotification(
    const message_center::Notification* notification) {
  std::vector<views::Label*> labels;
  FindDescendentsOfClass(FindViewForNotification(notification), &labels);
  for (auto* label : labels) {
    if (label->GetText() == notification->title())
      return label;
  }
  return nullptr;
}

// Performs a tap of the specified |view| and waits until the RunLoop idles.
void TapOnAndWait(const views::View* view) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveTouch(view->GetBoundsInScreen().CenterPoint());
  event_generator.PressTouch();
  event_generator.ReleaseTouch();
  base::RunLoop().RunUntilIdle();
}

// Performs a tap of the specified |widget| and waits until the RunLoop idles.
void TapOnAndWait(const views::Widget* widget) {
  aura::Window* root_window = widget->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveTouch(widget->GetWindowBoundsInScreen().CenterPoint());
  event_generator.PressTouch();
  event_generator.ReleaseTouch();
  base::RunLoop().RunUntilIdle();
}

// Mocks -----------------------------------------------------------------------

class MockMessageCenterObserver
    : public testing::NiceMock<MessageCenterObserver> {
 public:
  // MessageCenterObserver:
  MOCK_METHOD(void,
              OnNotificationAdded,
              (const std::string& notification_id),
              (override));

  MOCK_METHOD(void,
              OnNotificationUpdated,
              (const std::string& notification_id),
              (override));
};

}  // namespace

// AssistantTimersBrowserTest --------------------------------------------------

class AssistantTimersBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  AssistantTimersBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kAssistantTimersV2);
  }

  AssistantTimersBrowserTest(const AssistantTimersBrowserTest&) = delete;
  AssistantTimersBrowserTest& operator=(const AssistantTimersBrowserTest&) =
      delete;

  ~AssistantTimersBrowserTest() override = default;

  void ShowAssistantUi() {
    if (!tester()->IsVisible())
      tester()->PressAssistantKey();
  }

  AssistantTestMixin* tester() { return &tester_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedRestoreICUDefaultLocale locale_{"en_US"};
  AssistantTestMixin tester_{&mixin_host_, this, embedded_test_server(), kMode,
                             kVersion};
};

// Tests -----------------------------------------------------------------------

// Timer notifications should be dismissed when disabling Assistant in settings.
IN_PROC_BROWSER_TEST_F(AssistantTimersBrowserTest,
                       ShouldDismissTimerNotificationsWhenDisablingAssistant) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  // Confirm no Assistant notifications are currently being shown.
  EXPECT_TRUE(FindAssistantNotifications().empty());

  // Start a timer for one minute.
  tester()->SendTextQuery("Set a timer for 1 minute.");

  // Check for a stable substring of the expected answers.
  tester()->ExpectTextResponse("1 min.");

  // Expect that an Assistant timer notification is now showing.
  EXPECT_VISIBLE_NOTIFICATIONS_BY_PREFIXED_ID("assistant/timer");

  // Disable Assistant.
  tester()->SetAssistantEnabled(false);
  base::RunLoop().RunUntilIdle();

  // Confirm that our Assistant timer notification has been dismissed.
  EXPECT_TRUE(FindAssistantNotifications().empty());
}

// Pressing the "STOP" action button in a timer notification should result in
// the timer being removed.
IN_PROC_BROWSER_TEST_F(AssistantTimersBrowserTest,
                       ShouldRemoveTimerWhenStoppingViaNotification) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  // Confirm no Assistant notifications are currently being shown.
  EXPECT_TRUE(FindAssistantNotifications().empty());

  // Start a timer for five minutes.
  tester()->SendTextQuery("Set a timer for 5 minutes");
  tester()->ExpectAnyOfTheseTextResponses({
      "Alright, 5 min. Starting… now.",
      "OK, 5 min. And we're starting… now.",
      "OK, 5 min. Starting… now.",
      "Sure, 5 min. And that's starting… now.",
      "Sure, 5 min. Starting now.",
  });

  // Tap status area widget (to show notifications in the Message Center).
  TapOnAndWait(FindStatusAreaWidget());

  // Confirm that an Assistant timer notification is now showing.
  auto notifications = FindVisibleNotificationsByPrefixedId("assistant/timer");
  ASSERT_EQ(1u, notifications.size());

  // Find the action buttons for our notification.
  // NOTE: We expect action buttons for "STOP" and "ADD 1 MIN".
  auto action_buttons = FindActionButtonsForNotification(notifications.at(0));
  EXPECT_EQ(2u, action_buttons.size());

  // Tap the "CANCEL" action button in the notification.
  EXPECT_EQ(base::UTF8ToUTF16("CANCEL"), action_buttons.at(1)->GetText());
  TapOnAndWait(action_buttons.at(1));

  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  // Confirm that no timers exist anymore.
  tester()->SendTextQuery("Show my timers");
  tester()->ExpectAnyOfTheseTextResponses({
      "It looks like you don't have any timers set at the moment.",
  });
}

// Verifies that timer notifications are ticked at regular intervals.
IN_PROC_BROWSER_TEST_F(AssistantTimersBrowserTest,
                       ShouldTickNotificationsAtRegularIntervals) {
  // Observe notifications.
  MockMessageCenterObserver mock;
  ScopedObserver<MessageCenter, MessageCenterObserver> scoped_observer{&mock};
  scoped_observer.Add(MessageCenter::Get());

  // Show Assistant UI (once ready).
  tester()->StartAssistantAndWaitForReady();
  ShowAssistantUi();
  EXPECT_TRUE(tester()->IsVisible());

  // Start a timer for five seconds.
  tester()->SendTextQuery("Set a timer for 5 seconds");

  // We're going to cache the time of the last notification update so that we
  // can verify updates occur within an expected time frame.
  base::Time last_update;

  // Expect and wait for our five second timer notification to be created.
  base::RunLoop notification_add_run_loop;
  EXPECT_CALL(mock, OnNotificationAdded)
      .WillRepeatedly(testing::Invoke([&](const std::string& notification_id) {
        last_update = base::Time::Now();

        // Tap status area widget (to show notifications in the Message Center).
        TapOnAndWait(FindStatusAreaWidget());

        // Assert that the notification has the expected title.
        auto* notification = FindVisibleNotificationById(notification_id);
        auto* title_label = FindTitleLabelForNotification(notification);
        auto title = base::UTF16ToUTF8(title_label->GetText());
        EXPECT_EQ("0:05", title);

        // Allow test to proceed.
        notification_add_run_loop.QuitClosure().Run();
      }));
  notification_add_run_loop.Run();

  // We are going to assert that updates to our notification occur within an
  // expected time frame, allowing a degree of tolerance to reduce flakiness.
  constexpr auto kExpectedMillisBetweenUpdates = 1000;
  constexpr auto kMillisBetweenUpdatesTolerance = 100;

  // We're going to watch notification updates until 5 seconds past fire time.
  std::deque<std::string> expected_titles = {"0:04",  "0:03",  "0:02",  "0:01",
                                             "0:00",  "-0:01", "-0:02", "-0:03",
                                             "-0:04", "-0:05"};
  bool is_first_update = true;

  auto* title_label =
      FindTitleLabelForNotification(*FindAssistantNotifications().begin());

  // Watch |title_label| and await all expected notification updates.
  base::RunLoop notification_update_run_loop;
  auto notification_update_subscription =
      title_label->AddTextChangedCallback(base::BindLambdaForTesting([&]() {
        base::Time now = base::Time::Now();

        // Assert that the update was received within our expected time frame.
        if (is_first_update) {
          is_first_update = false;
          // Our updates are synced to the nearest full second, meaning our
          // first update can come anywhere from 1 ms to 1000 ms from the time
          // our notification was shown.
          EXPECT_LE((now - last_update).InMilliseconds(),
                    1000 + kMillisBetweenUpdatesTolerance);
        } else {
          // Consecutive updates must come regularly.
          EXPECT_NEAR((now - last_update).InMilliseconds(),
                      kExpectedMillisBetweenUpdates,
                      kMillisBetweenUpdatesTolerance);
        }

        // Assert that the notification has the expected title.
        auto title = base::UTF16ToUTF8(title_label->GetText());
        EXPECT_EQ(expected_titles.front(), title);

        // Update time of |last_update|.
        last_update = now;

        // When |expected_titles| is empty, our test is finished.
        expected_titles.pop_front();
        if (expected_titles.empty())
          notification_update_run_loop.QuitClosure().Run();
      }));
  notification_update_run_loop.Run();
}

}  // namespace assistant
}  // namespace chromeos
