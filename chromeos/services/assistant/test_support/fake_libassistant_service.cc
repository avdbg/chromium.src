// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/test_support/fake_libassistant_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

FakeLibassistantService::FakeLibassistantService() : receiver_(this) {}

FakeLibassistantService::~FakeLibassistantService() = default;

void FakeLibassistantService::Bind(
    mojo::PendingReceiver<libassistant::mojom::LibassistantService>
        pending_receiver) {
  EXPECT_FALSE(receiver_.is_bound())
      << "Cannot bind the LibassistantService twice";
  receiver_.Bind(std::move(pending_receiver));
}

void FakeLibassistantService::Unbind() {
  receiver_.reset();
  service_controller().Unbind();
}

mojo::PendingReceiver<libassistant::mojom::MediaController>
FakeLibassistantService::GetMediaControllerPendingReceiver() {
  EXPECT_TRUE(media_controller_pending_receiver_.is_valid());
  return std::move(media_controller_pending_receiver_);
}

mojo::PendingRemote<libassistant::mojom::MediaDelegate>
FakeLibassistantService::GetMediaDelegatePendingRemote() {
  EXPECT_TRUE(media_delegate_pending_remote_.is_valid());
  return std::move(media_delegate_pending_remote_);
}

mojo::PendingReceiver<libassistant::mojom::SpeakerIdEnrollmentController>
FakeLibassistantService::GetSpeakerIdEnrollmentControllerPendingReceiver() {
  EXPECT_TRUE(speaker_id_enrollment_controller_pending_receiver_.is_valid());
  return std::move(speaker_id_enrollment_controller_pending_receiver_);
}

void FakeLibassistantService::Bind(
    mojo::PendingReceiver<libassistant::mojom::AudioInputController>
        audio_input_controller,
    mojo::PendingReceiver<libassistant::mojom::ConversationController>
        conversation_controller,
    mojo::PendingReceiver<libassistant::mojom::DisplayController>
        display_controller,
    mojo::PendingReceiver<libassistant::mojom::MediaController>
        media_controller,
    mojo::PendingReceiver<libassistant::mojom::ServiceController>
        service_controller,
    mojo::PendingReceiver<libassistant::mojom::SpeakerIdEnrollmentController>
        speaker_id_enrollment_controller,
    mojo::PendingRemote<libassistant::mojom::AudioOutputDelegate>
        audio_output_delegate,
    mojo::PendingRemote<libassistant::mojom::MediaDelegate> media_delegate,
    mojo::PendingRemote<libassistant::mojom::PlatformDelegate>
        platform_delegate) {
  service_controller_.Bind(std::move(service_controller));
  media_controller_pending_receiver_ = std::move(media_controller);
  media_delegate_pending_remote_ = std::move(media_delegate);
  speaker_id_enrollment_controller_pending_receiver_ =
      std::move(speaker_id_enrollment_controller);
}

}  // namespace assistant
}  // namespace chromeos
