// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/metrics_reporting_ash.h"

#include <memory>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace {

class TestObserver : public mojom::MetricsReportingObserver {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  // crosapi::mojom::MetricsReportingObserver:
  void OnMetricsReportingChanged(bool enabled) override {
    metrics_enabled_ = enabled;
  }

  // Public because this is test code.
  base::Optional<bool> metrics_enabled_;
  mojo::Receiver<mojom::MetricsReportingObserver> receiver_{this};
};

class TestDelegate : public MetricsReportingAsh::Delegate {
 public:
  TestDelegate() = default;
  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;
  ~TestDelegate() override = default;

  // MetricsReportingAsh::Delegate:
  void SetMetricsReportingEnabled(bool enabled) override { enabled_ = enabled; }

  // Public because this is test code.
  base::Optional<bool> enabled_;
};

class MetricsReportingAshTest : public testing::Test {
 public:
  MetricsReportingAshTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()),
        local_state_(scoped_testing_local_state_.Get()) {}
  MetricsReportingAshTest(const MetricsReportingAshTest&) = delete;
  MetricsReportingAshTest& operator=(const MetricsReportingAshTest&) = delete;
  ~MetricsReportingAshTest() override = default;

  // Public because this is test code.
  base::test::TaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_testing_local_state_;
  PrefService* const local_state_;
};

TEST_F(MetricsReportingAshTest, Basics) {
  // Simulate metrics reporting enabled.
  local_state_->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);

  // Construct the object under test.
  MetricsReportingAsh metrics_reporting_ash(local_state_);
  mojo::Remote<mojom::MetricsReporting> metrics_reporting_remote;
  metrics_reporting_ash.BindReceiver(
      metrics_reporting_remote.BindNewPipeAndPassReceiver());

  // Adding an observer results in it being fired with the current state.
  TestObserver observer;
  metrics_reporting_remote->AddObserver(
      observer.receiver_.BindNewPipeAndPassRemote());
  metrics_reporting_remote.FlushForTesting();
  ASSERT_TRUE(observer.metrics_enabled_.has_value());
  EXPECT_TRUE(observer.metrics_enabled_.value());

  // Disabling metrics reporting in ash fires the observer with the new value.
  observer.metrics_enabled_.reset();
  local_state_->SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);
  observer.receiver_.FlushForTesting();
  ASSERT_TRUE(observer.metrics_enabled_.has_value());
  EXPECT_FALSE(observer.metrics_enabled_.value());
}

TEST_F(MetricsReportingAshTest, SetMetricsReportingEnabled) {
  // Simulate metrics reporting disabled.
  local_state_->SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);

  // Construct the object with a test delegate.
  auto delegate = std::make_unique<TestDelegate>();
  TestDelegate* test_delegate = delegate.get();
  MetricsReportingAsh metrics_reporting_ash(std::move(delegate), local_state_);
  mojo::Remote<mojom::MetricsReporting> metrics_reporting_remote;
  metrics_reporting_ash.BindReceiver(
      metrics_reporting_remote.BindNewPipeAndPassReceiver());

  // Calling SetMetricsReportingEnabled() over mojo calls through to the metrics
  // reporting subsystem.
  base::RunLoop run_loop;
  metrics_reporting_remote->SetMetricsReportingEnabled(true,
                                                       run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(test_delegate->enabled_.has_value());
  EXPECT_TRUE(test_delegate->enabled_.value());
}

}  // namespace
}  // namespace crosapi
