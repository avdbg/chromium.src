// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/digital_asset_links_cross_validator.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/clock.h"
#include "chrome/browser/profiles/profile.h"

namespace {
const char* kDigitalAssetLinkRecordType = "lookalikes/allowlist";

void RecordUMA(DigitalAssetLinkCrossValidator::Event event) {
  base::UmaHistogramEnumeration(
      DigitalAssetLinkCrossValidator::kEventHistogramName, event);
}
}

// static
const char DigitalAssetLinkCrossValidator::kEventHistogramName[] =
    "NavigationSuggestion.DigitalAssetLinks.Event";

DigitalAssetLinkCrossValidator::DigitalAssetLinkCrossValidator(
    Profile* profile,
    const url::Origin& lookalike_domain,
    const url::Origin& target_domain,
    base::TimeDelta timeout,
    base::Clock* clock,
    ResultCallback callback)
    : lookalike_domain_(lookalike_domain),
      target_domain_(target_domain),
      timeout_(timeout),
      clock_(clock),
      callback_(std::move(callback)),
      asset_link_handler_(
          std::make_unique<digital_asset_links::DigitalAssetLinksHandler>(
              profile->GetURLLoaderFactory())) {}

DigitalAssetLinkCrossValidator::~DigitalAssetLinkCrossValidator() = default;

void DigitalAssetLinkCrossValidator::Start() {
  RecordUMA(Event::kStarted);
  // Fetch and validate the manifest from the lookalike site.
  start_time_ = clock_->Now();
  asset_link_handler_->SetTimeoutDuration(timeout_);
  asset_link_handler_->CheckDigitalAssetLinkRelationship(
      lookalike_domain_.Serialize(), kDigitalAssetLinkRecordType, base::nullopt,
      {{"namespace", "web"}, {"site", target_domain_.Serialize()}},
      base::BindOnce(
          &DigitalAssetLinkCrossValidator::OnFetchLookalikeManifestComplete,
          base::Unretained(this)));
}

void DigitalAssetLinkCrossValidator::OnFetchLookalikeManifestComplete(
    digital_asset_links::RelationshipCheckResult result) {
  // Fail if the first manifest failed or we reached the timeout.
  const base::Time now = clock_->Now();
  const base::TimeDelta elapsed = now - start_time_;
  // Do the timeout check regardless of the result. This is to make testing
  // timeouts possible:
  // - DigitalAssetLinksHandler uses a SimpleURLLoader to load the URLs.
  // - SimpleURLLoader supports timeouts via a OneShotTimer and can take
  // an external clock source.
  // - However, once the URL load starts, we can't control its OneShotTimer, so
  // we can't force SimpleURLLoader to timeout in tests.
  // As a result, we check the elapsed time in addition to the URL load result
  // and record a timeout metric in that case.
  if (result != digital_asset_links::RelationshipCheckResult::kSuccess ||
      elapsed >= timeout_) {
    RecordUMA(elapsed >= timeout_ ? Event::kLookalikeManifestTimedOut
                                  : Event::kLookalikeManifestFailed);
    std::move(callback_).Run(false);
    return;
  }

  // Swap current and target domains and validate the new manifest.
  start_time_ = now;
  target_manifest_timeout_ = timeout_ - elapsed;
  asset_link_handler_->SetTimeoutDuration(target_manifest_timeout_);
  asset_link_handler_->CheckDigitalAssetLinkRelationship(
      target_domain_.Serialize(), kDigitalAssetLinkRecordType, base::nullopt,
      {{"namespace", "web"}, {"site", lookalike_domain_.Serialize()}},
      base::BindOnce(
          &DigitalAssetLinkCrossValidator::OnFetchTargetManifestComplete,
          base::Unretained(this)));
}

void DigitalAssetLinkCrossValidator::OnFetchTargetManifestComplete(
    digital_asset_links::RelationshipCheckResult result) {
  const base::TimeDelta elapsed = clock_->Now() - start_time_;
  bool success =
      result == digital_asset_links::RelationshipCheckResult::kSuccess;
  if (elapsed > target_manifest_timeout_) {
    RecordUMA(Event::kTargetManifestTimedOut);
    std::move(callback_).Run(false);
    return;
  }
  if (success) {
    UmaHistogramTimes("NavigationSuggestion.DigitalAssetLinks.ValidationTime",
                      elapsed);
    RecordUMA(Event::kValidationSucceeded);
    std::move(callback_).Run(true);
    return;
  }
  // We can differentiate between failures and timeouts by checking if the
  // elapsed time is close to the total timeout, but bucket these events
  // together for simplicity.
  RecordUMA(Event::kTargetManifestFailed);
  std::move(callback_).Run(false);
}
