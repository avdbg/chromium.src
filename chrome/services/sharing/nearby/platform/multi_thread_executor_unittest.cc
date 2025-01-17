// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/submittable_executor.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace location {
namespace nearby {
namespace chrome {

// To test Execute(), which has no return value, each task is assigned a unique
// ID. This ID is added to |executed_tasks_| when the task is Run(). Thus, the
// presence of an ID within |executed_tasks_| means the associated task has
// completed execution.
class MultiThreadExecutorTest : public testing::Test {
 protected:
  void ExecuteRunnableWithId(base::RunLoop& run_loop,
                             const base::UnguessableToken& task_id) {
    base::RunLoop wait_run_loop;
    multi_thread_executor_->Execute(
        CreateTrackedRunnable(run_loop, task_id, wait_run_loop));

    // Wait until runnable has started.
    wait_run_loop.Run();
  }

  bool SubmitRunnableWithId(base::RunLoop& run_loop,
                            const base::UnguessableToken& task_id) {
    base::RunLoop wait_run_loop;
    bool result = multi_thread_executor_->DoSubmit(
        CreateTrackedRunnable(run_loop, task_id, wait_run_loop));

    // Wait until runnable has started.
    wait_run_loop.Run();
    return result;
  }

  bool HasTaskStarted(const base::UnguessableToken& task_id) {
    base::AutoLock al(started_tasks_lock_);
    return started_tasks_.find(task_id) != started_tasks_.end();
  }

  bool HasTaskExecuted(const base::UnguessableToken& task_id) {
    base::AutoLock al(executed_tasks_lock_);
    return executed_tasks_.find(task_id) != executed_tasks_.end();
  }

  Runnable CreateTrackedRunnable(base::RunLoop& run_loop,
                                 const base::UnguessableToken& task_id,
                                 base::RunLoop& wait_run_loop) {
    return [&] {
      {
        base::AutoLock al(started_tasks_lock_);
        started_tasks_.insert(task_id);
      }

      // Notify SubmitParallelRunnableWithId thread is started
      wait_run_loop.Quit();

      thread_event_.Wait();

      {
        base::AutoLock al(executed_tasks_lock_);
        executed_tasks_.insert(task_id);
      }

      run_loop.Quit();
    };
  }

  void NotifyThreadWaitableEvent() { thread_event_.Signal(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SubmittableExecutor> multi_thread_executor_ =
      std::make_unique<SubmittableExecutor>(
          base::ThreadPool::CreateTaskRunner({base::MayBlock()}));

 private:
  base::Lock started_tasks_lock_;
  std::set<base::UnguessableToken> started_tasks_;
  base::Lock executed_tasks_lock_;
  std::set<base::UnguessableToken> executed_tasks_;
  base::WaitableEvent thread_event_;
};

TEST_F(MultiThreadExecutorTest, Submit) {
  base::RunLoop run_loop_1;
  base::UnguessableToken task_id_1 = base::UnguessableToken::Create();
  EXPECT_TRUE(SubmitRunnableWithId(run_loop_1, task_id_1));
  base::RunLoop run_loop_2;
  base::UnguessableToken task_id_2 = base::UnguessableToken::Create();
  EXPECT_TRUE(SubmitRunnableWithId(run_loop_2, task_id_2));
  base::RunLoop run_loop_3;
  base::UnguessableToken task_id_3 = base::UnguessableToken::Create();
  EXPECT_TRUE(SubmitRunnableWithId(run_loop_3, task_id_3));

  EXPECT_TRUE(HasTaskStarted(task_id_1));
  EXPECT_TRUE(HasTaskStarted(task_id_2));
  EXPECT_TRUE(HasTaskStarted(task_id_3));
  EXPECT_FALSE(HasTaskExecuted(task_id_1));
  EXPECT_FALSE(HasTaskExecuted(task_id_2));
  EXPECT_FALSE(HasTaskExecuted(task_id_3));

  NotifyThreadWaitableEvent();

  run_loop_1.Run();
  run_loop_2.Run();
  run_loop_3.Run();
  EXPECT_TRUE(HasTaskExecuted(task_id_1));
  EXPECT_TRUE(HasTaskExecuted(task_id_2));
  EXPECT_TRUE(HasTaskExecuted(task_id_3));
}

TEST_F(MultiThreadExecutorTest, Execute) {
  base::RunLoop run_loop_1;
  base::UnguessableToken task_id_1 = base::UnguessableToken::Create();
  ExecuteRunnableWithId(run_loop_1, task_id_1);
  base::RunLoop run_loop_2;
  base::UnguessableToken task_id_2 = base::UnguessableToken::Create();
  ExecuteRunnableWithId(run_loop_2, task_id_2);
  base::RunLoop run_loop_3;
  base::UnguessableToken task_id_3 = base::UnguessableToken::Create();
  ExecuteRunnableWithId(run_loop_3, task_id_3);

  EXPECT_TRUE(HasTaskStarted(task_id_1));
  EXPECT_TRUE(HasTaskStarted(task_id_2));
  EXPECT_TRUE(HasTaskStarted(task_id_3));
  EXPECT_FALSE(HasTaskExecuted(task_id_1));
  EXPECT_FALSE(HasTaskExecuted(task_id_2));
  EXPECT_FALSE(HasTaskExecuted(task_id_3));

  NotifyThreadWaitableEvent();

  run_loop_1.Run();
  run_loop_2.Run();
  run_loop_3.Run();
  EXPECT_TRUE(HasTaskExecuted(task_id_1));
  EXPECT_TRUE(HasTaskExecuted(task_id_2));
  EXPECT_TRUE(HasTaskExecuted(task_id_3));
}

TEST_F(MultiThreadExecutorTest, ShutdownPreventsFurtherTasks) {
  multi_thread_executor_->Shutdown();
  base::RunLoop run_loop;
  base::UnguessableToken task_id = base::UnguessableToken::Create();
  base::RunLoop wait_run_loop;
  EXPECT_FALSE(multi_thread_executor_->DoSubmit(
      CreateTrackedRunnable(run_loop, task_id, wait_run_loop)));

  EXPECT_FALSE(HasTaskExecuted(task_id));
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
