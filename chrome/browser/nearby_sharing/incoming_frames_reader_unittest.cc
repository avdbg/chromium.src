// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/incoming_frames_reader.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/services/sharing/public/proto/wire_format.pb.h"
#include "chromeos/services/nearby/public/cpp/mock_nearby_process_manager.h"
#include "chromeos/services/nearby/public/cpp/mock_nearby_sharing_decoder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr base::TimeDelta kTimeout = base::TimeDelta::FromMilliseconds(1000);

std::vector<uint8_t> GetIntroductionFrame() {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_INTRODUCTION);
  v1frame->mutable_introduction();

  std::vector<uint8_t> data;
  data.resize(frame.ByteSize());
  EXPECT_TRUE(frame.SerializeToArray(&data[0], frame.ByteSize()));

  return data;
}

std::vector<uint8_t> GetCancelFrame() {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_CANCEL);

  std::vector<uint8_t> data;
  data.resize(frame.ByteSize());
  EXPECT_TRUE(frame.SerializeToArray(&data[0], frame.ByteSize()));

  return data;
}

void ExpectIntroductionFrame(
    const base::Optional<sharing::mojom::V1FramePtr>& frame) {
  ASSERT_TRUE(frame);
  EXPECT_TRUE((*frame)->is_introduction());
}

void ExpectCancelFrame(
    const base::Optional<sharing::mojom::V1FramePtr>& frame) {
  ASSERT_TRUE(frame);
  EXPECT_TRUE((*frame)->is_cancel_frame());
}

}  // namespace

class IncomingFramesReaderTest : public testing::Test {
 public:
  IncomingFramesReaderTest()
      : frames_reader_(&mock_process_manager_, &mock_nearby_connection_) {}

  ~IncomingFramesReaderTest() override = default;

  void SetUp() override {
    EXPECT_CALL(mock_process_manager_, GetNearbyProcessReference)
        .WillRepeatedly([&](chromeos::nearby::NearbyProcessManager::
                                NearbyProcessStoppedCallback) {
          auto mock_reference_ptr =
              std::make_unique<chromeos::nearby::MockNearbyProcessManager::
                                   MockNearbyProcessReference>();

          EXPECT_CALL(*(mock_reference_ptr.get()), GetNearbySharingDecoder)
              .WillRepeatedly(
                  testing::ReturnRef(mock_decoder_.shared_remote()));

          return mock_reference_ptr;
        });
  }

  FakeNearbyConnection& connection() { return mock_nearby_connection_; }

  testing::StrictMock<chromeos::nearby::MockNearbySharingDecoder>& decoder() {
    return mock_decoder_;
  }

  IncomingFramesReader& frames_reader() { return frames_reader_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  FakeNearbyConnection mock_nearby_connection_;
  testing::StrictMock<chromeos::nearby::MockNearbyProcessManager>
      mock_process_manager_;
  testing::StrictMock<chromeos::nearby::MockNearbySharingDecoder> mock_decoder_;
  IncomingFramesReader frames_reader_;
};

TEST_F(IncomingFramesReaderTest, ReadTimedOut) {
  EXPECT_CALL(decoder(), DecodeFrame(testing::_, testing::_)).Times(0);

  base::RunLoop run_loop;
  frames_reader().ReadFrame(
      sharing::mojom::V1Frame::Tag::INTRODUCTION,
      base::BindLambdaForTesting(
          [&](base::Optional<sharing::mojom::V1FramePtr> frame) {
            EXPECT_FALSE(frame);
            run_loop.Quit();
          }),
      kTimeout);
  run_loop.Run();
}

TEST_F(IncomingFramesReaderTest, ReadAnyFrameSuccessful) {
  std::vector<uint8_t> introduction_frame = GetIntroductionFrame();
  connection().AppendReadableData(introduction_frame);

  EXPECT_CALL(decoder(),
              DecodeFrame(testing::Eq(introduction_frame), testing::_))
      .WillOnce(testing::Invoke(
          [&](const std::vector<uint8_t>& data,
              chromeos::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                  callback) {
            sharing::mojom::V1FramePtr mojo_v1frame =
                sharing::mojom::V1Frame::New();
            mojo_v1frame->set_introduction(
                sharing::mojom::IntroductionFrame::New());

            sharing::mojom::FramePtr mojo_frame = sharing::mojom::Frame::New();
            mojo_frame->set_v1(std::move(mojo_v1frame));
            std::move(callback).Run(std::move(mojo_frame));
          }));

  base::RunLoop run_loop;
  frames_reader().ReadFrame(base::BindLambdaForTesting(
      [&](base::Optional<sharing::mojom::V1FramePtr> frame) {
        ExpectIntroductionFrame(frame);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(IncomingFramesReaderTest, ReadSuccessful) {
  std::vector<uint8_t> introduction_frame = GetIntroductionFrame();
  connection().AppendReadableData(introduction_frame);

  EXPECT_CALL(decoder(),
              DecodeFrame(testing::Eq(introduction_frame), testing::_))
      .WillOnce(testing::Invoke(
          [&](const std::vector<uint8_t>& data,
              chromeos::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                  callback) {
            sharing::mojom::V1FramePtr mojo_v1frame =
                sharing::mojom::V1Frame::New();
            mojo_v1frame->set_introduction(
                sharing::mojom::IntroductionFrame::New());

            sharing::mojom::FramePtr mojo_frame = sharing::mojom::Frame::New();
            mojo_frame->set_v1(std::move(mojo_v1frame));
            std::move(callback).Run(std::move(mojo_frame));
          }));

  base::RunLoop run_loop;
  frames_reader().ReadFrame(
      sharing::mojom::V1Frame::Tag::INTRODUCTION,
      base::BindLambdaForTesting(
          [&](base::Optional<sharing::mojom::V1FramePtr> frame) {
            ExpectIntroductionFrame(frame);
            run_loop.Quit();
          }),
      kTimeout);
  run_loop.Run();
}

TEST_F(IncomingFramesReaderTest, ReadSuccessful_JumbledFramesOrdering) {
  std::vector<uint8_t> cancel_frame = GetCancelFrame();
  connection().AppendReadableData(cancel_frame);

  std::vector<uint8_t> introduction_frame = GetIntroductionFrame();
  connection().AppendReadableData(introduction_frame);

  EXPECT_CALL(decoder(), DecodeFrame(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const std::vector<uint8_t>& data,
              chromeos::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                  callback) {
            EXPECT_EQ(cancel_frame, data);
            sharing::mojom::V1FramePtr mojo_v1frame =
                sharing::mojom::V1Frame::New();
            mojo_v1frame->set_cancel_frame(sharing::mojom::CancelFrame::New());

            sharing::mojom::FramePtr mojo_frame = sharing::mojom::Frame::New();
            mojo_frame->set_v1(std::move(mojo_v1frame));
            std::move(callback).Run(std::move(mojo_frame));
          }))
      .WillOnce(testing::Invoke(
          [&](const std::vector<uint8_t>& data,
              chromeos::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                  callback) {
            EXPECT_EQ(introduction_frame, data);
            sharing::mojom::V1FramePtr mojo_v1frame =
                sharing::mojom::V1Frame::New();
            mojo_v1frame->set_introduction(
                sharing::mojom::IntroductionFrame::New());

            sharing::mojom::FramePtr mojo_frame = sharing::mojom::Frame::New();
            mojo_frame->set_v1(std::move(mojo_v1frame));
            std::move(callback).Run(std::move(mojo_frame));
          }));

  base::RunLoop run_loop_introduction;
  frames_reader().ReadFrame(
      sharing::mojom::V1Frame::Tag::INTRODUCTION,
      base::BindLambdaForTesting(
          [&](base::Optional<sharing::mojom::V1FramePtr> frame) {
            ExpectIntroductionFrame(frame);
            run_loop_introduction.Quit();
          }),
      kTimeout);
  run_loop_introduction.Run();
}

TEST_F(IncomingFramesReaderTest, JumbledFramesOrdering_ReadFromCache) {
  std::vector<uint8_t> cancel_frame = GetCancelFrame();
  connection().AppendReadableData(cancel_frame);

  std::vector<uint8_t> introduction_frame = GetIntroductionFrame();
  connection().AppendReadableData(introduction_frame);

  EXPECT_CALL(decoder(), DecodeFrame(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const std::vector<uint8_t>& data,
              chromeos::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                  callback) {
            EXPECT_EQ(cancel_frame, data);
            sharing::mojom::V1FramePtr mojo_v1frame =
                sharing::mojom::V1Frame::New();
            mojo_v1frame->set_cancel_frame(sharing::mojom::CancelFrame::New());

            sharing::mojom::FramePtr mojo_frame = sharing::mojom::Frame::New();
            mojo_frame->set_v1(std::move(mojo_v1frame));
            std::move(callback).Run(std::move(mojo_frame));
          }))
      .WillOnce(testing::Invoke(
          [&](const std::vector<uint8_t>& data,
              chromeos::nearby::MockNearbySharingDecoder::DecodeFrameCallback
                  callback) {
            EXPECT_EQ(introduction_frame, data);
            sharing::mojom::V1FramePtr mojo_v1frame =
                sharing::mojom::V1Frame::New();
            mojo_v1frame->set_introduction(
                sharing::mojom::IntroductionFrame::New());

            sharing::mojom::FramePtr mojo_frame = sharing::mojom::Frame::New();
            mojo_frame->set_v1(std::move(mojo_v1frame));
            std::move(callback).Run(std::move(mojo_frame));
          }));

  base::RunLoop run_loop_introduction;
  frames_reader().ReadFrame(
      sharing::mojom::V1Frame::Tag::INTRODUCTION,
      base::BindLambdaForTesting(
          [&](base::Optional<sharing::mojom::V1FramePtr> frame) {
            ExpectIntroductionFrame(frame);
            run_loop_introduction.Quit();
          }),
      kTimeout);
  run_loop_introduction.Run();

  // Reading any frame should return CancelFrame.
  base::RunLoop run_loop_cancel;
  frames_reader().ReadFrame(base::BindLambdaForTesting(
      [&](base::Optional<sharing::mojom::V1FramePtr> frame) {
        ExpectCancelFrame(frame);
        run_loop_cancel.Quit();
      }));
  run_loop_cancel.Run();
}

TEST_F(IncomingFramesReaderTest, ReadAfterConnectionClosed) {
  EXPECT_CALL(decoder(), DecodeFrame(testing::_, testing::_)).Times(0);

  base::RunLoop run_loop_before_close;
  frames_reader().ReadFrame(
      sharing::mojom::V1Frame::Tag::INTRODUCTION,
      base::BindLambdaForTesting(
          [&](base::Optional<sharing::mojom::V1FramePtr> frame) {
            EXPECT_FALSE(frame);
            run_loop_before_close.Quit();
          }),
      kTimeout);

  connection().Close();
  run_loop_before_close.Run();
}
