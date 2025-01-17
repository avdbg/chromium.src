// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_INPUT_STREAM_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_INPUT_STREAM_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom-forward.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom-forward.h"
#include "libassistant/shared/public/platform_audio_buffer.h"
#include "media/base/audio_capturer_source.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/audio/public/cpp/device_factory.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace chromeos {
namespace libassistant {

// A single audio stream. All captured packets will be sent to the given
// capture callback.
// The audio stream will be opened as soon as this class is created, and
// will be closed in the destructor.
class AudioInputStream {
 public:
  AudioInputStream(
      mojom::PlatformDelegate* delegate,
      const std::string& device_id,
      bool detect_dead_stream,
      assistant_client::BufferFormat buffer_format,
      media::AudioCapturerSource::CaptureCallback* capture_callback);
  AudioInputStream(AudioInputStream&) = delete;
  AudioInputStream& operator=(AudioInputStream&) = delete;
  ~AudioInputStream();

  const std::string& device_id() const { return device_id_; }

  bool has_dead_stream_detection() const { return detect_dead_stream_; }

 private:
  void Start();
  void Stop();

  media::AudioParameters GetAudioParameters() const;

  // Device used for recording.
  std::string device_id_;
  bool detect_dead_stream_;
  assistant_client::BufferFormat buffer_format_;
  mojom::PlatformDelegate* const delegate_;
  media::AudioCapturerSource::CaptureCallback* const capture_callback_;
  scoped_refptr<media::AudioCapturerSource> source_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_INPUT_STREAM_H_
