// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_INFOBAR_DELEGATE_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace translate {

namespace testing {

class MockLanguageModel : public language::LanguageModel {
  std::vector<LanguageDetails> GetLanguages() override;
};

class MockTranslateInfoBarDelegate
    : public translate::TranslateInfoBarDelegate {
 public:
  MockTranslateInfoBarDelegate(
      const base::WeakPtr<translate::TranslateManager>& translate_manager,
      bool is_off_the_record,
      translate::TranslateStep step,
      const std::string& original_language,
      const std::string& target_language,
      translate::TranslateErrors::Type error_type,
      bool triggered_from_menu);
  ~MockTranslateInfoBarDelegate() override;

  MOCK_CONST_METHOD0(num_languages, size_t());
  MOCK_CONST_METHOD1(language_code_at, std::string(size_t index));
  MOCK_CONST_METHOD1(language_name_at, base::string16(size_t index));
  MOCK_CONST_METHOD0(original_language_name, base::string16());
  MOCK_CONST_METHOD0(ShouldAlwaysTranslate, bool());
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
  MOCK_METHOD0(InfoBarDismissed, void());
  MOCK_METHOD0(Translate, void());
  MOCK_METHOD0(ToggleAlwaysTranslate, void());
  MOCK_METHOD0(ToggleTranslatableLanguageByPrefs, void());
  MOCK_METHOD0(ToggleNeverPrompt, void());
  MOCK_METHOD0(RevertWithoutClosingInfobar, void());
  MOCK_METHOD1(UpdateTargetLanguage, void(const std::string& language_code));
  MOCK_METHOD1(UpdateOriginalLanguage, void(const std::string& language_code));

  void GetLanguagesNames(std::vector<base::string16>* languages) const override;
  void GetLanguagesCodes(
      std::vector<std::string>* languages_codes) const override;
  void SetTranslateLanguagesForTest(
      std::vector<std::pair<std::string, base::string16>> languages);

  void SetContentLanguagesForTest(std::vector<LanguageNameTriple> languages);
  void GetContentLanguagesNames(
      std::vector<base::string16>* languages) const override;
  void GetContentLanguagesNativeNames(
      std::vector<base::string16>* native_languages) const override;
  void GetContentLanguagesCodes(std::vector<std::string>* codes) const override;

 private:
  std::vector<std::pair<std::string, base::string16>> languages_;
  std::vector<LanguageNameTriple> content_languages_;
};

class MockTranslateInfoBarDelegateFactory {
 public:
  MockTranslateInfoBarDelegateFactory(const std::string& original_language,
                                      const std::string& target_language);
  ~MockTranslateInfoBarDelegateFactory();

  std::unique_ptr<MockTranslateInfoBarDelegate>
  CreateMockTranslateInfoBarDelegate(translate::TranslateStep step);

  MockTranslateInfoBarDelegate* GetMockTranslateInfoBarDelegate() {
    return delegate_.get();
  }

 private:
  MockTranslateDriver driver_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<MockTranslateClient> client_;
  std::unique_ptr<MockTranslateRanker> ranker_;
  std::unique_ptr<MockLanguageModel> language_model_;
  std::unique_ptr<translate::TranslateManager> manager_;
  std::unique_ptr<MockTranslateInfoBarDelegate> delegate_;
};

}  // namespace testing

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_INFOBAR_DELEGATE_H_
