// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/system/sys_info.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "chromecast/app/cast_main_delegate.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/test_launcher.h"
#include "ipc/ipc_channel.h"
#include "mojo/core/embedder/embedder.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif  // defined(OS_WIN)

namespace chromecast {
namespace shell {

class CastTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  CastTestLauncherDelegate() {}
  ~CastTestLauncherDelegate() override {}

  int RunTestSuite(int argc, char** argv) override {
    base::TestSuite test_suite(argc, argv);
    // Browser tests are expected not to tear-down various globals .
    test_suite.DisableCheckForLeakedGlobals();
    return test_suite.Run();
  }

 protected:
#if !defined(OS_ANDROID)
  content::ContentMainDelegate* CreateContentMainDelegate() override {
    return new CastMainDelegate();
  }
#endif  // defined(OS_ANDROID)

 private:
  DISALLOW_COPY_AND_ASSIGN(CastTestLauncherDelegate);
};

}  // namespace shell
}  // namespace chromecast

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs(/*cores_per_job=*/2);
  if (parallel_jobs == 0U)
    return 1;

#if defined(OS_WIN)
  // Load and pin user32.dll to avoid having to load it once tests start while
  // on the main thread loop where blocking calls are disallowed.
  base::win::PinUser32();
#endif  // OS_WIN

  chromecast::shell::CastTestLauncherDelegate launcher_delegate;
  mojo::core::Init();
  content::ForceInProcessNetworkService(true);
  return content::LaunchTests(&launcher_delegate, parallel_jobs, argc, argv);
}
