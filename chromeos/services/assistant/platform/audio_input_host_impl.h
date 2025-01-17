// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_IMPL_H_

#include "chromeos/services/assistant/public/cpp/migration/audio_input_host.h"

#include <string>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/assistant/platform/audio_devices.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace assistant {

// Class that provides the bridge between the ChromeOS Browser thread and the
// Libassistant audio input mojom service.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AudioInputHostImpl
    : public AudioInputHost,
      private chromeos::PowerManagerClient::Observer,
      private AudioDevices::Observer {
 public:
  AudioInputHostImpl(
      mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
          pending_remote,
      CrasAudioHandler* cras_audio_handler,
      chromeos::PowerManagerClient* power_manager_client,
      const std::string& locale);
  AudioInputHostImpl(const AudioInputHost&) = delete;
  AudioInputHostImpl& operator=(const AudioInputHostImpl&) = delete;
  ~AudioInputHostImpl() override;

  // AudioInputHost implementation:
  void SetMicState(bool mic_open) override;
  void OnHotwordEnabled(bool enable) override;
  void OnConversationTurnStarted() override;
  void OnConversationTurnFinished() override;

  // AudioDevices::Observer implementation:
  void SetDeviceId(const base::Optional<std::string>& device_id) override;
  void SetHotwordDeviceId(
      const base::Optional<std::string>& device_id) override;

 private:
  // chromeos::PowerManagerClient::Observer overrides:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;

  void OnInitialLidStateReceived(
      base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  mojo::Remote<chromeos::libassistant::mojom::AudioInputController> remote_;
  chromeos::PowerManagerClient* const power_manager_client_;
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_;

  // Observes available audio devices and will set device-id/hotword-device-id
  // accordingly.
  AudioDevices audio_devices_;
  AudioDevices::ScopedObservation audio_devices_observation_{this};

  base::WeakPtrFactory<AudioInputHostImpl> weak_factory_{this};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_IMPL_H_
