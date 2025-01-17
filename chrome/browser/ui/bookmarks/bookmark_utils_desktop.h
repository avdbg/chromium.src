// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_DESKTOP_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_DESKTOP_H_

#include <vector>

#include "base/callback_forward.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"

class Browser;

namespace bookmarks {
class BookmarkNode;
}

namespace content {
class BrowserContext;
class PageNavigator;
}

namespace chrome {

// Number of bookmarks we'll open before prompting the user to see if they
// really want to open all.
//
// NOTE: treat this as a const. It is not const so unit tests can change the
// value.
extern size_t kNumBookmarkUrlsBeforePrompting;

// Tries to open all bookmarks in |nodes|. If there are many, prompts
// the user first. Returns immediately, opening the bookmarks
// asynchronously if prompting the user. |browser| is the browser from
// which the bookmarks were opened. Its window is used as the anchor for
// the dialog (if shown). |get_navigator| is used to fetch the
// PageNavigator used for opening the bookmarks. It may be called
// arbitrarily later as long as |browser| is alive. If it is not
// callable or returns null, this will fail gracefully.
void OpenAllIfAllowed(
    Browser* browser,
    base::OnceCallback<content::PageNavigator*()> get_navigator,
    const std::vector<const bookmarks::BookmarkNode*>& nodes,
    WindowOpenDisposition initial_disposition);

// Opens all the bookmarks in |nodes| that are of type url and all the child
// bookmarks that are of type url for folders in |nodes|. |initial_disposition|
// dictates how the first URL is opened, all subsequent URLs are opened as
// background tabs. |navigator| is used to open the URLs.
//
// This does not prompt the user. It will open an arbitrary number of
// bookmarks immediately.
void OpenAllNow(content::PageNavigator* navigator,
                const std::vector<const bookmarks::BookmarkNode*>& nodes,
                WindowOpenDisposition initial_disposition,
                content::BrowserContext* browser_context);

// Returns the count of bookmarks that would be opened by OpenAll. If
// |incognito_context| is set, the function will use it to check if the URLs
// can be opened in incognito mode, which may affect the count.
int OpenCount(gfx::NativeWindow parent,
              const std::vector<const bookmarks::BookmarkNode*>& nodes,
              content::BrowserContext* incognito_context = nullptr);

// Convenience for OpenCount() with a single BookmarkNode.
int OpenCount(gfx::NativeWindow parent,
              const bookmarks::BookmarkNode* node,
              content::BrowserContext* incognito_context = nullptr);

// Asks the user before deleting a non-empty bookmark folder.
bool ConfirmDeleteBookmarkNode(const bookmarks::BookmarkNode* node,
                               gfx::NativeWindow window);

// Shows the bookmark all tabs dialog.
void ShowBookmarkAllTabsDialog(Browser* browser);

// Returns true if OpenAll() can open at least one bookmark of type url
// in |selection|.
bool HasBookmarkURLs(
    const std::vector<const bookmarks::BookmarkNode*>& selection);

// Returns true if OpenAll() can open at least one bookmark of type url
// in |selection| with incognito mode.
bool HasBookmarkURLsAllowedInIncognitoMode(
    const std::vector<const bookmarks::BookmarkNode*>& selection,
    content::BrowserContext* browser_context);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_DESKTOP_H_
