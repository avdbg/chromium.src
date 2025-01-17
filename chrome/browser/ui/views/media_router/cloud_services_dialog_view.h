// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CLOUD_SERVICES_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CLOUD_SERVICES_DIALOG_VIEW_H_

#include "base/strings/string16.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

class Browser;

namespace media_router {

// Dialog that asks the user whether they want to enable cloud services for the
// Cast feature.
class CloudServicesDialogView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(CloudServicesDialogView);

  CloudServicesDialogView(const CloudServicesDialogView&) = delete;
  CloudServicesDialogView& operator=(const CloudServicesDialogView&) = delete;

  // Instantiates and shows the singleton dialog.
  static void ShowDialog(views::View* anchor_view, Browser* browser);

  // No-op if the dialog is currently not shown.
  static void HideDialog();

  static bool IsShowing();

  // Called by tests. Returns the singleton dialog instance.
  static CloudServicesDialogView* GetDialogForTest();

 private:
  CloudServicesDialogView(views::View* anchor_view, Browser* browser);
  ~CloudServicesDialogView() override;

  void OnDialogAccepted();

  // views::BubbleDialogDelegateView:
  void Init() override;
  void WindowClosing() override;

  // The singleton dialog instance. This is a nullptr when a dialog is not
  // shown.
  static CloudServicesDialogView* instance_;

  // Browser window that this dialog is attached to.
  Browser* const browser_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CLOUD_SERVICES_DIALOG_VIEW_H_
