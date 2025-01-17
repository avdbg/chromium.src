// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_IMPL_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/borealis/borealis_context.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager.h"
#include "chrome/browser/chromeos/borealis/infra/described.h"
#include "chrome/browser/chromeos/borealis/infra/transition.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/concierge_client.h"

namespace borealis {

class BorealisTask;

// The Borealis Context Manager is a keyed service responsible for managing
// the Borealis VM startup flow and guaranteeing its state to other processes.
class BorealisContextManagerImpl
    : public BorealisContextManager,
      public chromeos::ConciergeClient::VmObserver {
 public:
  explicit BorealisContextManagerImpl(Profile* profile);
  BorealisContextManagerImpl(const BorealisContextManagerImpl&) = delete;
  BorealisContextManagerImpl& operator=(const BorealisContextManagerImpl&) =
      delete;
  ~BorealisContextManagerImpl() override;

  // BorealisContextManager:
  void StartBorealis(ResultCallback callback) override;
  bool IsRunning() override;
  void ShutDownBorealis(base::OnceCallback<void(BorealisShutdownResult)>
                            on_shutdown_callback) override;

  // Public due to testing.
  virtual base::queue<std::unique_ptr<BorealisTask>> GetTasks();

 private:
  // TODO(b/): remove this once the context manager impl is a
  // BorealisStateManager.
  struct NotRunning {};

  // The startup transition is used to move the context manager from
  // "not-running" to "running".
  class Startup : public Transition<NotRunning,
                                    BorealisContext,
                                    Described<BorealisStartupResult>> {
   public:
    Startup(Profile* profile,
            base::queue<std::unique_ptr<BorealisTask>> task_queue);
    ~Startup() override;

    // Cancel this in-progress startup. Returns the partially-constructed
    // context, which can be used for cleaning up the incomplete startup.
    std::unique_ptr<BorealisContext> Abort();

   private:
    void NextTask();
    void TaskCallback(BorealisStartupResult result, std::string error);

    // Transition overrides.
    void Start(std::unique_ptr<NotRunning> current_state) override;

    Profile* const profile_;
    base::TimeTicks start_tick_;
    std::unique_ptr<BorealisContext> context_;
    base::queue<std::unique_ptr<BorealisTask>> task_queue_;
    base::WeakPtrFactory<BorealisContextManagerImpl::Startup> weak_factory_;
  };

  void AddCallback(ResultCallback callback);

  // Completes the startup with the given |result| and error messgae, invoking
  // all callbacks with the result. For any result except kSuccess the state of
  // the system will be as though StartBorealis() had not been called.
  void Complete(Startup::Result completion_result);

  // Returns the result of the startup (i.e. the context if it succeeds, or an
  // error if it doesn't).
  BorealisContextManager::ContextOrFailure GetResult(
      const Startup::Result& completion_result);

  // chromeos::ConciergeClient::VmObserver:
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;

  Profile* const profile_;

  std::unique_ptr<Startup> in_progress_startup_;
  std::unique_ptr<BorealisContext> context_;
  base::queue<ResultCallback> callback_queue_;
  base::WeakPtrFactory<BorealisContextManagerImpl> weak_factory_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_IMPL_H_
