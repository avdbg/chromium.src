// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/test_client_cert_saml_idp_mixin.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/https_forwarder.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

// Name of the "RelayState" URL parameter from the SAML specification.
constexpr char kSamlRelayStateUrlParam[] = "RelayState";

// URL path of the first SAML page. The FakeGaia will redirect the browser to
// this page when the sign-in for `kUserEmail` is started. This page will
// redirect to the second SAML page (see below).
constexpr char kSamlPageUrlPath[] = "saml-page";
// URL path of the second SAML page. This page is configured to authenticate the
// client via a client certificate.
constexpr char kSamlWithClientCertsPageUrlPath[] = "saml-client-cert-page";

// The response passed by the second SAML page to Gaia after successful
// authentication.
constexpr char kSamlResponse[] = "saml-response";

}  // namespace

TestClientCertSamlIdpMixin::TestClientCertSamlIdpMixin(
    InProcessBrowserTestMixinHost* host,
    FakeGaiaMixin* gaia_mixin,
    const std::vector<std::string>& client_cert_authorities)
    : InProcessBrowserTestMixin(host), gaia_mixin_(gaia_mixin) {
  saml_server_.RegisterRequestHandler(
      base::BindRepeating(&TestClientCertSamlIdpMixin::HandleSamlServerRequest,
                          base::Unretained(this)));

  // Set up `saml_with_client_certs_server_` to request a client certificate.
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  // TODO(crbug.com/792204): Enable TLS 1.3 after the
  // chrome.certificateProvider API supports it.
  ssl_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1_2;
  ssl_config.cert_authorities = client_cert_authorities;
  saml_with_client_certs_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK,
                                              ssl_config);
  saml_with_client_certs_server_.RegisterRequestHandler(base::BindRepeating(
      &TestClientCertSamlIdpMixin::HandleSamlWithClientCertsServerRequest,
      base::Unretained(this)));
}

TestClientCertSamlIdpMixin::~TestClientCertSamlIdpMixin() = default;

GURL TestClientCertSamlIdpMixin::GetSamlPageUrl() const {
  EXPECT_TRUE(saml_server_.Started());
  return saml_server_.GetURL(std::string("/") + kSamlPageUrlPath);
}

void TestClientCertSamlIdpMixin::SetUpOnMainThread() {
  ASSERT_TRUE(saml_server_.Start());
  ASSERT_TRUE(saml_with_client_certs_server_.Start());
}

std::unique_ptr<net::test_server::HttpResponse>
TestClientCertSamlIdpMixin::HandleSamlServerRequest(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().ExtractFileName() != kSamlPageUrlPath)
    return nullptr;

  // Extract the RelayState parameter specified by Gaia, so that we can pass
  // this value to subsequent SAML pages and finally back to Gaia.
  std::string saml_relay_state;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      request.GetURL(), kSamlRelayStateUrlParam, &saml_relay_state));

  // Redirect to the second SAML page.
  // TODO(crbug.com/1034451): Remove this HTML-based redirect (or even the
  // whole first SAML page) from the test once the Login Screen implementation
  // is fixed to support the client certificates on the very first SAML page.
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  GURL redirect_url = saml_with_client_certs_server_.GetURL(
      std::string("/") + kSamlWithClientCertsPageUrlPath);
  redirect_url = net::AppendQueryParameter(
      redirect_url, kSamlRelayStateUrlParam, saml_relay_state);
  response->set_content(base::ReplaceStringPlaceholders(
      R"(<!doctype html><html><head>
           <meta http-equiv="refresh" content="0; URL=$1" /></head></html>)",
      {redirect_url.spec()}, /*offsets=*/nullptr));
  return response;
}

std::unique_ptr<net::test_server::HttpResponse>
TestClientCertSamlIdpMixin::HandleSamlWithClientCertsServerRequest(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().ExtractFileName() != kSamlWithClientCertsPageUrlPath)
    return nullptr;
  // Obtain the RelayState parameter that was originally specified by Gaia.
  std::string saml_relay_state;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      request.GetURL(), kSamlRelayStateUrlParam, &saml_relay_state));
  // Redirect to the Gaia SAML assertion page.
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  response->AddCustomHeader("Location",
                            GetGaiaSamlAssertionUrl(saml_relay_state).spec());
  return response;
}

// Returns the URL to be used by the SAML page to redirect back to Gaia after
// the authentication completion.
GURL TestClientCertSamlIdpMixin::GetGaiaSamlAssertionUrl(
    const std::string& saml_relay_state) {
  GURL assertion_url =
      gaia_mixin_->gaia_https_forwarder()->GetURLForSSLHost("").Resolve("/SSO");
  assertion_url =
      net::AppendQueryParameter(assertion_url, "SAMLResponse", kSamlResponse);
  assertion_url = net::AppendQueryParameter(
      assertion_url, kSamlRelayStateUrlParam, saml_relay_state);
  return assertion_url;
}

}  // namespace chromeos
