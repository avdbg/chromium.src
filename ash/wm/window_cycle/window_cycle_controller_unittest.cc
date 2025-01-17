// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_controller.h"

#include <algorithm>
#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/focus_cycler.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/frame_throttler/mock_frame_throttling_observer.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/multi_user_window_manager_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/gestures/wm_gesture_handler.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_cycle/window_cycle_event_filter.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

constexpr int kNumFingersForMouseWheel = 2;
constexpr int kNumFingersForTrackpad = 3;

class EventCounter : public ui::EventHandler {
 public:
  EventCounter() : key_events_(0), mouse_events_(0) {}
  ~EventCounter() override = default;

  int GetKeyEventCountAndReset() {
    int count = key_events_;
    key_events_ = 0;
    return count;
  }

  int GetMouseEventCountAndReset() {
    int count = mouse_events_;
    mouse_events_ = 0;
    return count;
  }

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override { key_events_++; }
  void OnMouseEvent(ui::MouseEvent* event) override { mouse_events_++; }

 private:
  int key_events_;
  int mouse_events_;

  DISALLOW_COPY_AND_ASSIGN(EventCounter);
};

bool IsWindowMinimized(aura::Window* window) {
  return WindowState::Get(window)->IsMinimized();
}

bool InOverviewSession() {
  return Shell::Get()->overview_controller()->InOverviewSession();
}

const aura::Window* GetHighlightedWindow() {
  return InOverviewSession() ? GetOverviewHighlightedWindow() : nullptr;
}

bool IsNaturalScrollOn() {
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref->GetBoolean(prefs::kTouchpadEnabled) &&
         pref->GetBoolean(prefs::kNaturalScroll);
}

int GetOffsetX(int offset) {
  // The handler code uses the new directions which is the reverse of the old
  // handler code. Reverse the offset if the ReverseScrollGestures feature is
  // disabled so that the unit tests test the old behavior.
  return features::IsReverseScrollGesturesEnabled() ? offset : -offset;
}

int GetOffsetY(int offset) {
  // The handler code uses the new directions which is the reverse of the old
  // handler code. Reverse the offset if the ReverseScrollGestures feature is
  // disabled so that the unit tests test the old behavior.
  if (!features::IsReverseScrollGesturesEnabled() || IsNaturalScrollOn())
    return -offset;
  return offset;
}

}  // namespace

using aura::Window;
using aura::test::CreateTestWindowWithId;
using aura::test::TestWindowDelegate;

class WindowCycleControllerTest : public AshTestBase {
 public:
  WindowCycleControllerTest() = default;
  ~WindowCycleControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    WindowCycleList::DisableInitialDelayForTesting();

    shelf_view_test_.reset(
        new ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting()));
    shelf_view_test_->SetAnimationDuration(
        base::TimeDelta::FromMilliseconds(1));
  }

  const aura::Window::Windows GetWindows(WindowCycleController* controller) {
    return controller->window_cycle_list()->windows();
  }

  const views::Widget* GetWindowCycleListWidget() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->widget();
  }

  const views::View::Views& GetWindowCycleItemViews() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->GetWindowCycleItemViewsForTesting();
  }

  const views::View::Views& GetWindowCycleTabSliderButtons() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->GetWindowCycleTabSliderButtonsForTesting();
  }

  const views::Label* GetWindowCycleNoRecentItemsLabel() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->GetWindowCycleNoRecentItemsLabelForTesting();
  }

  const aura::Window* GetTargetWindow() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->GetTargetWindowForTesting();
  }

  bool CycleViewExists() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->cycle_view_for_testing();
  }

  int GetCurrentIndex() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->current_index_for_testing();
  }

  void CompleteCycling(WindowCycleController* controller) {
    controller->CompleteCycling();
    base::RunLoop().RunUntilIdle();
  }

  void CompleteCyclingAndDeskSwitching(WindowCycleController* controller) {
    DeskSwitchAnimationWaiter waiter;
    controller->CompleteCycling();
    base::RunLoop().RunUntilIdle();
    auto* desks_controller = Shell::Get()->desks_controller();
    if (desks_controller->AreDesksBeingModified())
      waiter.Wait();
  }

 private:
  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleControllerTest);
};

TEST_F(WindowCycleControllerTest, HandleCycleWindowBaseCases) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Cycling doesn't crash if there are no windows.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);

  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycling works for a single window, even though nothing changes.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

// Verifies if there is only one window and it isn't active that cycling
// activates it.
TEST_F(WindowCycleControllerTest, SingleWindowNotActive) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Rotate focus, this should move focus to another window that isn't part of
  // the default container.
  Shell::Get()->focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));

  // Cycling should activate the window.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(WindowCycleControllerTest, HandleCycleWindow) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Set up several windows to use to test cycling.  Create them in reverse
  // order so they are stacked 0 over 1 over 2.
  std::unique_ptr<Window> window2(CreateTestWindowInShellWithId(2));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(3u, GetWindows(controller).size());
  ASSERT_EQ(window0.get(), GetWindows(controller)[0]);
  ASSERT_EQ(window1.get(), GetWindows(controller)[1]);
  ASSERT_EQ(window2.get(), GetWindows(controller)[2]);

  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Pressing and releasing Alt-tab again should cycle back to the most-
  // recently-used window in the current child order.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cancelled cycling shouldn't move the active window.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  controller->CancelCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Pressing Alt-tab multiple times without releasing Alt should cycle through
  // all the windows and wrap around.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_TRUE(controller->IsCycling());

  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_TRUE(controller->IsCycling());

  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_TRUE(controller->IsCycling());

  CompleteCycling(controller);
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Reset our stacking order.
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());

  // Likewise we can cycle backwards through the windows.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kBackward);
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kBackward);
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Reset our stacking order.
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());

  // When the screen is locked, cycling window does not take effect.
  GetSessionControllerClient()->LockScreen();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_FALSE(controller->IsCycling());

  // Unlock, it works again.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // When a modal window is active, cycling window does not take effect.
  aura::Window* modal_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_SystemModalContainer);
  std::unique_ptr<Window> modal_window(
      CreateTestWindowWithId(-2, modal_container));
  modal_window->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
  wm::ActivateWindow(modal_window.get());
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kBackward);
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));

  modal_window.reset();
  std::unique_ptr<Window> skip_overview_window(
      CreateTestWindowInShellWithId(-3));
  skip_overview_window->SetProperty(kHideInOverviewKey, true);
  wm::ActivateWindow(window0.get());
  wm::ActivateWindow(skip_overview_window.get());
  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(skip_overview_window.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
}

TEST_F(WindowCycleControllerTest, Scroll) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Doesn't crash if there are no windows.
  controller->Scroll(WindowCycleController::WindowCyclingDirection::kForward);

  // Create test windows.
  std::unique_ptr<Window> w5 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  std::unique_ptr<Window> w4 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  std::unique_ptr<Window> w3 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  std::unique_ptr<Window> w2 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  std::unique_ptr<Window> w1 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  std::unique_ptr<Window> w0 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));

  auto ScrollAndReturnCurrentIndex =
      [this](WindowCycleController::WindowCyclingDirection direction,
             int num_of_scrolls) {
        WindowCycleController* controller =
            Shell::Get()->window_cycle_controller();
        for (int i = 0; i < num_of_scrolls; i++)
          controller->Scroll(direction);

        return GetCurrentIndex();
      };

  auto GetXOfCycleListCenterPoint = [this]() {
    return GetWindowCycleListWidget()
        ->GetWindowBoundsInScreen()
        .CenterPoint()
        .x();
  };

  auto GetXOfWindowCycleItemViewCenterPoint = [this](int index) {
    return GetWindowCycleItemViews()[index]
        ->GetBoundsInScreen()
        .CenterPoint()
        .x();
  };

  // Start cycling and scroll forward. The list should be not be centered around
  // w1. Since w1 is so close to the beginning of the list.
  controller->StartCycling();
  int current_index = ScrollAndReturnCurrentIndex(
      WindowCycleController::WindowCyclingDirection::kForward, 1);
  EXPECT_EQ(1, current_index);
  EXPECT_GT(GetXOfCycleListCenterPoint(),
            GetXOfWindowCycleItemViewCenterPoint(current_index));

  // Scroll forward twice. The list should be centered around w3.
  current_index = ScrollAndReturnCurrentIndex(
      WindowCycleController::WindowCyclingDirection::kForward, 2);
  EXPECT_EQ(3, current_index);
  EXPECT_EQ(GetXOfCycleListCenterPoint(),
            GetXOfWindowCycleItemViewCenterPoint(current_index));

  // Scroll backward once. The list should be centered around w2.
  current_index = ScrollAndReturnCurrentIndex(
      WindowCycleController::WindowCyclingDirection::kBackward, 1);
  EXPECT_EQ(2, current_index);
  EXPECT_EQ(GetXOfCycleListCenterPoint(),
            GetXOfWindowCycleItemViewCenterPoint(current_index));

  // Scroll backward three times. The list should not be centered around w5.
  current_index = ScrollAndReturnCurrentIndex(
      WindowCycleController::WindowCyclingDirection::kBackward, 3);
  EXPECT_EQ(5, current_index);
  EXPECT_LT(GetXOfCycleListCenterPoint(),
            GetXOfWindowCycleItemViewCenterPoint(current_index));

  // Cycle forward. Since the target window != current window, it should scroll
  // to target window then cycle. The target_window was w0 prior to cycling.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  current_index = GetCurrentIndex();
  EXPECT_EQ(1, current_index);
  EXPECT_GT(GetXOfCycleListCenterPoint(),
            GetXOfWindowCycleItemViewCenterPoint(current_index));
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Start cycling, scroll backward once and complete cycling. Scroll should not
  // affect the selected window.
  controller->StartCycling();
  current_index = ScrollAndReturnCurrentIndex(
      WindowCycleController::WindowCyclingDirection::kBackward, 1);
  EXPECT_EQ(5, current_index);
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// Cycles between a maximized and normal window.
TEST_F(WindowCycleControllerTest, MaximizedWindow) {
  // Create a couple of test windows.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  WindowState* window1_state = WindowState::Get(window1.get());
  window1_state->Maximize();
  window1_state->Activate();
  EXPECT_TRUE(window1_state->IsActive());

  // Rotate focus, this should move focus to window0.
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  CompleteCycling(controller);
  EXPECT_TRUE(WindowState::Get(window0.get())->IsActive());
  EXPECT_FALSE(window1_state->IsActive());

  // One more time.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  CompleteCycling(controller);
  EXPECT_TRUE(window1_state->IsActive());
}

// Cycles to a minimized window.
TEST_F(WindowCycleControllerTest, Minimized) {
  // Create a couple of test windows.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  WindowState* window0_state = WindowState::Get(window0.get());
  WindowState* window1_state = WindowState::Get(window1.get());

  window1_state->Minimize();
  window0_state->Activate();
  EXPECT_TRUE(window0_state->IsActive());

  // Rotate focus, this should move focus to window1 and unminimize it.
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  CompleteCycling(controller);
  EXPECT_FALSE(window0_state->IsActive());
  EXPECT_FALSE(window1_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsActive());

  // One more time back to w0.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  CompleteCycling(controller);
  EXPECT_TRUE(window0_state->IsActive());
}

// Tests that when all windows are minimized, cycling starts with the first one
// rather than the second.
TEST_F(WindowCycleControllerTest, AllAreMinimized) {
  // Create a couple of test windows.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  WindowState* window0_state = WindowState::Get(window0.get());
  WindowState* window1_state = WindowState::Get(window1.get());

  window0_state->Minimize();
  window1_state->Minimize();

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  CompleteCycling(controller);
  EXPECT_TRUE(window0_state->IsActive());
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // But it's business as usual when cycling backwards.
  window0_state->Minimize();
  window1_state->Minimize();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kBackward);
  CompleteCycling(controller);
  EXPECT_TRUE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsActive());
  EXPECT_FALSE(window1_state->IsMinimized());
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopWindow) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  Window* top_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window2(CreateTestWindowWithId(2, top_container));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(3u, GetWindows(controller).size());
  EXPECT_EQ(window0.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window2.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[2]);

  CompleteCycling(controller);
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopMultiWindow) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  Window* top_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window2(CreateTestWindowWithId(2, top_container));
  std::unique_ptr<Window> window3(CreateTestWindowWithId(3, top_container));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(4u, GetWindows(controller).size());
  EXPECT_EQ(window0.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window3.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window2.get(), GetWindows(controller)[2]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[3]);

  CompleteCycling(controller);
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopMultipleRootWindows) {
  // Set up a second root window
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Create two windows in the primary root.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  EXPECT_EQ(root_windows[0], window0->GetRootWindow());
  Window* top_container0 =
      Shell::GetContainer(root_windows[0], kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window1(CreateTestWindowWithId(1, top_container0));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());

  // Move the active root window to the secondary root and create two windows.
  display::ScopedDisplayForNewWindows display_for_new_windows(root_windows[1]);
  std::unique_ptr<Window> window2(CreateTestWindowInShellWithId(2));
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  Window* top_container1 =
      Shell::GetContainer(root_windows[1], kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window3(CreateTestWindowWithId(3, top_container1));
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());

  wm::ActivateWindow(window2.get());

  EXPECT_EQ(root_windows[0], window0->GetRootWindow());
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(4u, GetWindows(controller).size());
  EXPECT_EQ(window2.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window3.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[2]);
  EXPECT_EQ(window0.get(), GetWindows(controller)[3]);

  CompleteCycling(controller);
}

TEST_F(WindowCycleControllerTest, MostRecentlyUsed) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<Window> window2(CreateTestWindowInShellWithId(2));

  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(3u, GetWindows(controller).size());
  EXPECT_EQ(window0.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window2.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[2]);

  // Cycling through then stopping the cycling will activate a window.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Cycling alone (without CompleteCycling()) doesn't activate.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));

  CompleteCycling(controller);
}

// Tests that beginning window selection hides the app list.
TEST_F(WindowCycleControllerTest, SelectingHidesAppList) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window0.get());

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Make sure that dismissing the app list this way doesn't pass activation
  // to a different window.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));

  CompleteCycling(controller);
}

// Tests that beginning window selection doesn't hide the app list in tablet
// mode.
TEST_F(WindowCycleControllerTest, SelectingDoesNotHideAppListInTabletMode) {
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(TabletModeControllerTestApi().IsTabletModeStarted());
  EXPECT_TRUE(Shell::Get()->home_screen_controller()->IsHomeScreenVisible());

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window0.get());

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);

  window0->Hide();
  window1->Hide();
  EXPECT_TRUE(Shell::Get()->home_screen_controller()->IsHomeScreenVisible());
}

// Tests that cycling through windows doesn't change their minimized state.
TEST_F(WindowCycleControllerTest, CyclePreservesMinimization) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window1.get());
  WindowState::Get(window1.get())->Minimize();
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(IsWindowMinimized(window1.get()));

  // On window 2.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_TRUE(IsWindowMinimized(window1.get()));

  // Back on window 1.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_TRUE(IsWindowMinimized(window1.get()));

  CompleteCycling(controller);

  EXPECT_TRUE(IsWindowMinimized(window1.get()));
}

// Tests that the tab key events are not sent to the window.
TEST_F(WindowCycleControllerTest, TabKeyNotLeaked) {
  std::unique_ptr<Window> w0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> w1(CreateTestWindowInShellWithId(1));
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  w1->AddPreTargetHandler(&event_count);
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowState::Get(w0.get())->Activate();
  generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
  EXPECT_EQ(1, event_count.GetKeyEventCountAndReset());
  generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_EQ(0, event_count.GetKeyEventCountAndReset());
  generator->ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_EQ(0, event_count.GetKeyEventCountAndReset());
  generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  EXPECT_TRUE(WindowState::Get(w1.get())->IsActive());
  EXPECT_EQ(0, event_count.GetKeyEventCountAndReset());
}

// While the UI is active, mouse events are captured.
TEST_F(WindowCycleControllerTest, MouseEventsCaptured) {
  if (features::IsInteractiveWindowCycleListEnabled())
    return;

  // Set up a second root window
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  // This delegate allows the window to receive mouse events.
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<Window> w0(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<Window> w1(CreateTestWindowInShellWithId(1));
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  w1->SetTargetHandler(&event_count);
  ui::test::EventGenerator* generator = GetEventGenerator();
  wm::ActivateWindow(w0.get());

  // Events get through while not cycling.
  generator->MoveMouseToCenterOf(w0.get());
  generator->ClickLeftButton();
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());

  // Start cycling.
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);

  // Mouse events not over the cycle view don't get through.
  generator->PressLeftButton();
  EXPECT_EQ(0, event_count.GetMouseEventCountAndReset());

  // Although releases do, regardless of mouse position.
  generator->ReleaseLeftButton();
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());

  // Stop cycling: once again, events get through.
  CompleteCycling(controller);
  generator->ClickLeftButton();
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());

  // Click somewhere on the second root window.
  generator->MoveMouseToCenterOf(root_windows[1]);
  generator->ClickLeftButton();
  EXPECT_EQ(0, event_count.GetMouseEventCountAndReset());
}

// Tests that we can cycle past fullscreen windows: https://crbug.com/622396.
// Fullscreen windows are special in that they are allowed to handle alt+tab
// keypresses, which means the window cycle event filter should not handle
// the tab press else it prevents cycling past that window.
TEST_F(WindowCycleControllerTest, TabPastFullscreenWindow) {
  std::unique_ptr<Window> w0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> w1(CreateTestWindowInShellWithId(1));
  WMEvent maximize_event(WM_EVENT_FULLSCREEN);

  // To make this test work with or without the new alt+tab selector we make
  // both the initial window and the second window fullscreen.
  WindowState::Get(w0.get())->OnWMEvent(&maximize_event);
  WindowState::Get(w1.get())->Activate();
  WindowState::Get(w1.get())->OnWMEvent(&maximize_event);
  EXPECT_TRUE(WindowState::Get(w0.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(w1.get())->IsFullscreen());
  WindowState::Get(w0.get())->Activate();
  EXPECT_TRUE(WindowState::Get(w0.get())->IsActive());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);

  generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  generator->ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);

  // Because w0 and w1 are full-screen, the event should be passed to the
  // browser window to handle it (which if the browser doesn't handle it will
  // pass on the alt+tab to continue cycling). To make this test work with or
  // without the new alt+tab selector we check for the event on either
  // fullscreen window.
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  w1->AddPreTargetHandler(&event_count);
  generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_EQ(1, event_count.GetKeyEventCountAndReset());
}

// Tests that the Alt+Tab UI's position isn't affected by the origin of the
// display it's on. See crbug.com/675718
TEST_F(WindowCycleControllerTest, MultiDisplayPositioning) {
  int64_t primary_id = GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdListN(primary_id, 2);

  auto placements = {
      display::DisplayPlacement::BOTTOM,
      display::DisplayPlacement::TOP,
      display::DisplayPlacement::LEFT,
      display::DisplayPlacement::RIGHT,
  };

  gfx::Rect expected_bounds;
  for (auto placement : placements) {
    SCOPED_TRACE(placement);

    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], primary_id, placement, 0);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    // Use two displays.
    UpdateDisplay("500x500,600x600");

    gfx::Rect second_display_bounds =
        display_manager()->GetDisplayAt(1).bounds();
    std::unique_ptr<Window> window0(
        CreateTestWindowInShellWithBounds(second_display_bounds));
    // Activate this window so that the secondary display becomes the one where
    // the Alt+Tab UI is shown.
    wm::ActivateWindow(window0.get());
    std::unique_ptr<Window> window1(
        CreateTestWindowInShellWithBounds(second_display_bounds));

    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->HandleCycleWindow(
        WindowCycleController::WindowCyclingDirection::kForward);

    const gfx::Rect bounds =
        GetWindowCycleListWidget()->GetWindowBoundsInScreen();
    EXPECT_TRUE(second_display_bounds.Contains(bounds));
    EXPECT_FALSE(
        display_manager()->GetDisplayAt(0).bounds().Intersects(bounds));
    const gfx::Rect display_relative_bounds =
        bounds - second_display_bounds.OffsetFromOrigin();
    // Base case sets the expectation for other cases.
    if (expected_bounds.IsEmpty())
      expected_bounds = display_relative_bounds;
    else
      EXPECT_EQ(expected_bounds, display_relative_bounds);
    CompleteCycling(controller);
  }
}

TEST_F(WindowCycleControllerTest, CycleShowsAllDesksWindows) {
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(3u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  const Desk* desk_3 = desks_controller->desks()[2].get();
  ActivateDesk(desk_3);
  EXPECT_EQ(desk_3, desks_controller->active_desk());
  auto win3 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  // All desks' windows are included in the cycle list.
  auto cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(4u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win0.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win1.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win2.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win3.get()));

  // The MRU order is {win3, win2, win1, win0}. We're now at win2. Cycling one
  // more time and completing the cycle, will activate win1 which exists on a
  // desk_1. This should activate desk_1.
  {
    base::HistogramTester histogram_tester;
    cycle_controller->HandleCycleWindow(
        WindowCycleController::WindowCyclingDirection::kForward);
    CompleteCyclingAndDeskSwitching(cycle_controller);
    Desk* desk_1 = desks_controller->desks()[0].get();
    EXPECT_EQ(desk_1, desks_controller->active_desk());
    EXPECT_EQ(win1.get(), window_util::GetActiveWindow());
    histogram_tester.ExpectUniqueSample(
        "Ash.WindowCycleController.DesksSwitchDistance",
        /*desk distance of 3 - 1 = */ 2, /*expected_count=*/1);
  }

  // Cycle again and activate win2, which exist on desk_2. Expect that desk to
  // be activated, and a histogram sample of distance of 1 is recorded.
  // MRU is {win1, win3, win2, win0}.
  {
    base::HistogramTester histogram_tester;
    cycle_controller->HandleCycleWindow(
        WindowCycleController::WindowCyclingDirection::kForward);
    cycle_controller->HandleCycleWindow(
        WindowCycleController::WindowCyclingDirection::kForward);
    CompleteCyclingAndDeskSwitching(cycle_controller);
    EXPECT_EQ(desk_2, desks_controller->active_desk());
    EXPECT_EQ(win2.get(), window_util::GetActiveWindow());
    histogram_tester.ExpectUniqueSample(
        "Ash.WindowCycleController.DesksSwitchDistance",
        /*desk distance of 2 - 1 = */ 1, /*expected_count=*/1);
  }
}

// Tests that frame throttling starts and ends accordingly when window cycling
// starts and ends.
TEST_F(WindowCycleControllerTest, FrameThrottling) {
  MockFrameThrottlingObserver observer;
  FrameThrottlingController* frame_throttling_controller =
      Shell::Get()->frame_throttling_controller();
  uint8_t throttled_fps = frame_throttling_controller->throttled_fps();
  frame_throttling_controller->AddObserver(&observer);
  const int window_count = 5;
  std::unique_ptr<aura::Window> created_windows[window_count];
  std::vector<aura::Window*> windows(window_count, nullptr);
  for (int i = 0; i < window_count; ++i) {
    created_windows[i] = CreateAppWindow(gfx::Rect(), AppType::BROWSER);
    windows[i] = created_windows[i].get();
  }

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  EXPECT_CALL(observer,
              OnThrottlingStarted(testing::UnorderedElementsAreArray(windows),
                                  throttled_fps));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_CALL(observer,
              OnThrottlingStarted(testing::UnorderedElementsAreArray(windows),
                                  throttled_fps))
      .Times(0);
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_CALL(observer, OnThrottlingEnded());
  CompleteCycling(controller);

  EXPECT_CALL(observer,
              OnThrottlingStarted(testing::UnorderedElementsAreArray(windows),
                                  throttled_fps));
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_CALL(observer, OnThrottlingEnded());
  controller->CancelCycling();
  frame_throttling_controller->RemoveObserver(&observer);
}

// Tests that pressing Alt+Tab while there is an on-going desk animation
// prevents a new window cycle from starting.
TEST_F(WindowCycleControllerTest, DoubleAltTabWithDeskSwitch) {
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  auto win0 = CreateAppWindow(gfx::Rect(250, 100));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_0 = desks_controller->desks()[0].get();
  const Desk* desk_1 = desks_controller->desks()[1].get();
  ActivateDesk(desk_1);
  EXPECT_EQ(desk_1, desks_controller->active_desk());
  auto win1 = CreateAppWindow(gfx::Rect(300, 200));
  ASSERT_EQ(win1.get(), window_util::GetActiveWindow());
  auto desk_1_windows = desk_1->windows();
  EXPECT_EQ(1u, desk_1_windows.size());
  EXPECT_TRUE(base::Contains(desk_1_windows, win1.get()));

  DeskSwitchAnimationWaiter waiter;
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  cycle_controller->CompleteCycling();
  EXPECT_FALSE(cycle_controller->CanCycle());
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_FALSE(cycle_controller->IsCycling());
  waiter.Wait();
  EXPECT_EQ(desk_0, desks_controller->active_desk());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
}

// A regression test for crbug.com/1160676. Tests that the alt-key release
// to quit alt-tab is acknowledged by the accelerator controller.
TEST_F(WindowCycleControllerTest, AltKeyRelease) {
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  // Press Alt and start cycling.
  auto* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
  auto currently_pressed_keys = Shell::Get()
                                    ->accelerator_controller()
                                    ->accelerator_history()
                                    ->currently_pressed_keys();
  // Expect exactly one key pressed, which is Alt.
  EXPECT_EQ(1u, currently_pressed_keys.size());
  EXPECT_TRUE(base::Contains(currently_pressed_keys, ui::VKEY_MENU));

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);

  // Release Alt key to end alt-tab cycling and open up window0.
  generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(WindowState::Get(window0.get())->IsActive());

  // Expect all keys pressed to be released.
  currently_pressed_keys = Shell::Get()
                               ->accelerator_controller()
                               ->accelerator_history()
                               ->currently_pressed_keys();
  EXPECT_EQ(0u, currently_pressed_keys.size());
  EXPECT_FALSE(base::Contains(currently_pressed_keys, ui::VKEY_MENU));
}

class LimitedWindowCycleControllerTest : public WindowCycleControllerTest {
 public:
  LimitedWindowCycleControllerTest() = default;
  LimitedWindowCycleControllerTest(const LimitedWindowCycleControllerTest&) =
      delete;
  LimitedWindowCycleControllerTest& operator=(
      const LimitedWindowCycleControllerTest&) = delete;
  ~LimitedWindowCycleControllerTest() override = default;

  // WindowCycleControllerTest:
  void SetUp() override {
    // |features::kBento| overwrites |features::kLimitAltTabToActiveDesk|, so
    // Bento needs to be disabled first.
    scoped_feature_list_.InitWithFeatures({features::kLimitAltTabToActiveDesk},
                                          {features::kBento});
    WindowCycleControllerTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(LimitedWindowCycleControllerTest, CycleShowsActiveDeskWindows) {
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(3u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  const Desk* desk_3 = desks_controller->desks()[2].get();
  ActivateDesk(desk_3);
  EXPECT_EQ(desk_3, desks_controller->active_desk());
  auto win3 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Should contain only windows from |desk_3|.
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  auto cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(1u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win3.get()));
  CompleteCycling(cycle_controller);
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // Should contain only windows from |desk_2|.
  ActivateDesk(desk_2);
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(1u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win2.get()));
  CompleteCycling(cycle_controller);
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // Should contain only windows from |desk_1|.
  const Desk* desk_1 = desks_controller->desks()[0].get();
  ActivateDesk(desk_1);
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(2u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win0.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win1.get()));
  CompleteCycling(cycle_controller);
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  // Swap desks while cycling, contents should update.
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(2u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win0.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win1.get()));
  ActivateDesk(desk_2);
  EXPECT_TRUE(cycle_controller->IsCycling());
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(1u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win2.get()));
  CompleteCycling(cycle_controller);
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());
}

class InteractiveWindowCycleControllerTest : public WindowCycleControllerTest {
 public:
  InteractiveWindowCycleControllerTest() = default;
  InteractiveWindowCycleControllerTest(
      const InteractiveWindowCycleControllerTest&) = delete;
  InteractiveWindowCycleControllerTest& operator=(
      const InteractiveWindowCycleControllerTest&) = delete;
  ~InteractiveWindowCycleControllerTest() override = default;

  // WindowCycleControllerTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kInteractiveWindowCycleList);
    WindowCycleControllerTest::SetUp();
  }

  void Scroll(float x_offset, float y_offset, int fingers) {
    GetEventGenerator()->ScrollSequence(
        gfx::Point(), base::TimeDelta::FromMilliseconds(5),
        GetOffsetX(x_offset), GetOffsetY(y_offset), /*steps=*/100, fingers);
  }

  void MouseWheelScroll(int delta_x, int delta_y, int num_of_times) {
    auto* generator = GetEventGenerator();
    for (int i = 0; i < num_of_times; i++)
      generator->MoveMouseWheel(delta_x, delta_y);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that when the cycle view is not open, the event filter does not check
// whether events occur within the cycle view.
// TODO(chinsenj): Add this to WindowCycleControllerTest.MouseEventsCaptured
// after feature launch.
TEST_F(InteractiveWindowCycleControllerTest,
       MouseEventWhenCycleViewDoesNotExist) {
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<Window> w0(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 100, 100)));
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Mouse events get through if the cycle view is not open.
  // Cycling with one window open ensures the UI doesn't show but the event
  // filter is.
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  generator->MoveMouseToCenterOf(w0.get());
  generator->ClickLeftButton();
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_FALSE(CycleViewExists());
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());
  CompleteCycling(controller);
}

// When a user hovers their mouse over an item, it should cycle to it.
// The items in the list should not move, only the focus ring.
// If a user clicks on an item, it should complete cycling and activate
// the hovered item.
TEST_F(InteractiveWindowCycleControllerTest, MouseHoverAndSelect) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  std::unique_ptr<Window> w3 = CreateTestWindow();
  std::unique_ptr<Window> w4 = CreateTestWindow();
  std::unique_ptr<Window> w5 = CreateTestWindow();
  std::unique_ptr<Window> w6 = CreateTestWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Cycle to the third item, mouse over second item, and release alt-tab.
  // Starting order of windows in cycle list is [6,5,4,3,2,1,0].
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  gfx::Point target_item_center =
      GetWindowCycleItemViews()[1]->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(target_item_center);
  EXPECT_EQ(target_item_center,
            GetWindowCycleItemViews()[1]->GetBoundsInScreen().CenterPoint());
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(w5.get()));

  // Start cycle, mouse over third item, and release alt-tab.
  // Starting order of windows in cycle list is [5,6,4,3,2,1,0].
  controller->StartCycling();
  target_item_center =
      GetWindowCycleItemViews()[2]->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(target_item_center);
  EXPECT_EQ(target_item_center,
            GetWindowCycleItemViews()[2]->GetBoundsInScreen().CenterPoint());
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(w4.get()));

  // Start cycle, cycle to the fifth item, mouse over seventh item, and click.
  // Starting order of windows in cycle list is [4,5,6,3,2,1,0].
  controller->StartCycling();
  for (int i = 0; i < 5; i++)
    controller->HandleCycleWindow(
        WindowCycleController::WindowCyclingDirection::kForward);
  target_item_center =
      GetWindowCycleItemViews()[6]->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(target_item_center);
  EXPECT_EQ(target_item_center,
            GetWindowCycleItemViews()[6]->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));
}

// Tests that the left and right keys cycle after the cycle list has been
// initialized.
TEST_F(InteractiveWindowCycleControllerTest, LeftRightCycle) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Start cycle, simulating alt button being held down. Cycle right to the
  // third item.
  // Starting order of windows in cycle list is [2,1,0].
  controller->StartCycling();
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));

  // Start cycle. Cycle right once, then left two times.
  // Starting order of windows in cycle list is [0,2,1].
  controller->StartCycling();
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  generator->PressKey(ui::VKEY_LEFT, ui::EF_NONE);
  generator->PressKey(ui::VKEY_LEFT, ui::EF_NONE);
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Start cycle. Cycle right once, then left once, then right once.
  // Starting order of windows in cycle list is [0,2,1].
  controller->StartCycling();
  generator->PressKey(ui::VKEY_LEFT, ui::EF_ALT_DOWN);
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_ALT_DOWN);
  generator->PressKey(ui::VKEY_LEFT, ui::EF_ALT_DOWN);
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
}

// Tests that pressing the space key, pressing the enter key, or releasing the
// alt key during window cycle confirms a selection.
TEST_F(InteractiveWindowCycleControllerTest, KeysConfirmSelection) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Start cycle, simulating alt button being held down. Cycle right once and
  // complete cycle using space.
  // Starting order of windows in cycle list is [2,1,0].
  controller->StartCycling();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  generator->PressKey(ui::VKEY_SPACE, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Start cycle, simulating alt button being held down. Cycle right once and
  // complete cycle using enter.
  // Starting order of windows in cycle list is [1,2,0].
  controller->StartCycling();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  generator->PressKey(ui::VKEY_RETURN, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  // Start cycle, simulating alt button being held down. Cycle right once and
  // complete cycle by releasing alt key (Views uses VKEY_MENU for both left and
  // right alt keys).
  // Starting order of windows in cycle list is [2,1,0].
  controller->StartCycling();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// Tests that pressing the enter key or space key really quickly doesn't crash.
// See crbug.com/1187242.
TEST_F(InteractiveWindowCycleControllerTest, RapidConfirmSelection) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Start cycling and press space twice. This should not crash.
  controller->StartCycling();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  generator->PressKey(ui::VKEY_SPACE, ui::EF_NONE);
  generator->PressKey(ui::VKEY_SPACE, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Start cycling and press enter twice. This should not crash.
  controller->StartCycling();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  generator->PressKey(ui::VKEY_RETURN, ui::EF_NONE);
  generator->PressKey(ui::VKEY_RETURN, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  // Press down alt and tab. Release alt key and press enter. This should not
  // crash.
  generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_TRUE(controller->IsCycling());
  generator->ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  generator->PressKey(ui::VKEY_RETURN, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Start cycling and press enter once and then right key. This should not
  // crash and the right key should not affect the selection.
  controller->StartCycling();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  generator->PressKey(ui::VKEY_RETURN, ui::EF_NONE);
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
}

// Tests that mouse events are filtered until the mouse is actually used,
// preventing the mouse from unexpectedly triggering events.
// See crbug.com/1143275.
TEST_F(InteractiveWindowCycleControllerTest, FilterMouseEventsUntilUsed) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  EventCounter event_count;
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Start cycling.
  // Current window order is [2,1,0].
  controller->StartCycling();
  auto item_views = GetWindowCycleItemViews();
  item_views[2]->AddPreTargetHandler(&event_count);

  // Move the mouse over to the third item and complete cycling. These mouse
  // events shouldn't be filtered since the user has moved their mouse.
  generator->MoveMouseTo(gfx::Point(0, 0));
  const gfx::Point third_item_center =
      GetWindowCycleItemViews()[2]->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(third_item_center);
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());

  // Start cycling again while the mouse is over where the third item will be
  // when cycling starts.
  // Current window order is [0,2,1].
  controller->StartCycling();
  item_views = GetWindowCycleItemViews();
  item_views[2]->AddPreTargetHandler(&event_count);

  // Generate mouse events at the cursor's initial position. These mouse events
  // should be filtered because the user hasn't moved their mouse yet.
  generator->MoveMouseTo(third_item_center);
  CompleteCycling(controller);
  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));
  EXPECT_EQ(0, event_count.GetMouseEventCountAndReset());

  // Start cycling again and click. This should not be filtered out.
  // Current window order is [0,2,1].
  controller->StartCycling();
  generator->PressLeftButton();
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// When a user has the window cycle list open and clicks outside of it, it
// should cancel cycling.
TEST_F(InteractiveWindowCycleControllerTest,
       MousePressOutsideOfListCancelsCycling) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Cycle to second item, move to above the window cycle list, and click.
  controller->StartCycling();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  gfx::Point above_window_cycle_list =
      GetWindowCycleListWidget()->GetWindowBoundsInScreen().top_center();
  above_window_cycle_list.Offset(0, 100);
  generator->MoveMouseTo(above_window_cycle_list);
  generator->ClickLeftButton();
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// When the user has one window open, the window cycle view isn't shown. In this
// case we should not eat mouse events.
TEST_F(InteractiveWindowCycleControllerTest,
       MouseEventsNotEatenWhenCycleViewNotVisible) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Start cycling. Since there's only one window the cycle view shouldn't be
  // visible.
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  ASSERT_TRUE(controller->IsCycling());
  ASSERT_FALSE(controller->IsWindowListVisible());

  generator->MoveMouseToCenterOf(w0.get());
  generator->ClickLeftButton();
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());
}

// Tests three finger horizontal scroll gesture to move selection left or right.
TEST_F(InteractiveWindowCycleControllerTest,
       ThreeFingerHorizontalScrollInWindowCycleList) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  const float horizontal_scroll =
      WindowCycleEventFilter::kHorizontalThresholdDp;

  auto scroll_until_window_highlighted_and_confirm = [this](float x_offset,
                                                            float y_offset) {
    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->StartCycling();
    Scroll(GetOffsetX(x_offset), GetOffsetY(y_offset), kNumFingersForTrackpad);
    CompleteCycling(controller);
  };

  // Start cycle, simulating alt key being held down. Scroll right to fourth
  // item.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll * 3, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Start cycle. Scroll left to third item.
  // Current order is [2,5,4,3,1].
  scroll_until_window_highlighted_and_confirm(-horizontal_scroll * 3, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle. Scroll right to second item.
  // Current order is [4,2,5,3,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Open an overview session and window cycle list. Scroll right to second
  // item. Scroll should only go to the window cycle list.
  // Current order is [2,4,5,3,1].
  Shell::Get()->overview_controller()->StartOverview();
  EXPECT_TRUE(InOverviewSession());

  auto* cycle_controller = Shell::Get()->window_cycle_controller();
  cycle_controller->StartCycling();
  Scroll(GetOffsetX(horizontal_scroll), 0, kNumFingersForTrackpad);
  EXPECT_EQ(nullptr, GetHighlightedWindow());

  CompleteCycling(cycle_controller);
  EXPECT_FALSE(InOverviewSession());
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));
}

// Tests two finger horizontal scroll gesture to move selection left or right.
TEST_F(InteractiveWindowCycleControllerTest,
       TwoFingerHorizontalScrollInWindowCycleList) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  const float horizontal_scroll =
      WindowCycleEventFilter::kHorizontalThresholdDp;

  auto scroll_until_window_highlighted_and_confirm = [this](float x_offset,
                                                            float y_offset) {
    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->StartCycling();
    // Since two finger swipes are negated, negate in tests to mimic how this
    // actually behaves on devices.
    Scroll(GetOffsetX(-x_offset), GetOffsetY(y_offset),
           kNumFingersForMouseWheel);
    CompleteCycling(controller);
  };

  // Start cycle, simulating alt key being held down. Scroll right to fourth
  // item.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll * 3, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Start cycle. Scroll left to third item.
  // Current order is [2,5,4,3,1].
  scroll_until_window_highlighted_and_confirm(-horizontal_scroll * 3, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle. Scroll right to second item.
  // Current order is [4,2,5,3,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests mouse wheel scroll gesture to move selection left or right.
TEST_F(InteractiveWindowCycleControllerTest,
       MouseWheelScrollInWindowCycleList) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  const float horizontal_scroll =
      WindowCycleEventFilter::kHorizontalThresholdDp;

  auto scroll_until_window_highlighted_and_confirm = [this](float x_offset,
                                                            float y_offset,
                                                            int num_of_times) {
    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->StartCycling();
    MouseWheelScroll(x_offset, y_offset, num_of_times);
    CompleteCycling(controller);
  };

  // Start cycle, simulating alt key being held down. Scroll right to fourth
  // item.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(0, -horizontal_scroll, 3);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Start cycle. Scroll left to third item.
  // Current order is [2,5,4,3,1].
  scroll_until_window_highlighted_and_confirm(0, horizontal_scroll, 3);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle. Scroll right to second item.
  // Current order is [4,2,5,3,1].
  scroll_until_window_highlighted_and_confirm(0, -horizontal_scroll, 1);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that swiping up closes window cycle if it's open and starts overview
// mode.
TEST_F(InteractiveWindowCycleControllerTest, VerticalScroll) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  const float vertical_scroll = 2 * WmGestureHandler::kVerticalThresholdDp;
  const float horizontal_scroll =
      WindowCycleEventFilter::kHorizontalThresholdDp;
  auto* window_cycle_controller = Shell::Get()->window_cycle_controller();

  // Start cycling and then swipe up to open up overview.
  window_cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  Scroll(0, vertical_scroll, 3);
  EXPECT_TRUE(InOverviewSession());
  EXPECT_FALSE(window_cycle_controller->IsCycling());

  // Start cycling and then swipe down.
  window_cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  Scroll(0, -vertical_scroll, 3);
  EXPECT_TRUE(window_cycle_controller->IsCycling());

  // Swipe diagonally with horizontal bias.
  Scroll(horizontal_scroll * 3, vertical_scroll, 3);
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  EXPECT_FALSE(InOverviewSession());

  // Swipe diagonally with vertical bias.
  Scroll(horizontal_scroll, vertical_scroll, 3);
  EXPECT_FALSE(window_cycle_controller->IsCycling());
  EXPECT_TRUE(InOverviewSession());
}

// Tests that touch continuous scrolls for the window cycle list.
TEST_F(InteractiveWindowCycleControllerTest, TouchScroll) {
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  auto* shell = Shell::Get();
  auto* cycle_controller = shell->window_cycle_controller();
  auto* event_generator = GetEventGenerator();

  // Start cycling.
  cycle_controller->StartCycling();
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  ASSERT_TRUE(cycle_controller->IsCycling());
  ASSERT_EQ(window2.get(), GetTargetWindow());

  // There should be five preview items and the first three should be contained
  // by the screen. The fourth should be in the screen, but not contained. The
  // last one should not be in the screen at all.
  auto preview_items = GetWindowCycleItemViews();
  ASSERT_EQ(5u, preview_items.size());
  auto cycle_view_bounds =
      GetWindowCycleListWidget()->GetWindowBoundsInScreen();
  ASSERT_TRUE(cycle_view_bounds.x() <
              preview_items[0]->GetBoundsInScreen().x());
  ASSERT_TRUE(preview_items[2]->GetBoundsInScreen().x() <
              cycle_view_bounds.right());
  ASSERT_TRUE(cycle_view_bounds.right() <
              preview_items[3]->GetBoundsInScreen().right());
  ASSERT_TRUE(preview_items[3]->GetBoundsInScreen().x() <
              cycle_view_bounds.right());
  ASSERT_TRUE(cycle_view_bounds.right() <
              preview_items[4]->GetBoundsInScreen().x());

  // Drag from the middle of the first item to the right. The preview items
  // should not move since we're at the beginning of the cycle list. Also the
  // focus ring should not move.
  auto drag_origin = preview_items[0]->GetBoundsInScreen().CenterPoint();
  auto drag_dest = preview_items[1]->GetBoundsInScreen().CenterPoint();
  event_generator->GestureScrollSequence(drag_origin, drag_dest,
                                         base::TimeDelta::FromSeconds(1), 10);
  EXPECT_EQ(drag_origin, preview_items[0]->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(window2.get(), GetTargetWindow());

  // Drag from the middle of the second item to the left. The item should follow
  // the cursor and the focus ring should not move.
  drag_origin = preview_items[1]->GetBoundsInScreen().CenterPoint();
  drag_dest = preview_items[0]->GetBoundsInScreen().CenterPoint();
  event_generator->GestureScrollSequence(drag_origin, drag_dest,
                                         base::TimeDelta::FromSeconds(1), 10);
  EXPECT_TRUE(base::IsApproximatelyEqual(
      drag_dest.x(), preview_items[1]->GetBoundsInScreen().CenterPoint().x(),
      10));
  EXPECT_TRUE(preview_items[0]->GetBoundsInScreen().CenterPoint().x() <
              cycle_view_bounds.x());
  EXPECT_EQ(window2.get(), GetTargetWindow());

  // The last preview item should now be visible, but it shouldn't be contained.
  EXPECT_TRUE(preview_items[4]->GetBoundsInScreen().x() <
              cycle_view_bounds.right());
  EXPECT_TRUE(cycle_view_bounds.right() <
              preview_items[4]->GetBoundsInScreen().right());

  // Drag from the middle of the fourth item to the left one preview item's
  // width. Since the last item is already visible, the mirror container should
  // not be dragged the full amount and the last item's right edge should be at
  // the end of the cycle view.
  drag_origin = preview_items[3]->GetBoundsInScreen().CenterPoint();
  drag_dest = preview_items[1]->GetBoundsInScreen().CenterPoint();
  event_generator->GestureScrollSequence(drag_origin, drag_dest,
                                         base::TimeDelta::FromSeconds(1), 10);
  EXPECT_EQ(cycle_view_bounds.right(),
            preview_items[4]->GetBoundsInScreen().right() +
                WindowCycleList::kInsideBorderHorizontalPaddingDp);
  EXPECT_EQ(window2.get(), GetTargetWindow());

  // Diagonally drag from the middle of the fourth item to the right, ending up
  // outside of the cycle view. This should still drag the full distance.
  drag_origin = preview_items[3]->GetBoundsInScreen().CenterPoint();
  drag_dest = preview_items[4]->GetBoundsInScreen().CenterPoint();
  drag_dest.set_y(cycle_view_bounds.bottom() + 100);
  event_generator->GestureScrollSequence(drag_origin, drag_dest,
                                         base::TimeDelta::FromSeconds(1), 10);
  EXPECT_TRUE(base::IsApproximatelyEqual(
      drag_dest.x(), preview_items[3]->GetBoundsInScreen().CenterPoint().x(),
      10));
}

// When a user taps on an item, it should set the focus ring to that item. After
// they release their finger it should confirm the selection.
TEST_F(InteractiveWindowCycleControllerTest, TapSelect) {
  std::unique_ptr<aura::Window> w0 = CreateTestWindow();
  std::unique_ptr<aura::Window> w1 = CreateTestWindow();
  std::unique_ptr<aura::Window> w2 = CreateTestWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  auto generate_gesture_event = [](ui::test::EventGenerator* generator,
                                   const gfx::Point& location,
                                   ui::EventType type) {
    ui::GestureEvent event(location.x(), location.y(),
                           /*flags=*/0, base::TimeTicks::Now(),
                           ui::GestureEventDetails{type});
    generator->Dispatch(&event);
  };

  auto tap_without_release = [generate_gesture_event](
                                 ui::test::EventGenerator* generator,
                                 const gfx::Point& location) {
    // Generates the following events at |location| in the given order:
    // ET_GESTURE_BEGIN, ET_GESTURE_TAP_DOWN, ET_GESTURE_SHOW_PRESS
    generate_gesture_event(generator, location, ui::ET_GESTURE_BEGIN);
    generate_gesture_event(generator, location, ui::ET_GESTURE_TAP_DOWN);
    generate_gesture_event(generator, location, ui::ET_GESTURE_SHOW_PRESS);
  };

  // Start cycle and tap third item without releasing finger. On tap down, the
  // focus ring should be set to the third item. Selection should not be
  // confirmed since finger was not released. Starting order of windows in cycle
  // list is [2,1,0].
  controller->StartCycling();
  gfx::Point center_point =
      GetWindowCycleItemViews()[2]->GetBoundsInScreen().CenterPoint();
  tap_without_release(generator, center_point);
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_EQ(GetTargetWindow(), w0.get());

  // Complete cycling and confirm window 0 is active.
  CompleteCycling(controller);
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));

  // Start cycle and tap second item without releasing finger. On tap down, the
  // focus ring should be set to the second item. Selection should not be
  // confirmed since finger was not released. Starting order of windows in cycle
  // list is [0,2,1].
  controller->StartCycling();
  center_point =
      GetWindowCycleItemViews()[1]->GetBoundsInScreen().CenterPoint();
  tap_without_release(generator, center_point);
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_EQ(GetTargetWindow(), w2.get());

  // Complete cycling and confirm window 2 is active.
  CompleteCycling(controller);
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  // Start cycling again and tap and release.  This should confirm the
  // selection. Starting order of windows in cycle list is [2,0,1].
  controller->StartCycling();
  center_point =
      GetWindowCycleItemViews()[1]->GetBoundsInScreen().CenterPoint();
  generator->GestureTapDownAndUp(center_point);
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));
}

class ReverseGestureWindowCycleControllerTest
    : public InteractiveWindowCycleControllerTest {
 public:
  ReverseGestureWindowCycleControllerTest() = default;
  ReverseGestureWindowCycleControllerTest(
      const ReverseGestureWindowCycleControllerTest&) = delete;
  ReverseGestureWindowCycleControllerTest& operator=(
      const ReverseGestureWindowCycleControllerTest&) = delete;
  ~ReverseGestureWindowCycleControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kReverseScrollGestures);
    AshTestBase::SetUp();

    // Set natural scroll on.
    PrefService* pref =
        Shell::Get()->session_controller()->GetActivePrefService();
    pref->SetBoolean(prefs::kTouchpadEnabled, true);
    pref->SetBoolean(prefs::kNaturalScroll, true);
    pref->SetBoolean(prefs::kMouseReverseScroll, true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests mouse wheel scroll gesture to move selection left or right. Mouse
// reverse scroll should reverse its direction.
TEST_F(ReverseGestureWindowCycleControllerTest,
       MouseWheelScrollInWindowCycleList) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  const float horizontal_scroll =
      WindowCycleEventFilter::kHorizontalThresholdDp;

  auto scroll_until_window_highlighted_and_confirm = [this](float x_offset,
                                                            float y_offset,
                                                            int num_of_times) {
    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->StartCycling();
    MouseWheelScroll(x_offset, y_offset, num_of_times);
    CompleteCycling(controller);
  };

  // Start cycle, simulating alt key being held down. Scroll right to fourth
  // item.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(0, horizontal_scroll, 3);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Start cycle. Scroll left to third item.
  // Current order is [2,5,4,3,1].
  scroll_until_window_highlighted_and_confirm(0, -horizontal_scroll, 3);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle. Scroll right to second item.
  // Current order is [4,2,5,3,1].
  scroll_until_window_highlighted_and_confirm(0, horizontal_scroll, 1);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Turn mouse reverse scroll off.
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref->SetBoolean(prefs::kMouseReverseScroll, false);

  // Start cycle. Scroll left once.
  // Current order is [2,4,5,3,1].
  scroll_until_window_highlighted_and_confirm(0, horizontal_scroll, 1);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Start cycle. Scroll right once.
  // Current order is [1,2,4,5,3].
  scroll_until_window_highlighted_and_confirm(0, -horizontal_scroll, 1);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that natural scroll doesn't affect two and three finger horizontal
// scroll gestures for cycling window cycle list.
TEST_F(ReverseGestureWindowCycleControllerTest,
       WindowCycleListTrackpadGestures) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  const float horizontal_scroll =
      WindowCycleEventFilter::kHorizontalThresholdDp;

  auto scroll_until_window_highlighted_and_confirm = [this](float x_offset,
                                                            float y_offset,
                                                            int num_fingers) {
    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->StartCycling();
    Scroll(x_offset, y_offset, num_fingers);
    CompleteCycling(controller);
  };

  // Start cycle, scroll right with two finger gesture.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll, 0,
                                              kNumFingersForMouseWheel);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle, scroll right with three finger gesture.
  // Current order is [4,5,3,2,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll, 0,
                                              kNumFingersForTrackpad);
  EXPECT_TRUE(wm::IsActiveWindow(window5.get()));

  // Turn natural scroll off.
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref->SetBoolean(prefs::kNaturalScroll, false);

  // Start cycle, scroll right with two finger gesture. Note: two figner swipes
  // are negated, so negate in tests to mimic how this actually behaves on
  // devices.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(-horizontal_scroll, 0,
                                              kNumFingersForMouseWheel);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle, scroll right with three finger gesture.
  // Current order is [4,5,3,2,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll, 0,
                                              kNumFingersForTrackpad);
  EXPECT_TRUE(wm::IsActiveWindow(window5.get()));
}

class ModeSelectionWindowCycleControllerTest
    : public WindowCycleControllerTest {
 public:
  ModeSelectionWindowCycleControllerTest() = default;
  ModeSelectionWindowCycleControllerTest(
      const ModeSelectionWindowCycleControllerTest&) = delete;
  ModeSelectionWindowCycleControllerTest& operator=(
      const ModeSelectionWindowCycleControllerTest&) = delete;
  ~ModeSelectionWindowCycleControllerTest() override = default;

  // WindowCycleControllerTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kBento);
    WindowCycleControllerTest::SetUp();
    generator_ = GetEventGenerator();
  }

  void SwitchPerDeskAltTabMode(bool per_desk_mode) {
    ui::ScopedAnimationDurationScaleMode animation_scale(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    gfx::Point button_center =
        GetWindowCycleTabSliderButtons()[per_desk_mode ? 1 : 0]
            ->GetBoundsInScreen()
            .CenterPoint();
    generator_->MoveMouseTo(button_center);
    generator_->ClickLeftButton();
    EXPECT_EQ(per_desk_mode,
              Shell::Get()->window_cycle_controller()->IsAltTabPerActiveDesk());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::test::EventGenerator* generator_;
};

// Tests that if user uses only one desk, the tab slider and no recent items
// are not shown. Moreover, `SetAltTabMode()` should not change the windows
// list.
TEST_F(ModeSelectionWindowCycleControllerTest, SingleDeskHidesInteractiveMode) {
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Create two windows in the current desk.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(1u, desks_controller->desks().size());

  // Alt-tab should contain windows from all desks without any the tab slider
  // and no-recent-items view.
  cycle_controller->StartCycling();
  EXPECT_TRUE(!GetWindowCycleNoRecentItemsLabel());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  auto cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(2u, cycle_windows.size());
  EXPECT_EQ(cycle_windows.size(), GetWindowCycleItemViews().size());
  const gfx::Rect alttab_bounds_without_tab_slider =
      GetWindowCycleListWidget()->GetWindowBoundsInScreen();
  CompleteCycling(cycle_controller);

  // Create an empty desk_2 and start alt-tab to enter the all-desks mode.
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  cycle_controller->StartCycling();
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(2u, cycle_windows.size());
  EXPECT_EQ(cycle_windows.size(), GetWindowCycleItemViews().size());

  // Expect mode-switching buttons and no-recent-item label to exist.
  EXPECT_FALSE(!GetWindowCycleNoRecentItemsLabel());
  auto tab_slider_buttons = GetWindowCycleTabSliderButtons();
  EXPECT_EQ(2u, tab_slider_buttons.size());
  const gfx::Rect alttab_bounds_with_tab_slider =
      GetWindowCycleListWidget()->GetWindowBoundsInScreen();
  const int window_cycle_list_y =
      GetWindowCycleItemViews()[0]->GetBoundsInScreen().y();
  const gfx::Rect tab_slider_button_bound =
      tab_slider_buttons[0]->GetBoundsInScreen();
  // Expect that alt-tab views height is larger due to the tab slider insertion
  // and expect that window cycle list is placed below the tab slider.
  EXPECT_LT(alttab_bounds_without_tab_slider.height(),
            alttab_bounds_with_tab_slider.height());
  EXPECT_LT(tab_slider_button_bound.y() + tab_slider_button_bound.height(),
            window_cycle_list_y);

  CompleteCyclingAndDeskSwitching(cycle_controller);
}

// Tests that alt-tab shows all windows in an all-desk mode by default and
// shows only windows in the current desk in a current-desk mode. Switching
// between two modes should refresh the window list, while re-entering alt-tab
// should display the most recently selected mode.
TEST_F(ModeSelectionWindowCycleControllerTest, CycleShowsWindowsPerMode) {
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Create two windows for desk1 and three windows for desk2.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win3 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  auto win4 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  // By default should contain windows from all desks.
  auto* generator = GetEventGenerator();
  // Press and hold an alt key to test that alt + left clicking a button works.
  generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
  cycle_controller->StartCycling();
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  auto cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(5u, cycle_windows.size());
  EXPECT_EQ(cycle_windows.size(), GetWindowCycleItemViews().size());
  EXPECT_TRUE(base::Contains(cycle_windows, win0.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win1.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win2.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win3.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win4.get()));

  // Switching alt-tab to the current-desk mode should show windows in the
  // active desk.
  SwitchPerDeskAltTabMode(true);
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(3u, GetWindowCycleItemViews().size());
  EXPECT_EQ(cycle_windows.size(), GetWindowCycleItemViews().size());
  EXPECT_TRUE(base::Contains(cycle_windows, win2.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win3.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win4.get()));
  CompleteCycling(cycle_controller);

  // Activate desk1 and start alt-tab.
  const Desk* desk_1 = desks_controller->desks()[0].get();
  ActivateDesk(desk_1);
  cycle_controller->StartCycling();
  // Should start alt-tab with the current-desk mode and show only two windows
  // from desk1.
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(2u, GetWindowCycleItemViews().size());
  EXPECT_EQ(cycle_windows.size(), GetWindowCycleItemViews().size());
  EXPECT_TRUE(base::Contains(cycle_windows, win0.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win1.get()));

  // Switch to the all-desks mode, check and stop alt-tab.
  SwitchPerDeskAltTabMode(false);
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(5u, cycle_windows.size());
  EXPECT_EQ(cycle_windows.size(), GetWindowCycleItemViews().size());

  CompleteCyclingAndDeskSwitching(cycle_controller);
  generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
}

// For one window display, tests that alt-tab does not show up if there is only
// one window to be shown, but would continue to show a window in alt-tab if
// switching from the all-desks mode with multiple windows.
TEST_F(ModeSelectionWindowCycleControllerTest, OneWindowInActiveDesk) {
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Create two windows for desk1 and one window for desk2.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));

  // Starting alt-tab should shows all desks.
  cycle_controller->StartCycling();
  auto cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(3u, GetWindowCycleItemViews().size());
  EXPECT_EQ(cycle_windows.size(), GetWindowCycleItemViews().size());

  // Switching to an active desk mode should shows a single window in desk2.
  SwitchPerDeskAltTabMode(true);
  EXPECT_TRUE(cycle_controller->IsCycling());
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(1u, GetWindowCycleItemViews().size());
  EXPECT_EQ(cycle_windows.size(), GetWindowCycleItemViews().size());
  EXPECT_TRUE(base::Contains(cycle_windows, win2.get()));
  CompleteCycling(cycle_controller);

  // Closing alt-tab and trying to re-open again in the current-desk mode
  // should not work because there's only one window.
  cycle_controller->StartCycling();
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_FALSE(CycleViewExists());
  CompleteCycling(cycle_controller);
}

// Similar to OneWindowInActiveDesk, tests that alt-tab does not show up if
// there is no window to be shown, but would show "No recent items" if
// switching from the all-desks mode with multiple windows. Additionally,
// tests that while the focus is on the tab slider button, pressing the Down
// arrow key does nothing.
TEST_F(ModeSelectionWindowCycleControllerTest, NoWindowInActiveDesk) {
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Create two desks with all two windows in desk1.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();

  // Activate desk2.
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());

  // Starting alt-tab should show all windows from all desks.
  cycle_controller->StartCycling();
  auto cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(2u, GetWindowCycleItemViews().size());
  EXPECT_EQ(cycle_windows.size(), GetWindowCycleItemViews().size());
  EXPECT_FALSE(GetWindowCycleNoRecentItemsLabel()->GetVisible());

  // Switching to an current-desk mode should not show any mirror window
  // and should display "no recent items" label.
  SwitchPerDeskAltTabMode(true);
  EXPECT_TRUE(cycle_controller->IsCycling());
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(0u, GetWindowCycleItemViews().size());
  EXPECT_EQ(cycle_windows.size(), GetWindowCycleItemViews().size());
  EXPECT_TRUE(GetWindowCycleNoRecentItemsLabel()->GetVisible());

  // Switching back to an all-desks mode should hide the label.
  SwitchPerDeskAltTabMode(false);
  EXPECT_FALSE(GetWindowCycleNoRecentItemsLabel()->GetVisible());

  // Focus the current-desk button and make sure that pressing Down arrow
  // key does nothing, i.e. the focus remains on the mode button.
  generator->PressKey(ui::VKEY_UP, ui::EF_NONE);
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  generator->PressKey(ui::VKEY_DOWN, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());

  CompleteCycling(cycle_controller);
}

// Tests that switching between modes correctly reset the alt-tab-highlighted
// window to the second most recently used window, i.e. the next window to tab
// into from the currently used window. Since the window cycle list is ordered
// by MRU, such window is therefore the second window in the MRU list.
TEST_F(ModeSelectionWindowCycleControllerTest,
       SwitchingModeUpdatesWindowHighlight) {
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Create two windows for desk1 and three windows for desk2 in the reversed
  // order of the most recently active window.
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win3 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win1 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  auto win0 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  // Enter the all-desk mode by default with the window order [0, 1, 2, 3 ,4].
  cycle_controller->StartCycling();
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);

  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  auto cycle_windows = GetWindows(cycle_controller);
  // The window list is MRU ordered.
  EXPECT_EQ(win0.get(), cycle_windows[0]);
  EXPECT_EQ(win1.get(), cycle_windows[1]);
  EXPECT_EQ(win2.get(), cycle_windows[2]);
  EXPECT_EQ(win3.get(), cycle_windows[3]);
  EXPECT_EQ(win4.get(), cycle_windows[4]);
  // Alt-Tab should highlight the second most recently used window, which is
  // the second window in the MRU list, win1.
  EXPECT_EQ(win1.get(), GetTargetWindow());

  // Step to win2 and win3, so we are now select a window in a non-active desk.
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_EQ(win2.get(), GetTargetWindow());
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_EQ(win3.get(), GetTargetWindow());

  // Switching from the all-desks mode, which highlights a non-current-desk
  // window to the current-desk mode [0, 1, 2] should resolve the highlight
  // correctly to win1, the second window in the cycle list.
  SwitchPerDeskAltTabMode(true);
  EXPECT_EQ(win1.get(), GetTargetWindow());
  EXPECT_EQ(win1.get(), cycle_windows[1]);
  // Step to win2.
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_EQ(win2.get(), GetTargetWindow());

  // Switching back to the all-desk mode should reset highlight to win1 again.
  SwitchPerDeskAltTabMode(false);
  EXPECT_EQ(win1.get(), GetTargetWindow());
  CompleteCycling(cycle_controller);
}

// Similar to `SwitchingModeUpdatesWindowHighlight`, tests that switching the
// alt-tab mode updates the highlighted window to the first window (most
// recently used) in the special case where all windows are minimized.
// When they are minimized, cycling forward should help unminimize the most
// recently used window rather than trying to open the second most recently
// used window.
TEST_F(ModeSelectionWindowCycleControllerTest,
       SwitchingModeUpdatesMinimizedWindowHighlight) {
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Create two windows for desk1 and three windows for desk2 in the reversed
  // order of the most recently active window.
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win3 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win1 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  auto win0 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  // Minimize all windows to test this special case.
  WindowState::Get(win4.get())->Minimize();
  WindowState::Get(win3.get())->Minimize();
  WindowState::Get(win2.get())->Minimize();
  WindowState::Get(win1.get())->Minimize();
  WindowState::Get(win0.get())->Minimize();
  EXPECT_FALSE(WindowState::Get(win0.get())->IsActive());
  EXPECT_FALSE(WindowState::Get(win1.get())->IsActive());
  EXPECT_FALSE(WindowState::Get(win2.get())->IsActive());
  EXPECT_FALSE(WindowState::Get(win3.get())->IsActive());
  EXPECT_FALSE(WindowState::Get(win4.get())->IsActive());

  // Enter the all-desk mode by default with the window order [0, 1, 2, 3 ,4].
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  auto cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(5u, GetWindowCycleItemViews().size());
  // The window list is MRU ordered.
  EXPECT_EQ(win0.get(), cycle_windows[0]);
  EXPECT_EQ(win1.get(), cycle_windows[1]);
  EXPECT_EQ(win2.get(), cycle_windows[2]);
  EXPECT_EQ(win3.get(), cycle_windows[3]);
  EXPECT_EQ(win4.get(), cycle_windows[4]);
  // Step forward a few times and switch to all-desks mode. This should
  // highlight win0, the first window in the current-desk cycle list.
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  SwitchPerDeskAltTabMode(true);
  EXPECT_EQ(3u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win0.get(), GetTargetWindow());
  EXPECT_EQ(win0.get(), cycle_windows[0]);

  // Stepping to win1 and switching back to the all-desk mode should reset
  // a highlight to win0 again.
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_EQ(win1.get(), GetTargetWindow());
  SwitchPerDeskAltTabMode(false);
  EXPECT_EQ(5u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win0.get(), GetTargetWindow());
  CompleteCycling(cycle_controller);
}

// Tests that pressing an up arrow focus the active tab slider button.
// While a tab slider button is focus, user can switch to the other button
// via left or right key. Note that if user already selects the left button,
// attempting to go further left would do nothing.
TEST_F(ModeSelectionWindowCycleControllerTest, KeyboardNavigation) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Create two windows for desk1 and three windows for desk2 in the reversed
  // order of the most recently active window.
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win3 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win1 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  auto win0 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  // Start alt-tab.
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_EQ(win1.get(), GetTargetWindow());
  views::View::Views tab_slider_buttons = GetWindowCycleTabSliderButtons();
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());

  // Focus tab slider mode: pressing the up arrow key should focus the
  // default all-desks, which is the left button. This should not affect
  // the focus on the window cycle.
  generator->PressKey(ui::VKEY_UP, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(win1.get(), GetTargetWindow());

  // Switching to the right, current-desk button via a right arrow key changes
  // to current-desk mode and does not affect the highlighted window.
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(3u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win1.get(), GetTargetWindow());
  // Trying to move the focus further right should do nothing since it is
  // already on the right most button.
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_FALSE(wm::IsActiveWindow(win1.get()));
  EXPECT_EQ(win1.get(), GetTargetWindow());
  CompleteCycling(cycle_controller);
  // Exit alt-tab while focusing the tab slider and check that the keyboard
  // navigation within the tab slider does not affect the window activation.
  EXPECT_TRUE(wm::IsActiveWindow(win1.get()));

  // Start alt-tab and focus the tab slider. The order of cycle window is now
  // [1, 0, 2, 3, 4].
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  generator->PressKey(ui::VKEY_UP, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(win0.get(), GetTargetWindow());
  // Switching to the left, all-desks button via a left arrow key changes
  // to current-desk mode and does not affect the highlighted window.
  generator->PressKey(ui::VKEY_LEFT, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(5u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win0.get(), GetTargetWindow());
  // Trying to move the focus further left should do nothing since it is
  // already on the left most button.
  generator->PressKey(ui::VKEY_LEFT, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(win0.get(), GetTargetWindow());

  // Stop focusing the tab slider button by pressing a down arrow key to
  // continue navigation in the window cycle list.
  generator->PressKey(ui::VKEY_DOWN, ui::EF_NONE);
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(5u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win0.get(), GetTargetWindow());

  // Now navigating left and right should only affect the highlighted window
  // but not the tab slider buttons.
  // Pressing right twice should move the focus to win3.
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(win3.get(), GetTargetWindow());
  // Pressing left once should move focus back to win2.
  generator->PressKey(ui::VKEY_LEFT, ui::EF_NONE);
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(win2.get(), GetTargetWindow());

  CompleteCycling(cycle_controller);
  EXPECT_TRUE(wm::IsActiveWindow(win2.get()));
}

// Tests that clicking the alt-tab slider button removes the focus from the
// button and resets the focus to the target window in the cycle list. After
// clicking the button, the user then needs to press tge Up arrow key again if
// the user want to switch between the alt-tab modes via keyboard navigation.
TEST_F(ModeSelectionWindowCycleControllerTest, KeyboardNavigationAfterClick) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Create two windows for desk1 and three windows for desk2 in the reversed
  // order of the most recently active window.
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win3 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win1 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  auto win0 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  // Start alt-tab.
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_EQ(win1.get(), GetTargetWindow());
  views::View::Views tab_slider_buttons = GetWindowCycleTabSliderButtons();
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());

  // Focus tab slider mode: pressing the Up arrow key should focus the
  // default all-desks, which is the left button. This should not affect
  // the focus on the window cycle.
  generator->PressKey(ui::VKEY_UP, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(win1.get(), GetTargetWindow());

  // Switching to the right, current-desk button via the Right arrow key changes
  // to the current-desk mode and does not affect the highlighted window.
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(3u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win1.get(), GetTargetWindow());

  // Clicking the same current-desk button should do nothing.
  SwitchPerDeskAltTabMode(true);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(3u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win1.get(), GetTargetWindow());

  // Clicking the all-desk button should remove the focus from the alt-tab
  // slider and pressing the Left or Right arrow change the target cycle window
  // rather than switching the mode.
  SwitchPerDeskAltTabMode(false);
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(5u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win1.get(), GetTargetWindow());

  // Pressing the Right arrow key should cycle forward rather than switch to
  // the current-desk mode.
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(5u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win2.get(), GetTargetWindow());
  CompleteCycling(cycle_controller);
  // Make sure that cycling navigation after a click resets the focus does
  // not affect the correctness of window activation.
  EXPECT_TRUE(wm::IsActiveWindow(win2.get()));

  // The window order is now [2, 0, 1, 3, 4] in the all-desks mode.
  // Similar to the test above but focus the all-desks button before clicking
  // and make sure that exiting alt-tab after a click resets the focus
  // activates the right window.
  // Start alt-tab and press the Up arrow key to focus the all-desks button.
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(win0.get(), GetTargetWindow());
  generator->PressKey(ui::VKEY_UP, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(win0.get(), GetTargetWindow());

  // Click the current-desk button.
  SwitchPerDeskAltTabMode(true);
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(3u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win0.get(), GetTargetWindow());
  CompleteCycling(cycle_controller);

  // Exiting after the click resets the focus should activate the right window.
  EXPECT_TRUE(wm::IsActiveWindow(win0.get()));
}

// Tests that ChromeVox alerts the mode change, new target window and
// Down-arrow directional cue correctly when the user uses keyboard navigation
// and button clicking.
TEST_F(ModeSelectionWindowCycleControllerTest, ChromeVox) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Create two windows for desk1 and one window for desk2 in the reversed
  // order of the most recently active window.
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win1 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  win2->SetTitle(base::ASCIIToUTF16("win2"));
  win1->SetTitle(base::ASCIIToUTF16("win1"));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win0 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  win0->SetTitle(base::ASCIIToUTF16("win0"));

  TestAccessibilityControllerClient client;
  const std::string kAllDesksSelected =
      l10n_util::GetStringUTF8(IDS_ASH_ALT_TAB_ALL_DESKS_MODE_SELECTED_TITLE);
  const std::string kCurrentDeskSelected = l10n_util::GetStringUTF8(
      IDS_ASH_ALT_TAB_CURRENT_DESK_MODE_SELECTED_TITLE);
  const std::string kFocusWindowDirectionalCue =
      l10n_util::GetStringUTF8(IDS_ASH_ALT_TAB_FOCUS_WINDOW_LIST_TITLE);

  // Start alt-tab.
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_EQ(win1.get(), GetTargetWindow());
  views::View::Views tab_slider_buttons = GetWindowCycleTabSliderButtons();
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_NE(kAllDesksSelected, client.last_alert_message());

  // Pressing the up arrow key should focus and alert all-desks mode.
  generator->PressKey(ui::VKEY_UP, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(win1.get(), GetTargetWindow());
  EXPECT_EQ(kAllDesksSelected, client.last_alert_message());

  // Pressing (->) announces the new mode, the new focused window, and the
  // Down-arrow directional cue.
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(1u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win0.get(), GetTargetWindow());
  std::string last_alert_message = client.last_alert_message();
  EXPECT_TRUE(last_alert_message.find(kCurrentDeskSelected) !=
              std::string::npos);
  EXPECT_TRUE(last_alert_message.find(l10n_util::GetStringFUTF8(
                  IDS_ASH_ALT_TAB_WINDOW_SELECTED_TITLE, win0->GetTitle())) !=
              std::string::npos);
  EXPECT_TRUE(last_alert_message.find(kFocusWindowDirectionalCue) !=
              std::string::npos);

  // Pressing (<-) announces the new mode, the new focused window and the
  // Down-arrow directional cue.
  generator->PressKey(ui::VKEY_LEFT, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(3u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win1.get(), GetTargetWindow());
  last_alert_message = client.last_alert_message();
  EXPECT_TRUE(last_alert_message.find(kAllDesksSelected) != std::string::npos);
  EXPECT_TRUE(last_alert_message.find(l10n_util::GetStringFUTF8(
                  IDS_ASH_ALT_TAB_WINDOW_SELECTED_TITLE, win1->GetTitle())) !=
              std::string::npos);
  EXPECT_TRUE(last_alert_message.find(kFocusWindowDirectionalCue) !=
              std::string::npos);

  // Clicking the current-desk button notifies the new mode and the new focused
  // window but not the Down-arrow directional cue because the focus is moved
  // to the window, which is the bottom most component.
  SwitchPerDeskAltTabMode(true);
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(1u, GetWindowCycleItemViews().size());
  EXPECT_EQ(win0.get(), GetTargetWindow());
  last_alert_message = client.last_alert_message();
  EXPECT_TRUE(last_alert_message.find(kCurrentDeskSelected) !=
              std::string::npos);
  EXPECT_TRUE(last_alert_message.find(l10n_util::GetStringFUTF8(
                  IDS_ASH_ALT_TAB_WINDOW_SELECTED_TITLE, win0->GetTitle())) !=
              std::string::npos);
  EXPECT_FALSE(last_alert_message.find(kFocusWindowDirectionalCue) !=
               std::string::npos);

  // Pressing the Down arrow key while focusing the tab slider button should
  // alert only the focused window.
  generator->PressKey(ui::VKEY_UP, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  generator->PressKey(ui::VKEY_DOWN, ui::EF_NONE);
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(win0.get(), GetTargetWindow());
  last_alert_message = client.last_alert_message();
  EXPECT_FALSE(last_alert_message.find(kCurrentDeskSelected) !=
               std::string::npos);
  EXPECT_TRUE(last_alert_message.find(l10n_util::GetStringFUTF8(
                  IDS_ASH_ALT_TAB_WINDOW_SELECTED_TITLE, win0->GetTitle())) !=
              std::string::npos);
  EXPECT_FALSE(last_alert_message.find(kFocusWindowDirectionalCue) !=
               std::string::npos);

  CompleteCycling(cycle_controller);
  EXPECT_TRUE(wm::IsActiveWindow(win0.get()));
}

// Tests that ChromeVox alerts correctly when the current desk has no window
// during alt-tab mode switching via both keyboard navigation and button click.
TEST_F(ModeSelectionWindowCycleControllerTest, ChromeVoxNoWindow) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Create two desks with all two windows in the non-active desk.
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win0 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  win1->SetTitle(base::ASCIIToUTF16("win1"));
  win0->SetTitle(base::ASCIIToUTF16("win0"));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());

  TestAccessibilityControllerClient client;
  const std::string kAllDesksSelected =
      l10n_util::GetStringUTF8(IDS_ASH_ALT_TAB_ALL_DESKS_MODE_SELECTED_TITLE);
  const std::string kCurrentDeskSelected = l10n_util::GetStringUTF8(
      IDS_ASH_ALT_TAB_CURRENT_DESK_MODE_SELECTED_TITLE);
  const std::string kFocusWindowDirectionalCue =
      l10n_util::GetStringUTF8(IDS_ASH_ALT_TAB_FOCUS_WINDOW_LIST_TITLE);
  const std::string kNoRecentItems =
      l10n_util::GetStringUTF8(IDS_ASH_OVERVIEW_NO_RECENT_ITEMS);

  // Start alt-tab.
  cycle_controller->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
  EXPECT_EQ(win1.get(), GetTargetWindow());
  views::View::Views tab_slider_buttons = GetWindowCycleTabSliderButtons();
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_NE(kAllDesksSelected, client.last_alert_message());

  // Pressing the up arrow key should focus and alert all-desks mode.
  generator->PressKey(ui::VKEY_UP, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(win1.get(), GetTargetWindow());
  EXPECT_EQ(kAllDesksSelected, client.last_alert_message());

  // Pressing (->) announces the new mode and the new focused window but not
  // the Down-arrow directional cue, which is a useless move.
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(0u, GetWindowCycleItemViews().size());
  EXPECT_EQ(nullptr, GetTargetWindow());
  EXPECT_TRUE(GetWindowCycleNoRecentItemsLabel()->GetVisible());
  std::string last_alert_message = client.last_alert_message();
  EXPECT_TRUE(last_alert_message.find(kCurrentDeskSelected) !=
              std::string::npos);
  EXPECT_TRUE(last_alert_message.find(kNoRecentItems) != std::string::npos);
  EXPECT_FALSE(last_alert_message.find(kFocusWindowDirectionalCue) !=
               std::string::npos);

  // Pressing (<-) announces the new mode, the new focused window and the
  // Down-arrow directional cue.
  generator->PressKey(ui::VKEY_LEFT, ui::EF_NONE);
  EXPECT_TRUE(cycle_controller->IsTabSliderFocused());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(2u, GetWindowCycleItemViews().size());
  // Expect alt-tab to select the first window in the MRU because it is in
  // another desk.
  EXPECT_EQ(win0.get(), GetTargetWindow());
  EXPECT_FALSE(GetWindowCycleNoRecentItemsLabel()->GetVisible());

  // Similar to (->), Clicking the current-desk button notifies the new mode
  // and the new focused window but not the Down-arrow directional cue.
  SwitchPerDeskAltTabMode(true);
  EXPECT_FALSE(cycle_controller->IsTabSliderFocused());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(0u, GetWindowCycleItemViews().size());
  EXPECT_EQ(nullptr, GetTargetWindow());
  EXPECT_TRUE(GetWindowCycleNoRecentItemsLabel()->GetVisible());
  last_alert_message = client.last_alert_message();
  EXPECT_TRUE(last_alert_message.find(kCurrentDeskSelected) !=
              std::string::npos);
  EXPECT_TRUE(last_alert_message.find(kNoRecentItems) != std::string::npos);
  EXPECT_FALSE(last_alert_message.find(kFocusWindowDirectionalCue) !=
               std::string::npos);

  CompleteCycling(cycle_controller);
  EXPECT_FALSE(wm::IsActiveWindow(win0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(win1.get()));
}

namespace {

constexpr char kUser1Email[] = "user1@alttab";
constexpr char kUser2Email[] = "user2@alttab";

}  // namespace

class MultiUserWindowCycleControllerTest
    : public NoSessionAshTestBase,
      public MultiUserWindowManagerDelegate {
 public:
  MultiUserWindowCycleControllerTest() = default;
  MultiUserWindowCycleControllerTest(
      const MultiUserWindowCycleControllerTest&) = delete;
  MultiUserWindowCycleControllerTest& operator=(
      const MultiUserWindowCycleControllerTest&) = delete;
  ~MultiUserWindowCycleControllerTest() override = default;

  MultiUserWindowManager* multi_user_window_manager() {
    return multi_user_window_manager_.get();
  }
  TestingPrefServiceSimple* user_1_prefs() { return user_1_prefs_; }
  TestingPrefServiceSimple* user_2_prefs() { return user_2_prefs_; }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kBento);
    NoSessionAshTestBase::SetUp();

    WindowCycleList::DisableInitialDelayForTesting();
    shelf_view_test_.reset(
        new ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting()));
    shelf_view_test_->SetAnimationDuration(
        base::TimeDelta::FromMilliseconds(1));

    generator_ = GetEventGenerator();

    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();

    // Inject our own PrefServices for each user which enables us to setup the
    // desks restore data before the user signs in.
    auto user_1_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_1_prefs_ = user_1_prefs.get();
    RegisterUserProfilePrefs(user_1_prefs_->registry(), /*for_test=*/true);
    auto user_2_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_2_prefs_ = user_2_prefs.get();
    RegisterUserProfilePrefs(user_2_prefs_->registry(), /*for_test=*/true);
    session_controller->AddUserSession(kUser1Email,
                                       user_manager::USER_TYPE_REGULAR,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser1AccountId(),
                                           std::move(user_1_prefs));
    session_controller->AddUserSession(kUser2Email,
                                       user_manager::USER_TYPE_REGULAR,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser2AccountId(),
                                           std::move(user_2_prefs));
  }

  void TearDown() override {
    multi_user_window_manager_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // MultiUserWindowManagerDelegate:
  void OnWindowOwnerEntryChanged(aura::Window* window,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override {}
  void OnTransitionUserShelfToNewAccount() override {}

  void SwitchPerDeskAltTabModeFromUIAndCheckPrefs(bool per_desk_mode) {
    auto* cycle_controller = Shell::Get()->window_cycle_controller();
    EXPECT_TRUE(cycle_controller->IsCycling());
    gfx::Point button_center =
        GetWindowCycleTabSliderButtons()[per_desk_mode ? 1 : 0]
            ->GetBoundsInScreen()
            .CenterPoint();
    generator_->MoveMouseTo(button_center);
    generator_->ClickLeftButton();
    // Check that alt-tab mode in UI and user prefs are updated.
    EXPECT_EQ(per_desk_mode, cycle_controller->IsAltTabPerActiveDesk());
    EXPECT_EQ(per_desk_mode, IsActivePrefsPerDeskMode());
    EXPECT_TRUE(cycle_controller->IsCycling());
  }

  AccountId GetUser1AccountId() const {
    return AccountId::FromUserEmail(kUser1Email);
  }

  AccountId GetUser2AccountId() const {
    return AccountId::FromUserEmail(kUser2Email);
  }

  bool IsActivePrefsPerDeskMode() {
    PrefService* active_user_prefs =
        Shell::Get()->session_controller()->GetActivePrefService();
    DCHECK(active_user_prefs);
    return active_user_prefs->GetBoolean(prefs::kAltTabPerDesk);
  }

  void SetActivePrefsPerDeskMode(bool per_desk) {
    PrefService* active_user_prefs =
        Shell::Get()->session_controller()->GetActivePrefService();
    DCHECK(active_user_prefs);
    active_user_prefs->SetBoolean(prefs::kAltTabPerDesk, per_desk);
  }

  PrefService* GetUserPrefsService(bool primary) {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        primary ? GetUser1AccountId() : GetUser2AccountId());
  }

  void SwitchActiveUser(const AccountId& account_id) {
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  void SimulateUserLogin(const AccountId& account_id) {
    SwitchActiveUser(account_id);
    multi_user_window_manager_ =
        MultiUserWindowManager::Create(this, account_id);
    MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
        MultiUserWindowManagerImpl::ANIMATION_SPEED_DISABLED);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

  const aura::Window::Windows GetWindows(WindowCycleController* controller) {
    return controller->window_cycle_list()->windows();
  }

  const views::View::Views& GetWindowCycleItemViews() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->GetWindowCycleItemViewsForTesting();
  }

  const views::View::Views& GetWindowCycleTabSliderButtons() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->GetWindowCycleTabSliderButtonsForTesting();
  }

  const aura::Window* GetTargetWindow() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->GetTargetWindowForTesting();
  }

  void CompleteCycling(WindowCycleController* controller) {
    controller->CompleteCycling();
    base::RunLoop().RunUntilIdle();
  }

  void CompleteCyclingAndDeskSwitching(WindowCycleController* controller) {
    DeskSwitchAnimationWaiter waiter;
    controller->CompleteCycling();
    base::RunLoop().RunUntilIdle();
    auto* desks_controller = Shell::Get()->desks_controller();
    if (desks_controller->AreDesksBeingModified())
      waiter.Wait();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::test::EventGenerator* generator_;

  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_;

  std::unique_ptr<MultiUserWindowManager> multi_user_window_manager_;

  TestingPrefServiceSimple* user_1_prefs_ = nullptr;
  TestingPrefServiceSimple* user_2_prefs_ = nullptr;
};

// Tests that when the active user prefs' |prefs::kAltTabPerDesk| is updated,
// the tab slider UI and the window cycle list are refreshed.
TEST_F(MultiUserWindowCycleControllerTest, AltTabModePrefsUpdateUI) {
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();
  auto* desks_controller = DesksController::Get();
  // Login with user_1 and create two desks and three windows where two windows
  // are in the current desk to avoid failure to enter alt-tab.
  SimulateUserLogin(GetUser1AccountId());
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  multi_user_window_manager()->SetWindowOwner(win0.get(), GetUser1AccountId());
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  // Activate desk2 and create two windows.
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  multi_user_window_manager()->SetWindowOwner(win1.get(), GetUser1AccountId());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  multi_user_window_manager()->SetWindowOwner(win2.get(), GetUser1AccountId());

  // user_1 prefs and alt-tab mode should default to the all-desk mode.
  cycle_controller->StartCycling();
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_EQ(3u, GetWindowCycleItemViews().size());
  EXPECT_FALSE(IsActivePrefsPerDeskMode());

  // Setting alt-tab mode prefs to current-desk should update the alt-tab UI to
  // current-desk mode.
  bool per_desk = true;
  SetActivePrefsPerDeskMode(per_desk);
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_EQ(per_desk, IsActivePrefsPerDeskMode());
  EXPECT_EQ(IsActivePrefsPerDeskMode(),
            cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(2u, GetWindowCycleItemViews().size());

  // Setting alt-tab mode prefs to all-desks should update the alt-tab UI to
  // all-desks mode.
  per_desk = false;
  SetActivePrefsPerDeskMode(per_desk);
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_EQ(per_desk, IsActivePrefsPerDeskMode());
  EXPECT_EQ(IsActivePrefsPerDeskMode(),
            cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(3u, GetWindowCycleItemViews().size());
  CompleteCycling(cycle_controller);

  // Switch to the secondary user_2 and setup the profile with four windows.
  SwitchActiveUser(GetUser2AccountId());
  const Desk* desk_1 = desks_controller->desks()[0].get();
  EXPECT_TRUE(desk_1->is_active());
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win3.get(), GetUser2AccountId());
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win4.get(), GetUser2AccountId());
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win5 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  multi_user_window_manager()->SetWindowOwner(win5.get(), GetUser2AccountId());
  auto win6 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  multi_user_window_manager()->SetWindowOwner(win6.get(), GetUser2AccountId());

  // user_2 prefs and alt-tab mode should default to the all-desk mode.
  cycle_controller->StartCycling();
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_EQ(4u, GetWindowCycleItemViews().size());
  EXPECT_FALSE(IsActivePrefsPerDeskMode());

  // Setting alt-tab mode prefs to current-desk should update the alt-tab UI to
  // current-desk mode.
  per_desk = true;
  SetActivePrefsPerDeskMode(per_desk);
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_EQ(per_desk, IsActivePrefsPerDeskMode());
  EXPECT_EQ(IsActivePrefsPerDeskMode(),
            cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(2u, GetWindowCycleItemViews().size());

  // Setting alt-tab mode prefs to all-desks should update the alt-tab UI to
  // all-desks mode.
  per_desk = false;
  SetActivePrefsPerDeskMode(per_desk);
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_EQ(per_desk, IsActivePrefsPerDeskMode());
  EXPECT_EQ(IsActivePrefsPerDeskMode(),
            cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(4u, GetWindowCycleItemViews().size());
  CompleteCycling(cycle_controller);
}

// Tests that switching between users restores each user's alt-tab mode
// correctly. In addition, pressing a tab slider button to switch the mode,
// `SwitchPerDeskAltTabModeFromUIAndCheckPrefs()` checks that alt-tab
// successfully switches the mode and updates the user prefs.
TEST_F(MultiUserWindowCycleControllerTest,
       AltTabModeUserSwitchAndUIUpdatesPref) {
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Setup user_1 with two windows out of three in the current desk and
  // set the mode to non-default current-desk for test preparation.
  SimulateUserLogin(GetUser1AccountId());
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  multi_user_window_manager()->SetWindowOwner(win0.get(), GetUser1AccountId());
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  multi_user_window_manager()->SetWindowOwner(win1.get(), GetUser1AccountId());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  multi_user_window_manager()->SetWindowOwner(win2.get(), GetUser1AccountId());

  // In preparation for multi-user alt-tab mode switching, start alt-tab with
  // user_1 prefs set to current-desk mode.
  bool per_desk = true;
  SetActivePrefsPerDeskMode(per_desk);
  cycle_controller->StartCycling();
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_EQ(per_desk, IsActivePrefsPerDeskMode());
  EXPECT_EQ(IsActivePrefsPerDeskMode(),
            cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(2u, GetWindowCycleItemViews().size());
  CompleteCycling(cycle_controller);

  // Switch to user_2 and open up two windows out of four in the current desk.
  SwitchActiveUser(GetUser2AccountId());
  const Desk* desk_1 = desks_controller->desks()[0].get();
  EXPECT_TRUE(desk_1->is_active());
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win3.get(), GetUser2AccountId());
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win4.get(), GetUser2AccountId());
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win5 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win5.get(), GetUser2AccountId());
  auto win6 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win6.get(), GetUser2AccountId());

  // In preparation for multi-user alt-tab mode switching, start alt-tab with
  // user_2 prefs set to current-desk mode.
  SetActivePrefsPerDeskMode(per_desk);
  cycle_controller->StartCycling();
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_EQ(per_desk, IsActivePrefsPerDeskMode());
  EXPECT_EQ(IsActivePrefsPerDeskMode(),
            cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(2u, GetWindowCycleItemViews().size());

  // Test that the primary user_1's mode is loaded correctly after switch
  // from secondary user_2, who just changes the mode to the opposite.

  // Currently, both users choose the current-desk mode, so we try change
  // user_2 to all-desks mode from the tab slider UI to see if user_1's mode
  // remains correctly unaffected.
  SwitchPerDeskAltTabModeFromUIAndCheckPrefs(false);
  EXPECT_EQ(4u, GetWindowCycleItemViews().size());
  CompleteCycling(cycle_controller);

  // Switch back to user_1. Make sure that user_1 prefs remains unaffected
  // and the alt-tab enter with the correct current-desk mode.
  SwitchActiveUser(GetUser1AccountId());
  cycle_controller->StartCycling();
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_TRUE(IsActivePrefsPerDeskMode());
  EXPECT_EQ(IsActivePrefsPerDeskMode(),
            cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(2u, GetWindowCycleItemViews().size());
  CompleteCycling(cycle_controller);

  // In preparation for the next test, change user_2 back the current-desk mode
  // to make sure both users start at the same mode selection.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  cycle_controller->StartCycling();
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_FALSE(cycle_controller->IsAltTabPerActiveDesk());
  SwitchPerDeskAltTabModeFromUIAndCheckPrefs(true);
  EXPECT_EQ(2u, GetWindowCycleItemViews().size());
  CompleteCycling(cycle_controller);

  // Test that the secondary user_2's mode is loaded correctly after switching
  // from primary user_1, who just changes the mode to the opposite.

  // Currently, both users choose the current-desk mode, so we try change
  // user_1 to all-desks mode to see if user_2's mode will change.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  cycle_controller->StartCycling();
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  SwitchPerDeskAltTabModeFromUIAndCheckPrefs(false);
  EXPECT_EQ(3u, GetWindowCycleItemViews().size());
  CompleteCyclingAndDeskSwitching(cycle_controller);

  // Switch back to user_2 and make sure that the mode is restored
  // to the current-desk mode correctly.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  cycle_controller->StartCycling();
  EXPECT_TRUE(cycle_controller->IsCycling());
  EXPECT_TRUE(cycle_controller->IsAltTabPerActiveDesk());
  EXPECT_EQ(2u, GetWindowCycleItemViews().size());
  CompleteCycling(cycle_controller);
}

}  // namespace ash
