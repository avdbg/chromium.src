// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_constants.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router_factory.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_util.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

namespace {

std::unique_ptr<base::ListValue> RunTabGroupsQueryFunction(
    Browser* browser,
    const Extension* extension,
    const std::string& query_info) {
  auto function = base::MakeRefCounted<TabGroupsQueryFunction>();
  function->set_extension(extension);
  std::unique_ptr<base::Value> value(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), query_info, browser, api_test_utils::NONE));
  return base::ListValue::From(std::move(value));
}

std::unique_ptr<base::DictionaryValue> RunTabGroupsGetFunction(
    Browser* browser,
    const Extension* extension,
    const std::string& args) {
  auto function = base::MakeRefCounted<TabGroupsGetFunction>();
  function->set_extension(extension);
  std::unique_ptr<base::Value> value(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), args, browser, api_test_utils::NONE));
  return base::DictionaryValue::From(std::move(value));
}

// Creates an extension with "tabGroups" permission.
scoped_refptr<const Extension> CreateTabGroupsExtension() {
  return ExtensionBuilder("Extension with tabGroups permission")
      .AddPermission("tabGroups")
      .Build();
}

std::unique_ptr<KeyedService> BuildTabGroupsEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<TabGroupsEventRouter>(context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<extensions::EventRouter>(
      context, ExtensionPrefs::Get(context));
}

}  // namespace

class TabGroupsApiUnitTest : public ExtensionServiceTestBase {
 public:
  TabGroupsApiUnitTest() = default;
  TabGroupsApiUnitTest(const TabGroupsApiUnitTest&) = delete;
  TabGroupsApiUnitTest& operator=(const TabGroupsApiUnitTest&) = delete;
  ~TabGroupsApiUnitTest() override = default;

 protected:
  Browser* browser() { return browser_.get(); }
  TestBrowserWindow* browser_window() { return browser_window_.get(); }

  content::WebContents* web_contents(int index) {
    return web_contentses_[index];
  }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  // The original web contentses in order.
  std::vector<content::WebContents*> web_contentses_;
};

void TabGroupsApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  // Create a browser window.
  TestBrowserWindow* window = new TestBrowserWindow;
  // TestBrowserWindowOwner handles its own lifetime, and also cleans up
  // |window2|.
  new TestBrowserWindowOwner(window);
  Browser::CreateParams params(profile(), /* user_gesture */ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window;
  browser_ = std::unique_ptr<Browser>(Browser::Create(params));
  BrowserList::SetLastActive(browser_.get());

  // Add several tabs to the browser and get their tab IDs and web contents.
  constexpr int kNumTabs = 6;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    CreateSessionServiceTabHelper(contents.get());
    web_contentses_.push_back(contents.get());
    browser_->tab_strip_model()->AppendWebContents(std::move(contents),
                                                   /* foreground */ true);
  }

  TabGroupsEventRouterFactory::GetInstance()->SetTestingFactory(
      browser_context(), base::BindRepeating(&BuildTabGroupsEventRouter));

  EventRouterFactory::GetInstance()->SetTestingFactory(
      browser_context(), base::BindRepeating(&BuildEventRouter));

  // We need to call TabGroupsEventRouterFactory::Get() in order to instantiate
  // the keyed service, since it's not created by default in unit tests.
  TabGroupsEventRouterFactory::Get(browser_context());
}

void TabGroupsApiUnitTest::TearDown() {
  browser_->tab_strip_model()->CloseAllTabs();
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test that querying groups by title returns the correct groups.
TEST_F(TabGroupsApiUnitTest, TabGroupsQueryTitle) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Create 3 groups with different titles.
  const tab_groups::TabGroupColorId color = tab_groups::TabGroupColorId::kGrey;

  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});
  tab_groups::TabGroupVisualData visual_data1(
      base::ASCIIToUTF16("Sample title"), color);
  tab_group_model->GetTabGroup(group1)->SetVisualData(visual_data1);

  tab_groups::TabGroupId group2 = tab_strip_model->AddToNewGroup({1});
  tab_groups::TabGroupVisualData visual_data2(
      base::ASCIIToUTF16("Sample title suffixed"), color);
  tab_group_model->GetTabGroup(group2)->SetVisualData(visual_data2);

  tab_groups::TabGroupId group3 = tab_strip_model->AddToNewGroup({2});
  tab_groups::TabGroupVisualData visual_data3(
      base::ASCIIToUTF16("Prefixed Sample title"), color);
  tab_group_model->GetTabGroup(group3)->SetVisualData(visual_data3);

  // Query by title and verify results.
  const char* kTitleQueryInfo = R"([{"title": "Sample title"}])";
  std::unique_ptr<base::ListValue> groups_list(
      RunTabGroupsQueryFunction(browser(), extension.get(), kTitleQueryInfo));
  ASSERT_TRUE(groups_list);
  ASSERT_EQ(1u, groups_list->GetSize());

  const base::Value& group_info = groups_list->GetList()[0];
  ASSERT_EQ(base::Value::Type::DICTIONARY, group_info.type());
  EXPECT_EQ(
      tab_groups_util::GetGroupId(group1),
      group_info.FindKeyOfType("id", base::Value::Type::INTEGER)->GetInt());
}

// Test that querying groups by color returns the correct groups.
TEST_F(TabGroupsApiUnitTest, TabGroupsQueryColor) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Create 3 groups with different colors.
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});
  tab_groups::TabGroupVisualData visual_data1(
      base::string16(), tab_groups::TabGroupColorId::kGrey);
  tab_group_model->GetTabGroup(group1)->SetVisualData(visual_data1);

  tab_groups::TabGroupId group2 = tab_strip_model->AddToNewGroup({1});
  tab_groups::TabGroupVisualData visual_data2(
      base::string16(), tab_groups::TabGroupColorId::kRed);
  tab_group_model->GetTabGroup(group2)->SetVisualData(visual_data2);

  tab_groups::TabGroupId group3 = tab_strip_model->AddToNewGroup({2});
  tab_groups::TabGroupVisualData visual_data3(
      base::string16(), tab_groups::TabGroupColorId::kBlue);
  tab_group_model->GetTabGroup(group3)->SetVisualData(visual_data3);

  // Query by color and verify results.
  const char* kColorQueryInfo = R"([{"color": "blue"}])";
  std::unique_ptr<base::ListValue> groups_list(
      RunTabGroupsQueryFunction(browser(), extension.get(), kColorQueryInfo));
  ASSERT_TRUE(groups_list);
  ASSERT_EQ(1u, groups_list->GetSize());

  const base::Value& group_info = groups_list->GetList()[0];
  ASSERT_EQ(base::Value::Type::DICTIONARY, group_info.type());
  EXPECT_EQ(
      tab_groups_util::GetGroupId(group3),
      group_info.FindKeyOfType("id", base::Value::Type::INTEGER)->GetInt());
}

// Test that getting a group returns the correct metadata.
TEST_F(TabGroupsApiUnitTest, TabGroupsGetSuccess) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Create a group.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({0, 1, 2});
  tab_groups::TabGroupVisualData visual_data(
      base::ASCIIToUTF16("Title"), tab_groups::TabGroupColorId::kBlue);
  tab_group_model->GetTabGroup(group)->SetVisualData(visual_data);
  int group_id = tab_groups_util::GetGroupId(group);

  // Use the TabGroupsGetFunction to get the group object.
  constexpr char kFormatArgs[] = R"([%d])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  std::unique_ptr<base::DictionaryValue> group_info(
      RunTabGroupsGetFunction(browser(), extension.get(), args));

  EXPECT_EQ(
      group_id,
      group_info->FindKeyOfType("id", base::Value::Type::INTEGER)->GetInt());

  EXPECT_EQ("Title",
            group_info->FindKeyOfType("title", base::Value::Type::STRING)
                ->GetString());
}

// Test that tabGroups.get() fails on a nonexistent group.
TEST_F(TabGroupsApiUnitTest, TabGroupsGetError) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Try to get a non-existent group and expect an error.
  auto function = base::MakeRefCounted<TabGroupsGetFunction>();
  function->set_extension(extension);
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), "[0]", browser(), api_test_utils::NONE);
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(
                tab_groups_constants::kGroupNotFoundError, "0"),
            error);
}

// Test that updating group metadata works as expected.
TEST_F(TabGroupsApiUnitTest, TabGroupsUpdateSuccess) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Create a group.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({0, 1, 2});
  tab_groups::TabGroupVisualData visual_data(
      base::ASCIIToUTF16("Initial title"), tab_groups::TabGroupColorId::kBlue);
  tab_group_model->GetTabGroup(group)->SetVisualData(visual_data);
  int group_id = tab_groups_util::GetGroupId(group);

  // Use the TabGroupsUpdateFunction to update the title and color.
  auto function = base::MakeRefCounted<TabGroupsUpdateFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] =
      R"([%d, {"title": "New title", "color": "red"}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), args, browser(), api_test_utils::NONE));

  // Verify the new group metadata.
  const tab_groups::TabGroupVisualData* new_visual_data =
      tab_group_model->GetTabGroup(group)->visual_data();
  EXPECT_EQ(new_visual_data->title(), base::ASCIIToUTF16("New title"));
  EXPECT_EQ(new_visual_data->color(), tab_groups::TabGroupColorId::kRed);
}

// Test that tabGroups.update() fails on a nonexistent group.
TEST_F(TabGroupsApiUnitTest, TabGroupsUpdateError) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Try to update a non-existent group and expect an error.
  auto function = base::MakeRefCounted<TabGroupsUpdateFunction>();
  function->set_extension(extension);
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), "[0, {}]", browser(), api_test_utils::NONE);
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(
                tab_groups_constants::kGroupNotFoundError, "0"),
            error);
}

// Test that moving a group to the right results in the correct tab order.
TEST_F(TabGroupsApiUnitTest, TabGroupsMoveRight) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Create a group with multiple tabs.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({1, 2, 3});
  int group_id = tab_groups_util::GetGroupId(group);

  // Use the TabGroupsMoveFunction to move the group to index 2.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 2}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), args, browser(), api_test_utils::NONE));

  EXPECT_EQ(tab_strip_model->GetWebContentsAt(0), web_contents(0));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(1), web_contents(4));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(2), web_contents(1));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(3), web_contents(2));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(4), web_contents(3));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(5), web_contents(5));

  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(2).value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(3).value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(4).value());
}

// Test that moving a group to the left results in the correct tab order.
TEST_F(TabGroupsApiUnitTest, TabGroupsMoveLeft) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Create a group with multiple tabs.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({2, 3, 4});
  int group_id = tab_groups_util::GetGroupId(group);

  // Use the TabGroupsMoveFunction to move the group to index 0.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 0}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), args, browser(), api_test_utils::NONE));

  EXPECT_EQ(tab_strip_model->GetWebContentsAt(0), web_contents(2));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(1), web_contents(3));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(2), web_contents(4));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(3), web_contents(0));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(4), web_contents(1));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(5), web_contents(5));

  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(0).value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(1).value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(2).value());
}

// Test that moving a group to another window works as expected.
TEST_F(TabGroupsApiUnitTest, TabGroupsMoveAcrossWindows) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Create a group with multiple tabs.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({2, 3, 4});
  int group_id = tab_groups_util::GetGroupId(group);

  // Create a new window and add a few tabs.
  TestBrowserWindow* window2 = new TestBrowserWindow;
  // TestBrowserWindowOwner handles its own lifetime, and also cleans up
  // |window2|.
  new TestBrowserWindowOwner(window2);
  Browser::CreateParams params(profile(), /* user_gesture */ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window2;
  std::unique_ptr<Browser> browser2(Browser::Create(params));
  BrowserList::SetLastActive(browser2.get());
  int window_id2 = ExtensionTabUtil::GetWindowId(browser2.get());

  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  constexpr int kNumTabs2 = 3;
  for (int i = 0; i < kNumTabs2; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    CreateSessionServiceTabHelper(contents.get());
    tab_strip_model2->AppendWebContents(std::move(contents),
                                        /* foreground */ true);
  }
  ASSERT_EQ(kNumTabs2, tab_strip_model2->count());

  // Use the TabGroupsMoveFunction to move the group to index 1 in the other
  // window.
  constexpr int kNumTabsMovedAcrossWindows = 3;
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"windowId": %d, "index": 1}])";
  const std::string args =
      base::StringPrintf(kFormatArgs, group_id, window_id2);
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), args, browser(), api_test_utils::NONE));

  ASSERT_EQ(kNumTabs2 + kNumTabsMovedAcrossWindows, tab_strip_model2->count());
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(1), web_contents(2));
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(2), web_contents(3));
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(3), web_contents(4));

  EXPECT_EQ(group, tab_strip_model2->GetTabGroupForTab(1).value());
  EXPECT_EQ(group, tab_strip_model2->GetTabGroupForTab(2).value());
  EXPECT_EQ(group, tab_strip_model2->GetTabGroupForTab(3).value());

  // Clean up.
  tab_strip_model2->CloseAllTabs();
}

// Test that a group is cannot be moved into the pinned tabs region.
TEST_F(TabGroupsApiUnitTest, TabGroupsMoveToPinnedError) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Pin the first 3 tabs.
  tab_strip_model->SetTabPinned(0, /* pinned */ true);
  tab_strip_model->SetTabPinned(1, /* pinned */ true);
  tab_strip_model->SetTabPinned(2, /* pinned */ true);

  // Create a group with an unpinned tab.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({4});
  int group_id = tab_groups_util::GetGroupId(group);

  // Try to move the group to index 1 and expect an error.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 1}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), args, browser(), api_test_utils::NONE);
  EXPECT_EQ(tab_groups_constants::kCannotMoveGroupIntoMiddleOfPinnedTabsError,
            error);
}

// Test that a group cannot be moved into the middle of another group.
TEST_F(TabGroupsApiUnitTest, TabGroupsMoveToOtherGroupError) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Create two tab groups, one with multiple tabs and the other to move.
  tab_strip_model->AddToNewGroup({0, 1, 2});
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({4});
  int group_id = tab_groups_util::GetGroupId(group);

  // Try to move the second group to index 1 and expect an error.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 1}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), args, browser(), api_test_utils::NONE);
  EXPECT_EQ(tab_groups_constants::kCannotMoveGroupIntoMiddleOfOtherGroupError,
            error);
}

TEST_F(TabGroupsApiUnitTest, TabGroupsOnCreated) {
  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  browser()->tab_strip_model()->AddToNewGroup({1, 2, 3});

  EXPECT_EQ(2u, event_observer.events().size());
  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnCreated::kEventName));
  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnUpdated::kEventName));
}

TEST_F(TabGroupsApiUnitTest, TabGroupsOnUpdated) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({1, 2, 3});

  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  tab_groups::TabGroupVisualData visual_data(base::ASCIIToUTF16("Title"),
                                             tab_groups::TabGroupColorId::kRed);
  tab_strip_model->group_model()->GetTabGroup(group)->SetVisualData(
      visual_data);

  EXPECT_EQ(1u, event_observer.events().size());
  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnUpdated::kEventName));
}

TEST_F(TabGroupsApiUnitTest, TabGroupsOnRemoved) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->AddToNewGroup({1, 2, 3});

  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  tab_strip_model->RemoveFromGroup({1, 2, 3});

  EXPECT_EQ(1u, event_observer.events().size());
  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnRemoved::kEventName));
}

TEST_F(TabGroupsApiUnitTest, TabGroupsOnMoved) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({1, 2, 3});

  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  tab_strip_model->MoveGroupTo(group, 0);

  EXPECT_EQ(1u, event_observer.events().size());
  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnMoved::kEventName));
}

}  // namespace extensions
