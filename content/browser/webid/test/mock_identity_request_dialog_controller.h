// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include "content/public/browser/identity_request_dialog_controller.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockIdentityRequestDialogController
    : public IdentityRequestDialogController {
 public:
  MockIdentityRequestDialogController();

  ~MockIdentityRequestDialogController() override;

  MockIdentityRequestDialogController(
      const MockIdentityRequestDialogController&) = delete;
  MockIdentityRequestDialogController& operator=(
      const MockIdentityRequestDialogController&) = delete;

  MOCK_METHOD3(ShowInitialPermissionDialog,
               void(WebContents*, const GURL&, InitialApprovalCallback));
  MOCK_METHOD4(ShowIdProviderWindow,
               void(WebContents*,
                    WebContents*,
                    const GURL&,
                    IdProviderWindowClosedCallback));
  MOCK_METHOD0(CloseIdProviderWindow, void());
  MOCK_METHOD3(ShowTokenExchangePermissionDialog,
               void(content::WebContents*,
                    const GURL&,
                    TokenExchangeApprovalCallback));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
