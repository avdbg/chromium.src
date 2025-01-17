// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/factory_reset_delegate.h"

#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"

namespace arc {

FactoryResetDelegate::FactoryResetDelegate() = default;
FactoryResetDelegate::~FactoryResetDelegate() = default;

void FactoryResetDelegate::ResetArc() {
  ArcSessionManager::Get()->RequestArcDataRemoval();
  ArcSessionManager::Get()->StopAndEnableArc();
}

}  // namespace arc
