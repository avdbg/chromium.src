// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_demo_mode_delegate_impl.h"

#include <memory>

#include "base/optional.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcDemoModeDelegateImplTest : public testing::Test {
 public:
  ArcDemoModeDelegateImplTest()
      : user_manager_enabler_(
            std::make_unique<chromeos::FakeChromeUserManager>()) {}
  ~ArcDemoModeDelegateImplTest() override = default;
  ArcDemoModeDelegateImplTest(const ArcDemoModeDelegateImplTest&) = delete;
  ArcDemoModeDelegateImplTest& operator=(const ArcDemoModeDelegateImplTest&) =
      delete;

 protected:
  chromeos::DemoModeTestHelper* demo_helper() { return &demo_helper_; }

  ArcDemoModeDelegateImpl* delegate() { return &delegate_; }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  user_manager::ScopedUserManager user_manager_enabler_;
  chromeos::DemoModeTestHelper demo_helper_;
  ArcDemoModeDelegateImpl delegate_;
};

// Test that EnsureOfflineResourcesLoaded returns immediately if demo mode is
// not enabled.
TEST_F(ArcDemoModeDelegateImplTest, EnsureOfflineResourcesLoaded_NotEnabled) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kNone);

  bool was_called = false;
  base::OnceClosure callback =
      base::BindOnce([](bool* was_called) { *was_called = true; }, &was_called);
  delegate()->EnsureOfflineResourcesLoaded(std::move(callback));
  EXPECT_TRUE(was_called);
}

// Test that EnsureOfflineResourcesLoaded returns after resources are loaded if
// demo mode is enabled.
TEST_F(ArcDemoModeDelegateImplTest, EnsureOfflineResourcesLoaded_Enabled) {
  demo_helper()->InitializeSessionWithPendingComponent();

  bool was_called = false;
  base::OnceClosure callback =
      base::BindOnce([](bool* was_called) { *was_called = true; }, &was_called);
  delegate()->EnsureOfflineResourcesLoaded(std::move(callback));
  EXPECT_FALSE(was_called);

  demo_helper()->FinishLoadingComponent();
  EXPECT_TRUE(was_called);
}

// Test that GetDemoAppsPath returns empty path if demo mode is not enabled.
TEST_F(ArcDemoModeDelegateImplTest, GetDemoAppsPath_NotEnabled) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kNone);

  base::FilePath demo_session_apps_path = delegate()->GetDemoAppsPath();
  EXPECT_TRUE(demo_session_apps_path.empty());
}

// Test that GetDemoAppsPath returns the correct path if demo mode is enabled.
TEST_F(ArcDemoModeDelegateImplTest, GetDemoAppsPath_Enabled) {
  demo_helper()->InitializeSession();

  base::FilePath demo_session_apps_path = delegate()->GetDemoAppsPath();
  EXPECT_FALSE(demo_session_apps_path.empty());
  EXPECT_EQ(
      demo_helper()->GetDemoResourcesPath().Append("android_demo_apps.squash"),
      demo_session_apps_path);
}

}  // namespace
}  // namespace arc
