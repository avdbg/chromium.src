// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/permission_chip.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

// This test is used to verify UI of dedicated row with screenshots verification
// from the base class. This cannot be reworked as a unit test, logic in
// VerifyUI is secondary and isn't as important as UI verification from
// screenshots.
class OmniboxSuggestionButtonRowBrowserTest : public DialogBrowserTest {
 public:
  OmniboxSuggestionButtonRowBrowserTest() {
    feature_list_.InitWithFeatures({omnibox::kOmniboxSuggestionButtonRow,
                                    omnibox::kOmniboxPedalSuggestions,
                                    omnibox::kOmniboxKeywordSearchButton},
                                   {});
  }

  OmniboxSuggestionButtonRowBrowserTest(
      const OmniboxSuggestionButtonRowBrowserTest&) = delete;
  OmniboxSuggestionButtonRowBrowserTest& operator=(
      const OmniboxSuggestionButtonRowBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    OmniboxViewViews* omnibox_view = GetOmniboxViewViews();
    ASSERT_TRUE(omnibox_view);

    // Populate suggestions for the omnibox popup.
    AutocompleteController* autocomplete_controller =
        omnibox_view->model()->popup_model()->autocomplete_controller();
    AutocompleteResult& results = autocomplete_controller->result_;
    ACMatches matches;
    TermMatches termMatches = {{0, 0, 0}};

    AutocompleteMatch search_match(nullptr, 500, false,
                                   AutocompleteMatchType::HISTORY_URL);
    search_match.allowed_to_be_default_match = true;
    search_match.contents = base::ASCIIToUTF16("https://footube.com");
    search_match.description = base::ASCIIToUTF16("The FooTube");
    search_match.contents_class = ClassifyTermMatches(
        termMatches, search_match.contents.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::URL);
    search_match.keyword = base::ASCIIToUTF16("match");
    search_match.associated_keyword = std::make_unique<AutocompleteMatch>();

    AutocompleteMatch switch_to_tab_match(nullptr, 500, false,
                                          AutocompleteMatchType::HISTORY_URL);
    switch_to_tab_match.contents = base::ASCIIToUTF16("https://foobar.com");
    switch_to_tab_match.description = base::ASCIIToUTF16("The Foo Of All Bars");
    switch_to_tab_match.contents_class = ClassifyTermMatches(
        termMatches, switch_to_tab_match.contents.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::URL);
    switch_to_tab_match.has_tab_match = true;

    AutocompleteMatch pedal_match(nullptr, 500, false,
                                  AutocompleteMatchType::SEARCH_SUGGEST);
    pedal_match.contents = base::ASCIIToUTF16("clear data");
    pedal_match.description = base::ASCIIToUTF16("Search");
    pedal_match.description_class = ClassifyTermMatches(
        termMatches, pedal_match.description.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::DIM);
    pedal_ = std::make_unique<OmniboxPedal>(
        OmniboxPedalId::CLEAR_BROWSING_DATA,
        OmniboxPedal::LabelStrings(
            IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
            IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT_SHORT,
            IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS,
            IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUFFIX,
            IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA),
        GURL());
    pedal_match.pedal = pedal_.get();

    AutocompleteMatch multiple_actions_match(
        nullptr, 500, false, AutocompleteMatchType::HISTORY_URL);
    multiple_actions_match.contents =
        base::ASCIIToUTF16("https://foobarzon.com");
    multiple_actions_match.description = base::ASCIIToUTF16("The FooBarZon");
    multiple_actions_match.contents_class = ClassifyTermMatches(
        termMatches, multiple_actions_match.contents.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::URL);
    multiple_actions_match.keyword = base::ASCIIToUTF16("match");
    multiple_actions_match.associated_keyword =
        std::make_unique<AutocompleteMatch>();
    multiple_actions_match.has_tab_match = true;

    matches.push_back(search_match);
    matches.push_back(switch_to_tab_match);
    matches.push_back(pedal_match);
    matches.push_back(multiple_actions_match);
    results.AppendMatches(autocomplete_controller->input_, matches);

    // The omnibox popup should open with suggestions displayed.
    omnibox_view->model()->popup_model()->OnResultChanged();
    EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());
  }

  bool VerifyUi() override {
    OmniboxPopupContentsView* popup_view =
        GetOmniboxViewViews()->GetPopupContentsViewForTesting();

    popup_view->model()->SetSelection(
        OmniboxPopupModel::Selection(0, OmniboxPopupModel::KEYWORD_MODE));
    if (!VerifyActiveButtonText(popup_view->result_view_at(0), "Search"))
      return false;

    popup_view->model()->SetSelection(OmniboxPopupModel::Selection(
        1, OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH));
    if (!VerifyActiveButtonText(popup_view->result_view_at(1), "Switch"))
      return false;

    popup_view->model()->SetSelection(OmniboxPopupModel::Selection(
        2, OmniboxPopupModel::FOCUSED_BUTTON_PEDAL));
    if (!VerifyActiveButtonText(popup_view->result_view_at(2), "Clear"))
      return false;

    popup_view->model()->SetSelection(
        OmniboxPopupModel::Selection(3, OmniboxPopupModel::KEYWORD_MODE));
    if (!VerifyActiveButtonText(popup_view->result_view_at(3), "Search"))
      return false;

    popup_view->model()->SetSelection(OmniboxPopupModel::Selection(
        3, OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH));
    if (!VerifyActiveButtonText(popup_view->result_view_at(3), "Switch"))
      return false;

    return DialogBrowserTest::VerifyUi();
  }

  std::string GetNonDialogName() override {
    return "RoundedOmniboxResultsFrameWindow";
  }

  OmniboxViewViews* GetOmniboxViewViews() {
    LocationBar* location_bar = browser()->window()->GetLocationBar();
    return static_cast<OmniboxViewViews*>(location_bar->GetOmniboxView());
  }

  bool VerifyActiveButtonText(OmniboxResultView* result_view,
                              std::string text) {
    views::LabelButton* button = static_cast<views::LabelButton*>(
        result_view->GetActiveAuxiliaryButtonForAccessibility());
    return button->GetText().find(base::ASCIIToUTF16(text)) !=
           std::string::npos;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<OmniboxPedal> pedal_;
};

IN_PROC_BROWSER_TEST_F(OmniboxSuggestionButtonRowBrowserTest, InvokeUi) {
  ShowAndVerifyUi();
}
