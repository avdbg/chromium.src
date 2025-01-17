// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_WINDOW_ERROR_WINDOW_ERROR_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_FEATURES_WINDOW_ERROR_WINDOW_ERROR_JAVA_SCRIPT_FEATURE_H_

#include "base/callback.h"
#include "ios/web/public/js_messaging/java_script_feature.h"
#include "url/gurl.h"

@class WKScriptMessage;

namespace web {

// A feature which listens for JavaScript errors in all frames and executes a
// given callback with details about each received error.
class WindowErrorJavaScriptFeature : public JavaScriptFeature {
 public:
  // Wraps information about an error.
  struct ErrorDetails {
   public:
    ErrorDetails();
    ~ErrorDetails();

    // The filename in which the error occurred.
    NSString* filename;

    // The line number at which the error occurred.
    int line_number;

    // The error message.
    NSString* message;

    // The url where the error occurred.
    GURL url;

    // Whether or not this error occurred in the main frame.
    bool is_main_frame;
  };

  WindowErrorJavaScriptFeature(
      base::RepeatingCallback<void(ErrorDetails)> callback);
  ~WindowErrorJavaScriptFeature() override;

  WindowErrorJavaScriptFeature(const WindowErrorJavaScriptFeature&) = delete;
  WindowErrorJavaScriptFeature& operator=(const WindowErrorJavaScriptFeature&) =
      delete;

 private:
  // JavaScriptFeature:
  base::Optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(BrowserState* browser_state,
                             WKScriptMessage* message) override;

  base::RepeatingCallback<void(ErrorDetails)> callback_;
};

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_WINDOW_ERROR_WINDOW_ERROR_JAVA_SCRIPT_FEATURE_H_
