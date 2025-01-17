// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/fake_accessibility_controller.h"

FakeAccessibilityController::FakeAccessibilityController() = default;

FakeAccessibilityController::~FakeAccessibilityController() = default;

void FakeAccessibilityController::SetClient(
    ash::AccessibilityControllerClient* client) {
  was_client_set_ = true;
}

void FakeAccessibilityController::SetDarkenScreen(bool darken) {}

void FakeAccessibilityController::BrailleDisplayStateChanged(bool connected) {}

void FakeAccessibilityController::SetFocusHighlightRect(
    const gfx::Rect& bounds_in_screen) {}

void FakeAccessibilityController::SetCaretBounds(
    const gfx::Rect& bounds_in_screen) {}

void FakeAccessibilityController::SetAccessibilityPanelAlwaysVisible(
    bool always_visible) {}

void FakeAccessibilityController::SetAccessibilityPanelBounds(
    const gfx::Rect& bounds,
    ash::AccessibilityPanelState state) {}

void FakeAccessibilityController::SetSelectToSpeakState(
    ash::SelectToSpeakState state) {}

void FakeAccessibilityController::SetSelectToSpeakEventHandlerDelegate(
    ash::SelectToSpeakEventHandlerDelegate* delegate) {}

void FakeAccessibilityController::ShowSelectToSpeakPanel(
    const gfx::Rect& anchor,
    bool is_paused,
    double speed) {}

void FakeAccessibilityController::HideSelectToSpeakPanel() {}

void FakeAccessibilityController::OnSelectToSpeakPanelAction(
    ash::SelectToSpeakPanelAction action,
    double value) {}

void FakeAccessibilityController::HideSwitchAccessBackButton() {}

void FakeAccessibilityController::HideSwitchAccessMenu() {}

void FakeAccessibilityController::ShowSwitchAccessBackButton(
    const gfx::Rect& anchor) {}

void FakeAccessibilityController::ShowSwitchAccessMenu(
    const gfx::Rect& anchor,
    std::vector<std::string> actions) {}

void FakeAccessibilityController::StartPointScan() {}

void FakeAccessibilityController::StopPointScan() {}

void FakeAccessibilityController::SetDictationActive(bool is_active) {}

void FakeAccessibilityController::ToggleDictationFromSource(
    ash::DictationToggleSource source) {}

void FakeAccessibilityController::HandleAutoclickScrollableBoundsFound(
    gfx::Rect& bounds_in_screen) {}

base::string16 FakeAccessibilityController::GetBatteryDescription() const {
  return base::string16();
}

void FakeAccessibilityController::SetVirtualKeyboardVisible(bool is_visible) {}

void FakeAccessibilityController::PerformAcceleratorAction(
    ash::AcceleratorAction accelerator_action) {}

void FakeAccessibilityController::NotifyAccessibilityStatusChanged() {}

bool FakeAccessibilityController::IsAccessibilityFeatureVisibleInTrayMenu(
    const std::string& path) {
  return true;
}

void FakeAccessibilityController::
    DisableSwitchAccessDisableConfirmationDialogTesting() {}
