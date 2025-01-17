// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/android/chrome_app_banner_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
#include "chrome/browser/android/webapk/webapk_ukm_recorder.h"
#include "chrome/browser/banners/android/jni_headers/AppBannerInProductHelpControllerProvider_jni.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/webapps/android/features.h"
#include "chrome/browser/webapps/android/pwa_bottom_sheet_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace webapps {

namespace {

// The key to look up what the minimum engagement score is for showing the
// in-product help.
constexpr char kMinEngagementForIphKey[] = "x_min_engagement_for_iph";

// The key to look up whether the in-product help should replace the toolbar or
// complement it.
constexpr char kIphReplacesToolbar[] = "x_iph_replaces_toolbar";

}  // anonymous namespace

ChromeAppBannerManagerAndroid::ChromeAppBannerManagerAndroid(
    content::WebContents* web_contents)
    : AppBannerManagerAndroid(web_contents) {}

ChromeAppBannerManagerAndroid::~ChromeAppBannerManagerAndroid() = default;

InstallableParams
ChromeAppBannerManagerAndroid::ParamsToPerformInstallableWebAppCheck() {
  InstallableParams params =
      AppBannerManagerAndroid::ParamsToPerformInstallableWebAppCheck();
  if (base::FeatureList::IsEnabled(
          webapps::features::kPwaInstallUseBottomSheet)) {
    params.fetch_screenshots = true;
  }

  return params;
}

void ChromeAppBannerManagerAndroid::OnDidPerformInstallableWebAppCheck(
    const InstallableData& data) {
  if (data.NoBlockingErrors())
    WebApkUkmRecorder::RecordWebApkableVisit(data.manifest_url);
  screenshots_ = data.screenshots;

  AppBannerManagerAndroid::OnDidPerformInstallableWebAppCheck(data);
}

void ChromeAppBannerManagerAndroid::MaybeShowAmbientBadge() {
  if (MaybeShowInProductHelp() &&
      base::GetFieldTrialParamByFeatureAsBool(
          feature_engagement::kIPHPwaInstallAvailableFeature,
          kIphReplacesToolbar, false)) {
    DVLOG(2) << "Install infobar overridden by IPH, as per experiment.";
    return;
  }

  AppBannerManagerAndroid::MaybeShowAmbientBadge();
}

void ChromeAppBannerManagerAndroid::ShowAmbientBadge() {
  WebappInstallSource install_source = InstallableMetrics::GetInstallSource(
      web_contents(), InstallTrigger::AMBIENT_BADGE);
  if (!MaybeShowPwaBottomSheetController(/* expand_sheet= */ false,
                                         install_source)) {
    AppBannerManagerAndroid::ShowAmbientBadge();
  }
}

void ChromeAppBannerManagerAndroid::ShowBannerUi(
    WebappInstallSource install_source) {
  if (!native_app_data_.is_null()) {
    AppBannerManagerAndroid::ShowBannerUi(install_source);
    return;
  }

  if (!MaybeShowPwaBottomSheetController(/* expand_sheet= */ true,
                                         install_source)) {
    AppBannerManagerAndroid::ShowBannerUi(install_source);
    return;
  }

  ReportStatus(SHOWING_WEB_APP_BANNER);
}

bool ChromeAppBannerManagerAndroid::MaybeShowPwaBottomSheetController(
    bool expand_sheet,
    WebappInstallSource install_source) {
  auto a2hs_params = CreateAddToHomescreenParams(install_source);
  return PwaBottomSheetController::MaybeShow(
      web_contents(), GetAppName(), primary_icon_, has_maskable_primary_icon_,
      manifest_.start_url, screenshots_,
      manifest_.description.value_or(base::string16()), expand_sheet,
      std::move(a2hs_params),
      base::BindRepeating(&ChromeAppBannerManagerAndroid::OnInstallEvent,
                          ChromeAppBannerManagerAndroid::GetAndroidWeakPtr()));
}

void ChromeAppBannerManagerAndroid::RecordExtraMetricsForInstallEvent(
    AddToHomescreenInstaller::Event event,
    const AddToHomescreenParams& a2hs_params) {
  if (a2hs_params.app_type == AddToHomescreenParams::AppType::WEBAPK &&
      event == AddToHomescreenInstaller::Event::UI_CANCELLED) {
    webapk::TrackInstallEvent(
        webapk::ADD_TO_HOMESCREEN_DIALOG_DISMISSED_BEFORE_INSTALLATION);
  }
}

bool ChromeAppBannerManagerAndroid::MaybeShowInProductHelp() const {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHPwaInstallAvailableFeature)) {
    DVLOG(2) << "Feature not enabled";
    return false;
  }

  if (!web_contents()) {
    DVLOG(2) << "IPH for PWA aborted: null WebContents";
    return false;
  }

  double last_engagement_score =
      GetSiteEngagementService()->GetScore(validated_url_);
  int min_engagement = base::GetFieldTrialParamByFeatureAsInt(
      feature_engagement::kIPHPwaInstallAvailableFeature,
      kMinEngagementForIphKey, 0);
  if (last_engagement_score < min_engagement) {
    DVLOG(2) << "IPH for PWA aborted: Engagement score too low: "
             << last_engagement_score << " < " << min_engagement;
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::string error_message = base::android::ConvertJavaStringToUTF8(
      Java_AppBannerInProductHelpControllerProvider_showInProductHelp(
          env, web_contents()->GetJavaWebContents()));
  if (!error_message.empty()) {
    DVLOG(2) << "IPH for PWA showing aborted. " << error_message;
    return false;
  }

  DVLOG(2) << "Showing IPH.";
  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeAppBannerManagerAndroid)

}  // namespace webapps
