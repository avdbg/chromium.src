// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BULLETED_LABEL_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BULLETED_LABEL_LIST_VIEW_H_

#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class BulletedLabelListView : public views::View {
 public:
  METADATA_HEADER(BulletedLabelListView);
  BulletedLabelListView();
  explicit BulletedLabelListView(const std::vector<base::string16>& texts);
  BulletedLabelListView(const BulletedLabelListView&) = delete;
  BulletedLabelListView& operator=(const BulletedLabelListView&) = delete;
  ~BulletedLabelListView() override;

  void AddLabel(const base::string16& text);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BULLETED_LABEL_LIST_VIEW_H_
