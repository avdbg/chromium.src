// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_PERMISSIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_PERMISSIONS_VIEW_H_

#include <vector>

#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace extensions {
struct InstallPromptPermissions;
}  // namespace extensions

// A custom view for the permissions section of the extension info. It contains
// the labels for each permission and the views for their associated details, if
// there are any.
class ExtensionPermissionsView : public views::View {
 public:
  METADATA_HEADER(ExtensionPermissionsView);
  explicit ExtensionPermissionsView(int available_width);
  ExtensionPermissionsView(const ExtensionPermissionsView&) = delete;
  ExtensionPermissionsView& operator=(const ExtensionPermissionsView&) = delete;

  // Adds a single pair of |permission_text| and |permission_details| to
  // be rendered in the view.
  void AddItem(const base::string16& permission_text,
               const base::string16& permission_details);

  // Adds the set of |permissions| to be rendered in the view.
  void AddPermissions(const extensions::InstallPromptPermissions& permissions);

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  int available_width_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_PERMISSIONS_VIEW_H_
