// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DLP_FAKE_DLP_CLIENT_H_
#define CHROMEOS_DBUS_DLP_FAKE_DLP_CLIENT_H_

#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "dbus/object_proxy.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeDlpClient
    : public DlpClient,
      public DlpClient::TestInterface {
 public:
  FakeDlpClient();
  FakeDlpClient(const FakeDlpClient&) = delete;
  FakeDlpClient& operator=(const FakeDlpClient&) = delete;
  ~FakeDlpClient() override;

  // DlpClient implementation:
  void SetDlpFilesPolicy(const dlp::SetDlpFilesPolicyRequest request,
                         SetDlpFilesPolicyCallback callback) override;
  DlpClient::TestInterface* GetTestInterface() override;

  // DlpClient::TestInterface implementation:
  int GetSetDlpFilesPolicyCount() const override;

 private:
  int set_dlp_files_policy_count_ = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DLP_FAKE_DLP_CLIENT_H_
