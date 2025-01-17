// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_user_script_loader.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/common/host_id.h"
#include "extensions/test/test_content_script_load_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::URLPatternSet;

namespace {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}
}

namespace extensions {

// Test bringing up a script loader on a specific directory, putting a script
// in there, etc.
class ExtensionUserScriptLoaderTest : public testing::Test {
 public:
  ExtensionUserScriptLoaderTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  // Directory containing user scripts.
  base::ScopedTempDir temp_dir_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUserScriptLoaderTest);
};

// Test that we get notified even when there are no scripts.
TEST_F(ExtensionUserScriptLoaderTest, NoScripts) {
  TestingProfile profile;
  ExtensionUserScriptLoader loader(&profile, ExtensionId(),
                                   /*listen_for_extension_system_loaded=*/true,
                                   /*content_verifier=*/nullptr);
  ContentScriptLoadWaiter waiter(&loader);
  loader.StartLoad();
  waiter.Wait();
  content::RunAllTasksUntilIdle();
}

TEST_F(ExtensionUserScriptLoaderTest, Parse1) {
  const std::string text(
    "// This is my awesome script\n"
    "// It does stuff.\n"
    "// ==UserScript==   trailing garbage\n"
    "// @name foobar script\n"
    "// @namespace http://www.google.com/\n"
    "// @include *mail.google.com*\n"
    "// \n"
    "// @othergarbage\n"
    "// @include *mail.yahoo.com*\r\n"
    "// @include  \t *mail.msn.com*\n" // extra spaces after "@include" OK
    "//@include not-recognized\n" // must have one space after "//"
    "// ==/UserScript==  trailing garbage\n"
    "\n"
    "\n"
    "alert('hoo!');\n");

  UserScript script;
  EXPECT_TRUE(ExtensionUserScriptLoader::ParseMetadataHeader(text, &script));
  ASSERT_EQ(3U, script.globs().size());
  EXPECT_EQ("*mail.google.com*", script.globs()[0]);
  EXPECT_EQ("*mail.yahoo.com*", script.globs()[1]);
  EXPECT_EQ("*mail.msn.com*", script.globs()[2]);
}

TEST_F(ExtensionUserScriptLoaderTest, Parse2) {
  const std::string text("default to @include *");

  UserScript script;
  EXPECT_TRUE(ExtensionUserScriptLoader::ParseMetadataHeader(text, &script));
  ASSERT_EQ(1U, script.globs().size());
  EXPECT_EQ("*", script.globs()[0]);
}

TEST_F(ExtensionUserScriptLoaderTest, Parse3) {
  const std::string text(
    "// ==UserScript==\n"
    "// @include *foo*\n"
    "// ==/UserScript=="); // no trailing newline

  UserScript script;
  ExtensionUserScriptLoader::ParseMetadataHeader(text, &script);
  ASSERT_EQ(1U, script.globs().size());
  EXPECT_EQ("*foo*", script.globs()[0]);
}

TEST_F(ExtensionUserScriptLoaderTest, Parse4) {
  const std::string text(
    "// ==UserScript==\n"
    "// @match http://*.mail.google.com/*\n"
    "// @match  \t http://mail.yahoo.com/*\n"
    "// ==/UserScript==\n");

  URLPatternSet expected_patterns;
  AddPattern(&expected_patterns, "http://*.mail.google.com/*");
  AddPattern(&expected_patterns, "http://mail.yahoo.com/*");

  UserScript script;
  EXPECT_TRUE(ExtensionUserScriptLoader::ParseMetadataHeader(text, &script));
  EXPECT_EQ(0U, script.globs().size());
  EXPECT_EQ(expected_patterns, script.url_patterns());
}

TEST_F(ExtensionUserScriptLoaderTest, Parse5) {
  const std::string text(
    "// ==UserScript==\n"
    "// @match http://*mail.google.com/*\n"
    "// ==/UserScript==\n");

  // Invalid @match value.
  UserScript script;
  EXPECT_FALSE(ExtensionUserScriptLoader::ParseMetadataHeader(text, &script));
}

TEST_F(ExtensionUserScriptLoaderTest, Parse6) {
  const std::string text(
    "// ==UserScript==\n"
    "// @include http://*.mail.google.com/*\n"
    "// @match  \t http://mail.yahoo.com/*\n"
    "// ==/UserScript==\n");

  // Allowed to match @include and @match.
  UserScript script;
  EXPECT_TRUE(ExtensionUserScriptLoader::ParseMetadataHeader(text, &script));
}

TEST_F(ExtensionUserScriptLoaderTest, Parse7) {
  // Greasemonkey allows there to be any leading text before the comment marker.
  const std::string text(
    "// ==UserScript==\n"
    "adsasdfasf// @name hello\n"
    "  // @description\twiggity woo\n"
    "\t// @match  \t http://mail.yahoo.com/*\n"
    "// ==/UserScript==\n");

  UserScript script;
  EXPECT_TRUE(ExtensionUserScriptLoader::ParseMetadataHeader(text, &script));
  ASSERT_EQ("hello", script.name());
  ASSERT_EQ("wiggity woo", script.description());
  ASSERT_EQ(1U, script.url_patterns().patterns().size());
  EXPECT_EQ("http://mail.yahoo.com/*",
            script.url_patterns().begin()->GetAsString());
}

TEST_F(ExtensionUserScriptLoaderTest, Parse8) {
  const std::string text(
    "// ==UserScript==\n"
    "// @name myscript\n"
    "// @match http://www.google.com/*\n"
    "// @exclude_match http://www.google.com/foo*\n"
    "// ==/UserScript==\n");

  UserScript script;
  EXPECT_TRUE(ExtensionUserScriptLoader::ParseMetadataHeader(text, &script));
  ASSERT_EQ("myscript", script.name());
  ASSERT_EQ(1U, script.url_patterns().patterns().size());
  EXPECT_EQ("http://www.google.com/*",
            script.url_patterns().begin()->GetAsString());
  ASSERT_EQ(1U, script.exclude_url_patterns().patterns().size());
  EXPECT_EQ("http://www.google.com/foo*",
            script.exclude_url_patterns().begin()->GetAsString());
}

TEST_F(ExtensionUserScriptLoaderTest, SkipBOMAtTheBeginning) {
  base::FilePath path = temp_dir_.GetPath().AppendASCII("script.user.js");
  const std::string content("\xEF\xBB\xBF alert('hello');");
  size_t written = base::WriteFile(path, content.c_str(), content.size());
  ASSERT_EQ(written, content.size());

  std::unique_ptr<UserScript> user_script(new UserScript());
  user_script->js_scripts().push_back(std::make_unique<UserScript::File>(
      temp_dir_.GetPath(), path.BaseName(), GURL()));

  auto user_scripts = std::make_unique<UserScriptList>();
  user_scripts->push_back(std::move(user_script));

  TestingProfile profile;
  ExtensionUserScriptLoader loader(&profile, ExtensionId(),
                                   /*listen_for_extension_system_loaded=*/true,
                                   /*content_verifier=*/nullptr);
  user_scripts = loader.LoadScriptsForTest(std::move(user_scripts));

  EXPECT_EQ(content.substr(3),
            (*user_scripts)[0]->js_scripts()[0]->GetContent().as_string());
}

TEST_F(ExtensionUserScriptLoaderTest, LeaveBOMNotAtTheBeginning) {
  base::FilePath path = temp_dir_.GetPath().AppendASCII("script.user.js");
  const std::string content("alert('here's a BOOM: \xEF\xBB\xBF');");
  size_t written = base::WriteFile(path, content.c_str(), content.size());
  ASSERT_EQ(written, content.size());

  std::unique_ptr<UserScript> user_script(new UserScript());
  user_script->js_scripts().push_back(std::make_unique<UserScript::File>(
      temp_dir_.GetPath(), path.BaseName(), GURL()));

  auto user_scripts = std::make_unique<UserScriptList>();
  user_scripts->push_back(std::move(user_script));

  TestingProfile profile;
  ExtensionUserScriptLoader loader(&profile, ExtensionId(),
                                   /*listen_for_extension_system_loaded=*/true,
                                   /*content_verifier=*/nullptr);
  user_scripts = loader.LoadScriptsForTest(std::move(user_scripts));

  EXPECT_EQ(content,
            (*user_scripts)[0]->js_scripts()[0]->GetContent().as_string());
}

TEST_F(ExtensionUserScriptLoaderTest, ComponentExtensionContentScriptIsLoaded) {
  base::FilePath resources_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_RESOURCES, &resources_dir));

  const base::FilePath extension_path = resources_dir.AppendASCII("pdf");
  const base::FilePath resource_path(FILE_PATH_LITERAL("main.js"));

  auto user_script = std::make_unique<UserScript>();
  user_script->js_scripts().push_back(std::make_unique<UserScript::File>(
      extension_path, resource_path, GURL()));

  auto user_scripts = std::make_unique<UserScriptList>();
  user_scripts->push_back(std::move(user_script));

  TestingProfile profile;
  ExtensionUserScriptLoader loader(&profile, ExtensionId(),
                                   /*listen_for_extension_system_loaded=*/true,
                                   /*content_verifier=*/nullptr);
  user_scripts = loader.LoadScriptsForTest(std::move(user_scripts));

  EXPECT_FALSE((*user_scripts)[0]->js_scripts()[0]->GetContent().empty());
}

}  // namespace extensions
