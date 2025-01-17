// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/native_widget_types.h"

class Profile;
class SharesheetBubbleView;

namespace views {
class View;
}

namespace gfx {
struct VectorIcon;
}

namespace sharesheet {

class SharesheetService;

// The SharesheetServiceDelegate is the middle point between the UI and the
// business logic in the sharesheet.
class SharesheetServiceDelegate : public SharesheetController {
 public:
  SharesheetServiceDelegate(gfx::NativeWindow native_window,
                            SharesheetService* sharesheet_service);
  ~SharesheetServiceDelegate() override;
  SharesheetServiceDelegate(const SharesheetServiceDelegate&) = delete;
  SharesheetServiceDelegate& operator=(const SharesheetServiceDelegate&) =
      delete;

  void ShowBubble(std::vector<TargetInfo> targets,
                  apps::mojom::IntentPtr intent,
                  sharesheet::CloseCallback close_callback);
  void OnBubbleClosed(const base::string16& active_action);
  void OnTargetSelected(const base::string16& target_name,
                        const TargetType type,
                        apps::mojom::IntentPtr intent,
                        views::View* share_action_view);
  void OnActionLaunched();
  const gfx::VectorIcon* GetVectorIcon(const base::string16& display_name);
  gfx::NativeWindow GetNativeWindow();

  // SharesheetController overrides
  Profile* GetProfile() override;
  void SetSharesheetSize(const int& width, const int& height) override;
  void CloseSharesheet() override;

 private:
  bool is_bubble_open_ = false;

  // Only used for ID purposes. NativeWindow will always outlive the
  // SharesheetServiceDelegate.
  gfx::NativeWindow native_window_;

  base::string16 active_action_;
  std::unique_ptr<SharesheetBubbleView> sharesheet_bubble_view_;
  SharesheetService* sharesheet_service_;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_
