// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_BLOCKING_PAGE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_BLOCKING_PAGE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

// This class is responsible for showing/hiding the interstitial page that
// occurs when a new domain is visited and it looks suspiciously like another
// more popular domain.
class LookalikeUrlBlockingPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  LookalikeUrlBlockingPage(
      content::WebContents* web_contents,
      const GURL& safe_url,
      const GURL& request_url,
      ukm::SourceId source_id,
      LookalikeUrlMatchType match_type,
      bool is_signed_exchange,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller);

  ~LookalikeUrlBlockingPage() override;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

  bool is_signed_exchange_for_testing() const { return is_signed_exchange_; }

 protected:
  // SecurityInterstitialPage implementation:
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;
  void OnInterstitialClosing() override;
  bool ShouldDisplayURL() const override;
  int GetHTMLTemplateId() override;

 private:
  friend class LookalikeUrlNavigationThrottleBrowserTest;

  // The URL suggested to the user as the safe URL. Can be empty, in which case
  // the default action on the interstitial takes the user to the new tab page.
  const GURL safe_url_;
  ukm::SourceId source_id_;
  LookalikeUrlMatchType match_type_;
  // True if the throttle encountered a response with
  // is_signed_exchange_inner_response flag. Only checked in tests.
  const bool is_signed_exchange_;

  DISALLOW_COPY_AND_ASSIGN(LookalikeUrlBlockingPage);
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_BLOCKING_PAGE_H_
