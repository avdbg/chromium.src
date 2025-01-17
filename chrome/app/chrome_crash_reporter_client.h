// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_H_
#define CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_H_

#include "build/chromeos_buildflags.h"

#if !defined(OS_WIN)

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"

class ChromeCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  static void Create();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If true, processes of this type should pass crash-loop-before down to the
  // crash reporter and to their children (if the children's type is a process
  // type that wants crash-loop-before).
  static bool ShouldPassCrashLoopBefore(const std::string& process_type);
#endif

  // crash_reporter::CrashReporterClient implementation.
#if !defined(OS_MAC) && !defined(OS_ANDROID)
  void SetCrashReporterClientIdFromGUID(
      const std::string& client_guid) override;
#endif

#if defined(OS_POSIX)
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;
  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override;
#if !defined(OS_MACOSX)
  base::FilePath GetReporterLogFilename() override;
#endif
#endif

  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;

#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS)
  bool GetCrashMetricsLocation(base::FilePath* metrics_dir) override;
#endif

  bool IsRunningUnattended() override;

  bool GetCollectStatsConsent() override;

#if defined(OS_MAC)
  bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled) override;
#endif

#if defined(OS_ANDROID)
  int GetAndroidMinidumpDescriptor() override;
#endif

#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS)
  bool ShouldMonitorCrashHandlerExpensively() override;
#endif

  bool EnableBreakpadForProcess(const std::string& process_type) override;

 private:
  friend class base::NoDestructor<ChromeCrashReporterClient>;

  ChromeCrashReporterClient();
  ~ChromeCrashReporterClient() override;

  DISALLOW_COPY_AND_ASSIGN(ChromeCrashReporterClient);
};

#endif  // OS_WIN

#endif  // CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_H_
