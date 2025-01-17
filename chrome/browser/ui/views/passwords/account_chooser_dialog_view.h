// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_ACCOUNT_CHOOSER_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_ACCOUNT_CHOOSER_DIALOG_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

class CredentialManagerDialogController;

class AccountChooserDialogView : public views::BubbleDialogDelegateView,
                                 public AccountChooserPrompt {
 public:
  METADATA_HEADER(AccountChooserDialogView);
  AccountChooserDialogView(CredentialManagerDialogController* controller,
                           content::WebContents* web_contents);
  AccountChooserDialogView(const AccountChooserDialogView&) = delete;
  AccountChooserDialogView& operator=(const AccountChooserDialogView&) = delete;
  ~AccountChooserDialogView() override;

  // AccountChooserPrompt:
  void ShowAccountChooser() override;
  void ControllerGone() override;

 private:
  // WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  // DialogDelegate:
  bool Accept() override;

  // Sets up the child views.
  void InitWindow();

  void CredentialsItemPressed(const password_manager::PasswordForm* form);

  // A weak pointer to the controller.
  CredentialManagerDialogController* controller_;
  content::WebContents* web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_ACCOUNT_CHOOSER_DIALOG_VIEW_H_
