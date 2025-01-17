// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

class ExtensionInstallBlockedDialogViewTest : public DialogBrowserTest {
 public:
  ExtensionInstallBlockedDialogViewTest() = default;
  ~ExtensionInstallBlockedDialogViewTest() override = default;

  void ShowUi(const std::string& name) override {
    chrome::ShowExtensionInstallBlockedDialog(
        "extension_name", message_, CreateExtensionIcon(),
        browser()->tab_strip_model()->GetWebContentsAt(0), base::DoNothing());
  }

  // Creates a big icon so that dialog will downscale it.
  gfx::ImageSkia CreateExtensionIcon() {
    SkBitmap icon;
    icon.allocN32Pixels(800, 800);
    icon.eraseColor(SK_ColorBLUE);
    return gfx::ImageSkia::CreateFrom1xBitmap(icon);
  }

  void set_message(const base::string16& message) { message_ = message; }

 private:
  base::string16 message_;
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallBlockedDialogViewTest,
                       InvokeUi_WithoutCustomMessage) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallBlockedDialogViewTest,
                       InvokeUi_WithCustomMessage) {
  set_message(base::ASCIIToUTF16("message"));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallBlockedDialogViewTest,
                       InvokeUi_WithLongCustomMessage) {
  set_message(
      base::ASCIIToUTF16("long\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nmessage"));
  ShowAndVerifyUi();
}
