// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_PROXY_CALL_TRANSLATOR_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_PROXY_CALL_TRANSLATOR_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/cma/backend/proxy/buffer_id_manager.h"
#include "chromecast/media/cma/backend/proxy/cast_runtime_audio_channel_broker.h"
#include "chromecast/media/cma/backend/proxy/cma_proxy_handler.h"
#include "chromecast/media/cma/backend/proxy/push_buffer_queue.h"

namespace chromecast {

class TaskRunner;

namespace media {

struct AudioConfig;

// This class is responsible for translating between entities used by the
// client CmaBackend and entities used by the internal gRPC Channel.
// Calls made to all methods of this class may be made from any thread.
class ProxyCallTranslator : public CmaProxyHandler,
                            public CastRuntimeAudioChannelBroker::Handler {
 public:
  // Creates a new ProxyCallTranslator. All provided entities must exist for the
  // duration of this instance's lifetime. All calls to |client| will be made
  // on |client_task_runner|.
  ProxyCallTranslator(TaskRunner* client_task_runner,
                      CmaProxyHandler::Client* client);
  ~ProxyCallTranslator() override;

  // CmaProxyHandler overrides:
  void Initialize(
      const std::string& cast_session_id,
      CmaProxyHandler::AudioDecoderOperationMode decoder_mode) override;
  void Start(int64_t start_pts, const TargetBufferInfo& target_buffer) override;
  void Stop() override;
  void Pause() override;
  void Resume(const TargetBufferInfo& target_buffer) override;
  void SetPlaybackRate(float rate) override;
  void SetVolume(float multiplier) override;
  bool SetConfig(const AudioConfig& config) override;
  bool PushBuffer(scoped_refptr<DecoderBufferBase> buffer,
                  BufferIdManager::BufferId buffer_id) override;

 private:
  friend class ProxyCallTranslatorTest;

  using MediaTime = CastRuntimeAudioChannelBroker::Handler::MediaTime;
  using PipelineState = CastRuntimeAudioChannelBroker::Handler::PipelineState;
  using PushBufferRequest =
      CastRuntimeAudioChannelBroker::Handler::PushBufferRequest;

  ProxyCallTranslator(
      TaskRunner* client_task_runner,
      CmaProxyHandler::Client* client,
      std::unique_ptr<CastRuntimeAudioChannelBroker> decoder_channel);

  // CastRuntimeAudioChannelBroker::Handler overrides:
  base::Optional<PushBufferRequest> GetBufferedData() override;
  bool HasBufferedData() override;
  void HandleInitializeResponse(
      CastRuntimeAudioChannelBroker::StatusCode status) override;
  void HandleStateChangeResponse(
      PipelineState state,
      CastRuntimeAudioChannelBroker::StatusCode status) override;
  void HandleSetVolumeResponse(
      CastRuntimeAudioChannelBroker::StatusCode status) override;
  void HandleSetPlaybackResponse(
      CastRuntimeAudioChannelBroker::StatusCode status) override;
  void HandlePushBufferResponse(
      int64_t decoded_bytes,
      CastRuntimeAudioChannelBroker::StatusCode status) override;
  void HandleGetMediaTimeResponse(
      base::Optional<MediaTime> time,
      CastRuntimeAudioChannelBroker::StatusCode status) override;

  // Helper to share error handling code.
  bool HandleError(CastRuntimeAudioChannelBroker::StatusCode status);

  // Helpers to simplify use of callbacks for tasks posted to
  // |client_task_runner_|.
  void OnErrorTask();
  void OnPipelineStateChangeTask(CmaProxyHandler::PipelineState state);
  void OnBytesDecodedTask(int64_t decoded_byte_count);

  // Queue storing data from PushBuffer and SetConfig calls.
  PushBufferQueue push_buffer_queue_;

  std::unique_ptr<CastRuntimeAudioChannelBroker> decoder_channel_;
  TaskRunner* const client_task_runner_;
  CmaProxyHandler::Client* const client_;

  // NOTE: All weak_ptrs created from this factory must be dereferenced on
  // |client_task_runner_|. Unfortunately, due to the structure of the
  // chromecast::TaskRunner class, weak_ptr validation is not guaranteed so this
  // assumption cannot be validated outside of the WeakPtr class.
  base::WeakPtrFactory<ProxyCallTranslator> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_PROXY_CALL_TRANSLATOR_H_
