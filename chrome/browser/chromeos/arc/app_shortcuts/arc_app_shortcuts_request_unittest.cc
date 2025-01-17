// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/app_shortcuts/arc_app_shortcuts_request.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/chromeos/arc/app_shortcuts/arc_app_shortcut_item.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcAppShortcutsRequestTest : public testing::Test {
 public:
  ArcAppShortcutsRequestTest() = default;
  ~ArcAppShortcutsRequestTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    arc_app_test_.SetUp(profile_.get());
    IconDecodeRequest::DisableSafeDecodingForTesting();
  }

  void TearDown() override {
    arc_app_test_.TearDown();
    profile_.reset();
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_app_test_;

  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppShortcutsRequestTest);
};

TEST_F(ArcAppShortcutsRequestTest, Basic) {
  base::RunLoop run_loop;
  std::unique_ptr<ArcAppShortcutItems> items;
  auto arc_app_shortcuts_request =
      std::make_unique<ArcAppShortcutsRequest>(base::BindLambdaForTesting(
          [&](std::unique_ptr<ArcAppShortcutItems> returned_items) {
            items = std::move(returned_items);
            run_loop.Quit();
          }));
  arc_app_shortcuts_request->StartForPackage("Test");
  run_loop.Run();

  DCHECK(items);
  for (size_t i = 0; i < items->size(); ++i) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %zu", i),
              items->at(i).short_label);
    EXPECT_EQ(base::StringPrintf("ShortcutId %zu", i),
              items->at(i).shortcut_id);
  }
}

}  // namespace arc
