// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_CONTEXT_MANAGER_H_
#define ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_CONTEXT_MANAGER_H_

#include <android/native_window.h>
#include <jni.h>
#include <memory>

#include "android_webview/test/shell/src/draw_fn/overlays_manager.h"
#include "base/android/scoped_java_ref.h"

namespace draw_fn {

class ContextManager {
 public:
  ContextManager();
  virtual ~ContextManager();

  void SetSurface(JNIEnv* env,
                  const base::android::JavaRef<jobject>& surface,
                  int width,
                  int height);
  virtual void ResizeSurface(JNIEnv* env, int width, int height) = 0;
  void SetOverlaysSurface(JNIEnv* env,
                          const base::android::JavaRef<jobject>& surface);
  void Sync(JNIEnv* env, int functor, bool apply_force_dark);
  virtual base::android::ScopedJavaLocalRef<jintArray> Draw(
      JNIEnv* env,
      int width,
      int height,
      int scroll_x,
      int scroll_y,
      jboolean readback_quadrants) = 0;

 protected:
  void CreateContext(JNIEnv* env,
                     const base::android::JavaRef<jobject>& surface,
                     int width,
                     int height);
  virtual void DoCreateContext(JNIEnv* env, int width, int height) = 0;
  virtual void DestroyContext() = 0;
  virtual void CurrentFunctorChanged() = 0;

  base::android::ScopedJavaGlobalRef<jobject> java_surface_;
  ANativeWindow* native_window_ = nullptr;

  int current_functor_ = 0;

  OverlaysManager overlays_manager_;
};

}  // namespace draw_fn

#endif  // ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_CONTEXT_MANAGER_H_
