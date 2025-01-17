// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_DOMAIN_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_DOMAIN_LIST_VIEW_H_

#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class ContentSettingDomainListView : public views::View {
 public:
  METADATA_HEADER(ContentSettingDomainListView);
  ContentSettingDomainListView(const base::string16& title,
                               const std::set<std::string>& domains);
  ContentSettingDomainListView(const ContentSettingDomainListView&) = delete;
  ContentSettingDomainListView& operator=(const ContentSettingDomainListView&) =
      delete;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_DOMAIN_LIST_VIEW_H_
