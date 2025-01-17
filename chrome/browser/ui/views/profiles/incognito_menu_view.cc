// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/incognito_menu_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/style/typography.h"

#if defined(OS_WIN)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#endif

IncognitoMenuView::IncognitoMenuView(views::Button* anchor_button,
                                     Browser* browser)
    : ProfileMenuViewBase(anchor_button, browser) {
  DCHECK(browser->profile()->IsIncognitoProfile());
  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::INCOGNITO_WINDOW_COUNT);

  base::RecordAction(base::UserMetricsAction("IncognitoMenu_Show"));
}

IncognitoMenuView::~IncognitoMenuView() = default;

void IncognitoMenuView::BuildMenu() {
  int incognito_window_count =
      BrowserList::GetOffTheRecordBrowsersActiveForProfile(
          browser()->profile());

  ui::ThemedVectorIcon header_art_icon(
      &kIncognitoMenuArtIcon, ui::NativeTheme::kColorId_AvatarHeaderArt);
  SetProfileIdentityInfo(
      /*profile_name=*/base::string16(),
      /*background_color=*/SK_ColorTRANSPARENT,
      /*edit_button=*/base::nullopt,
      ui::ImageModel::FromVectorIcon(
          kIncognitoProfileIcon, ui::NativeTheme::kColorId_AvatarIconIncognito),
      l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_TITLE),
      incognito_window_count > 1
          ? l10n_util::GetPluralStringFUTF16(IDS_INCOGNITO_WINDOW_COUNT_MESSAGE,
                                             incognito_window_count)
          : base::string16(),
      header_art_icon);

#if defined(OS_WIN)
  if (ProfileShortcutManager::IsFeatureEnabled() &&
      base::FeatureList::IsEnabled(
          features::kEnableIncognitoShortcutOnDesktop)) {
    // TODO(crbug.com/1113162): Add desktop shortcut icon to the menu entry.
    AddFeatureButton(
        l10n_util::GetStringUTF16(
            IDS_INCOGNITO_PROFILE_MENU_CREATE_SHORTCUT_BUTTON),
        base::BindRepeating(&IncognitoMenuView::OnCreateShortcutButtonClicked,
                            base::Unretained(this)));
  }
#endif

  const bool new_menu_design =
      base::FeatureList::IsEnabled(features::kNewProfilePicker);

  AddFeatureButton(
      l10n_util::GetStringUTF16(
          new_menu_design ? IDS_INCOGNITO_PROFILE_MENU_CLOSE_BUTTON_NEW
                          : IDS_INCOGNITO_PROFILE_MENU_CLOSE_BUTTON),
      base::BindRepeating(&IncognitoMenuView::OnExitButtonClicked,
                          base::Unretained(this)),
      new_menu_design ? vector_icons::kCloseIcon : kCloseAllIcon);
}

base::string16 IncognitoMenuView::GetAccessibleWindowTitle() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_INCOGNITO_BUBBLE_ACCESSIBLE_TITLE,
      BrowserList::GetOffTheRecordBrowsersActiveForProfile(
          browser()->profile()));
}

#if defined(OS_WIN)
void IncognitoMenuView::OnCreateShortcutButtonClicked() {
  RecordClick(ActionableItem::kCreateIncognitoShortcutButton);
  ProfileShortcutManager* shortcut_manager =
      g_browser_process->profile_manager()->profile_shortcut_manager();

  DCHECK(shortcut_manager);
  if (shortcut_manager)
    shortcut_manager->CreateIncognitoProfileShortcut(
        browser()->profile()->GetPath());
}
#endif

void IncognitoMenuView::OnExitButtonClicked() {
  RecordClick(ActionableItem::kExitProfileButton);
  base::RecordAction(base::UserMetricsAction("IncognitoMenu_ExitClicked"));
  // Skipping before-unload trigger to give incognito mode users a chance to
  // quickly close all incognito windows without needing to confirm closing the
  // open forms.
  BrowserList::CloseAllBrowsersWithIncognitoProfile(
      browser()->profile(), base::DoNothing(), base::DoNothing(),
      true /* skip_beforeunload */);
}
