// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/register_jni.h"

#include "chrome/browser/android/vr/register_gvr_jni.h"

namespace vr {

bool RegisterJni(JNIEnv* env) {
  // The GVR Java code is normally in the vr DFM, which will be loaded into the
  // base class loader. If the enable_chrome_module gn arg is enabled, the GVR
  // Java code will be in the chrome DFM, which is loaded as an isolated split.
  // This means the Java code is no longer automatically loaded in the base
  // class loader. Automatic JNI registration only works for native methods
  // associated with the base class loader (which loaded libmonochrome.so, so
  // will look for symbols there). Most of Chrome's native methods are in
  // GEN_JNI.java which is present in the base module, so do not need manual
  // registration. Since GVR has native methods outside of GEN_JNI.java which
  // are not present in the base module, these must be manually registered.
  if (!vr::RegisterGvrJni(env)) {
    return false;
  }
  return true;
}

}  // namespace vr
