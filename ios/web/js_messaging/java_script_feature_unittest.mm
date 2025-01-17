// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/java_script_feature.h"

#import <WebKit/WebKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/web/js_messaging/page_script_util.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef PlatformTest JavaScriptFeatureTest;

// Tests the creation of FeatureScripts.
TEST_F(JavaScriptFeatureTest, CreateFeatureScript) {
  auto document_start_injection_time =
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart;
  auto target_frames_all =
      web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames;
  auto feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "base_js", document_start_injection_time, target_frames_all);

  EXPECT_EQ(document_start_injection_time, feature_script.GetInjectionTime());
  EXPECT_EQ(target_frames_all, feature_script.GetTargetFrames());
  EXPECT_TRUE([feature_script.GetScriptString() containsString:@"__gCrWeb"]);

  auto document_end_injection_time =
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd;
  auto target_frames_main =
      web::JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame;
  auto feature_script2 =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "common_js", document_end_injection_time, target_frames_main);

  EXPECT_EQ(document_end_injection_time, feature_script2.GetInjectionTime());
  EXPECT_EQ(target_frames_main, feature_script2.GetTargetFrames());
  EXPECT_TRUE(
      [feature_script2.GetScriptString() containsString:@"__gCrWeb.common"]);
}

// Tests the creation of FeatureScripts with different reinjection behaviors.
TEST_F(JavaScriptFeatureTest, FeatureScriptReinjectionBehavior) {
  auto once_feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "base_js",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
          web::JavaScriptFeature::FeatureScript::ReinjectionBehavior::
              kInjectOncePerWindow);

  auto reinject_feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "base_js",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
          web::JavaScriptFeature::FeatureScript::ReinjectionBehavior::
              kReinjectOnDocumentRecreation);

  EXPECT_NSNE(once_feature_script.GetScriptString(),
              reinject_feature_script.GetScriptString());
}

// Tests that FeatureScripts are only injected once when created with
// |ReinjectionBehavior::kInjectOncePerWindow|.
TEST_F(JavaScriptFeatureTest, ReinjectionBehaviorOnce) {
  auto feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "base_js",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
          web::JavaScriptFeature::FeatureScript::ReinjectionBehavior::
              kInjectOncePerWindow);

  WKWebView* web_view = [[WKWebView alloc] init];
  web::test::ExecuteJavaScript(web_view, feature_script.GetScriptString());

  // Ensure __gCrWeb was injected.
  ASSERT_TRUE(web::test::ExecuteJavaScript(
      web_view, @"try { !!window.__gCrWeb; } catch (err) {false;}"));

  // Store a value within |window.__gCrWeb|.
  web::test::ExecuteJavaScript(web_view, @"window.__gCrWeb.someData = 1;");
  ASSERT_NSEQ(@(1), web::test::ExecuteJavaScript(web_view,
                                                 @"window.__gCrWeb.someData"));

  // Execute feature script again, which should not overwrite window state.
  web::test::ExecuteJavaScript(web_view, feature_script.GetScriptString());
  // The |someData| value should still exist.
  EXPECT_NSEQ(@(1), web::test::ExecuteJavaScript(web_view,
                                                 @"window.__gCrWeb.someData"));
}

// Tests that FeatureScripts are re-injected when created with
// |ReinjectionBehavior::kReinjectOnDocumentRecreation|.
TEST_F(JavaScriptFeatureTest, ReinjectionBehaviorReinject) {
  auto feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "base_js",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
          web::JavaScriptFeature::FeatureScript::ReinjectionBehavior::
              kReinjectOnDocumentRecreation);

  WKWebView* web_view = [[WKWebView alloc] init];
  web::test::ExecuteJavaScript(web_view, feature_script.GetScriptString());

  // Ensure __gCrWeb was injected.
  ASSERT_TRUE(web::test::ExecuteJavaScript(
      web_view, @"try { !!window.__gCrWeb; } catch (err) {false;}"));

  // Store a value within |window.__gCrWeb|.
  web::test::ExecuteJavaScript(web_view, @"window.__gCrWeb.someData = 1;");
  ASSERT_NSEQ(@(1), web::test::ExecuteJavaScript(web_view,
                                                 @"window.__gCrWeb.someData"));

  // Execute feature script again, which should overwrite |window.__gCrWeb|.
  web::test::ExecuteJavaScript(web_view, feature_script.GetScriptString());
  // The |someData| value should no longer exist.
  EXPECT_FALSE(web::test::ExecuteJavaScript(
      web_view, @"try { window.__gCrWeb.someData; } catch (err) {false;}"));
}

// Tests creating a JavaScriptFeature.
TEST_F(JavaScriptFeatureTest, CreateFeature) {
  auto document_start_injection_time =
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart;
  auto target_frames_all =
      web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames;
  const web::JavaScriptFeature::FeatureScript feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "base_js", document_start_injection_time, target_frames_all);

  auto any_content_world =
      web::JavaScriptFeature::ContentWorld::kAnyContentWorld;
  web::JavaScriptFeature feature(any_content_world, {feature_script});

  EXPECT_EQ(any_content_world, feature.GetSupportedContentWorld());
  EXPECT_EQ(0ul, feature.GetDependentFeatures().size());

  auto feature_scripts = feature.GetScripts();
  ASSERT_EQ(1ul, feature_scripts.size());
  EXPECT_NSEQ(feature_script.GetScriptString(),
              feature_scripts[0].GetScriptString());
}

// Tests creating a JavaScriptFeature with replacements map.
TEST_F(JavaScriptFeatureTest, CreateFeatureWithPlaceholder) {
  auto document_end_injection_time =
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd;
  auto target_frames_all =
      web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames;
  std::map<std::string, NSString*> replacements{
      {"$(PLUGIN_NOT_SUPPORTED_TEXT)", @"TEST_PLACEHOLDER_VALUE"}};

  const web::JavaScriptFeature::FeatureScript feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "plugin_placeholder_js", document_end_injection_time,
          target_frames_all,
          web::JavaScriptFeature::FeatureScript::ReinjectionBehavior::
              kReinjectOnDocumentRecreation,
          replacements);

  auto any_content_world =
      web::JavaScriptFeature::ContentWorld::kAnyContentWorld;
  web::JavaScriptFeature feature(any_content_world, {feature_script});

  EXPECT_EQ(any_content_world, feature.GetSupportedContentWorld());
  EXPECT_EQ(0ul, feature.GetDependentFeatures().size());
  auto feature_scripts = feature.GetScripts();
  ASSERT_EQ(1ul, feature_scripts.size());
  NSString* original_script = web::GetPageScript(@"plugin_placeholder_js");
  NSString* final_script = feature_scripts[0].GetScriptString();
  NSString* placeholder = base::SysUTF8ToNSString(replacements.begin()->first);
  NSString* replacement = replacements.begin()->second;

  EXPECT_NSEQ(feature_script.GetScriptString(), final_script);
  NSRange placeholder_range = [original_script rangeOfString:placeholder
                                                     options:NSLiteralSearch];
  EXPECT_TRUE(placeholder_range.location != NSNotFound);
  EXPECT_FALSE([final_script containsString:placeholder]);
  NSRange replacement_range = [final_script rangeOfString:replacement
                                                  options:NSLiteralSearch];
  EXPECT_EQ(placeholder_range.location, replacement_range.location);
}

// Tests creating a JavaScriptFeature which relies on a dependent feature.
TEST_F(JavaScriptFeatureTest, CreateFeatureWithDependentFeature) {
  auto document_start_injection_time =
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart;
  auto target_frames_all =
      web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames;
  const web::JavaScriptFeature::FeatureScript dependent_feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "base_js", document_start_injection_time, target_frames_all);

  auto document_end_injection_time =
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd;
  auto target_frames_main =
      web::JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame;
  const web::JavaScriptFeature::FeatureScript feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "common_js", document_end_injection_time, target_frames_main);

  auto page_content_world =
      web::JavaScriptFeature::ContentWorld::kPageContentWorld;
  web::JavaScriptFeature dependent_feature(page_content_world,
                                           {dependent_feature_script});
  web::JavaScriptFeature feature(page_content_world, {feature_script},
                                 {&dependent_feature});
  EXPECT_EQ(page_content_world, feature.GetSupportedContentWorld());

  auto feature_scripts = feature.GetScripts();
  ASSERT_EQ(1ul, feature_scripts.size());
  auto dependent_features = feature.GetDependentFeatures();
  ASSERT_EQ(1ul, dependent_features.size());
  auto dependent_feature_scripts = dependent_features[0]->GetScripts();
  ASSERT_EQ(1ul, dependent_feature_scripts.size());
  EXPECT_NSEQ(feature_script.GetScriptString(),
              feature_scripts[0].GetScriptString());
  EXPECT_NSEQ(dependent_feature_script.GetScriptString(),
              dependent_feature_scripts[0].GetScriptString());
}
