// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

class ClipboardLacrosBrowserTest : public InProcessBrowserTest {
 protected:
  ClipboardLacrosBrowserTest() = default;

  ClipboardLacrosBrowserTest(const ClipboardLacrosBrowserTest&) = delete;
  ClipboardLacrosBrowserTest& operator=(const ClipboardLacrosBrowserTest&) =
      delete;

  void WaitForClipboardText(const std::string& text) {
    base::RunLoop run_loop;
    auto look_for_clipboard_text = base::BindRepeating(
        [](base::RunLoop* run_loop, std::string text) {
          auto* lacros_chrome_service =
              chromeos::LacrosChromeServiceImpl::Get();
          std::string read_text = "";
          {
            mojo::ScopedAllowSyncCallForTesting allow_sync_call;
            lacros_chrome_service->clipboard_remote()->GetCopyPasteText(
                &read_text);
          }
          if (read_text == text)
            run_loop->Quit();
        },
        &run_loop, text);
    base::RepeatingTimer timer;
    timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(1),
                std::move(look_for_clipboard_text));
    run_loop.Run();
  }

  ~ClipboardLacrosBrowserTest() override = default;
};

// Tests that accessing the text of the copy-paste clipboard succeeds.
// TODO(https://crbug.com/1157314): This test is not safe to run in parallel
// with other clipboard tests since there's a single exo clipboard.
IN_PROC_BROWSER_TEST_F(ClipboardLacrosBrowserTest, GetCopyPasteText) {
  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
  ASSERT_TRUE(lacros_chrome_service);

  if (!lacros_chrome_service->IsClipboardAvailable())
    return;

  aura::Window* window = BrowserView::GetBrowserViewForBrowser(browser())
                             ->frame()
                             ->GetNativeWindow();
  std::string id = browser_test_util::GetWindowId(window->GetRootWindow());
  browser_test_util::WaitForWindowCreation(id);
  browser_test_util::SendAndWaitForMouseClick(window->GetRootWindow());

  // Write some clipboard text and read it back.
  std::string write_text =
      base::StringPrintf("clipboard text %lu", base::RandUint64());
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(base::UTF8ToUTF16(write_text));
  }

  WaitForClipboardText(write_text);
}
