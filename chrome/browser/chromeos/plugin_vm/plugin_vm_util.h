// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace aura {
class Window;
}  // namespace aura

class Profile;
class GURL;

namespace plugin_vm {

class PluginVmPolicySubscription;

// This is used by both the Plugin VM app and its installer.
// Generated as crx_file::id_util::GenerateId("org.chromium.plugin_vm");
extern const char kPluginVmShelfAppId[];

// Name of the Plugin VM.
extern const char kPluginVmName[];

// Base directory for shared paths in Plugin VM, formatted for display.
extern const char kChromeOSBaseDirectoryDisplayText[];

const net::NetworkTrafficAnnotationTag kPluginVmNetworkTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("plugin_vm_image_download", R"(
      semantics {
        sender: "Plugin VM image manager"
        description: "Request to download Plugin VM image is sent in order "
          "to allow user to run Plugin VM."
        trigger: "User clicking on Plugin VM icon when Plugin VM is not yet "
          "installed."
        data: "Request to download Plugin VM image. Sends cookies to "
          "authenticate the user."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        chrome_policy {
          PluginVmImage {
            PluginVmImage: "{'url': 'example.com', 'hash': 'sha256hash'}"
          }
        }
      }
    )");

// Determines if the default Plugin VM is running and visible.
bool IsPluginVmRunning(Profile* profile);

void ShowPluginVmInstallerView(Profile* profile);

// Checks if an window is for the Plugin VM app. Note that it returns false for
// the Plugin VM installer.
bool IsPluginVmAppWindow(const aura::Window* window);

// Retrieves the license key to be used for Plugin VM. If
// none is set this will return an empty string.
std::string GetPluginVmLicenseKey();

// Retrieves the User Id to be used for Plugin VM. If none is set this will
// return an empty string.
std::string GetPluginVmUserIdForProfile(const Profile* profile);

// Sets fake policy values and enables Plugin VM for testing. These set global
// state so this should be called with empty strings on tear down.
// TODO(crbug.com/1025136): Remove this once Tast supports setting test
// policies.
void SetFakePluginVmPolicy(Profile* profile,
                           const std::string& image_path,
                           const std::string& image_hash,
                           const std::string& license_key);
bool FakeLicenseKeyIsSet();
bool FakeUserIdIsSet();

// Used to clean up the Plugin VM Drive download directory if it did not get
// removed when it should have, perhaps due to a crash.
void RemoveDriveDownloadDirectoryIfExists();

// Returns nullopt if not a drive URL.
base::Optional<std::string> GetIdFromDriveUrl(const GURL& url);

// A subscription for changes to PluginVm policy that may affect
// PluginVmFeatures::Get()->IsAllowed.
class PluginVmPolicySubscription {
 public:
  using PluginVmAllowedChanged = base::RepeatingCallback<void(bool is_allowed)>;
  PluginVmPolicySubscription(Profile* profile, PluginVmAllowedChanged callback);
  ~PluginVmPolicySubscription();

  PluginVmPolicySubscription(const PluginVmPolicySubscription&) = delete;
  PluginVmPolicySubscription& operator=(const PluginVmPolicySubscription&) =
      delete;

 private:
  // Internal callback for policy changes.
  void OnPolicyChanged();

  Profile* profile_;

  // Whether Plugin VM was previously allowed for the profile.
  bool is_allowed_;

  // The user-provided callback method.
  PluginVmAllowedChanged callback_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::CallbackListSubscription device_allowed_subscription_;
  base::CallbackListSubscription license_subscription_;
  base::CallbackListSubscription fake_license_subscription_;
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_
