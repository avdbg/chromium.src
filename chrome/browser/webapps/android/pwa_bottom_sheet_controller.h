// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_ANDROID_PWA_BOTTOM_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_WEBAPPS_ANDROID_PWA_BOTTOM_SHEET_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/android/add_to_homescreen_installer.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace webapps {

// A Controller for the BottomSheet install UI for progressive web apps.
// If successfully created, the lifetime of this object is tied to the lifetime
// of the BottomSheet UI being shown and the object is destroyed from Java when
// the UI is dismissed. This class can be instantiated from both the Java side
// (when the user selects Install App from the App Menu) and from the C++ side,
// when the engagement score for the web site is high enough to promote the
// install of a PWA.
class PwaBottomSheetController {
 public:
  // If possible, shows/expand the PWA Bottom Sheet installer and returns true.
  // Otherwise does nothing and returns false.
  static bool MaybeShow(
      content::WebContents* web_contents,
      const base::string16& app_name,
      const SkBitmap& primary_icon,
      const bool is_primary_icon_maskable,
      const GURL& start_url,
      const std::vector<SkBitmap>& screenshots,
      const base::string16& description,
      bool expand_sheet,
      std::unique_ptr<AddToHomescreenParams> a2hs_params,
      base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                   const AddToHomescreenParams&)>
          a2hs_event_callback);

  virtual ~PwaBottomSheetController();

  // Called from the Java side and destructs this object.
  void Destroy(JNIEnv* env);

  // Called from the Java side when install source needs to be updated (e.g. if
  // the bottom sheet is created as an ambient badge, but then the user uses the
  // menu item to expand it, we will need to update the source from
  // AMBIENT_BADGE to MENU).
  void UpdateInstallSource(JNIEnv* env, int install_source);

  // Called from the Java side when bottom sheet got expanded.
  void OnSheetExpanded(JNIEnv* env);

  // Called from the Java side when the user opts to install.
  void OnAddToHomescreen(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents);

 private:
  PwaBottomSheetController(
      const base::string16& app_name,
      const SkBitmap& primary_icon,
      const bool is_primary_icon_maskable,
      const GURL& start_url,
      const std::vector<SkBitmap>& screenshots,
      const base::string16& description,
      std::unique_ptr<AddToHomescreenParams> a2hs_params,
      base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                   const AddToHomescreenParams&)>
          a2hs_event_callback);
  PwaBottomSheetController(const PwaBottomSheetController&) = delete;
  PwaBottomSheetController& operator=(const PwaBottomSheetController&) = delete;

  // Shows the Bottom Sheet installer UI for a given |web_contents|.
  void ShowBottomSheetInstaller(content::WebContents* web_contents,
                                bool expand_sheet);

  // Called for each screenshot available. Updates the Java side with the new
  // image.
  void UpdateScreenshot(const SkBitmap& screenshot,
                        content::WebContents* web_contents);

  const base::string16 app_name_;
  const SkBitmap primary_icon_;
  const bool is_primary_icon_maskable_ = false;
  const GURL& start_url_;
  const std::vector<SkBitmap>& screenshots_;
  const base::string16 description_;
  // Contains app parameters such as its type and the install source used that
  // will be passed to |a2hs_event_callback_| eventually.
  std::unique_ptr<AddToHomescreenParams> a2hs_params_;
  // Called to provide input into the state of the installation process.
  base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                               const AddToHomescreenParams&)>
      a2hs_event_callback_;
  // Whether the bottom sheet has been expanded.
  bool sheet_expanded_ = false;
  // Whether the install flow was triggered.
  bool install_triggered_ = false;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_WEBAPPS_ANDROID_PWA_BOTTOM_SHEET_CONTROLLER_H_
