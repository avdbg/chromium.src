// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/logging.h"

#include <guiddef.h>

#include "base/logging.h"
#include "base/logging_win.h"

namespace remoting {

void InitHostLogging() {
  // Write logs to the system debug log.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  // Enable trace control and transport through event tracing for Windows.
  logging::LogEventProvider::Initialize(kRemotingHostLogProviderGuid);
}

}  // namespace remoting
