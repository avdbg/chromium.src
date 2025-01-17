// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_CAST_STREAMING_TEST_CAST_STREAMING_TEST_RECEIVER_H_
#define FUCHSIA_CAST_STREAMING_TEST_CAST_STREAMING_TEST_RECEIVER_H_

#include "base/callback.h"
#include "base/optional.h"
#include "components/cast/message_port/message_port.h"
#include "fuchsia/cast_streaming/public/cast_streaming_session.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"

namespace cast_streaming {

// Cast Streaming Receiver implementation for testing. This class provides basic
// functionality for starting a Cast Streaming Receiver and receiving audio and
// video frames to a Cast Streaming Sender. Example usage:
//
// std::unique_ptr<cast_api_bindings::MessagePort> sender_message_port;
// std::unique_ptr<cast_api_bindings::MessagePort> receiver_message_port;
// cast_api_bindings::MessagePort::CreatePair(&sender_message_port,
//                                            &receiver_message_port);
//
// // Send |sender_message_port| to a Sender and start it.
//
// CastStreamingTestReceiver receiver;
// receiver.Start(std::move(receiver_message_port));
// receiver.RunUntilStarted();
//
// if (!receiver.RunUntilAudioFramesCountIsAtLeast(1u)) {
//   return;
// }
// // Handle receiver.audio_frames()[0] here.
//
// if (!receiver.RunUntilVideoFramesCountIsAtLeast(1u)) {
//   return;
// }
// // Handle receiver.video_frames()[0] here.
//
// receiver.Stop();
// receiver.RunUntilStopped();
class CastStreamingTestReceiver : public CastStreamingSession::Client {
 public:
  CastStreamingTestReceiver();
  ~CastStreamingTestReceiver() final;

  CastStreamingTestReceiver(const CastStreamingTestReceiver&) = delete;
  CastStreamingTestReceiver& operator=(const CastStreamingTestReceiver&) =
      delete;

  // Uses |message_port| as the Receiverr-end of a Cast Streaming MessagePort to
  // start a Cast Streaming Session.
  void Start(std::unique_ptr<cast_api_bindings::MessagePort> message_port);
  void Stop();

  // Helper methods running the Receiver until it is properly started or
  // stopped. Only one of the Run*() methods can be called at any given time.
  void RunUntilStarted();
  void RunUntilStopped();

  // Helper methods running the Receiver until |*frames_count| audio or video
  // frames have been received or until the Receiver has stopped. Returns true
  // if the session is still active. Only one of the Run*() methods can be
  // called at any given time.
  bool RunUntilAudioFramesCountIsAtLeast(size_t audio_frames_count);
  bool RunUntilVideoFramesCountIsAtLeast(size_t video_frames_count);

  bool is_active() { return is_active_; }
  base::Optional<media::AudioDecoderConfig> audio_config() {
    return audio_config_;
  }
  base::Optional<media::VideoDecoderConfig> video_config() {
    return video_config_;
  }
  const std::vector<scoped_refptr<media::DecoderBuffer>>& audio_buffers() {
    return audio_buffers_;
  }
  const std::vector<scoped_refptr<media::DecoderBuffer>>& video_buffers() {
    return video_buffers_;
  }

 private:
  // Callbacks for |audio_decoder_buffer_reader_| and
  // |video_decoder_buffer_reader_|, respectively.
  void OnAudioBufferRead(scoped_refptr<media::DecoderBuffer> buffer);
  void OnVideoBufferRead(scoped_refptr<media::DecoderBuffer> buffer);

  // CastStreamingSession::Client implementation.
  void OnSessionInitialization(
      base::Optional<CastStreamingSession::AudioStreamInfo> audio_stream_info,
      base::Optional<CastStreamingSession::VideoStreamInfo> video_stream_info)
      final;
  void OnAudioBufferReceived(media::mojom::DecoderBufferPtr buffer) final;
  void OnVideoBufferReceived(media::mojom::DecoderBufferPtr buffer) final;
  void OnSessionReinitialization(
      base::Optional<CastStreamingSession::AudioStreamInfo> audio_stream_info,
      base::Optional<CastStreamingSession::VideoStreamInfo> video_stream_info)
      final;
  void OnSessionEnded() final;

  CastStreamingSession receiver_session_;
  bool is_active_ = false;
  base::OnceClosure receiver_updated_closure_;

  base::Optional<media::AudioDecoderConfig> audio_config_;
  std::unique_ptr<media::MojoDecoderBufferReader> audio_decoder_buffer_reader_;
  std::vector<scoped_refptr<media::DecoderBuffer>> audio_buffers_;

  base::Optional<media::VideoDecoderConfig> video_config_;
  std::unique_ptr<media::MojoDecoderBufferReader> video_decoder_buffer_reader_;
  std::vector<scoped_refptr<media::DecoderBuffer>> video_buffers_;
};

}  // namespace cast_streaming

#endif  // FUCHSIA_CAST_STREAMING_TEST_CAST_STREAMING_TEST_RECEIVER_H_
