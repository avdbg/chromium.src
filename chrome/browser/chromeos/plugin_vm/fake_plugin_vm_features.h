// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_FAKE_PLUGIN_VM_FEATURES_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_FAKE_PLUGIN_VM_FEATURES_H_

#include "base/optional.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_features.h"

class Profile;

namespace plugin_vm {

// FakePluginVmFeatures implements a fake version of PluginVmFeatures which can
// be used for testing.  It captures the current global PluginVmFeatures object
// and replaces it for the scope of this object.  It overrides only the
// features that you set and uses the previous object for other features.
class FakePluginVmFeatures : public PluginVmFeatures {
 public:
  FakePluginVmFeatures();
  ~FakePluginVmFeatures() override;

  // PluginVmFeatures:
  bool IsAllowed(const Profile* profile, std::string* reason) override;
  bool IsConfigured(const Profile* profile) override;
  bool IsEnabled(const Profile* profile) override;

  void set_allowed(bool allowed, const std::string& reason) {
    allowed_ = allowed;
    disallowed_reason_ = reason;
  }
  void set_configured(bool configured) { configured_ = configured; }
  void set_enabled(bool enabled) { enabled_ = enabled; }

 private:
  // Original global static when this instance is created. It is captured when
  // FakePluginVmFeatures is created and replaced at destruction.
  PluginVmFeatures* original_features_;

  base::Optional<bool> allowed_;
  std::string disallowed_reason_;
  base::Optional<bool> configured_;
  base::Optional<bool> enabled_;
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_FAKE_PLUGIN_VM_FEATURES_H_
