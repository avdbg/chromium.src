// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_verbatim_match_provider.h"

#include "base/feature_list.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/omnibox/common/omnibox_features.h"

namespace {
// The relevance score for verbatim match.
// Must outrank the QueryTiles relevance score.
const int kVerbatimMatchRelevanceScore = 1600;

// Returns whether specific context is eligible for a verbatim match.
// Only offer verbatim match on a site visit and SRP (no NTP etc).
bool IsVerbatimMatchEligible(
    metrics::OmniboxEventProto::PageClassification context) {
  // Only offer verbatim match on a site visit and SRP (no NTP etc).
  switch (context) {
    case metrics::OmniboxEventProto::
        SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT:
    case metrics::OmniboxEventProto::
        SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT:
      return base::FeatureList::IsEnabled(
          omnibox::kOmniboxSearchReadyIncognito);
    case metrics::OmniboxEventProto::ANDROID_SEARCH_WIDGET:
    case metrics::OmniboxEventProto::OTHER:
      return true;
    default:
      return false;
  }
}

}  // namespace

ZeroSuggestVerbatimMatchProvider::ZeroSuggestVerbatimMatchProvider(
    AutocompleteProviderClient* client)
    : AutocompleteProvider(TYPE_VERBATIM_MATCH), client_(client) {}

ZeroSuggestVerbatimMatchProvider::~ZeroSuggestVerbatimMatchProvider() = default;

void ZeroSuggestVerbatimMatchProvider::Start(const AutocompleteInput& input,
                                             bool minimal_changes) {
  Stop(true, false);
  if (!IsVerbatimMatchEligible(input.current_page_classification()))
    return;

  // Only offer verbatim match after the user just focused the Omnibox,
  // or if the input field is empty.
  if (input.focus_type() == OmniboxFocusType::DEFAULT)
    return;

  // For consistency with other zero-prefix providers.
  const auto& page_url = input.current_url();
  if (input.type() != metrics::OmniboxInputType::EMPTY &&
      !(page_url.is_valid() &&
        ((page_url.scheme() == url::kHttpScheme) ||
         (page_url.scheme() == url::kHttpsScheme) ||
         (page_url.scheme() == url::kAboutScheme) ||
         (page_url.scheme() ==
          client_->GetEmbedderRepresentationOfAboutScheme())))) {
    return;
  }

  AutocompleteInput verbatim_input = input;
  verbatim_input.set_prevent_inline_autocomplete(true);
  verbatim_input.set_allow_exact_keyword_match(false);

  AutocompleteMatch match = VerbatimMatchForURL(
      client_, verbatim_input, page_url, input.current_title(), nullptr,
      kVerbatimMatchRelevanceScore);

  // In the case of native pages, the classifier may replace the URL with an
  // empty content, resulting with a verbatim match that does not point
  // anywhere.
  if (!match.destination_url.is_valid())
    return;

  match.provider = this;
  matches_.push_back(match);
}

void ZeroSuggestVerbatimMatchProvider::Stop(bool clear_cached_results,
                                            bool due_to_user_inactivity) {
  matches_.clear();
}
