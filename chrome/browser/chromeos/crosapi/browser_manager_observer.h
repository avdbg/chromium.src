// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace crosapi {

class BrowserManagerObserver : public base::CheckedObserver {
 public:
  // Invoked when the Mojo connection to lacros-chrome is disconnected.
  virtual void OnMojoDisconnected() {}
  // Invoked when lacros-chrome state changes, without specifying the state.
  virtual void OnStateChanged() {}
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_MANAGER_OBSERVER_H_
