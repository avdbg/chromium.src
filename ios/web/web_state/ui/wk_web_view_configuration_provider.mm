// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#include <vector>

#include "base/check.h"
#include "base/ios/ios_util.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "components/safe_browsing/core/features.h"
#include "ios/web/common/features.h"
#import "ios/web/js_messaging/crw_wk_script_message_router.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#include "ios/web/js_messaging/java_script_feature_util_impl.h"
#import "ios/web/js_messaging/page_script_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider_observer.h"
#import "ios/web/webui/crw_web_ui_scheme_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

// A key used to associate a WKWebViewConfigurationProvider with a BrowserState.
const char kWKWebViewConfigProviderKeyName[] = "wk_web_view_config_provider";

// Returns a WKUserScript for JavsScript injected into the main frame at the
// beginning of the document load.
WKUserScript* InternalGetDocumentStartScriptForMainFrame(
    BrowserState* browser_state) {
  return [[WKUserScript alloc]
        initWithSource:GetDocumentStartScriptForMainFrame(browser_state)
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:YES];
}

// Returns a WKUserScript for JavsScript injected into the main frame at the
// end of the document load.
WKUserScript* InternalGetDocumentEndScriptForMainFrame(
    BrowserState* browser_state) {
  return [[WKUserScript alloc]
        initWithSource:GetDocumentEndScriptForMainFrame(browser_state)
         injectionTime:WKUserScriptInjectionTimeAtDocumentEnd
      forMainFrameOnly:YES];
}

// Returns a WKUserScript for JavsScript injected into all frames at the
// beginning of the document load.
WKUserScript* InternalGetDocumentStartScriptForAllFrames(
    BrowserState* browser_state) {
  return [[WKUserScript alloc]
        initWithSource:GetDocumentStartScriptForAllFrames(browser_state)
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:NO];
}

// Returns a WKUserScript for JavsScript injected into all frames at the
// end of the document load.
WKUserScript* InternalGetDocumentEndScriptForAllFrames(
    BrowserState* browser_state) {
  return [[WKUserScript alloc]
        initWithSource:GetDocumentEndScriptForAllFrames(browser_state)
         injectionTime:WKUserScriptInjectionTimeAtDocumentEnd
      forMainFrameOnly:NO];
}

}  // namespace

// static
WKWebViewConfigurationProvider&
WKWebViewConfigurationProvider::FromBrowserState(BrowserState* browser_state) {
  DCHECK([NSThread isMainThread]);
  DCHECK(browser_state);
  if (!browser_state->GetUserData(kWKWebViewConfigProviderKeyName)) {
    browser_state->SetUserData(
        kWKWebViewConfigProviderKeyName,
        base::WrapUnique(new WKWebViewConfigurationProvider(browser_state)));
  }
  return *(static_cast<WKWebViewConfigurationProvider*>(
      browser_state->GetUserData(kWKWebViewConfigProviderKeyName)));
}

WKWebViewConfigurationProvider::WKWebViewConfigurationProvider(
    BrowserState* browser_state)
    : browser_state_(browser_state),
      content_rule_list_provider_(
          std::make_unique<WKContentRuleListProvider>(browser_state)) {}

WKWebViewConfigurationProvider::~WKWebViewConfigurationProvider() = default;

void WKWebViewConfigurationProvider::ResetWithWebViewConfiguration(
    WKWebViewConfiguration* configuration) {
  DCHECK([NSThread isMainThread]);

  if (!configuration) {
    configuration = [[WKWebViewConfiguration alloc] init];
  } else {
    configuration = [configuration copy];
  }
  if (configuration_) {
    Purge();
  }
  configuration_ = configuration;

  if (browser_state_->IsOffTheRecord()) {
    [configuration_
        setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
  }

  [configuration_ setIgnoresViewportScaleLimits:YES];

  if (@available(iOS 13, *)) {
    @try {
      // Disable system context menu on iOS 13 and later. Disabling
      // "longPressActions" prevents the WKWebView ContextMenu from being
      // displayed and also prevents the iOS 13 ContextMenu delegate methods
      // from being called.
      // https://github.com/WebKit/webkit/blob/1233effdb7826a5f03b3cdc0f67d713741e70976/Source/WebKit/UIProcess/API/Cocoa/WKWebViewConfiguration.mm#L307
      BOOL disable_long_press_system_actions =

          web::GetWebClient()->EnableLongPressAndForceTouchHandling() ||
          web::GetWebClient()->EnableLongPressUIContextMenu();
      [configuration_ setValue:@(!disable_long_press_system_actions)
                        forKey:@"longPressActionsEnabled"];
    } @catch (NSException* exception) {
      NOTREACHED() << "Error setting value for longPressActionsEnabled";
    }

    // WKWebView's "fradulentWebsiteWarning" is an iOS 13+ feature that is
    // conceptually similar to Safe Browsing but uses a non-Google provider and
    // only works for devices in certain locales. Disable this feature since
    // Chrome uses Google-provided Safe Browsing.
    [[configuration_ preferences] setFraudulentWebsiteWarningEnabled:NO];
  }

  [configuration_ setAllowsInlineMediaPlayback:YES];
  // setJavaScriptCanOpenWindowsAutomatically is required to support popups.
  [[configuration_ preferences] setJavaScriptCanOpenWindowsAutomatically:YES];
  UpdateScripts();

  if (!scheme_handler_) {
    scoped_refptr<network::SharedURLLoaderFactory> shared_loader_factory =
        browser_state_->GetSharedURLLoaderFactory();
    scheme_handler_ = [[CRWWebUISchemeHandler alloc]
        initWithURLLoaderFactory:shared_loader_factory];
  }
  WebClient::Schemes schemes;
  GetWebClient()->AddAdditionalSchemes(&schemes);
  GetWebClient()->GetAdditionalWebUISchemes(&(schemes.standard_schemes));
  for (std::string scheme : schemes.standard_schemes) {
    [configuration_ setURLSchemeHandler:scheme_handler_
                           forURLScheme:base::SysUTF8ToNSString(scheme)];
  }

  content_rule_list_provider_->SetUserContentController(
      configuration_.userContentController);

  for (auto& observer : observers_)
    observer.DidCreateNewConfiguration(this, configuration_);

  // Workaround to force the creation of the WKWebsiteDataStore. This
  // workaround need to be done here, because this method returns a copy of
  // the already created configuration.
  NSSet* data_types = [NSSet setWithObject:WKWebsiteDataTypeCookies];
  [configuration_.websiteDataStore
      fetchDataRecordsOfTypes:data_types
            completionHandler:^(NSArray<WKWebsiteDataRecord*>* records){
            }];
}

WKWebViewConfiguration*
WKWebViewConfigurationProvider::GetWebViewConfiguration() {
  DCHECK([NSThread isMainThread]);
  if (!configuration_) {
    ResetWithWebViewConfiguration(nil);
  }

  // This is a shallow copy to prevent callers from changing the internals of
  // configuration.
  return [configuration_ copy];
}

CRWWKScriptMessageRouter*
WKWebViewConfigurationProvider::GetScriptMessageRouter() {
  DCHECK([NSThread isMainThread]);
  if (!router_) {
    WKUserContentController* userContentController =
        [GetWebViewConfiguration() userContentController];
    router_ = [[CRWWKScriptMessageRouter alloc]
        initWithUserContentController:userContentController];
  }
  return router_;
}

WKContentRuleListProvider*
WKWebViewConfigurationProvider::GetContentRuleListProvider() {
  return content_rule_list_provider_.get();
}

void WKWebViewConfigurationProvider::UpdateScripts() {
  [configuration_.userContentController removeAllUserScripts];

  JavaScriptFeatureManager* java_script_feature_manager =
      JavaScriptFeatureManager::FromBrowserState(browser_state_);

  std::vector<JavaScriptFeature*> features;
  for (JavaScriptFeature* feature :
       java_script_features::GetBuiltInJavaScriptFeatures(browser_state_)) {
    features.push_back(feature);
  }
  for (JavaScriptFeature* feature :
       GetWebClient()->GetJavaScriptFeatures(browser_state_)) {
    features.push_back(feature);
  }
  java_script_feature_manager->ConfigureFeatures(features);

  // Main frame script depends upon scripts injected into all frames, so the
  // "AllFrames" scripts must be injected first.
  [configuration_.userContentController
      addUserScript:InternalGetDocumentStartScriptForAllFrames(browser_state_)];
  [configuration_.userContentController
      addUserScript:InternalGetDocumentStartScriptForMainFrame(browser_state_)];
  [configuration_.userContentController
      addUserScript:InternalGetDocumentEndScriptForAllFrames(browser_state_)];
  [configuration_.userContentController
      addUserScript:InternalGetDocumentEndScriptForMainFrame(browser_state_)];
}

void WKWebViewConfigurationProvider::Purge() {
  DCHECK([NSThread isMainThread]);
  configuration_ = nil;
  router_ = nil;
}

void WKWebViewConfigurationProvider::AddObserver(
    WKWebViewConfigurationProviderObserver* observer) {
  observers_.AddObserver(observer);
}

void WKWebViewConfigurationProvider::RemoveObserver(
    WKWebViewConfigurationProviderObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace web
