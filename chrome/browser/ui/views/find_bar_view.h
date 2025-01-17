// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIND_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FIND_BAR_VIEW_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "chrome/browser/ui/views/dropdown_bar_host_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout_view.h"

class FindBarHost;
class FindBarMatchCountLabel;

namespace find_in_page {
class FindNotificationDetails;
}

namespace gfx {
class Range;
}

namespace views {
class Painter;
class Separator;
class Textfield;
}

////////////////////////////////////////////////////////////////////////////////
//
// The FindBarView is responsible for drawing the UI controls of the
// FindBar, the find text box, the 'Find' button and the 'Close'
// button. It communicates the user search words to the FindBarHost.
//
////////////////////////////////////////////////////////////////////////////////
class FindBarView : public views::BoxLayoutView,
                    public DropdownBarHostDelegate,
                    public views::TextfieldController {
 public:
  METADATA_HEADER(FindBarView);

  explicit FindBarView(FindBarHost* host = nullptr);
  ~FindBarView() override;

  void SetHost(FindBarHost* host);

  // Accessors for the text and selection displayed in the text box.
  void SetFindTextAndSelectedRange(const base::string16& find_text,
                                   const gfx::Range& selected_range);
  base::string16 GetFindText() const;
  gfx::Range GetSelectedRange() const;

  // Gets the selected text in the text box.
  base::string16 GetFindSelectedText() const;

  // Gets the match count text displayed in the text box.
  base::string16 GetMatchCountText() const;

  // Updates the label inside the Find text box that shows the ordinal of the
  // active item and how many matches were found.
  void UpdateForResult(const find_in_page::FindNotificationDetails& result,
                       const base::string16& find_text);

  // Clears the current Match Count value in the Find text box.
  void ClearMatchCount();

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  // DropdownBarHostDelegate:
  void FocusAndSelectAll() override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void OnAfterUserAction(views::Textfield* sender) override;
  void OnAfterPaste() override;

 private:
  // Starts finding |search_text|.  If the text is empty, stops finding.
  void Find(const base::string16& search_text);

  // Find the next/previous occurrence of search text when clicking the
  // next/previous button.
  void FindNext(bool reverse = false);
  // End the current find session and close the find bubble.
  void EndFindSession();

  // Updates the appearance for the match count label.
  void UpdateMatchCountAppearance(bool no_match);

  // Returns the color for the icons on the buttons per the current NativeTheme.
  SkColor GetTextColorForIcon();

  // The OS-specific view for the find bar that acts as an intermediary
  // between us and the WebContentsView.
  FindBarHost* find_bar_host_;

  // Used to detect if the input text, not including the IME composition text,
  // has changed or not.
  base::string16 last_searched_text_;

  // The controls in the window.
  views::Textfield* find_text_;
  std::unique_ptr<views::Painter> find_text_border_;
  FindBarMatchCountLabel* match_count_text_;
  views::Separator* separator_;
  views::ImageButton* find_previous_button_;
  views::ImageButton* find_next_button_;
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(FindBarView);
};

BEGIN_VIEW_BUILDER(/* no export */, FindBarView, views::BoxLayoutView)
VIEW_BUILDER_PROPERTY(FindBarHost*, Host)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, FindBarView)

#endif  // CHROME_BROWSER_UI_VIEWS_FIND_BAR_VIEW_H_
