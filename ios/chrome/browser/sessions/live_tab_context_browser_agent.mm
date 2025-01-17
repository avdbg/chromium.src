// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/live_tab_context_browser_agent.h"

#include <memory>
#include <utility>

#include "base/notreached.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "components/sessions/core/session_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sessions/session_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(LiveTabContextBrowserAgent)

LiveTabContextBrowserAgent::LiveTabContextBrowserAgent(Browser* browser)
    : browser_state_(browser->GetBrowserState()),
      web_state_list_(browser->GetWebStateList()),
      session_id_(SessionID::NewUnique()) {}

LiveTabContextBrowserAgent::~LiveTabContextBrowserAgent() {}

void LiveTabContextBrowserAgent::ShowBrowserWindow() {
  // No need to do anything here, as the singleton browser "window" is already
  // shown.
}

SessionID LiveTabContextBrowserAgent::GetSessionID() const {
  return session_id_;
}

int LiveTabContextBrowserAgent::GetTabCount() const {
  return web_state_list_->count();
}

int LiveTabContextBrowserAgent::GetSelectedIndex() const {
  return web_state_list_->active_index();
}

std::string LiveTabContextBrowserAgent::GetAppName() const {
  return std::string();
}

std::string LiveTabContextBrowserAgent::GetUserTitle() const {
  return std::string();
}

sessions::LiveTab* LiveTabContextBrowserAgent::GetLiveTabAt(int index) const {
  return nullptr;
}

sessions::LiveTab* LiveTabContextBrowserAgent::GetActiveLiveTab() const {
  return nullptr;
}

bool LiveTabContextBrowserAgent::IsTabPinned(int index) const {
  // Not supported by iOS.
  return false;
}

base::Optional<tab_groups::TabGroupId>
LiveTabContextBrowserAgent::GetTabGroupForTab(int index) const {
  // Not supported by iOS.
  return base::nullopt;
}

const tab_groups::TabGroupVisualData*
LiveTabContextBrowserAgent::GetVisualDataForGroup(
    const tab_groups::TabGroupId& group) const {
  // Since we never return a group from GetTabGroupForTab(), this should never
  // be called.
  NOTREACHED();
  return nullptr;
}

void LiveTabContextBrowserAgent::SetVisualDataForGroup(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  // Not supported on iOS.
}

const gfx::Rect LiveTabContextBrowserAgent::GetRestoredBounds() const {
  // Not supported by iOS.
  return gfx::Rect();
}

ui::WindowShowState LiveTabContextBrowserAgent::GetRestoredState() const {
  // Not supported by iOS.
  return ui::SHOW_STATE_NORMAL;
}

std::string LiveTabContextBrowserAgent::GetWorkspace() const {
  // Not supported by iOS.
  return std::string();
}

sessions::LiveTab* LiveTabContextBrowserAgent::AddRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    base::Optional<tab_groups::TabGroupId> group,
    const tab_groups::TabGroupVisualData& group_visual_data,
    bool select,
    bool pin,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const SessionID* tab_id) {
  // TODO(crbug.com/661636): Handle tab-switch animation somehow...
  web_state_list_->InsertWebState(
      tab_index,
      session_util::CreateWebStateWithNavigationEntries(
          browser_state_, selected_navigation, navigations),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  return nullptr;
}

sessions::LiveTab* LiveTabContextBrowserAgent::ReplaceRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    base::Optional<tab_groups::TabGroupId> group,
    int selected_navigation,
    const std::string& extension_app_id,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const sessions::SerializedUserAgentOverride& user_agent_override) {
  web_state_list_->ReplaceWebStateAt(
      web_state_list_->active_index(),
      session_util::CreateWebStateWithNavigationEntries(
          browser_state_, selected_navigation, navigations));

  return nullptr;
}

void LiveTabContextBrowserAgent::CloseTab() {
  web_state_list_->CloseWebStateAt(web_state_list_->active_index(),
                                   WebStateList::CLOSE_USER_ACTION);
}
