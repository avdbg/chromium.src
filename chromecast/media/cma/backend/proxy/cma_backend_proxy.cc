// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/cma_backend_proxy.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/notreached.h"
#include "chromecast/media/cma/backend/proxy/multizone_audio_decoder_proxy_impl.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {
namespace {

// The maximum allowed difference between the audio and video decoders used for
// the CmaBackendProxy.
// TODO(b/168748626): Determine the correct value for this variable
// experimentally.
int64_t kMaxAllowedPtsDrift = 500;

}  // namespace

CmaBackendProxy::CmaBackendProxy(const MediaPipelineDeviceParams& params,
                                 std::unique_ptr<CmaBackend> delegated_pipeline)
    : CmaBackendProxy(base::BindOnce(&CmaBackendProxy::CreateAudioDecoderProxy,
                                     base::Unretained(this),
                                     params),
                      std::move(delegated_pipeline)) {}

CmaBackendProxy::CmaBackendProxy(
    CmaBackendProxy::AudioDecoderFactoryCB audio_decoder_factory,
    std::unique_ptr<CmaBackend> delegated_pipeline)
    : delegated_pipeline_(std::move(delegated_pipeline)),
      audio_decoder_factory_(std::move(audio_decoder_factory)) {
  DCHECK(delegated_pipeline_);
  DCHECK(audio_decoder_factory_);
}

CmaBackendProxy::~CmaBackendProxy() = default;

CmaBackend::AudioDecoder* CmaBackendProxy::CreateAudioDecoder() {
  DCHECK(!audio_decoder_);
  DCHECK(audio_decoder_factory_);
  audio_decoder_ = std::move(audio_decoder_factory_).Run();
  return audio_decoder_.get();
}

CmaBackend::VideoDecoder* CmaBackendProxy::CreateVideoDecoder() {
  has_video_decoder_ = true;
  return delegated_pipeline_->CreateVideoDecoder();
}

bool CmaBackendProxy::Initialize() {
  if (audio_decoder_) {
    audio_decoder_->Initialize();
  }

  if (audio_decoder_ || has_video_decoder_) {
    return delegated_pipeline_->Initialize();
  } else {
    return true;
  }
}

bool CmaBackendProxy::Start(int64_t start_pts) {
  if (audio_decoder_) {
    audio_decoder_->Start(start_pts);
  }

  if (audio_decoder_ || has_video_decoder_) {
    return delegated_pipeline_->Start(start_pts);
  } else {
    return true;
  }
}

void CmaBackendProxy::Stop() {
  if (has_video_decoder_ || audio_decoder_) {
    delegated_pipeline_->Stop();
  }

  if (audio_decoder_) {
    audio_decoder_->Stop();
  }
}

bool CmaBackendProxy::Pause() {
  if (audio_decoder_) {
    audio_decoder_->Pause();
  }

  if (audio_decoder_ || has_video_decoder_) {
    return delegated_pipeline_->Pause();
  } else {
    return true;
  }
}

bool CmaBackendProxy::Resume() {
  if (audio_decoder_) {
    audio_decoder_->Resume();
  }

  if (audio_decoder_ || has_video_decoder_) {
    return delegated_pipeline_->Resume();
  } else {
    return true;
  }
}

int64_t CmaBackendProxy::GetCurrentPts() {
  if (audio_decoder_) {
    const int64_t audio_pts = audio_decoder_->GetCurrentPts();
    const int64_t video_pts = delegated_pipeline_->GetCurrentPts();
    const int64_t min = std::min(audio_pts, video_pts);
    LOG_IF(WARNING, std::max(audio_pts, video_pts) - min > kMaxAllowedPtsDrift);
    return min;
  } else if (has_video_decoder_) {
    return delegated_pipeline_->GetCurrentPts();
  } else {
    return std::numeric_limits<int64_t>::min();
  }
}

bool CmaBackendProxy::SetPlaybackRate(float rate) {
  if (audio_decoder_) {
    audio_decoder_->SetPlaybackRate(rate);
  }

  if (audio_decoder_ || has_video_decoder_) {
    return delegated_pipeline_->SetPlaybackRate(rate);
  } else {
    return true;
  }
}

void CmaBackendProxy::LogicalPause() {
  if (has_video_decoder_ || audio_decoder_) {
    delegated_pipeline_->LogicalPause();
  }

  if (audio_decoder_) {
    audio_decoder_->LogicalPause();
  }
}

void CmaBackendProxy::LogicalResume() {
  if (has_video_decoder_ || audio_decoder_) {
    delegated_pipeline_->LogicalResume();
  }

  if (audio_decoder_) {
    audio_decoder_->LogicalResume();
  }
}

std::unique_ptr<MultizoneAudioDecoderProxy>
CmaBackendProxy::CreateAudioDecoderProxy(
    const MediaPipelineDeviceParams& params) {
  CmaBackend::AudioDecoder* downstream_decoder =
      delegated_pipeline_->CreateAudioDecoder();
  return std::make_unique<MultizoneAudioDecoderProxyImpl>(params,
                                                          downstream_decoder);
}

}  // namespace media
}  // namespace chromecast
