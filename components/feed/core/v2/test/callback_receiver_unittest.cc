// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/test/callback_receiver.h"

#include "base/optional.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

TEST(CallbackReceiverTest, OneResult) {
  CallbackReceiver<int> cr1;
  cr1.Done(42);

  ASSERT_NE(cr1.GetResult(), base::nullopt);
  EXPECT_EQ(*cr1.GetResult(), 42);
  EXPECT_EQ(*cr1.GetResult<0>(), 42);
  EXPECT_EQ(*cr1.GetResult<int>(), 42);
}

TEST(CallbackReceiverTest, MultipleResults) {
  CallbackReceiver<std::string, bool> cr2;
  EXPECT_EQ(cr2.GetResult<0>(), base::nullopt);
  EXPECT_EQ(cr2.GetResult<1>(), base::nullopt);
  cr2.Done("asdfasdfasdf", false);

  ASSERT_NE(cr2.GetResult<0>(), base::nullopt);
  EXPECT_EQ(*cr2.GetResult<0>(), "asdfasdfasdf");
  EXPECT_EQ(*cr2.GetResult<std::string>(), "asdfasdfasdf");
  ASSERT_NE(cr2.GetResult<1>(), base::nullopt);
  EXPECT_EQ(*cr2.GetResult<1>(), false);
  EXPECT_EQ(*cr2.GetResult<bool>(), false);
}

TEST(CallbackReceiverTest, Clear) {
  CallbackReceiver<int, bool> cr;
  cr.Done(10, true);
  cr.Clear();
  EXPECT_EQ(cr.GetResult<0>(), base::nullopt);
  EXPECT_EQ(cr.GetResult<1>(), base::nullopt);
}

TEST(CallbackReceiverTest, RunAndGetResult) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  CallbackReceiver<int> cr1;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(cr1.Bind(), 42));
  EXPECT_EQ(42, cr1.RunAndGetResult());
}

TEST(CallbackReceiverTest, RunAndGetResultExternalRunLoop) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::RunLoop run_loop;
  CallbackReceiver<int> cr1(&run_loop);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(cr1.Bind(), 42));
  EXPECT_EQ(42, cr1.RunAndGetResult());
}

}  // namespace feed
