// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_FAKE_BROWSER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_FAKE_BROWSER_MANAGER_H_

#include "base/values.h"
#include "chrome/browser/chromeos/crosapi/browser_manager.h"

namespace crosapi {

// A fake implementation of BrowserManager, used for testing.
class FakeBrowserManager : public BrowserManager {
 public:
  FakeBrowserManager();
  FakeBrowserManager(const FakeBrowserManager&) = delete;
  FakeBrowserManager& operator=(const FakeBrowserManager&) = delete;

  ~FakeBrowserManager() override;

  void set_is_running(bool value) { is_running_ = value; }
  void set_wait_for_mojo_disconnect(bool value) {
    wait_for_mojo_disconnect_ = value;
  }

  // Set up response data to be sent for the callback of Fetch.
  void SetGetFeedbackDataResponse(base::Value response);

  // Simulates crosapi mojo disconnection event observed.
  void SignalMojoDisconnected();

  // BrowserManager:
  bool IsRunning() const override;
  bool IsRunningOrWillRun() const override;
  void GetFeedbackData(GetFeedbackDataCallback callback) override;

 private:
  // State indicating Lacros is running or not.
  bool is_running_ = false;

  // If this flag is set to true, simulate the case that mojo disconnect
  // signal is received before the log data is fetched.
  bool wait_for_mojo_disconnect_ = false;

  // Stores the response to be sent back for GetFeedbackData callback.
  base::Value feedback_response_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_FAKE_BROWSER_MANAGER_H_
