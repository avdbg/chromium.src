// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_java_script_feature.h"

#import <WebKit/WebKit.h>

#import "base/callback.h"
#import "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "ios/web/js_features/context_menu/context_menu_params_utils.h"
#import "ios/web/public/browser_state.h"
#include "ios/web/public/js_messaging/java_script_feature_util.h"
#include "ios/web/public/js_messaging/web_frame_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kContextMenuJavaScriptFeatureKeyName[] =
    "context_menu_java_script_feature";

const char kAllFramesContextMenuScript[] = "all_frames_context_menu_js";
const char kMainFrameContextMenuScript[] = "main_frame_context_menu_js";

const char kFindElementResultHandlerName[] = "FindElementResultHandler";
}

namespace web {

ContextMenuJavaScriptFeature::ContextMenuJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kAnyContentWorld,
          {FeatureScript::CreateWithFilename(
               kAllFramesContextMenuScript,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames),
           FeatureScript::CreateWithFilename(
               kMainFrameContextMenuScript,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kMainFrame)},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}
ContextMenuJavaScriptFeature::~ContextMenuJavaScriptFeature() = default;

// static
ContextMenuJavaScriptFeature* ContextMenuJavaScriptFeature::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);

  ContextMenuJavaScriptFeature* feature =
      static_cast<ContextMenuJavaScriptFeature*>(
          browser_state->GetUserData(kContextMenuJavaScriptFeatureKeyName));
  if (!feature) {
    feature = new ContextMenuJavaScriptFeature();
    browser_state->SetUserData(kContextMenuJavaScriptFeatureKeyName,
                               base::WrapUnique(feature));
  }
  return feature;
}

void ContextMenuJavaScriptFeature::GetElementAtPoint(
    WebState* web_state,
    std::string requestID,
    CGPoint point,
    CGSize web_content_size,
    ElementDetailsCallback callback) {
  callbacks_[requestID] = std::move(callback);

  WebFrame* main_frame = GetMainFrame(web_state);
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(requestID));
  parameters.push_back(base::Value(point.x));
  parameters.push_back(base::Value(point.y));
  parameters.push_back(base::Value(web_content_size.width));
  parameters.push_back(base::Value(web_content_size.height));
  CallJavaScriptFunction(main_frame, "findElementAtPoint", parameters);
}

base::Optional<std::string>
ContextMenuJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kFindElementResultHandlerName;
}

void ContextMenuJavaScriptFeature::ScriptMessageReceived(
    BrowserState* browser_state,
    WKScriptMessage* message) {
  DCHECK(message);

  if (![message.body isKindOfClass:[NSDictionary class]]) {
    // Ignore malformed responses.
    return;
  }

  NSString* ns_request_id = message.body[@"requestId"];
  if (![ns_request_id isKindOfClass:[NSString class]] ||
      ![ns_request_id length]) {
    // Ignore malformed responses.
    return;
  }

  std::string request_id = base::SysNSStringToUTF8(ns_request_id);

  auto callback_it = callbacks_.find(request_id);
  if (callback_it == callbacks_.end()) {
    return;
  }

  ElementDetailsCallback callback = std::move(callback_it->second);
  if (callback.is_null()) {
    return;
  }

  web::ContextMenuParams params =
      web::ContextMenuParamsFromElementDictionary(message.body);
  params.is_main_frame = message.frameInfo.mainFrame;

  std::move(callback).Run(request_id, params);
}

}  // namespace web
