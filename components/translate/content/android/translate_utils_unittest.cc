// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/android/translate_utils.h"

#include "base/android/jni_array.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/metrics/metrics_log.h"
#include "components/translate/core/browser/mock_translate_infobar_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::Test;

using translate::testing::MockTranslateInfoBarDelegate;
using translate::testing::MockTranslateInfoBarDelegateFactory;

namespace translate {
namespace {
const std::vector<base::string16> kLanguageNames = {
    base::UTF8ToUTF16("English"), base::UTF8ToUTF16("German"),
    base::UTF8ToUTF16("Polish")};
const std::vector<base::string16> kNativeNames = {base::UTF8ToUTF16("English"),
                                                  base::UTF8ToUTF16("Deutsch"),
                                                  base::UTF8ToUTF16("polski")};
const std::vector<std::string> kCodes = {"en", "de", "pl"};
const std::vector<int> kHashCodes = {metrics::MetricsLog::Hash("en"),
                                     metrics::MetricsLog::Hash("de"),
                                     metrics::MetricsLog::Hash("pl")};

}  // namespace
class TranslateUtilsTest : public ::testing::Test {
 public:
  TranslateUtilsTest() = default;

 protected:
  void SetUp() override {
    delegate_factory_ = new MockTranslateInfoBarDelegateFactory("en", "pl");
    delegate_ = delegate_factory_->GetMockTranslateInfoBarDelegate();
    env_ = base::android::AttachCurrentThread();
  }

  MockTranslateInfoBarDelegateFactory* delegate_factory_;
  MockTranslateInfoBarDelegate* delegate_;
  JNIEnv* env_;
};
// Tests that content languages information in the java format is correct for
// content languages (names, native names, codes are as expected, hashcodes are
// empty).
TEST_F(TranslateUtilsTest, GetJavaContentLangauges) {
  // Set up the mock delegate.
  LanguageNameTriple en;
  en.name = base::UTF8ToUTF16("English");
  en.native_name = base::UTF8ToUTF16("English");
  en.code = "en";
  LanguageNameTriple de;
  de.name = base::UTF8ToUTF16("German");
  de.native_name = base::UTF8ToUTF16("Deutsch");
  de.code = "de";
  LanguageNameTriple pl;
  pl.name = base::UTF8ToUTF16("Polish");
  pl.native_name = base::UTF8ToUTF16("polski");
  pl.code = "pl";
  std::vector<LanguageNameTriple> test_languages = {en, de, pl};
  delegate_->SetContentLanguagesForTest(test_languages);

  JavaLanguageInfoWrapper contentLanguages =
      TranslateUtils::GetContentLanguagesInJavaFormat(env_, delegate_);

  // Test language names are as expected.
  std::vector<base::string16> actual_language_names;
  base::android::AppendJavaStringArrayToStringVector(
      env_, contentLanguages.java_languages, &actual_language_names);
  EXPECT_THAT(actual_language_names, ::testing::ContainerEq(kLanguageNames));

  // Test content language names are as expected.
  std::vector<base::string16> actual_native_names;
  base::android::AppendJavaStringArrayToStringVector(
      env_, contentLanguages.java_native_languages, &actual_native_names);
  EXPECT_THAT(actual_native_names, ::testing::ContainerEq(kNativeNames));

  // Test language codes are as expected.
  std::vector<std::string> actual_codes;
  base::android::AppendJavaStringArrayToStringVector(
      env_, contentLanguages.java_codes, &actual_codes);
  EXPECT_THAT(actual_codes, ::testing::ContainerEq(kCodes));
}

// Tests that application handles empty content language data gracefully.
TEST_F(TranslateUtilsTest, GetJavaContentLangaugesEmpty) {
  std::vector<LanguageNameTriple> empty;
  delegate_->SetContentLanguagesForTest(empty);
  JavaLanguageInfoWrapper contentLanguages =
      TranslateUtils::GetContentLanguagesInJavaFormat(env_, delegate_);

  // Test language names are empty.
  std::vector<base::string16> actual_language_names;
  base::android::AppendJavaStringArrayToStringVector(
      env_, contentLanguages.java_languages, &actual_language_names);
  ASSERT_TRUE(actual_language_names.empty());

  // Test content language names are empty.
  std::vector<base::string16> actual_native_names;
  base::android::AppendJavaStringArrayToStringVector(
      env_, contentLanguages.java_native_languages, &actual_native_names);
  ASSERT_TRUE(actual_native_names.empty());

  // Test language codes are empty.
  std::vector<std::string> actual_codes;
  base::android::AppendJavaStringArrayToStringVector(
      env_, contentLanguages.java_codes, &actual_codes);
  ASSERT_TRUE(actual_codes.empty());
}

// Test that language information in the java format is correct for all
// translate languages (names, codes and hashcodes are as expected, no native
// names).
TEST_F(TranslateUtilsTest, GetJavaLangauges) {
  std::vector<std::pair<std::string, base::string16>> translate_languages = {
      {"en", base::UTF8ToUTF16("English")},
      {"de", base::UTF8ToUTF16("German")},
      {"pl", base::UTF8ToUTF16("Polish")}};
  delegate_->SetTranslateLanguagesForTest(translate_languages);
  // Test that all languages in Java format are returned property.
  JavaLanguageInfoWrapper contentLanguages =
      TranslateUtils::GetTranslateLanguagesInJavaFormat(env_, delegate_);

  // Test language names are as expected.
  std::vector<base::string16> actual_language_names;
  base::android::AppendJavaStringArrayToStringVector(
      env_, contentLanguages.java_languages, &actual_language_names);
  EXPECT_THAT(actual_language_names, ::testing::ContainerEq(kLanguageNames));

  // Test native names are empty for all languages.
  std::vector<base::string16> actual_native_names;
  base::android::AppendJavaStringArrayToStringVector(
      env_, contentLanguages.java_native_languages, &actual_native_names);
  ASSERT_TRUE(actual_native_names.empty());

  // Test language codes
  std::vector<std::string> actual_codes;
  base::android::AppendJavaStringArrayToStringVector(
      env_, contentLanguages.java_codes, &actual_codes);
  EXPECT_THAT(actual_codes, ::testing::ContainerEq(kCodes));

  std::vector<int> actual_hash_codes;
  base::android::JavaIntArrayToIntVector(env_, contentLanguages.java_hash_codes,
                                         &actual_hash_codes);
  EXPECT_THAT(actual_hash_codes, ::testing::ContainerEq(kHashCodes));
}

}  // namespace translate
