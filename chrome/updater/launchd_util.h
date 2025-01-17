// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LAUNCHD_UTIL_H_
#define CHROME_UPDATER_LAUNCHD_UTIL_H_

#include <string>

#include "base/callback_forward.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace updater {

enum class LaunchctlPresence {
  kAbsent,
  kPresent,
};

// Query `launchctl list |service|` with retries, until either the
// `expectation` about the presence of `service` is met, or `timeout` is
// reached. Calls `callback` with 'true' if the expectation is met, and false
// if |timeout| is reached.  Must be called in a sequence. The callback is
// posted to the same sequence.
void PollLaunchctlList(const std::string& service,
                       LaunchctlPresence expectation,
                       base::TimeDelta timeout,
                       base::OnceCallback<void(bool)> callback);

}  // namespace updater

#endif  // CHROME_UPDATER_LAUNCHD_UTIL_H_
