// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/translation_result_loader.h"

#include <memory>
#include <string>

#include "ash/public/cpp/quick_answers/controller/quick_answers_browser_client.h"
#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {
namespace {

constexpr char kCloudTranslationApiRequest[] =
    "https://translation.googleapis.com/language/translate/v2";

constexpr char kValidResponse[] = R"(
  {
    "data": {
      "translations": [
        {
          "translatedText": "prueba"
        }
      ]
    }
  }
)";

constexpr char kTestTranslationTitle[] = "test · inglés";
constexpr char kTestTranslationResult[] = "prueba";

const auto kTestTranslationIntent =
    IntentInfo("test", IntentType::kTranslation, "en", "es");

class FakeQuickAnswersBrowserClient : public ash::QuickAnswersBrowserClient {
 public:
  FakeQuickAnswersBrowserClient() = default;
  ~FakeQuickAnswersBrowserClient() override = default;

  // ash::QuickAnswersBrowserClient:
  void RequestAccessToken(GetAccessTokenCallback callback) override {
    std::move(callback).Run(std::string());
  }
};

}  // namespace

class TranslationResultLoaderTest : public testing::Test {
 public:
  TranslationResultLoaderTest() = default;

  TranslationResultLoaderTest(const TranslationResultLoaderTest&) = delete;
  TranslationResultLoaderTest& operator=(const TranslationResultLoaderTest&) =
      delete;

  // testing::Test:
  void SetUp() override {
    quick_answers_browser_client_ =
        std::make_unique<FakeQuickAnswersBrowserClient>();
    mock_delegate_ = std::make_unique<MockResultLoaderDelegate>();
    loader_ = std::make_unique<TranslationResultLoader>(
        &test_url_loader_factory_, mock_delegate_.get());
  }

  void TearDown() override { loader_.reset(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<FakeQuickAnswersBrowserClient> quick_answers_browser_client_;
  std::unique_ptr<TranslationResultLoader> loader_;
  std::unique_ptr<MockResultLoaderDelegate> mock_delegate_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(TranslationResultLoaderTest, Success) {
  std::unique_ptr<QuickAnswer> expected_quick_answer =
      std::make_unique<QuickAnswer>();
  expected_quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>(kTestTranslationResult));
  expected_quick_answer->title.push_back(
      std::make_unique<QuickAnswerText>(kTestTranslationTitle));
  test_url_loader_factory_.AddResponse(kCloudTranslationApiRequest,
                                       kValidResponse);
  EXPECT_CALL(
      *mock_delegate_,
      OnQuickAnswerReceived(QuickAnswerEqual(expected_quick_answer.get())));
  EXPECT_CALL(*mock_delegate_, OnNetworkError()).Times(0);
  loader_->Fetch(PreprocessRequest(kTestTranslationIntent));
  base::RunLoop().RunUntilIdle();
}

TEST_F(TranslationResultLoaderTest, NetworkError) {
  test_url_loader_factory_.AddResponse(
      GURL(kCloudTranslationApiRequest), network::mojom::URLResponseHead::New(),
      std::string(), network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  EXPECT_CALL(*mock_delegate_, OnNetworkError());
  EXPECT_CALL(*mock_delegate_, OnQuickAnswerReceived(testing::_)).Times(0);
  loader_->Fetch(PreprocessRequest(kTestTranslationIntent));
  base::RunLoop().RunUntilIdle();
}

TEST_F(TranslationResultLoaderTest, EmptyResponse) {
  test_url_loader_factory_.AddResponse(kCloudTranslationApiRequest,
                                       std::string());
  EXPECT_CALL(*mock_delegate_, OnQuickAnswerReceived(testing::Eq(nullptr)));
  EXPECT_CALL(*mock_delegate_, OnNetworkError()).Times(0);
  loader_->Fetch(PreprocessRequest(kTestTranslationIntent));
  base::RunLoop().RunUntilIdle();
}

}  // namespace quick_answers
}  // namespace chromeos
