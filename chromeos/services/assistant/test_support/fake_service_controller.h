// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTROLLER_H_
#define CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTROLLER_H_

#include <mutex>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

// Fake implementation of the Mojom |ServiceController|.
// This implementation will inform the registered |StateObserver| instances of
// any state change, just like the real implementation.
class FakeServiceController : public libassistant::mojom::ServiceController {
 public:
  // Value returned when optional fields |access_token| or |user_id| are
  // missing. Note we use this instead of a |base::Optional| because this
  // results in a much nicer error message if the test fails. (otherwise you get
  // a message like this:
  //     Expected equality of these values:
  //           "<new-user-id-wrong>"
  //     with 32-byte object <01-00 snip 00-00>
  static constexpr const char* kNoValue = "<no-value>";

  using State = libassistant::mojom::ServiceState;
  using InitializeCallback =
      base::OnceCallback<void(assistant_client::AssistantManager*,
                              assistant_client::AssistantManagerInternal*)>;

  FakeServiceController();
  FakeServiceController(FakeServiceController&) = delete;
  FakeServiceController& operator=(FakeServiceController&) = delete;
  ~FakeServiceController() override;

  // Puts the service in the given state. Will inform all observers of the state
  // change.
  void SetState(State new_state);
  State state() const { return state_; }

  // Returns the Libassistant config that was passed to Initialize().
  const libassistant::mojom::BootupConfig& libassistant_config() {
    return libassistant_config_;
  }

  void Bind(mojo::PendingReceiver<libassistant::mojom::ServiceController>
                pending_receiver);
  void Unbind();

  void SetInitializeCallback(InitializeCallback callback);

  // Call this to block any call to |Start|. The observers will not be invoked
  // as long as the start call is blocked. Unblock these calls using
  // |UnblockStartCalls|. This is not enabled by default, so unless you call
  // |BlockStartCalls| any |Start| call will simply finish immediately.
  void BlockStartCalls();
  void UnblockStartCalls();

  // Return the access-token that was passed to |SetAuthenticationTokens|, or
  // |kNoValue| if an empty vector was passed in.
  std::string access_token();
  // Return the user-id that was passed to |SetAuthenticationTokens|, or
  // |kNoValue| if an empty vector was passed in.
  std::string gaia_id();

  // True if ResetAllDataAndStop() was called.
  bool has_data_been_reset() const { return has_data_been_reset_; }

 private:
  // mojom::ServiceController implementation:
  void Initialize(libassistant::mojom::BootupConfigPtr config,
                  mojo::PendingRemote<network::mojom::URLLoaderFactory>
                      url_loader_factory) override;
  void Start() override;
  void Stop() override;
  void ResetAllDataAndStop() override;
  void AddAndFireStateObserver(
      mojo::PendingRemote<libassistant::mojom::StateObserver> pending_observer)
      override;
  void SetSpokenFeedbackEnabled(bool value) override {}
  void SetAuthenticationTokens(
      std::vector<libassistant::mojom::AuthenticationTokenPtr> tokens) override;
  void SetHotwordEnabled(bool value) override {}

  // Mutex taken in |Start| to allow the calls to block if |BlockStartCalls| was
  // called.
  std::mutex start_mutex_;

  // Config passed to LibAssistant when it was started.
  libassistant::mojom::BootupConfig libassistant_config_;

  // True if ResetAllDataAndStop() was called.
  bool has_data_been_reset_ = false;

  // Authentication tokens passed to SetAuthenticationTokens().
  std::vector<libassistant::mojom::AuthenticationTokenPtr>
      authentication_tokens_;

  InitializeCallback initialize_callback_;

  State state_ = State::kStopped;
  mojo::Receiver<libassistant::mojom::ServiceController> receiver_;
  mojo::RemoteSet<libassistant::mojom::StateObserver> state_observers_;
  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;

  base::WeakPtrFactory<FakeServiceController> weak_factory_{this};
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTROLLER_H_
