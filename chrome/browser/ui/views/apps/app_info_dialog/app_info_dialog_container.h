// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_CONTAINER_H_

#include <memory>

#include "base/callback_forward.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/geometry/size.h"

namespace views {
class DialogDelegateView;
class View;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Creates a new dialog containing |view| that can be displayed inside the app
// list, covering the entire app list and adding a close button.
views::DialogDelegateView* CreateAppListContainerForView(
    std::unique_ptr<views::View> view);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Creates a new native dialog of the given |size| containing |view| with a
// close button and draggable titlebar.
views::DialogDelegateView* CreateDialogContainerForView(
    std::unique_ptr<views::View> view,
    const gfx::Size& size,
    base::OnceClosure close_callback);

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_CONTAINER_H_
