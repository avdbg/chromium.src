// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/window_command_source.h"

#include <numeric>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/commander/entity_match.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"

namespace commander {

namespace {

// TODO(lgrey): Specifically not deduping this with BookmarkCommandSource right
// now since I'm not actually sure if we want the same threshold for different
// nouns.
size_t constexpr kNounFirstMinimum = 2;


// Activates `browser` if it's still present.
void SwitchToBrowser(base::WeakPtr<Browser> browser) {
  if (browser.get())
    browser->window()->Show();
}

// Merges all tabs from `source` into `target`, if they are both present.
void MergeBrowsers(base::WeakPtr<Browser> source,
                   base::WeakPtr<Browser> target) {
  if (!source.get() || !target.get())
    return;
  size_t source_count = source->tab_strip_model()->count();
  std::vector<int> indices(source_count);
  std::iota(indices.begin(), indices.end(), 0);
  chrome::MoveTabsToExistingWindow(source.get(), target.get(), indices);
}

// Returns browser windows whose titles fuzzy match `input`. If input is empty,
// returns all eligible browser windows with score reflecting MRU order.
// `browser_to_exclude` is excluded from the list, as are all browser windows
// from a different profile unless `match_profile` is false.

std::unique_ptr<CommandItem> CreateSwitchWindowItem(const WindowMatch& match) {
  auto item = match.ToCommandItem();
  item->command = base::BindOnce(&SwitchToBrowser, match.browser->AsWeakPtr());
  return item;
}

std::unique_ptr<CommandItem> CreateMergeWindowItem(Browser* source,
                                                   const WindowMatch& target) {
  auto item = target.ToCommandItem();
  item->command = base::BindOnce(&MergeBrowsers, source->AsWeakPtr(),
                                 target.browser->AsWeakPtr());
  return item;
}

CommandSource::CommandResults SwitchCommandsForWindowsMatching(
    Browser* browser_to_exclude,
    const base::string16& input) {
  CommandSource::CommandResults results;
  for (auto& match : WindowsMatchingInput(browser_to_exclude, input))
    results.push_back(CreateSwitchWindowItem(match));
  return results;
}

CommandSource::CommandResults MergeCommandsForWindowsMatching(
    Browser* source_browser,
    const base::string16& input) {
  CommandSource::CommandResults results;
  for (auto& match : WindowsMatchingInput(source_browser, input, true))
    results.push_back(CreateMergeWindowItem(source_browser, match));
  return results;
}

}  // namespace

WindowCommandSource::WindowCommandSource() = default;
WindowCommandSource::~WindowCommandSource() = default;

CommandSource::CommandResults WindowCommandSource::GetCommands(
    const base::string16& input,
    Browser* browser) const {
  CommandSource::CommandResults results;
  BrowserList* browser_list = BrowserList::GetInstance();
  if (browser_list->size() < 2)
    return results;
  if (input.size() >= kNounFirstMinimum) {
    results = SwitchCommandsForWindowsMatching(browser, input);
  }
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  // TODO(lgrey): Temporarily using untranslated strings since it's not
  // yet clear which commands will ship.
  base::string16 open_title = base::ASCIIToUTF16("Switch to window...");
  base::string16 merge_title =
      base::ASCIIToUTF16("Merge current window into...");

  double score = finder.Find(open_title, &ranges);
  if (score > 0) {
    auto verb = std::make_unique<CommandItem>(open_title, score, ranges);
    verb->command = std::make_pair(
        open_title, base::BindRepeating(&SwitchCommandsForWindowsMatching,
                                        base::Unretained(browser)));
    results.push_back(std::move(verb));
  }
  score = finder.Find(merge_title, &ranges);
  if (score > 0) {
    auto verb = std::make_unique<CommandItem>(merge_title, score, ranges);
    verb->command = std::make_pair(
        merge_title, base::BindRepeating(&MergeCommandsForWindowsMatching,
                                         base::Unretained(browser)));
    results.push_back(std::move(verb));
  }
  return results;
}
}  // namespace commander
