// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_helper_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/feature_list.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/favicon/core/favicon_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#include "components/history/core/browser/top_sites.h"
#import "components/history/ios/browser/web_state_top_sites_observer.h"
#include "components/keyed_service/core/service_access_type.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/safe_browsing/core/features.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_state/ios/insecure_input_tab_helper.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"
#import "ios/chrome/browser/autofill/autofill_tab_helper.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/complex_tasks/ios_task_tab_helper.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/features.h"
#import "ios/chrome/browser/download/ar_quick_look_tab_helper.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_tab_helper.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/history_tab_helper.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_factory_impl.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_tab_helper.h"
#import "ios/chrome/browser/infobars/overlays/translate_overlay_tab_helper.h"
#import "ios/chrome/browser/itunes_urls/itunes_urls_handler_tab_helper.h"
#import "ios/chrome/browser/link_to_text/link_to_text_tab_helper.h"
#import "ios/chrome/browser/metrics/pageload_foreground_duration_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/open_in/open_in_tab_helper.h"
#import "ios/chrome/browser/overscroll_actions/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/passwords/well_known_change_password_tab_helper.h"
#import "ios/chrome/browser/policy/policy_features.h"
#import "ios/chrome/browser/policy_url_blocking/policy_url_blocking_tab_helper.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/reading_list_web_state_observer.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_query_manager.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/chrome/browser/search_engines/search_engine_tab_helper.h"
#import "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"
#import "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/u2f/u2f_tab_helper.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/voice/voice_search_navigations_tab_helper.h"
#import "ios/chrome/browser/web/blocked_popup_tab_helper.h"
#include "ios/chrome/browser/web/error_page_controller_bridge.h"
#import "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/font_size_tab_helper.h"
#import "ios/chrome/browser/web/image_fetch_tab_helper.h"
#import "ios/chrome/browser/web/invalid_url_tab_helper.h"
#import "ios/chrome/browser/web/load_timing_tab_helper.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/print_tab_helper.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web/web_state_delegate_tab_helper.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/components/security_interstitials/legacy_tls/legacy_tls_tab_allow_list.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_helper.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/common/features.h"
#import "ios/web/public/web_state.h"

void AttachTabHelpers(web::WebState* web_state, bool for_prerender) {
  // TabIdHelper sets up the tab ID.
  TabIdTabHelper::CreateForWebState(web_state);

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());

  // IOSChromeSessionTabHelper sets up the session ID used by other helpers,
  // so it needs to be created before them.
  IOSChromeSessionTabHelper::CreateForWebState(web_state);

  WebStateDelegateTabHelper::CreateForWebState(web_state);

  NSString* tab_id = TabIdTabHelper::FromWebState(web_state)->tab_id();
  VoiceSearchNavigationTabHelper::CreateForWebState(web_state);
  IOSChromeSyncedTabDelegate::CreateForWebState(web_state);
  InfoBarManagerImpl::CreateForWebState(web_state);
  BlockedPopupTabHelper::CreateForWebState(web_state);
  FindTabHelper::CreateForWebState(web_state);
  U2FTabHelper::CreateForWebState(web_state);
  StoreKitTabHelper::CreateForWebState(web_state);
  ITunesUrlsHandlerTabHelper::CreateForWebState(web_state);
  HistoryTabHelper::CreateForWebState(web_state);
  LoadTimingTabHelper::CreateForWebState(web_state);
  OverscrollActionsTabHelper::CreateForWebState(web_state);
  IOSTaskTabHelper::CreateForWebState(web_state);
  AppLauncherTabHelper::CreateForWebState(web_state);
  security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
      web_state);
  password_manager::WellKnownChangePasswordTabHelper::CreateForWebState(
      web_state);
  ErrorPageControllerBridge::CreateForWebState(web_state);

  if (base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage)) {
    InvalidUrlTabHelper::CreateForWebState(web_state);
  }

  if (base::FeatureList::IsEnabled(kInfobarOverlayUI)) {
    InfobarOverlayRequestInserter::CreateForWebState(web_state);
    InfobarOverlayTabHelper::CreateForWebState(web_state);
    TranslateOverlayTabHelper::CreateForWebState(web_state);
  }

  if (base::FeatureList::IsEnabled(web::kWebPageTextAccessibility)) {
    FontSizeTabHelper::CreateForWebState(web_state);
  }

  if (base::FeatureList::IsEnabled(kLogBreadcrumbs)) {
    BreadcrumbManagerTabHelper::CreateForWebState(web_state);
  }

  SafeBrowsingQueryManager::CreateForWebState(web_state);
  SafeBrowsingTabHelper::CreateForWebState(web_state);
  SafeBrowsingUrlAllowList::CreateForWebState(web_state);
  SafeBrowsingUnsafeResourceContainer::CreateForWebState(web_state);

  if (IsURLBlocklistEnabled()) {
    PolicyUrlBlockingTabHelper::CreateForWebState(web_state);
  }

  ImageFetchTabHelper::CreateForWebState(web_state);

  NewTabPageTabHelper::CreateForWebState(web_state);
  OpenInTabHelper::CreateForWebState(web_state);
  ChromeBrowserState* original_browser_state =
      browser_state->GetOriginalChromeBrowserState();
  favicon::WebFaviconDriver::CreateForWebState(
      web_state,
      ios::FaviconServiceFactory::GetForBrowserState(
          original_browser_state, ServiceAccessType::IMPLICIT_ACCESS));
  history::WebStateTopSitesObserver::CreateForWebState(
      web_state,
      ios::TopSitesFactory::GetForBrowserState(original_browser_state).get());

  UniqueIDDataTabHelper::CreateForWebState(web_state);

  PasswordTabHelper::CreateForWebState(web_state);

  AutofillTabHelper::CreateForWebState(
      web_state,
      PasswordTabHelper::FromWebState(web_state)->GetPasswordManager());

  // Depends on favicon::WebFaviconDriver, must be created after it.
    SearchEngineTabHelper::CreateForWebState(web_state);

  FormSuggestionTabHelper::CreateForWebState(web_state, @[
    PasswordTabHelper::FromWebState(web_state)->GetSuggestionProvider(),
    AutofillTabHelper::FromWebState(web_state)->GetSuggestionProvider(),
  ]);

  InsecureInputTabHelper::CreateForWebState(web_state);

  ukm::InitializeSourceUrlRecorderForWebState(web_state);

  ARQuickLookTabHelper::CreateForWebState(web_state);

  PageloadForegroundDurationTabHelper::CreateForWebState(web_state);

  LookalikeUrlTabHelper::CreateForWebState(web_state);
  LookalikeUrlTabAllowList::CreateForWebState(web_state);
  LookalikeUrlContainer::CreateForWebState(web_state);

  if (base::FeatureList::IsEnabled(web::features::kIOSLegacyTLSInterstitial)) {
    LegacyTLSTabAllowList::CreateForWebState(web_state);
  }

  // TODO(crbug.com/794115): pre-rendered WebState have lots of unnecessary
  // tab helpers for historical reasons. For the moment, AttachTabHelpers
  // allows to inhibit the creation of some of them. Once PreloadController
  // has been refactored to only create the necessary tab helpers, this
  // condition can be removed.
  if (!for_prerender) {
    SadTabTabHelper::CreateForWebState(web_state);
    SnapshotTabHelper::CreateForWebState(web_state, tab_id);
    PagePlaceholderTabHelper::CreateForWebState(web_state);
    PrintTabHelper::CreateForWebState(web_state);
    InfobarBadgeTabHelper::CreateForWebState(web_state);
  }

  OmniboxGeolocationTabHelper::CreateForWebState(web_state);

  // Allow the embedder to attach tab helpers.
  ios::GetChromeBrowserProvider()->AttachTabHelpers(web_state);

  if (base::FeatureList::IsEnabled(kSharedHighlightingIOS)) {
    LinkToTextTabHelper::CreateForWebState(web_state);
  }
}
