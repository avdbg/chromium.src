// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/submittable_executor.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"

namespace location {
namespace nearby {
namespace chrome {

SubmittableExecutor::SubmittableExecutor(
    scoped_refptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

SubmittableExecutor::~SubmittableExecutor() {
  {
    base::AutoLock al(lock_);
    is_shut_down_ = true;
    if (num_incomplete_tasks_ == 0)
      last_task_completed_.Signal();
  }

  // Block until all pending tasks are finished.
  last_task_completed_.Wait();

#if DCHECK_IS_ON()
  base::AutoLock al(lock_);
  DCHECK_EQ(num_incomplete_tasks_, 0);
#endif  // DCHECK_IS_ON()
}

// Once called, this method will prevent any future calls to Submit() or
// Execute() from posting additional tasks. Previously posted asks will be
// allowed to complete normally.
void SubmittableExecutor::Shutdown() {
  base::AutoLock al(lock_);
  is_shut_down_ = true;
}

int SubmittableExecutor::GetTid(int index) const {
  // SubmittableExecutor does not own a thread pool directly nor manages
  // threads, thus cannot support this debug feature.
  return 0;
}

// Posts the given |runnable| and returns true immediately. If Shutdown() has
// been called, this method will return false.
bool SubmittableExecutor::DoSubmit(Runnable&& runnable) {
  base::AutoLock al(lock_);
  if (is_shut_down_)
    return false;

  ++num_incomplete_tasks_;
  return task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SubmittableExecutor::RunTask,
                                base::Unretained(this), std::move(runnable)));
}

// Posts the given |runnable| and returns immediately. If Shutdown() has been
// called, this method will do nothing.
void SubmittableExecutor::Execute(Runnable&& runnable) {
  base::AutoLock al(lock_);
  if (is_shut_down_)
    return;

  ++num_incomplete_tasks_;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SubmittableExecutor::RunTask,
                                base::Unretained(this), std::move(runnable)));
}

void SubmittableExecutor::RunTask(Runnable&& runnable) {
  {
    // base::ScopedAllowBaseSyncPrimitives is required as code inside the
    // runnable uses blocking primitive, which lives outside Chrome.
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    runnable();
  }

  base::AutoLock al(lock_);
  DCHECK_GE(num_incomplete_tasks_, 1);
  if (--num_incomplete_tasks_ == 0 && is_shut_down_)
    last_task_completed_.Signal();
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
