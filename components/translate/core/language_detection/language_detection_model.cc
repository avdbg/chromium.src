// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_model.h"

#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/utf_string_conversions.h"
#include "components/language/core/common/language_util.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/language_detection/language_detection_resolver.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "third_party/tflite-support/src/tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"

namespace {

struct sort_category {
  inline bool operator()(const tflite::task::core::Category& c1,
                         const tflite::task::core::Category& c2) {
    return c1.score > c2.score;
  }
};

// TODO(crbug.com/1175942): Make the threshold Finch controllable for
// experimentation.
constexpr float kDefaultReliabilityThreshold = .7;

// The number of characters to sample and provide as a buffer to the model
// for determining its language.
constexpr int kTextSampleLength = 250;

// The number of samples of |kTextSampleLength| to evaluate the model when
// determining the language of the page content.
constexpr int kNumTextSamples = 3;

}  // namespace

namespace {

constexpr char kTFLiteModelVersion[] = "TFLite_v1";

// Util class for recording the result of loading the detection model. The
// result is recorded when it goes out of scope and its destructor is called.
class ScopedLanguageDetectionModelStateRecorder {
 public:
  explicit ScopedLanguageDetectionModelStateRecorder(
      translate::LanguageDetectionModelState state)
      : state_(state) {}
  ~ScopedLanguageDetectionModelStateRecorder() {
    UMA_HISTOGRAM_ENUMERATION(
        "LanguageDetection.TFLiteModel.LanguageDetectionModelState", state_);
  }

  void set_state(translate::LanguageDetectionModelState state) {
    state_ = state;
  }

 private:
  translate::LanguageDetectionModelState state_;
};

}  // namespace

namespace translate {

LanguageDetectionModel::LanguageDetectionModel() = default;

LanguageDetectionModel::~LanguageDetectionModel() = default;

void LanguageDetectionModel::UpdateWithFile(base::File model_file) {
  ScopedLanguageDetectionModelStateRecorder recorder(
      LanguageDetectionModelState::kModelFileInvalid);

  if (!model_file.IsValid())
    return;

  if (!model_fb_.Initialize(std::move(model_file)))
    return;

  recorder.set_state(
      LanguageDetectionModelState::kModelFileValidAndMemoryMapped);

  auto statusor_classifier = tflite::task::text::nlclassifier::NLClassifier::
      CreateFromBufferAndOptions(
          reinterpret_cast<const char*>(model_fb_.data()), model_fb_.length(),
          {.input_tensor_index = 0,
           .output_score_tensor_index = 0,
           .output_label_tensor_index = 2},
          CreateLangIdResolver());
  if (!statusor_classifier.ok()) {
    LOCAL_HISTOGRAM_BOOLEAN("LanguageDetection.TFLiteModel.InvalidModelFile",
                            true);
    return;
  }

  lang_detection_model_ = std::move(*statusor_classifier);
}

bool LanguageDetectionModel::IsAvailable() const {
  return lang_detection_model_ != nullptr;
}

std::pair<std::string, float> LanguageDetectionModel::DetectTopLanguage(
    const std::string& sampled_str) const {
  DCHECK(IsAvailable());
  std::vector<tflite::task::core::Category> categories =
      lang_detection_model_->Classify(sampled_str);
  std::sort(categories.begin(), categories.end(), sort_category());

  if (categories.empty())
    return std::make_pair(translate::kUnknownLanguageCode, 0.0);

  return std::make_pair(categories[0].class_name, categories[0].score);
}

std::string LanguageDetectionModel::DeterminePageLanguage(
    const std::string& code,
    const std::string& html_lang,
    const base::string16& contents,
    std::string* predicted_language,
    bool* is_prediction_reliable,
    float& prediction_reliability_score) const {
  DCHECK(IsAvailable());

  if (!predicted_language || !is_prediction_reliable)
    return translate::kUnknownLanguageCode;

  *is_prediction_reliable = false;
  *predicted_language = translate::kUnknownLanguageCode;
  prediction_reliability_score = 0.0;

  if (!lang_detection_model_)
    return translate::kUnknownLanguageCode;

  std::vector<std::pair<std::string, float>> model_predictions;
  // First evaluate the model on the entire contents based on the model's
  // implementation, for v1 it is the first 128 tokens that are unicode
  // "letters". We do not need to have the model's length in sync with
  // the sampling logic for v1 as 128 tokens is unlikely to be changed.
  model_predictions.emplace_back(
      DetectTopLanguage(base::UTF16ToUTF8(contents)));
  if (contents.length() > kNumTextSamples * kTextSampleLength) {
    // Strings with UTF-8 have different widths so substr should be performed on
    // the UTF16 strings to ensure alignment and then convert down to UTF-8
    // strings for model evaluation.
    std::string sampled_str = base::UTF16ToUTF8(contents.substr(
        contents.length() - kTextSampleLength, kTextSampleLength));
    // Evaluate on the last |kTextSampleLength| characters.
    model_predictions.emplace_back(DetectTopLanguage(sampled_str));

    // Sample and evaluate on the middle |kTextSampleLength| characters.
    sampled_str = base::UTF16ToUTF8(
        contents.substr(contents.length() / 2, kTextSampleLength));
    model_predictions.emplace_back(DetectTopLanguage(sampled_str));
  }

  const auto top_language_result = std::max_element(
      model_predictions.begin(), model_predictions.end(),
      [](auto& left, auto& right) { return left.second < right.second; });

  prediction_reliability_score = top_language_result->second;

  bool is_reliable =
      prediction_reliability_score > kDefaultReliabilityThreshold;

  std::string final_prediction = translate::FilterDetectedLanguage(
      base::UTF16ToUTF8(contents), top_language_result->first, is_reliable);
  *predicted_language = final_prediction;
  *is_prediction_reliable = is_reliable;
  language::ToTranslateLanguageSynonym(&final_prediction);

  LOCAL_HISTOGRAM_BOOLEAN("LanguageDetection.TFLite.DidAttemptDetection", true);
  return translate::DeterminePageLanguage(code, html_lang, final_prediction,
                                          is_reliable);
}

std::string LanguageDetectionModel::GetModelVersion() const {
  // TODO(crbug.com/1177992): Return the model version provided
  // by the model itself.
  return kTFLiteModelVersion;
}

}  // namespace translate
