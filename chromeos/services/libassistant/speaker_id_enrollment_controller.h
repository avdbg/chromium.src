// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_SPEAKER_ID_ENROLLMENT_CONTROLLER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_SPEAKER_ID_ENROLLMENT_CONTROLLER_H_

#include "chromeos/assistant/libassistant/src/third_party/chromium/base/memory/weak_ptr.h"
#include "chromeos/services/libassistant/assistant_manager_observer.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom-forward.h"
#include "chromeos/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace libassistant {

class SpeakerIdEnrollmentController
    : public mojom::SpeakerIdEnrollmentController,
      public AssistantManagerObserver {
 public:
  explicit SpeakerIdEnrollmentController(
      mojom::AudioInputController* audio_input);
  SpeakerIdEnrollmentController(const SpeakerIdEnrollmentController&) = delete;
  SpeakerIdEnrollmentController& operator=(
      const SpeakerIdEnrollmentController&) = delete;
  ~SpeakerIdEnrollmentController() override;

  void Bind(mojo::PendingReceiver<mojom::SpeakerIdEnrollmentController>
                pending_receiver);

  // mojom::SpeakerIdEnrollmentController implementation:
  void StartSpeakerIdEnrollment(
      const std::string& user_gaia_id,
      bool skip_cloud_enrollment,
      ::mojo::PendingRemote<mojom::SpeakerIdEnrollmentClient> client) override;
  void StopSpeakerIdEnrollment() override;
  void GetSpeakerIdEnrollmentStatus(
      const std::string& user_gaia_id,
      GetSpeakerIdEnrollmentStatusCallback callback) override;

  // AssistantManagerObserver implementation:
  void OnAssistantManagerStarted(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      override;
  void OnDestroyingAssistantManager(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      override;

 private:
  class EnrollmentSession;
  class GetStatusWaiter;

  mojo::Receiver<mojom::SpeakerIdEnrollmentController> receiver_{this};
  mojom::AudioInputController* const audio_input_;

  std::unique_ptr<EnrollmentSession> active_enrollment_session_;
  std::unique_ptr<GetStatusWaiter> get_status_waiter_;

  assistant_client::AssistantManagerInternal* assistant_manager_internal_ =
      nullptr;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_SPEAKER_ID_ENROLLMENT_CONTROLLER_H_
