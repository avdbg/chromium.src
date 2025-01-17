// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "third_party/microsoft_webauthn/webauthn.h"
#endif  // defined(OS_WIN)

#if defined(OS_MAC)
#include "device/fido/mac/authenticator_config.h"
#endif  // defined(OS_MAC)

class ChromeAuthenticatorRequestDelegateTest
    : public ChromeRenderViewHostTestHarness {};

class TestAuthenticatorModelObserver
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit TestAuthenticatorModelObserver(
      AuthenticatorRequestDialogModel* model)
      : model_(model) {
    last_step_ = model_->current_step();
  }

  AuthenticatorRequestDialogModel::Step last_step() { return last_step_; }

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override { last_step_ = model_->current_step(); }

  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
    model_ = nullptr;
  }

 private:
  AuthenticatorRequestDialogModel* model_;
  AuthenticatorRequestDialogModel::Step last_step_;
};

TEST_F(ChromeAuthenticatorRequestDelegateTest, TestTransportPrefType) {
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  EXPECT_FALSE(delegate.GetLastTransportUsed());
  delegate.UpdateLastTransportUsed(device::FidoTransportProtocol::kInternal);
  const auto transport = delegate.GetLastTransportUsed();
  ASSERT_TRUE(transport);
  EXPECT_EQ(device::FidoTransportProtocol::kInternal, transport);
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, ConditionalUI) {
  // Enabling conditional mode should cause the modal dialog to stay hidden at
  // the beginning of a request. An omnibar icon might be shown instead.
  for (bool conditional_ui : {true, false}) {
    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetConditionalRequest(conditional_ui);
    delegate.SetRelyingPartyId(/*rp_id=*/"example.com");
    AuthenticatorRequestDialogModel* model = delegate.dialog_model();
    TestAuthenticatorModelObserver observer(model);
    model->AddObserver(&observer);
    EXPECT_EQ(observer.last_step(),
              AuthenticatorRequestDialogModel::Step::kNotStarted);
    delegate.OnTransportAvailabilityEnumerated(
        AuthenticatorRequestDialogModel::TransportAvailabilityInfo());
    EXPECT_EQ(observer.last_step() ==
                  AuthenticatorRequestDialogModel::Step::kSubtleUI,
              conditional_ui);
  }
}

#if defined(OS_MAC)
API_AVAILABLE(macos(10.12.2))
std::string TouchIdMetadataSecret(
    ChromeAuthenticatorRequestDelegate* delegate) {
  return delegate->GetTouchIdAuthenticatorConfig()->metadata_secret;
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, TouchIdMetadataSecret) {
  if (__builtin_available(macOS 10.12.2, *)) {
    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    std::string secret = TouchIdMetadataSecret(&delegate);
    EXPECT_EQ(secret.size(), 32u);
    EXPECT_EQ(secret, TouchIdMetadataSecret(&delegate));
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_EqualForSameProfile) {
  if (__builtin_available(macOS 10.12.2, *)) {
    // Different delegates on the same BrowserContext (Profile) should return
    // the same secret.
    ChromeAuthenticatorRequestDelegate delegate1(main_rfh());
    ChromeAuthenticatorRequestDelegate delegate2(main_rfh());
    EXPECT_EQ(TouchIdMetadataSecret(&delegate1),
              TouchIdMetadataSecret(&delegate2));
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_NotEqualForDifferentProfiles) {
  if (__builtin_available(macOS 10.12.2, *)) {
    // Different profiles have different secrets. (No way to reset
    // browser_context(), so we have to create our own.)
    auto browser_context = CreateBrowserContext();
    auto web_contents = content::WebContentsTester::CreateTestWebContents(
        browser_context.get(), nullptr);
    ChromeAuthenticatorRequestDelegate delegate1(main_rfh());
    ChromeAuthenticatorRequestDelegate delegate2(web_contents->GetMainFrame());
    EXPECT_NE(TouchIdMetadataSecret(&delegate1),
              TouchIdMetadataSecret(&delegate2));
    // Ensure this second secret is actually valid.
    EXPECT_EQ(32u, TouchIdMetadataSecret(&delegate2).size());
  }
}
#endif  // defined(OS_MAC)

#if defined(OS_WIN)

static constexpr char kRelyingPartyID[] = "example.com";

// Tests that ShouldReturnAttestation() returns with true if |authenticator|
// is the Windows native WebAuthn API with WEBAUTHN_API_VERSION_2 or higher,
// where Windows prompts for attestation in its own native UI.
//
// Ideally, this would also test the inverse case, i.e. that with
// WEBAUTHN_API_VERSION_1 Chrome's own attestation prompt is shown. However,
// there seems to be no good way to test AuthenticatorRequestDialogModel UI.
TEST_F(ChromeAuthenticatorRequestDelegateTest, ShouldPromptForAttestationWin) {
  ::device::FakeWinWebAuthnApi win_webauthn_api;
  win_webauthn_api.set_version(WEBAUTHN_API_VERSION_2);
  ::device::WinWebAuthnApiAuthenticator authenticator(
      /*current_window=*/nullptr, &win_webauthn_api);

  ::device::test::ValueCallbackReceiver<bool> cb;
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.ShouldReturnAttestation(kRelyingPartyID, &authenticator,
                                   /*is_enterprise_attestation=*/false,
                                   cb.callback());
  cb.WaitForCallback();
  EXPECT_EQ(cb.value(), true);
}

#endif  // defined(OS_WIN)
