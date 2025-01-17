// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/services/speech/soda/soda_client.h"

#include <unistd.h>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "chrome/services/speech/soda/proto/soda_api.pb.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace soda {

constexpr base::FilePath::CharType kSodaResourcesDir[] =
    FILE_PATH_LITERAL("third_party/soda/resources");

constexpr base::FilePath::CharType kSodaTestBinaryRelativePath[] =
    FILE_PATH_LITERAL("libsoda_for_testing.so");

constexpr base::FilePath::CharType kSodaTestonfigRelativePath[] =
    FILE_PATH_LITERAL("en_us");

constexpr base::FilePath::CharType kSodaTestAudioRelativePath[] =
    FILE_PATH_LITERAL("hey_google.wav");

class SodaClientUnitTest : public testing::Test {
 public:
  SodaClientUnitTest() = default;
  ~SodaClientUnitTest() override = default;

  static void RecognitionCallback(const char* result,
                                  const bool is_final,
                                  void* callback_handle);

  void AddRecognitionResult(std::string result);

 protected:
  void SetUp() override;

  // The root directory for test files.
  base::FilePath test_data_dir_;
  std::unique_ptr<soda::SodaClient> soda_client_;
  std::vector<std::string> recognition_results_;
};

void OnSodaResponse(const char* serialized_proto,
                    int length,
                    void* callback_handle) {
  ASSERT_TRUE(callback_handle);
  speech::soda::chrome::SodaResponse response;
  if (!response.ParseFromArray(serialized_proto, length)) {
    LOG(ERROR) << "Unable to parse result from SODA.";
    NOTREACHED();
  }

  if (response.soda_type() == speech::soda::chrome::SodaResponse::RECOGNITION) {
    speech::soda::chrome::SodaRecognitionResult result =
        response.recognition_result();
    ASSERT_TRUE(result.hypothesis_size());
    static_cast<soda::SodaClientUnitTest*>(callback_handle)
        ->AddRecognitionResult(result.hypothesis(0));
  }
}

void SodaClientUnitTest::AddRecognitionResult(std::string result) {
  recognition_results_.push_back(std::move(result));
}

void SodaClientUnitTest::SetUp() {
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir_));
  auto libsoda_path = test_data_dir_.Append(base::FilePath(kSodaResourcesDir))
                          .Append(base::FilePath(kSodaTestBinaryRelativePath));
  ASSERT_TRUE(base::PathExists(libsoda_path));
  soda_client_ = std::make_unique<soda::SodaClient>(libsoda_path);
  ASSERT_TRUE(soda_client_.get());
  ASSERT_FALSE(soda_client_->IsInitialized());
}

TEST_F(SodaClientUnitTest, CreateSodaClient) {
  auto audio_file = test_data_dir_.Append(base::FilePath(kSodaResourcesDir))
                        .Append(base::FilePath(kSodaTestAudioRelativePath));
  ASSERT_TRUE(base::PathExists(audio_file));

  std::string buffer;
  ASSERT_TRUE(base::ReadFileToString(audio_file, &buffer));

  auto handler = media::WavAudioHandler::Create(buffer);
  ASSERT_TRUE(handler.get());
  ASSERT_EQ(handler->num_channels(), 1);

  auto config_file_path =
      test_data_dir_.Append(base::FilePath(kSodaResourcesDir))
          .Append(base::FilePath(kSodaTestonfigRelativePath));
  ASSERT_TRUE(base::PathExists(config_file_path));

  speech::soda::chrome::ExtendedSodaConfigMsg config_msg;
  config_msg.set_channel_count(handler->num_channels());
  config_msg.set_sample_rate(handler->sample_rate());
  config_msg.set_language_pack_directory(config_file_path.value().c_str());
  config_msg.set_simulate_realtime_testonly(false);
  config_msg.set_enable_lang_id(false);
  config_msg.set_recognition_mode(soda::api::SerializedSodaConfigMsg::CAPTION);

  // The test binary does not verify the execution context.
  config_msg.set_api_key("");

  auto serialized = config_msg.SerializeAsString();

  SerializedSodaConfig config;
  config.soda_config = serialized.c_str();
  config.soda_config_size = serialized.size();
  config.callback = &OnSodaResponse;
  config.callback_handle = this;
  soda_client_->Reset(config, handler->sample_rate(), handler->num_channels());
  ASSERT_TRUE(soda_client_->IsInitialized());

  auto bus =
      media::AudioBus::Create(handler->num_channels(), handler->total_frames());

  size_t bytes_written = 0u;
  ASSERT_TRUE(handler->CopyTo(bus.get(), 0, &bytes_written));

  std::vector<int16_t> audio_data(bus->frames());
  bus->ToInterleaved<media::SignedInt16SampleTypeTraits>(bus->frames(),
                                                         audio_data.data());

  constexpr size_t kMaxChunkSize = 1024;
  constexpr int kReplayAudioCount = 2;

  for (int i = 0; i < kReplayAudioCount; i++) {
    int chunk_start = 0;
    // Upload chunks of 1024 frames at a time.
    while (chunk_start < static_cast<int>(audio_data.size())) {
      int chunk_size = kMaxChunkSize < audio_data.size() - chunk_start
                           ? kMaxChunkSize
                           : audio_data.size() - chunk_start;
      soda_client_->AddAudio(
          reinterpret_cast<const char*>(&audio_data[chunk_start]),
          sizeof(int16_t) * chunk_size);

      chunk_start += chunk_size;

      // Sleep for 20ms to simulate real-time audio. SODA requires audio
      // streaming in order to return events.
      usleep(20000);
    }
  }

  ASSERT_GT(static_cast<int>(recognition_results_.size()), kReplayAudioCount);
  ASSERT_EQ(recognition_results_.back(), "Hey Google Hey Google");
}

}  // namespace soda
