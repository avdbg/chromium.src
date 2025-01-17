// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/audio/audio_output_provider_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/services/libassistant/audio/audio_stream_handler.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "libassistant/shared/public/platform_audio_buffer.h"
#include "media/audio/audio_device_description.h"

namespace chromeos {
namespace libassistant {

namespace {

bool IsEncodedFormat(const assistant_client::OutputStreamFormat& format) {
  return format.encoding ==
             assistant_client::OutputStreamEncoding::STREAM_MP3 ||
         format.encoding ==
             assistant_client::OutputStreamEncoding::STREAM_OPUS_IN_OGG;
}

// Instances of this class will be owned by Libassistant, so any public method
// (including the constructor and destructor) can and will be called from other
// threads.
class AudioOutputImpl : public assistant_client::AudioOutput {
 public:
  AudioOutputImpl(
      mojo::PendingRemote<audio::mojom::StreamFactory> stream_factory,
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      chromeos::assistant::mojom::AssistantAudioDecoderFactory*
          audio_decoder_factory,
      mojom::AudioOutputDelegate* audio_output_delegate,
      assistant_client::OutputStreamType type,
      assistant_client::OutputStreamFormat format,
      const std::string& device_id)
      : main_task_runner_(main_task_runner),
        stream_factory_(std::move(stream_factory)),
        audio_decoder_factory_(audio_decoder_factory),
        audio_output_delegate_(audio_output_delegate),
        stream_type_(type),
        format_(format) {
    // The constructor runs on the Libassistant thread, so we need to detach the
    // main sequence checker.
    DETACH_FROM_SEQUENCE(main_sequence_checker_);
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioOutputImpl::InitializeOnMainThread,
                                  weak_ptr_factory_.GetWeakPtr(), device_id));
  }

  ~AudioOutputImpl() override {
    main_task_runner_->DeleteSoon(FROM_HERE, device_owner_.release());
    main_task_runner_->DeleteSoon(FROM_HERE, audio_stream_handler_.release());
  }

  // assistant_client::AudioOutput overrides:
  assistant_client::OutputStreamType GetType() override { return stream_type_; }

  void Start(assistant_client::AudioOutput::Delegate* delegate) override {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioOutputImpl::StartOnMainThread,
                                  weak_ptr_factory_.GetWeakPtr(), delegate));
  }

  void Stop() override {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioOutputImpl::StopOnMainThread,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void InitializeOnMainThread(const std::string& device_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    audio_stream_handler_ = std::make_unique<AudioStreamHandler>();
    device_owner_ = std::make_unique<AudioDeviceOwner>(device_id);
  }

  void StartOnMainThread(assistant_client::AudioOutput::Delegate* delegate) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    // TODO(llin): Remove getting audio focus here after libassistant handles
    // acquiring audio focus for the internal media player.
    if (stream_type_ == assistant_client::OutputStreamType::STREAM_MEDIA) {
      audio_output_delegate_->RequestAudioFocus(
          mojom::AudioOutputStreamType::kMediaStream);
    }

    if (IsEncodedFormat(format_)) {
      audio_stream_handler_->StartAudioDecoder(
          audio_decoder_factory_, delegate,
          base::BindOnce(&AudioDeviceOwner::Start,
                         base::Unretained(device_owner_.get()),
                         audio_output_delegate_, audio_stream_handler_.get(),
                         std::move(stream_factory_)));
    } else {
      device_owner_->Start(audio_output_delegate_, delegate,
                           std::move(stream_factory_), format_);
    }
  }

  void StopOnMainThread() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    // TODO(llin): Remove abandoning audio focus here after libassistant handles
    // abandoning audio focus for the internal media player.
    if (stream_type_ == assistant_client::OutputStreamType::STREAM_MEDIA) {
      audio_output_delegate_->AbandonAudioFocusIfNeeded();
    }

    if (IsEncodedFormat(format_)) {
      device_owner_->SetDelegate(nullptr);
      audio_stream_handler_->OnStopped();
    } else {
      device_owner_->Stop();
    }
  }

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  mojo::PendingRemote<audio::mojom::StreamFactory> stream_factory_
      GUARDED_BY_CONTEXT(main_sequence_checker_);
  chromeos::assistant::mojom::AssistantAudioDecoderFactory*
      audio_decoder_factory_ GUARDED_BY_CONTEXT(main_sequence_checker_);
  mojom::AudioOutputDelegate* const audio_output_delegate_
      GUARDED_BY_CONTEXT(main_sequence_checker_);

  // Accessed from both Libassistant and main sequence, so should remain
  // |const|.
  const assistant_client::OutputStreamType stream_type_;

  assistant_client::OutputStreamFormat format_
      GUARDED_BY_CONTEXT(main_sequence_checker_);

  std::unique_ptr<AudioStreamHandler> audio_stream_handler_
      GUARDED_BY_CONTEXT(main_sequence_checker_);
  std::unique_ptr<AudioDeviceOwner> device_owner_
      GUARDED_BY_CONTEXT(main_sequence_checker_);

  // This class is used both from the Libassistant and main thread.
  SEQUENCE_CHECKER(main_sequence_checker_);

  base::WeakPtrFactory<AudioOutputImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioOutputImpl);
};

}  // namespace

AudioOutputProviderImpl::AudioOutputProviderImpl(const std::string& device_id)
    : loop_back_input_(media::AudioDeviceDescription::kLoopbackInputDeviceId),
      volume_control_impl_(),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      device_id_(device_id) {}

void AudioOutputProviderImpl::Bind(
    mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate,
    mojom::PlatformDelegate* platform_delegate) {
  platform_delegate_ = platform_delegate;
  platform_delegate_->BindAudioDecoderFactory(
      audio_decoder_factory_.BindNewPipeAndPassReceiver());

  audio_output_delegate_.Bind(std::move(audio_output_delegate));

  volume_control_impl_.Initialize(audio_output_delegate_.get(),
                                  platform_delegate);
  loop_back_input_.Initialize(platform_delegate);
}

AudioOutputProviderImpl::~AudioOutputProviderImpl() = default;

// Called from the Libassistant thread.
assistant_client::AudioOutput* AudioOutputProviderImpl::CreateAudioOutput(
    assistant_client::OutputStreamType type,
    const assistant_client::OutputStreamFormat& stream_format) {
  mojo::PendingRemote<audio::mojom::StreamFactory> stream_factory;
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputProviderImpl::BindStreamFactory,
                     weak_ptr_factory_.GetWeakPtr(),
                     stream_factory.InitWithNewPipeAndPassReceiver()));
  // Owned by one arbitrary thread inside libassistant. It will be destroyed
  // once assistant_client::AudioOutput::Delegate::OnStopped() is called.
  return new AudioOutputImpl(std::move(stream_factory), main_task_runner_,
                             audio_decoder_factory_.get(),
                             audio_output_delegate_.get(), type, stream_format,
                             device_id_);
}

// Called from the Libassistant thread.
std::vector<assistant_client::OutputStreamEncoding>
AudioOutputProviderImpl::GetSupportedStreamEncodings() {
  return std::vector<assistant_client::OutputStreamEncoding>{
      assistant_client::OutputStreamEncoding::STREAM_PCM_S16,
      assistant_client::OutputStreamEncoding::STREAM_PCM_S32,
      assistant_client::OutputStreamEncoding::STREAM_PCM_F32,
      assistant_client::OutputStreamEncoding::STREAM_MP3,
      assistant_client::OutputStreamEncoding::STREAM_OPUS_IN_OGG,
  };
}

// Called from the Libassistant thread.
assistant_client::AudioInput* AudioOutputProviderImpl::GetReferenceInput() {
  return &loop_back_input_;
}

// Called from the Libassistant thread.
bool AudioOutputProviderImpl::SupportsPlaybackTimestamp() const {
  // TODO(muyuanli): implement.
  return false;
}

// Called from the Libassistant thread.
assistant_client::VolumeControl& AudioOutputProviderImpl::GetVolumeControl() {
  return volume_control_impl_;
}

// Called from the Libassistant thread.
void AudioOutputProviderImpl::RegisterAudioEmittingStateCallback(
    AudioEmittingStateCallback callback) {
  // TODO(muyuanli): implement.
}

void AudioOutputProviderImpl::BindStreamFactory(
    mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) {
  platform_delegate_->BindAudioStreamFactory(std::move(receiver));
}

}  // namespace libassistant
}  // namespace chromeos
