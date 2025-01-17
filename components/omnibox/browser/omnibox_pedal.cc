// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal.h"

#include <algorithm>
#include <cctype>
#include <numeric>

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/l10n/l10n_util.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

namespace {
// Find and erase one or all instances of given sequence from |from| sequence.
bool EraseTokenSubsequence(OmniboxPedal::Tokens* from,
                           const OmniboxPedal::Tokens& erase_sequence,
                           bool erase_only_once) {
  bool changed = false;
  for (;;) {
    const auto found =
        std::search(from->begin(), from->end(), erase_sequence.begin(),
                    erase_sequence.end());
    if (found != from->end()) {
      from->erase(found, found + erase_sequence.size());
      changed = true;
      if (erase_only_once) {
        break;
      }
    } else {
      break;
    }
  }
  return changed;
}

}  // namespace

OmniboxPedal::LabelStrings::LabelStrings(int id_hint,
                                         int id_hint_short,
                                         int id_suggestion_contents,
                                         int id_accessibility_suffix,
                                         int id_accessibility_hint)
    : hint(l10n_util::GetStringUTF16(id_hint)),
      hint_short(l10n_util::GetStringUTF16(id_hint_short)),
      suggestion_contents(l10n_util::GetStringUTF16(id_suggestion_contents)),
      id_accessibility_suffix(id_accessibility_suffix),
      accessibility_hint(l10n_util::GetStringUTF16(id_accessibility_hint)) {}

OmniboxPedal::LabelStrings::LabelStrings(const LabelStrings&) = default;

OmniboxPedal::LabelStrings::~LabelStrings() = default;

// =============================================================================

OmniboxPedal::SynonymGroup::SynonymGroup(bool required,
                                         bool match_once,
                                         size_t reserve_size)
    : required_(required), match_once_(match_once) {
  synonyms_.reserve(reserve_size);
}

OmniboxPedal::SynonymGroup::SynonymGroup(SynonymGroup&&) = default;

OmniboxPedal::SynonymGroup::~SynonymGroup() = default;

OmniboxPedal::SynonymGroup& OmniboxPedal::SynonymGroup::operator=(
    SynonymGroup&&) = default;

bool OmniboxPedal::SynonymGroup::EraseMatchesIn(
    OmniboxPedal::Tokens* remaining) const {
  bool changed = false;
  for (const auto& synonym : synonyms_) {
    if (EraseTokenSubsequence(remaining, synonym, match_once_)) {
      changed = true;
      if (match_once_) {
        break;
      }
    }
  }
  return changed || !required_;
}

void OmniboxPedal::SynonymGroup::AddSynonym(OmniboxPedal::Tokens&& synonym) {
#if DCHECK_IS_ON()
  if (synonyms_.size() > size_t{0}) {
    DCHECK_GE(synonyms_.back().size(), synonym.size());
  }
#endif
  synonyms_.push_back(std::move(synonym));
}

size_t OmniboxPedal::SynonymGroup::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(synonyms_);
}

// =============================================================================

namespace base {
namespace trace_event {
size_t EstimateMemoryUsage(const OmniboxPedal::LabelStrings& self) {
  size_t total = 0;
  total += base::trace_event::EstimateMemoryUsage(self.hint);
  total += base::trace_event::EstimateMemoryUsage(self.hint_short);
  total += base::trace_event::EstimateMemoryUsage(self.suggestion_contents);
  total += base::trace_event::EstimateMemoryUsage(self.accessibility_hint);
  return total;
}
}  // namespace trace_event
}  // namespace base

// =============================================================================

OmniboxPedal::OmniboxPedal(OmniboxPedalId id, LabelStrings strings, GURL url)
    : id_(id), strings_(strings), url_(url) {}

OmniboxPedal::~OmniboxPedal() {}

const OmniboxPedal::LabelStrings& OmniboxPedal::GetLabelStrings() const {
  return strings_;
}

bool OmniboxPedal::IsNavigation() const {
  return !url_.is_empty();
}

const GURL& OmniboxPedal::GetNavigationUrl() const {
  return url_;
}

void OmniboxPedal::Execute(OmniboxPedal::ExecutionContext& context) const {
  DCHECK(IsNavigation());
  OpenURL(context, url_);
}

bool OmniboxPedal::IsReadyToTrigger(
    const AutocompleteInput& input,
    const AutocompleteProviderClient& client) const {
  return true;
}

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
const gfx::VectorIcon& OmniboxPedal::GetVectorIcon() const {
  return omnibox::kPedalIcon;
}
#endif

bool OmniboxPedal::IsTriggerMatch(const Tokens& match_sequence) const {
  return IsConceptMatch(match_sequence);
}

void OmniboxPedal::AddSynonymGroup(SynonymGroup&& group) {
  synonym_groups_.push_back(std::move(group));
}

size_t OmniboxPedal::EstimateMemoryUsage() const {
  size_t total = 0;
  total += base::trace_event::EstimateMemoryUsage(url_);
  total += base::trace_event::EstimateMemoryUsage(strings_);
  total += base::trace_event::EstimateMemoryUsage(synonym_groups_);
  return total;
}

bool OmniboxPedal::IsConceptMatch(const Tokens& match_sequence) const {
  Tokens remaining(match_sequence);
  for (const auto& group : synonym_groups_) {
    if (!group.EraseMatchesIn(&remaining))
      return false;
  }
  return remaining.empty();
}

void OmniboxPedal::OpenURL(OmniboxPedal::ExecutionContext& context,
                           const GURL& url) const {
  // destination_url_entered_without_scheme is used to determine whether
  // navigations typed without a scheme and upgraded to HTTPS should fall back
  // to HTTP. The URL might have been entered without a scheme, but pedal
  // destination URLs don't need a fallback so it's fine to pass false here.
  context.controller_.OnAutocompleteAccept(
      url, nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_GENERATED, AutocompleteMatchType::PEDAL,
      context.match_selection_timestamp_,
      /*destination_url_entered_without_scheme=*/false);
}
