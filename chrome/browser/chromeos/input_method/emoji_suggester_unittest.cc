// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/emoji_suggester.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {

ui::KeyEvent CreateKeyEventFromCode(const ui::DomCode& code) {
  return ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, code, ui::EF_NONE,
                      ui::DomKey::NONE, ui::EventTimeForNow());
}

const char kEmojiData[] = "happy,😀;😃;😄";

class TestSuggestionHandler : public SuggestionHandlerInterface {
 public:
  bool SetButtonHighlighted(int context_id,
                            const ui::ime::AssistiveWindowButton& button,
                            bool highlighted,
                            std::string* error) override {
    switch (button.id) {
      case ui::ime::ButtonId::kLearnMore:
        learn_more_button_highlighted_ = highlighted;
        return true;
      case ui::ime::ButtonId::kSuggestion:
        // If highlighted, needs to unhighlight previously highlighted button.
        if (currently_highlighted_index_ != INT_MAX && highlighted) {
          candidate_highlighted_[currently_highlighted_index_] = 0;
        }
        currently_highlighted_index_ = highlighted ? button.index : INT_MAX;
        candidate_highlighted_[button.index] = highlighted ? 1 : 0;
        return true;
      default:
        return false;
    }
  }

  bool SetAssistiveWindowProperties(
      int context_id,
      const AssistiveWindowProperties& assistive_window,
      std::string* error) override {
    candidate_highlighted_.clear();
    for (size_t i = 0; i < assistive_window.candidates.size(); i++) {
      candidate_highlighted_.push_back(0);
    }
    show_indices_ = assistive_window.show_indices;
    show_setting_link_ = assistive_window.show_setting_link;
    return true;
  }

  void VerifyShowIndices(bool show_indices) {
    EXPECT_EQ(show_indices_, show_indices);
  }

  void VerifyLearnMoreButtonHighlighted(const bool highlighted) {
    EXPECT_EQ(learn_more_button_highlighted_, highlighted);
  }

  void VerifyCandidateHighlighted(const int index, const bool highlighted) {
    int expect = highlighted ? 1 : 0;
    EXPECT_EQ(candidate_highlighted_[index], expect);
  }

  void VerifyShowSettingLink(const bool show_setting_link) {
    EXPECT_EQ(show_setting_link_, show_setting_link);
  }

  bool DismissSuggestion(int context_id, std::string* error) override {
    return false;
  }

  bool AcceptSuggestion(int context_id, std::string* error) override {
    return false;
  }

  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {}

  void ClickButton(const ui::ime::AssistiveWindowButton& button) override {}

  bool AcceptSuggestionCandidate(int context_id,
                                 const base::string16& candidate,
                                 std::string* error) override {
    return false;
  }

  bool SetSuggestion(int context_id,
                     const ui::ime::SuggestionDetails& details,
                     std::string* error) override {
    return false;
  }

 private:
  bool show_indices_ = false;
  bool show_setting_link_ = false;
  bool learn_more_button_highlighted_ = false;
  std::vector<int> candidate_highlighted_;
  size_t currently_highlighted_index_ = INT_MAX;
};

class EmojiSuggesterTest : public testing::Test {
 protected:
  void SetUp() override {
    engine_ = std::make_unique<TestSuggestionHandler>();
    profile_ = std::make_unique<TestingProfile>();
    emoji_suggester_ =
        std::make_unique<EmojiSuggester>(engine_.get(), profile_.get());
    emoji_suggester_->LoadEmojiMapForTesting(kEmojiData);
    chrome_keyboard_controller_client_ =
        ChromeKeyboardControllerClient::CreateForTest();
    chrome_keyboard_controller_client_->set_keyboard_visible_for_test(false);
  }

  SuggestionStatus Press(ui::DomCode code) {
    return emoji_suggester_->HandleKeyEvent(CreateKeyEventFromCode(code));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmojiSuggester> emoji_suggester_;
  std::unique_ptr<TestSuggestionHandler> engine_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;
};

TEST_F(EmojiSuggesterTest, SuggestWhenStringEndsWithSpace) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
}

TEST_F(EmojiSuggesterTest, SuggestWhenStringStartsWithOpenBracket) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("(happy ")));
}

TEST_F(EmojiSuggesterTest, SuggestWhenStringEndsWithSpaceAndIsUppercase) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("HAPPY ")));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenStringEndsWithNewLine) {
  EXPECT_FALSE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy\n")));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenStringDoesNotEndWithSpace) {
  EXPECT_FALSE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy")));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenWordNotInMap) {
  EXPECT_FALSE(emoji_suggester_->Suggest(base::UTF8ToUTF16("hapy ")));
}

TEST_F(EmojiSuggesterTest, DoNotShowSuggestionWhenVirtualKeyboardEnabled) {
  chrome_keyboard_controller_client_->set_keyboard_visible_for_test(true);
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  EXPECT_FALSE(emoji_suggester_->GetSuggestionShownForTesting());
}

TEST_F(EmojiSuggesterTest, ReturnkBrowsingWhenPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  ui::KeyEvent event = CreateKeyEventFromCode(ui::DomCode::ARROW_DOWN);
  EXPECT_EQ(SuggestionStatus::kBrowsing,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkBrowsingWhenPressingUp) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  ui::KeyEvent event = CreateKeyEventFromCode(ui::DomCode::ARROW_UP);
  EXPECT_EQ(SuggestionStatus::kBrowsing,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkDismissWhenPressingEsc) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  ui::KeyEvent event = CreateKeyEventFromCode(ui::DomCode::ESCAPE);
  EXPECT_EQ(SuggestionStatus::kDismiss,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkNotHandledWhenPressDownThenValidNumber) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  ui::KeyEvent event1 = CreateKeyEventFromCode(ui::DomCode::ARROW_DOWN);
  emoji_suggester_->HandleKeyEvent(event1);
  ui::KeyEvent event2 = CreateKeyEventFromCode(ui::DomCode::DIGIT1);
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest, ReturnkNotHandledWhenPressDownThenNotANumber) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  ui::KeyEvent event1 = CreateKeyEventFromCode(ui::DomCode::ARROW_DOWN);
  emoji_suggester_->HandleKeyEvent(event1);
  ui::KeyEvent event2 = CreateKeyEventFromCode(ui::DomCode::US_A);
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest,
       ReturnkNotHandledWhenPressingEnterAndACandidateHasNotBeenChosen) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  ui::KeyEvent event = CreateKeyEventFromCode(ui::DomCode::ENTER);
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest,
       ReturnkAcceptWhenPressingEnterAndACandidateHasBeenChosenByPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  // Press ui::DomCode::ARROW_DOWN to choose a candidate.
  ui::KeyEvent event1 = CreateKeyEventFromCode(ui::DomCode::ARROW_DOWN);
  emoji_suggester_->HandleKeyEvent(event1);
  ui::KeyEvent event2 = CreateKeyEventFromCode(ui::DomCode::ENTER);
  EXPECT_EQ(SuggestionStatus::kAccept,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest, HighlightFirstCandidateWhenPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  Press(ui::DomCode::ARROW_DOWN);
  engine_->VerifyCandidateHighlighted(0, true);
}

TEST_F(EmojiSuggesterTest, HighlightButtonCorrectlyWhenPressingUp) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  // Go into the window.
  Press(ui::DomCode::ARROW_DOWN);

  // Press ui::DomCode::ARROW_UP to choose learn more button.
  Press(ui::DomCode::ARROW_UP);
  engine_->VerifyLearnMoreButtonHighlighted(true);

  // Press ui::DomCode::ARROW_UP to go through candidates;
  for (size_t i = emoji_suggester_->GetCandidatesSizeForTesting(); i > 0; i--) {
    Press(ui::DomCode::ARROW_UP);
    engine_->VerifyCandidateHighlighted(i - 1, true);
    engine_->VerifyLearnMoreButtonHighlighted(false);
    if (i != emoji_suggester_->GetCandidatesSizeForTesting()) {
      engine_->VerifyCandidateHighlighted(i, false);
    }
  }

  // Press ui::DomCode::ARROW_UP to go to learn more button from first
  // candidate.
  Press(ui::DomCode::ARROW_UP);
  engine_->VerifyLearnMoreButtonHighlighted(true);
}

TEST_F(EmojiSuggesterTest, HighlightButtonCorrectlyWhenPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  // Press ui::DomCode::ARROW_DOWN to go through candidates.
  for (size_t i = 0; i < emoji_suggester_->GetCandidatesSizeForTesting(); i++) {
    Press(ui::DomCode::ARROW_DOWN);
    engine_->VerifyCandidateHighlighted(i, true);
    engine_->VerifyLearnMoreButtonHighlighted(false);
    if (i != 0) {
      engine_->VerifyCandidateHighlighted(i - 1, false);
    }
  }

  // Go to LearnMore Button
  Press(ui::DomCode::ARROW_DOWN);
  engine_->VerifyLearnMoreButtonHighlighted(true);
  engine_->VerifyCandidateHighlighted(
      emoji_suggester_->GetCandidatesSizeForTesting() - 1, false);

  // Go to first candidate
  Press(ui::DomCode::ARROW_DOWN);
  engine_->VerifyLearnMoreButtonHighlighted(false);
  engine_->VerifyCandidateHighlighted(0, true);
}

TEST_F(EmojiSuggesterTest,
       OpenSettingWhenPressingEnterAndLearnMoreButtonIsChosen) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  // Go into the window.
  Press(ui::DomCode::ARROW_DOWN);
  // Choose Learn More Button.
  Press(ui::DomCode::ARROW_UP);
  engine_->VerifyLearnMoreButtonHighlighted(true);

  EXPECT_EQ(Press(ui::DomCode::ENTER), SuggestionStatus::kOpenSettings);
}

TEST_F(EmojiSuggesterTest, DoesNotShowIndicesWhenFirstSuggesting) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest, DoesNotShowIndexAfterPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  Press(ui::DomCode::ARROW_DOWN);

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest, DoesNotShowIndicesAfterGettingSuggestionsTwice) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest,
       DoesNotShowIndicesAfterPressingDownThenGetNewSuggestions) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  Press(ui::DomCode::ARROW_DOWN);
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest, ShowSettingLinkCorrectly) {
  for (int i = 0; i < kEmojiSuggesterShowSettingMaxCount; i++) {
    emoji_suggester_->Suggest(base::UTF8ToUTF16("happy "));
    // Dismiss suggestion.
    Press(ui::DomCode::ESCAPE);
    engine_->VerifyShowSettingLink(true);
  }
  emoji_suggester_->Suggest(base::UTF8ToUTF16("happy "));
  engine_->VerifyShowSettingLink(false);
}

TEST_F(EmojiSuggesterTest, RecordsTimeToAccept) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToAccept.Emoji",
                                    0);
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  // Press ui::DomCode::ARROW_DOWN to choose and accept a candidate.
  Press(ui::DomCode::ARROW_DOWN);
  Press(ui::DomCode::ENTER);
  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToAccept.Emoji",
                                    1);
}

TEST_F(EmojiSuggesterTest, RecordsTimeToDismiss) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToDismiss.Emoji",
                                    0);
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  // Press ui::DomCode::ESCAPE to dismiss.
  Press(ui::DomCode::ESCAPE);
  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToDismiss.Emoji",
                                    1);
}

}  // namespace chromeos
