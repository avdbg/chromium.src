// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_OBSOLETE_SYSTEM_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_OBSOLETE_SYSTEM_INFOBAR_DELEGATE_H_

#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

class InfoBarService;

// An infobar that displays a message saying the system (OS or hardware) is
// obsolete, along with a "Learn More" link.
class ObsoleteSystemInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an obsolete system infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service);

  ObsoleteSystemInfoBarDelegate(const ObsoleteSystemInfoBarDelegate&) = delete;
  ObsoleteSystemInfoBarDelegate& operator=(
      const ObsoleteSystemInfoBarDelegate&) = delete;

 private:
  ObsoleteSystemInfoBarDelegate();
  ~ObsoleteSystemInfoBarDelegate() override = default;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
};

#endif  // CHROME_BROWSER_UI_STARTUP_OBSOLETE_SYSTEM_INFOBAR_DELEGATE_H_
