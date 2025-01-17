// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace {
ui::MouseEvent GetDummyEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(),
                        base::TimeTicks::Now(), 0, 0);
}
}  // namespace

// Integration tests for interactions between TabStripModel and TabStrip.
class TabStripBrowsertest : public InProcessBrowserTest {
 public:
  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  TabStrip* tab_strip() {
    return BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  }

  void AppendTab() { chrome::AddTabAt(browser(), GURL(), -1, true); }

  tab_groups::TabGroupId AddTabToNewGroup(int tab_index) {
    tab_strip_model()->AddToNewGroup({tab_index});
    return tab_strip_model()->GetTabGroupForTab(tab_index).value();
  }

  void AddTabToExistingGroup(int tab_index, tab_groups::TabGroupId group) {
    tab_strip_model()->AddToExistingGroup({tab_index}, group);
  }

  std::vector<content::WebContents*> GetWebContentses() {
    std::vector<content::WebContents*> contentses;
    for (int i = 0; i < tab_strip()->GetTabCount(); ++i)
      contentses.push_back(tab_strip_model()->GetWebContentsAt(i));
    return contentses;
  }

  std::vector<content::WebContents*> GetWebContentsesInOrder(
      const std::vector<int>& order) {
    std::vector<content::WebContents*> contentses;
    for (int i = 0; i < tab_strip()->GetTabCount(); ++i)
      contentses.push_back(tab_strip_model()->GetWebContentsAt(order[i]));
    return contentses;
  }
};

// Regression test for crbug.com/983961.
IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabAndDeleteGroup) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToNewGroup(2);

  Tab* tab0 = tab_strip()->tab_at(0);
  Tab* tab1 = tab_strip()->tab_at(1);
  Tab* tab2 = tab_strip()->tab_at(2);

  tab_strip_model()->AddToExistingGroup({2}, group);

  EXPECT_EQ(tab0, tab_strip()->tab_at(0));
  EXPECT_EQ(tab2, tab_strip()->tab_at(1));
  EXPECT_EQ(tab1, tab_strip()->tab_at(2));

  EXPECT_EQ(group, tab_strip_model()->GetTabGroupForTab(1));

  std::vector<tab_groups::TabGroupId> groups =
      tab_strip_model()->group_model()->ListTabGroups();
  EXPECT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0], group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabPrevious_Success) {
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({1, 0, 2});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabPrevious_AddsToGroup) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);

  // Instead of moving, the tab should be added to the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(2)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabPrevious_PastCollapsedGroup_Success) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));

  const auto expected = GetWebContentsesInOrder({2, 0, 1});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), base::nullopt);
  EXPECT_EQ(tab_strip()->tab_at(1)->group().value(), group);
  EXPECT_EQ(tab_strip()->tab_at(2)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabPrevious_BetweenTwoCollapsedGroups_Success) {
  AppendTab();
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group1);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group1));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group1);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group1));

  tab_groups::TabGroupId group2 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group2);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group2));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group2);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group2));

  const auto expected = GetWebContentsesInOrder({0, 1, 4, 2, 3});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(4));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group1));
  EXPECT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group2));
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group1);
  EXPECT_EQ(tab_strip()->tab_at(1)->group().value(), group1);
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), base::nullopt);
  EXPECT_EQ(tab_strip()->tab_at(3)->group().value(), group2);
  EXPECT_EQ(tab_strip()->tab_at(4)->group().value(), group2);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabPrevious_RemovesFromGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabPrevious_ShiftsBetweenGroups) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from its old group, then added
  // to the new group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group(), base::nullopt);
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabPrevious_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(0));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabPrevious_Failure_Pinned) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(1));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabNext_Success) {
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({1, 0, 2});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabNext_AddsToGroup) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);

  // Instead of moving, the tab should be added to the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabNext_PastCollapsedGroup_Success) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));

  const auto expected = GetWebContentsesInOrder({1, 2, 0});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group);
  EXPECT_EQ(tab_strip()->tab_at(1)->group().value(), group);
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabNext_BetweenTwoCollapsedGroups_Success) {
  AppendTab();
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group1);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group1));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group1);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group1));

  tab_groups::TabGroupId group2 = AddTabToNewGroup(3);
  AddTabToExistingGroup(4, group2);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group2));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group2);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group2));

  const auto expected = GetWebContentsesInOrder({1, 2, 0, 3, 4});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group1));
  EXPECT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group2));
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group1);
  EXPECT_EQ(tab_strip()->tab_at(1)->group().value(), group1);
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), base::nullopt);
  EXPECT_EQ(tab_strip()->tab_at(3)->group().value(), group2);
  EXPECT_EQ(tab_strip()->tab_at(4)->group().value(), group2);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabNext_RemovesFromGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabNext_ShiftsBetweenGroups) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(0);
  tab_groups::TabGroupId group = AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from its old group, then added
  // to the new group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), base::nullopt);
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabNext_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(2));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabNext_Failure_Pinned) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_NoPinnedTabs_Success) {
  AppendTab();
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({2, 0, 1, 3});
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_PinnedTabs_Success) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto expected = GetWebContentsesInOrder({0, 2, 1, 3});
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_DoesNotAddToGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(0);

  tab_strip()->MoveTabFirst(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_RemovesFromGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(0);
  AddTabToNewGroup(1);

  tab_strip()->MoveTabFirst(tab_strip()->tab_at(0));
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), base::nullopt);

  tab_strip()->MoveTabFirst(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_NoPinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(0));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_PinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(1));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       MoveTabFirst_MovePinnedTab_Success) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto expected = GetWebContentsesInOrder({2, 0, 1, 3});
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_NoPinnedTabs_Success) {
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({1, 2, 0});
  tab_strip()->MoveTabLast(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_MovePinnedTab_Success) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto expected = GetWebContentsesInOrder({0, 2, 1, 3});
  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_AllPinnedTabs_Success) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto expected = GetWebContentsesInOrder({0, 2, 1});
  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_DoesNotAddToGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(2);

  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_RemovesFromGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(1);
  AddTabToNewGroup(2);

  tab_strip()->MoveTabLast(tab_strip()->tab_at(2));
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), base::nullopt);

  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_NoPinnedTabs_Failure) {
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabLast(tab_strip()->tab_at(2));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_PinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_AllPinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabLast(tab_strip()->tab_at(2));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupLeft_Success) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);

  const auto expected = GetWebContentsesInOrder({1, 2, 0});
  tab_strip()->ShiftGroupLeft(group);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupLeft_OtherGroup) {
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group1);

  tab_groups::TabGroupId group2 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group2);

  const auto expected = GetWebContentsesInOrder({2, 3, 0, 1});
  tab_strip()->ShiftGroupLeft(group1);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftGroupLeft_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupLeft(group);
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupLeft_Failure_Pinned) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupLeft(group);
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupRight_Success) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);

  const auto expected = GetWebContentsesInOrder({2, 0, 1});
  tab_strip()->ShiftGroupRight(group);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupRight_OtherGroup) {
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group1);

  tab_groups::TabGroupId group2 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group2);

  const auto expected = GetWebContentsesInOrder({2, 3, 0, 1});
  tab_strip()->ShiftGroupRight(group1);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftGroupRight_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupRight(group);
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftCollapsedGroupLeft_Success) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));

  const auto expected = GetWebContentsesInOrder({1, 2, 0});
  tab_strip()->ShiftGroupLeft(group);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftCollapsedGroupLeft_OtherCollapsedGroup) {
  AppendTab();
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group1);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group1));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group1);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group1));

  tab_groups::TabGroupId group2 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group2);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group2));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group2);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group2));

  const auto expected = GetWebContentsesInOrder({2, 3, 0, 1, 4});
  tab_strip()->ShiftGroupLeft(group1);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftCollapsedGroupLeft_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupLeft(group);

  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftCollapsedGroupLeft_Failure_Pinned) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupLeft(group);

  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftCollapsedGroupRight_Success) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));

  const auto expected = GetWebContentsesInOrder({2, 0, 1});
  tab_strip()->ShiftGroupRight(group);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftCollapsedGroupRight_OtherCollapsedGroup) {
  AppendTab();
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group1);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group1));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group1);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group1));

  tab_groups::TabGroupId group2 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group2);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group2));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group2);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group2));

  const auto expected = GetWebContentsesInOrder({2, 3, 0, 1, 4});
  tab_strip()->ShiftGroupRight(group1);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftCollapsedGroupRight_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupRight(group);
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       CollapseGroup_WithActiveTabInGroup_SelectsNext) {
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  tab_strip()->SelectTab(tab_strip()->tab_at(0), GetDummyEvent());
  ASSERT_EQ(0, tab_strip()->controller()->GetActiveIndex());
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);

  EXPECT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));
  EXPECT_EQ(1, tab_strip()->controller()->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       CollapseGroup_WithActiveTabInGroup_SelectsPrevious) {
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  tab_strip()->SelectTab(tab_strip()->tab_at(1), GetDummyEvent());
  ASSERT_EQ(1, tab_strip()->controller()->GetActiveIndex());
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);

  EXPECT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));
  EXPECT_EQ(0, tab_strip()->controller()->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(
    TabStripBrowsertest,
    CollapseGroup_WithActiveTabOutsideGroup_DoesNotChangeActiveTab) {
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  tab_strip()->SelectTab(tab_strip()->tab_at(1), GetDummyEvent());
  ASSERT_EQ(1, tab_strip()->controller()->GetActiveIndex());
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);

  EXPECT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));
  EXPECT_EQ(1, tab_strip()->controller()->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, CollapseGroup_Fails) {
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  tab_strip_model()->AddToExistingGroup({1}, group);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);

  EXPECT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ActivateTabInCollapsedGroup_ExpandsCollapsedGroup) {
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  ASSERT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
  tab_strip()->controller()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->controller()->IsGroupCollapsed(group));
  ASSERT_EQ(1, tab_strip()->controller()->GetActiveIndex());

  tab_strip()->SelectTab(tab_strip()->tab_at(0), GetDummyEvent());
  EXPECT_FALSE(tab_strip()->controller()->IsGroupCollapsed(group));
}
