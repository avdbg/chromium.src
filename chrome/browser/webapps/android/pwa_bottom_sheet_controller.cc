// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/android/pwa_bottom_sheet_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/banners/android/chrome_app_banner_manager_android.h"
#include "chrome/browser/webapps/android/features.h"
#include "chrome/browser/webapps/android/jni_headers/PwaBottomSheetControllerProvider_jni.h"
#include "chrome/browser/webapps/android/jni_headers/PwaBottomSheetController_jni.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/android/java_bitmap.h"

using base::ASCIIToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

bool CanShowBottomSheet(content::WebContents* web_contents,
                        const std::vector<SkBitmap>& screenshots) {
  if (!base::FeatureList::IsEnabled(
          webapps::features::kPwaInstallUseBottomSheet)) {
    return false;
  }

  if (screenshots.size() == 0)
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PwaBottomSheetControllerProvider_canShowPwaBottomSheetInstaller(
      env, web_contents->GetJavaWebContents());
}

}  // anonymous namespace

namespace webapps {

PwaBottomSheetController::~PwaBottomSheetController() = default;

// static
jboolean JNI_PwaBottomSheetController_RequestOrExpandBottomSheetInstaller(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    int install_trigger) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  auto* app_banner_manager =
      ChromeAppBannerManagerAndroid::FromWebContents(web_contents);

  WebappInstallSource install_source = InstallableMetrics::GetInstallSource(
      web_contents, static_cast<InstallTrigger>(install_trigger));
  return app_banner_manager->MaybeShowPwaBottomSheetController(
      /* expand_sheet= */ true, install_source);
}

// static
bool PwaBottomSheetController::MaybeShow(
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
        a2hs_event_callback) {
  if (!CanShowBottomSheet(web_contents, screenshots))
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();
  if (Java_PwaBottomSheetControllerProvider_doesBottomSheetExist(
          env, web_contents->GetJavaWebContents())) {
    Java_PwaBottomSheetControllerProvider_updateState(
        env, web_contents->GetJavaWebContents(),
        jint(a2hs_params->install_source), expand_sheet);
  } else {
    // Lifetime of this object is managed by the Java counterpart, iff bottom
    // sheets can be shown (otherwise an infobar is used and this class is no
    // longer needed).
    PwaBottomSheetController* controller = new PwaBottomSheetController(
        app_name, primary_icon, is_primary_icon_maskable, start_url,
        screenshots, description, std::move(a2hs_params),
        std::move(a2hs_event_callback));
    controller->ShowBottomSheetInstaller(web_contents, expand_sheet);
  }
  return true;
}

PwaBottomSheetController::PwaBottomSheetController(
    const base::string16& app_name,
    const SkBitmap& primary_icon,
    const bool is_primary_icon_maskable,
    const GURL& start_url,
    const std::vector<SkBitmap>& screenshots,
    const base::string16& description,
    std::unique_ptr<AddToHomescreenParams> a2hs_params,
    base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                 const AddToHomescreenParams&)>
        a2hs_event_callback)
    : app_name_(app_name),
      primary_icon_(primary_icon),
      is_primary_icon_maskable_(is_primary_icon_maskable),
      start_url_(start_url),
      screenshots_(screenshots),
      description_(description),
      a2hs_params_(std::move(a2hs_params)),
      a2hs_event_callback_(a2hs_event_callback) {}

void PwaBottomSheetController::Destroy(JNIEnv* env) {
  // When the bottom sheet hasn't been expanded, it is considered equivalent to
  // the regular install infobar and the expanded state equivalent
  // to the regular install dialog prompt. Therefore, we send UI_CANCELLED
  // only if the bottom sheet was ever expanded.
  if (!install_triggered_ && sheet_expanded_) {
    a2hs_event_callback_.Run(AddToHomescreenInstaller::Event::UI_CANCELLED,
                             *a2hs_params_);
  }
  delete this;
}

void PwaBottomSheetController::UpdateInstallSource(JNIEnv* env,
                                                   int install_source) {
  a2hs_params_->install_source =
      static_cast<WebappInstallSource>(install_source);
}

void PwaBottomSheetController::OnSheetExpanded(JNIEnv* env) {
  a2hs_event_callback_.Run(AddToHomescreenInstaller::Event::UI_SHOWN,
                           *a2hs_params_);
  sheet_expanded_ = true;
}

void PwaBottomSheetController::OnAddToHomescreen(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return;
  auto* app_banner_manager =
      ChromeAppBannerManagerAndroid::FromWebContents(web_contents);
  if (!app_banner_manager)
    return;

  install_triggered_ = true;
  app_banner_manager->Install(*a2hs_params_, std::move(a2hs_event_callback_));
}

void PwaBottomSheetController::ShowBottomSheetInstaller(
    content::WebContents* web_contents,
    bool expand_sheet) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_user_title =
      ConvertUTF16ToJavaString(env, app_name_);
  // Trim down the app URL to the origin. Elide cryptographic schemes so HTTP
  // is still shown.
  ScopedJavaLocalRef<jstring> j_url = ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               start_url_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  ScopedJavaLocalRef<jstring> j_description =
      ConvertUTF16ToJavaString(env, description_);

  ScopedJavaLocalRef<jobject> j_bitmap =
      gfx::ConvertToJavaBitmap(primary_icon_);

  Java_PwaBottomSheetControllerProvider_showPwaBottomSheetInstaller(
      env, reinterpret_cast<intptr_t>(this), web_contents->GetJavaWebContents(),
      j_bitmap, is_primary_icon_maskable_, j_user_title, j_url, j_description);

  for (const auto& screenshot : screenshots_) {
    if (!screenshot.isNull())
      UpdateScreenshot(screenshot, web_contents);
  }

  if (expand_sheet) {
    Java_PwaBottomSheetControllerProvider_expandPwaBottomSheetInstaller(
        env, web_contents->GetJavaWebContents());
  }
}

void PwaBottomSheetController::UpdateScreenshot(
    const SkBitmap& screenshot,
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_screenshot =
      gfx::ConvertToJavaBitmap(screenshot);
  Java_PwaBottomSheetController_addWebAppScreenshot(
      env, java_screenshot, web_contents->GetJavaWebContents());
}

}  // namespace webapps
