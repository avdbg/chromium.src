// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/fake_data_transfer_manager_interop.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/task/post_task.h"
#include "chrome/browser/webshare/win/fake_data_transfer_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webshare {

// static
bool FakeDataTransferManagerInterop::IsSupportedEnvironment() {
  return FakeDataTransferManager::IsSupportedEnvironment();
}

FakeDataTransferManagerInterop::FakeDataTransferManagerInterop() = default;

FakeDataTransferManagerInterop::~FakeDataTransferManagerInterop() {
  // Though it is legal for consuming code to hold on to a DataTransferManager
  // after releasing all references to the DataTransferManagerInterop, in a
  // test environment the DataTransferManagerInterop is only expected to be
  // torn down as part of the test cleanup, at which point the
  // DataTransferManager references should also have been cleaned up.
  for (auto& manager : managers_)
    EXPECT_EQ(0u, manager.second.Reset());
}

IFACEMETHODIMP
FakeDataTransferManagerInterop::GetForWindow(HWND app_window,
                                             REFIID riid,
                                             void** data_transfer_manager) {
  auto it = managers_.find(app_window);
  if (it != managers_.end()) {
    *data_transfer_manager = it->second.Get();
    it->second->AddRef();
  } else {
    auto mock = Microsoft::WRL::Make<FakeDataTransferManager>();
    managers_.insert({app_window, mock});
    *data_transfer_manager = mock.Get();
    mock->AddRef();
  }
  return S_OK;
}

IFACEMETHODIMP FakeDataTransferManagerInterop::ShowShareUIForWindow(
    HWND app_window) {
  auto it = managers_.find(app_window);
  if (it == managers_.end()) {
    ADD_FAILURE() << "ShowShareUIForWindow called for HWND with no "
                     "DataTransferManager (or DataRequested handler) defined.";
    return E_FAIL;
  }
  switch (show_share_ui_for_window_behavior_) {
    case ShowShareUIForWindowBehavior::FailImmediately:
      return E_FAIL;
    case ShowShareUIForWindowBehavior::InvokeEventSynchronously:
      std::move(it->second->GetDataRequestedInvoker()).Run();
      return S_OK;
    case ShowShareUIForWindowBehavior::InvokeEventSynchronouslyAndReturnFailure:
      std::move(it->second->GetDataRequestedInvoker()).Run();
      return E_FAIL;
    case ShowShareUIForWindowBehavior::ScheduleEvent:
      EXPECT_TRUE(base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                                 it->second->GetDataRequestedInvoker()));
      return S_OK;
    case ShowShareUIForWindowBehavior::SucceedWithoutAction:
      return S_OK;
  }
  NOTREACHED();
  return E_UNEXPECTED;
}

base::OnceClosure FakeDataTransferManagerInterop::GetDataRequestedInvoker(
    HWND app_window) {
  auto it = managers_.find(app_window);
  if (it == managers_.end()) {
    ADD_FAILURE() << "GetDataRequestedInvoker called when no DataRequested "
                     "event handler has been registered";
    return base::DoNothing();
  }
  return it->second->GetDataRequestedInvoker();
}

bool FakeDataTransferManagerInterop::HasDataRequestedListener(HWND app_window) {
  auto it = managers_.find(app_window);
  if (it == managers_.end())
    return false;
  return it->second->HasDataRequestedListener();
}

void FakeDataTransferManagerInterop::SetShowShareUIForWindowBehavior(
    ShowShareUIForWindowBehavior behavior) {
  show_share_ui_for_window_behavior_ = behavior;
}

}  // namespace webshare
