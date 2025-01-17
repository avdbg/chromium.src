// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_

#include <map>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/translate/source_language_combobox_model.h"
#include "chrome/browser/ui/translate/target_language_combobox_model.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/language/core/common/language_experiments.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/window/non_client_view.h"

class Browser;

namespace views {
class Checkbox;
class Combobox;
class LabelButton;
class View;
}  // namespace views

class TranslateBubbleView : public LocationBarBubbleDelegateView,
                            public ui::SimpleMenuModel::Delegate,
                            public views::TabbedPaneListener {
 public:
  // Item IDs for the option button's menu.
  enum OptionsMenuItem {
    ALWAYS_TRANSLATE_LANGUAGE,
    NEVER_TRANSLATE_LANGUAGE,
    NEVER_TRANSLATE_SITE,
    CHANGE_TARGET_LANGUAGE,
    CHANGE_SOURCE_LANGUAGE
  };

  ~TranslateBubbleView() override;

  // Shows the Translate bubble. Returns the newly created bubble's Widget or
  // nullptr in cases when the bubble already exists or when the bubble is not
  // created.
  //
  // |is_user_gesture| is true when the bubble is shown on the user's deliberate
  // action.
  static views::Widget* ShowBubble(views::View* anchor_view,
                                   views::Button* highlighted_button,
                                   content::WebContents* web_contents,
                                   translate::TranslateStep step,
                                   const std::string& source_language,
                                   const std::string& target_language,
                                   translate::TranslateErrors::Type error_type,
                                   DisplayReason reason);

  // Closes the current bubble if it exists.
  static void CloseCurrentBubble();

  // Returns the bubble view currently shown. This may return NULL.
  static TranslateBubbleView* GetCurrentBubble();

  TranslateBubbleModel* model() { return model_.get(); }

  // LocationBarBubbleDelegateView:
  void Init() override;
  View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowTitle() const override;
  void WindowClosing() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetClosing(views::Widget* widget) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // Returns the current view state.
  TranslateBubbleModel::ViewState GetViewState() const;

 protected:
  // LocationBarBubbleDelegateView:
  void CloseBubble() override;

 private:
  enum ButtonID {
    BUTTON_ID_DONE = 1,
    BUTTON_ID_TRY_AGAIN,
    BUTTON_ID_ALWAYS_TRANSLATE,
    BUTTON_ID_OPTIONS_MENU,
    BUTTON_ID_CLOSE,
    BUTTON_ID_RESET
  };

  friend class TranslateBubbleViewTest;
  friend void ::translate::test_utils::PressTranslate(::Browser*);
  friend void ::translate::test_utils::PressRevert(::Browser*);
  friend void ::translate::test_utils::SelectTargetLanguageByDisplayName(
      ::Browser*,
      const ::base::string16&);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TranslateButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxShortcut);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxAndCloseButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxAndDoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, SourceDoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TargetDoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           DoneButtonWithoutTranslating);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           OptionsMenuNeverTranslateLanguage);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           OptionsMenuRespectsBlocklistSite);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           MenuOptionsHiddenOnUnknownSource);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           OptionsMenuNeverTranslateSite);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateLanguageMenuItem);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           TabSelectedAfterTranslation);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateTriggerTranslation);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           ShowOriginalUpdatesViewState);

  TranslateBubbleView(views::View* anchor_view,
                      std::unique_ptr<TranslateBubbleModel> model,
                      translate::TranslateErrors::Type error_type,
                      content::WebContents* web_contents);

  // views::TabbedPaneListener:
  void TabSelectedAt(int index) override;

  // Returns the current child view.
  views::View* GetCurrentView() const;

  // Triggers options menu.
  void ShowOptionsMenu(views::Button* source);

  // Handles the event when the user changes an index of a combobox.
  void SourceLanguageChanged();
  void TargetLanguageChanged();

  void AlwaysTranslatePressed();

  // Updates the visibilities of child views according to the current view type.
  void UpdateChildVisibilities();

  // Creates the view used before/during/after translate.
  std::unique_ptr<views::View> CreateView();

  // AddTab function requires a view element to be shown below each tab.
  // This function creates an empty view so no extra white space below the tab.
  std::unique_ptr<views::View> CreateEmptyPane();

  // Creates the 'error' view for Button UI. Caller takes ownership of the
  // returned view.
  std::unique_ptr<views::View> CreateViewError();

  // Creates the 'error' view skeleton UI with no title. Caller takes ownership
  // of the returned view.
  std::unique_ptr<views::View> CreateViewErrorNoTitle(
      std::unique_ptr<views::Button> advanced_button);

  // Creates source language label and combobox for Tab UI advanced view. Caller
  // takes ownership of the returned view.
  std::unique_ptr<views::View> CreateViewAdvancedSource();

  // Creates source language label and combobox for Tab UI advanced view. Caller
  // takes ownership of the returned view.
  std::unique_ptr<views::View> CreateViewAdvancedTarget();

  // Creates the 'advanced' view to show source/target language combobox. Caller
  // takes ownership of the returned view.
  std::unique_ptr<views::View> CreateViewAdvanced(
      std::unique_ptr<views::Combobox> combobox,
      std::unique_ptr<views::Label> language_title_label,
      std::unique_ptr<views::Button> advance_done_button,
      std::unique_ptr<views::Checkbox> advanced_always_translate_checkbox);

  // Creates a translate icon for when the bottom branding isn't showing. This
  // should only be used on non-Chrome-branded builds.
  std::unique_ptr<views::ImageView> CreateTranslateIcon();

  // Creates a three dot options menu button.
  std::unique_ptr<views::Button> CreateOptionsMenuButton();

  // Creates a close button.
  std::unique_ptr<views::Button> CreateCloseButton();

  // Get the current always translate checkbox
  views::Checkbox* GetAlwaysTranslateCheckbox();

  // Sets the window title. The window title still needs to be set, even when it
  // is not shown, for accessiblity purposes.
  void SetWindowTitle(TranslateBubbleModel::ViewState view_state);

  // Updates the view state. Whenever the view state is updated, the title needs
  // to be updated for accessibility.
  void UpdateViewState(TranslateBubbleModel::ViewState view_state);

  // Switches the view type.
  void SwitchView(TranslateBubbleModel::ViewState view_state);

  // Handles tab switching on when the view type switches.
  void SwitchTabForViewState(TranslateBubbleModel::ViewState view_state);

  // Switches to the error view.
  void SwitchToErrorView(translate::TranslateErrors::Type error_type);

  // Updates the advanced view.
  void UpdateAdvancedView();

  // Actions for button presses shared with accelerators.
  void Translate();
  void ShowOriginal();
  void ConfirmAdvancedOptions();

  // Handles the reset button in advanced view under Tab UI.
  void ResetLanguage();

  // Retrieve the names of the from/to languages and reset the language
  // indices.
  void UpdateLanguageNames(base::string16* original_language_name,
                           base::string16* target_language_name);

  void UpdateInsets(TranslateBubbleModel::ViewState state);

  // If the page is already translated, revert it. Otherwise decline
  // translation. Then close the bubble view.
  void RevertOrDeclineTranslation();

  static TranslateBubbleView* translate_bubble_view_;

  views::View* translate_view_ = nullptr;
  views::View* error_view_ = nullptr;
  views::View* advanced_view_source_ = nullptr;
  views::View* advanced_view_target_ = nullptr;

  std::unique_ptr<SourceLanguageComboboxModel> source_language_combobox_model_;
  std::unique_ptr<TargetLanguageComboboxModel> target_language_combobox_model_;

  views::Combobox* source_language_combobox_ = nullptr;
  views::Combobox* target_language_combobox_ = nullptr;

  views::Checkbox* always_translate_checkbox_ = nullptr;
  views::Checkbox* advanced_always_translate_checkbox_ = nullptr;
  views::TabbedPane* tabbed_pane_ = nullptr;

  views::LabelButton* advanced_done_button_source_ = nullptr;
  views::LabelButton* advanced_done_button_target_ = nullptr;

  // Default source/target language without user interaction.
  int previous_source_language_index_;
  int previous_target_language_index_;

  std::unique_ptr<ui::SimpleMenuModel> options_menu_model_;
  std::unique_ptr<views::MenuRunner> options_menu_runner_;

  std::unique_ptr<TranslateBubbleModel> model_;

  translate::TranslateErrors::Type error_type_;

  // Whether the window is an incognito window.
  const bool is_in_incognito_window_;

  bool should_always_translate_ = false;
  bool should_never_translate_language_ = false;
  bool should_never_translate_site_ = false;

  std::unique_ptr<WebContentMouseHandler> mouse_handler_;

  DISALLOW_COPY_AND_ASSIGN(TranslateBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_
