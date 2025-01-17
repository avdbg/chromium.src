// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blocklist_manager.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/browser/url_blocklist_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"
#include "url/url_util.h"

using url_matcher::URLMatcher;
using url_matcher::URLMatcherCondition;
using url_matcher::URLMatcherConditionFactory;
using url_matcher::URLMatcherConditionSet;
using url_matcher::URLMatcherPortFilter;
using url_matcher::URLMatcherSchemeFilter;
using url_matcher::URLQueryElementMatcherCondition;

namespace policy {

using url_util::CreateConditionSet;
using url_util::FilterComponents;
using url_util::FilterToComponents;

namespace {

// List of schemes of URLs that should not be blocked by the "*" wildcard in
// the blocklist. Note that URLs with these schemes can still be blocked with
// a more specific filter e.g. "chrome-extension://*".
// The schemes are hardcoded here to avoid dependencies on //extensions and
// //chrome.
const char* kBypassBlocklistWildcardForSchemes[] = {
    // For internal extension URLs e.g. the Bookmark Manager and the File
    // Manager on Chrome OS.
    "chrome-extension",

    // NTP on Android.
    "chrome-native",

    // NTP on other platforms.
    "chrome-search",
};

#if defined(OS_IOS)
// The two schemes used on iOS for the NTP.
constexpr char kIosNtpAboutScheme[] = "about";
constexpr char kIosNtpChromeScheme[] = "chrome";
// The host string used on iOS for the NTP.
constexpr char kIosNtpHost[] = "newtab";
#endif

// Returns a blocklist based on the given |block| and |allow| pattern lists.
std::unique_ptr<URLBlocklist> BuildBlocklist(const base::ListValue* block,
                                             const base::ListValue* allow) {
  auto blocklist = std::make_unique<URLBlocklist>();
  blocklist->Block(block);
  blocklist->Allow(allow);
  return blocklist;
}

bool BypassBlocklistWildcardForURL(const GURL& url) {
  const std::string& scheme = url.scheme();
  for (const char* bypass_scheme : kBypassBlocklistWildcardForSchemes) {
    if (scheme == bypass_scheme)
      return true;
  }
#if defined(OS_IOS)
  // Compare the chrome scheme and host against the chrome://newtab version of
  // the NTP URL.
  if (scheme == kIosNtpChromeScheme && url.host() == kIosNtpHost) {
    return true;
  }
  // Compare the URL scheme and path to the about:newtab version of the NTP URL.
  // Leading and trailing slashes must be removed because the host name is
  // parsed as the URL path (which may contain slashes).
  base::StringPiece trimmed_path =
      base::TrimString(url.path_piece(), "/", base::TrimPositions::TRIM_ALL);
  if (scheme == kIosNtpAboutScheme && trimmed_path == kIosNtpHost) {
    return true;
  }
#endif
  return false;
}

}  // namespace

URLBlocklist::URLBlocklist() : id_(0), url_matcher_(new URLMatcher) {}

URLBlocklist::~URLBlocklist() = default;

void URLBlocklist::Block(const base::ListValue* filters) {
  url_util::AddFilters(url_matcher_.get(), false, &id_, filters, &filters_);
}

void URLBlocklist::Allow(const base::ListValue* filters) {
  url_util::AddFilters(url_matcher_.get(), true, &id_, filters, &filters_);
}

bool URLBlocklist::IsURLBlocked(const GURL& url) const {
  return URLBlocklist::GetURLBlocklistState(url) ==
         URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

URLBlocklist::URLBlocklistState URLBlocklist::GetURLBlocklistState(
    const GURL& url) const {
  std::set<URLMatcherConditionSet::ID> matching_ids =
      url_matcher_->MatchURL(url);

  const FilterComponents* max = nullptr;
  for (auto id = matching_ids.begin(); id != matching_ids.end(); ++id) {
    auto it = filters_.find(*id);
    DCHECK(it != filters_.end());
    const FilterComponents& filter = it->second;
    if (!max || FilterTakesPrecedence(filter, *max))
      max = &filter;
  }

  // Default neutral.
  if (!max)
    return URLBlocklist::URLBlocklistState::URL_NEUTRAL_STATE;

  // Some of the internal Chrome URLs are not affected by the "*" in the
  // blocklist. Note that the "*" is the lowest priority filter possible, so
  // any higher priority filter will be applied first.
  if (max->IsBlocklistWildcard() && BypassBlocklistWildcardForURL(url))
    return URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST;

  return max->allow ? URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST
                    : URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

size_t URLBlocklist::Size() const {
  return filters_.size();
}

// static
bool URLBlocklist::FilterTakesPrecedence(const FilterComponents& lhs,
                                         const FilterComponents& rhs) {
  // The "*" wildcard is the lowest priority filter.
  if (rhs.IsBlocklistWildcard())
    return true;

  if (lhs.match_subdomains && !rhs.match_subdomains)
    return false;
  if (!lhs.match_subdomains && rhs.match_subdomains)
    return true;

  size_t host_length = lhs.host.length();
  size_t other_host_length = rhs.host.length();
  if (host_length != other_host_length)
    return host_length > other_host_length;

  size_t path_length = lhs.path.length();
  size_t other_path_length = rhs.path.length();
  if (path_length != other_path_length)
    return path_length > other_path_length;

  if (lhs.number_of_key_value_pairs != rhs.number_of_key_value_pairs)
    return lhs.number_of_key_value_pairs > rhs.number_of_key_value_pairs;

  if (lhs.allow && !rhs.allow)
    return true;

  return false;
}

URLBlocklistManager::URLBlocklistManager(PrefService* pref_service)
    : pref_service_(pref_service), blocklist_(new URLBlocklist) {
  // This class assumes that it is created on the same thread that
  // |pref_service_| lives on.
  ui_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT});

  pref_change_registrar_.Init(pref_service_);
  base::RepeatingClosure callback = base::BindRepeating(
      &URLBlocklistManager::ScheduleUpdate, base::Unretained(this));
  pref_change_registrar_.Add(policy_prefs::kUrlBlocklist, callback);
  pref_change_registrar_.Add(policy_prefs::kUrlAllowlist, callback);

  // Start enforcing the policies without a delay when they are present at
  // startup.
  if (pref_service_->HasPrefPath(policy_prefs::kUrlBlocklist) ||
      pref_service_->HasPrefPath(policy_prefs::kUrlAllowlist)) {
    SetBlocklist(
        BuildBlocklist(pref_service_->GetList(policy_prefs::kUrlBlocklist),
                       pref_service_->GetList(policy_prefs::kUrlAllowlist)));
  }
}

URLBlocklistManager::~URLBlocklistManager() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  pref_change_registrar_.RemoveAll();
}

void URLBlocklistManager::ScheduleUpdate() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  // Cancel pending updates, if any. This can happen if two preferences that
  // change the blocklist are updated in one message loop cycle. In those cases,
  // only rebuild the blocklist after all the preference updates are processed.
  ui_weak_ptr_factory_.InvalidateWeakPtrs();
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&URLBlocklistManager::Update,
                                           ui_weak_ptr_factory_.GetWeakPtr()));
}

void URLBlocklistManager::Update() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  // The URLBlocklist is built in the background. Once it's ready, it is passed
  // to the URLBlocklistManager back on ui_task_runner_.
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &BuildBlocklist,
          base::Owned(
              pref_service_->GetList(policy_prefs::kUrlBlocklist)->DeepCopy()),
          base::Owned(
              pref_service_->GetList(policy_prefs::kUrlAllowlist)->DeepCopy())),
      base::BindOnce(&URLBlocklistManager::SetBlocklist,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void URLBlocklistManager::SetBlocklist(
    std::unique_ptr<URLBlocklist> blocklist) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  blocklist_ = std::move(blocklist);
}

bool URLBlocklistManager::IsURLBlocked(const GURL& url) const {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  // Ignore blob scheme for two reasons:
  // 1) Its used to deliver the response to the renderer.
  // 2) A page on the allowlist can use blob URLs internally.
  return !url.SchemeIs(url::kBlobScheme) && blocklist_->IsURLBlocked(url);
}

URLBlocklist::URLBlocklistState URLBlocklistManager::GetURLBlocklistState(
    const GURL& url) const {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  return blocklist_->GetURLBlocklistState(url);
}

// static
void URLBlocklistManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(policy_prefs::kUrlBlocklist);
  registry->RegisterListPref(policy_prefs::kUrlAllowlist);
  registry->RegisterIntegerPref(
      policy_prefs::kSafeSitesFilterBehavior,
      static_cast<int>(SafeSitesFilterBehavior::kSafeSitesFilterDisabled));
}

}  // namespace policy
