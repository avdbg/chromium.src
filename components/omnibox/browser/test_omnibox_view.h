// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_VIEW_H_
#define COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_VIEW_H_

#include <stddef.h>

#include "base/strings/string16.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ui/gfx/range/range.h"

struct AutocompleteMatch;
class OmniboxEditController;

// Fake implementation of OmniboxView for use in tests.
class TestOmniboxView : public OmniboxView {
 public:
  explicit TestOmniboxView(OmniboxEditController* controller)
      : OmniboxView(controller, nullptr) {}

  TestOmniboxView(const TestOmniboxView&) = delete;
  TestOmniboxView& operator=(const TestOmniboxView&) = delete;

  void SetModel(std::unique_ptr<OmniboxEditModel> model);

  const base::string16& inline_autocompletion() const {
    return inline_autocompletion_;
  }

  static State CreateState(std::string text,
                           size_t sel_start,
                           size_t sel_end,
                           size_t all_sel_length);

  // OmniboxView:
  void Update() override {}
  void OpenMatch(const AutocompleteMatch& match,
                 WindowOpenDisposition disposition,
                 const GURL& alternate_nav_url,
                 const base::string16& pasted_text,
                 size_t selected_line,
                 base::TimeTicks match_selection_timestamp) override {}
  base::string16 GetText() const override;
  void SetWindowTextAndCaretPos(const base::string16& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override {}
  void SetAdditionalText(const base::string16& text) override {}
  void EnterKeywordModeForDefaultSearchProvider() override {}
  bool IsSelectAll() const override;
  void GetSelectionBounds(size_t* start, size_t* end) const override;
  size_t GetAllSelectionsLength() const override;
  void SelectAll(bool reversed) override;
  void RevertAll() override {}
  void UpdatePopup() override {}
  void SetFocus(bool is_user_initiated) override {}
  void ApplyCaretVisibility() override {}
  void OnTemporaryTextMaybeChanged(const base::string16& display_text,
                                   const AutocompleteMatch& match,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  void OnInlineAutocompleteTextMaybeChanged(const base::string16& display_text,
                                            std::vector<gfx::Range> selections,
                                            size_t user_text_length) override;
  void OnInlineAutocompleteTextCleared() override;
  void OnRevertTemporaryText(const base::string16& display_text,
                             const AutocompleteMatch& match) override;
  void OnBeforePossibleChange() override {}
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetRelativeWindowForPopup() const override;
  bool IsImeComposing() const override;
  int GetOmniboxTextLength() const override;
  void EmphasizeURLComponents() override {}
  void SetEmphasis(bool emphasize, const gfx::Range& range) override {}
  void UpdateSchemeStyle(const gfx::Range& range) override {}
  using OmniboxView::GetStateChanges;

 private:
  base::string16 text_;
  base::string16 inline_autocompletion_;
  gfx::Range selection_;
  gfx::Range saved_temporary_selection_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_VIEW_H_
