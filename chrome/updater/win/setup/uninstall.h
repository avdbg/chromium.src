// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_SETUP_UNINSTALL_H_
#define CHROME_UPDATER_WIN_SETUP_UNINSTALL_H_

namespace updater {

int Uninstall(bool is_machine);

int UninstallCandidate(bool is_machine);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_SETUP_UNINSTALL_H_
