// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LONG_SCREENSHOTS_LONG_SCREENSHOTS_TAB_SERVICE_H_
#define CHROME_BROWSER_LONG_SCREENSHOTS_LONG_SCREENSHOTS_TAB_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/paint_preview_policy.h"

namespace content {
class WebContents;
}  // namespace content

namespace long_screenshots {

// A service for capturing Long Screenshots using PaintPreview. Writes the
// retrieved bitmap to file.
// TODO(tgupta): Handle the deletion of old files when the long screenshots
// feature ends or when Chrome starts up (to handle when Chrome is killed in the
// background and there was no opportunity to clean the files).
class LongScreenshotsTabService
    : public paint_preview::PaintPreviewBaseService {
 public:
  LongScreenshotsTabService(
      std::unique_ptr<paint_preview::PaintPreviewFileMixin> file_mixin,
      std::unique_ptr<paint_preview::PaintPreviewPolicy> policy,
      bool is_off_the_record);
  ~LongScreenshotsTabService() override;

  // Define a list of statuses to describe the calling of paint preview and
  // generation of the bitmap.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.share.long_screenshots.bitmap_generation)
  enum Status {
    kUnknown = 0,
    kOk = 1,
    kDirectoryCreationFailed = 2,
    kCaptureFailed = 3,
    kProtoSerializationFailed = 4,
    kWebContentsGone = 5,
    kNativeServiceUninitialized = 6,
    kLowMemoryDetected = 7,
    kProtoDeserializationFailed = 8,
    kNativeServiceNotInitialized = 9,
  };

  // Captures a Paint Preview of |contents| which should be associated with
  // |tab_id| for storage. |callback| is invoked on completion to indicate
  // status.
  // Clip args specify the bounds of the capture:
  // clipX: Where to start the capture on the X axis
  // clipY: Where to start the capture on the Y axis
  // clipWidth: How wide of a capture relative to clipX
  // clipHeight: How wide of a capture relative to clipY
  void CaptureTab(int tab_id,
                  content::WebContents* contents,
                  int clipX,
                  int clipY,
                  int clipWidth,
                  int clipHeight);

  // Delete all old long screenshot files.
  void DeleteAllLongScreenshotFiles();

  // JNI wrapped versions of the above methods
  void CaptureTabAndroid(
      JNIEnv* env,
      jint j_tab_id,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      jint clipX,
      jint clipY,
      jint clipWidth,
      jint clipHeight);
  void LongScreenshotsClosedAndroid(JNIEnv* env);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaRef() { return java_ref_; }

 private:
  // Retrieves the content::WebContents from the |frame_tree_node_id|
  // (confirming that the contents are alive using the |frame_routing_id|).
  // Calls PaintPreviewBaseService to retrieve the bitmap and write it to file.
  void CaptureTabInternal(int tab_id,
                          const paint_preview::DirectoryKey& key,
                          int frame_tree_node_id,
                          content::GlobalFrameRoutingId frame_routing_id,
                          int clipX,
                          int clipY,
                          int clipWidth,
                          int clipHeight,
                          const base::Optional<base::FilePath>& file_path);

  void OnCaptured(int tab_id,
                  const paint_preview::DirectoryKey& key,
                  int frame_tree_node_id,
                  paint_preview::PaintPreviewBaseService::CaptureStatus status,
                  std::unique_ptr<paint_preview::CaptureResult> result);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  base::WeakPtrFactory<LongScreenshotsTabService> weak_ptr_factory_{this};
};

}  // namespace long_screenshots

#endif  // CHROME_BROWSER_LONG_SCREENSHOTS_LONG_SCREENSHOTS_TAB_SERVICE_H_
