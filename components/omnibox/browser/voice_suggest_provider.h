// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_VOICE_SUGGEST_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_VOICE_SUGGEST_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

// Autocomplete provider serving Voice suggestions on Android.
class VoiceSuggestProvider : public BaseSearchProvider {
 public:
  VoiceSuggestProvider(AutocompleteProviderClient* client,
                       AutocompleteProviderListener* listener);

  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;

  // Adds voice suggestion to the list of reported AutocompleteMatches.
  // The voice suggestion is next converted to a proper Search suggestion
  // associated with user-selected search engine, with a relevance score
  // computed from the match_score.
  void AddVoiceSuggestion(base::string16 match_text, float match_score);

  // Clear all cached voice matches.
  void ClearCache();

 private:
  // BaseSearchProvider:
  ~VoiceSuggestProvider() override;
  const TemplateURL* GetTemplateURL(bool is_keyword) const override;
  const AutocompleteInput GetInput(bool is_keyword) const override;
  bool ShouldAppendExtraParams(
      const SearchSuggestionParser::SuggestResult& result) const override;
  void RecordDeletionResult(bool success) override;

  // A list of voice matches and their confidence scores. The first element
  // indicates how confident the voice recognition system is about the accuracy
  // of the match, whereas the second element of the pair holds the match text
  // itself.
  // Multiple matches may hold the same confidence score and/or match text -
  // the score will next be used to filter out low-quality matches, and compute
  // the relevance score for matches.
  // Duplicate voice matches will be deduplicated automatically to the higher
  // ranked match.
  std::vector<std::pair<float, base::string16>> voice_matches_;

  // A pointer to the current AutocompleteInput, retained during the active
  // stage of operation only. Used by the BaseSearchProvider to construct the
  // final AutocompleteMatch objects.
  const AutocompleteInput* autocomplete_input_{};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_VOICE_SUGGEST_PROVIDER_H_
