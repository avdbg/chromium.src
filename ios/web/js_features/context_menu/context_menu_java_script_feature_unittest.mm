// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_java_script_feature.h"

#import <CoreGraphics/CoreGraphics.h>

#include "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

namespace web {

typedef WebTestWithWebState ContextMenuJavaScriptFeatureTest;

TEST_F(ContextMenuJavaScriptFeatureTest, FetchElement) {
  ContextMenuJavaScriptFeature* feature =
      ContextMenuJavaScriptFeature::FromBrowserState(GetBrowserState());

  JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({feature});

  NSString* html =
      @"<html><head>"
       "<style>body { font-size:14em; }</style>"
       "<meta name=\"viewport\" content=\"user-scalable=no, width=100\">"
       "</head><body><p><a id=\"linkID\" "
       "href=\"http://destination/\">link</a></p></body></html>";
  LoadHtml(html);

  std::string request_id("123");

  __block bool callback_called = false;
  feature->GetElementAtPoint(
      web_state(), request_id, CGPointMake(10.0, 10.0),
      CGSizeMake(100.0, 100.0),
      base::BindOnce(^(const std::string& callback_request_id,
                       const web::ContextMenuParams& params) {
        EXPECT_EQ(request_id, callback_request_id);
        EXPECT_EQ(true, params.is_main_frame);
        EXPECT_NSEQ(@"destination", params.menu_title);
        EXPECT_NSEQ(@"link", params.link_text);
        EXPECT_EQ("http://destination/", params.link_url.spec());
        callback_called = true;
      }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return callback_called;
  }));
}

}  // namespace web
