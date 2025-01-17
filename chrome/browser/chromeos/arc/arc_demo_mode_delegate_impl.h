// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_DEMO_MODE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_DEMO_MODE_DELEGATE_IMPL_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "components/arc/session/arc_client_adapter.h"

namespace arc {

class ArcDemoModeDelegateImpl : public ArcClientAdapter::DemoModeDelegate {
 public:
  ArcDemoModeDelegateImpl() = default;
  ~ArcDemoModeDelegateImpl() override = default;
  ArcDemoModeDelegateImpl(const ArcDemoModeDelegateImpl&) = delete;
  ArcDemoModeDelegateImpl& operator=(const ArcDemoModeDelegateImpl&) = delete;

  // ArcClientAdapter::DemoModeDelegate overrides:
  void EnsureOfflineResourcesLoaded(base::OnceClosure callback) override;
  base::FilePath GetDemoAppsPath() override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_DEMO_MODE_DELEGATE_IMPL_H_
