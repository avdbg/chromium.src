
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_READING_LIST_READING_LIST_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_READING_LIST_READING_LIST_BRIDGE_H_

#include "chrome/browser/reading_list/android/reading_list_notification_delegate.h"

class ReadingListBridge : public ReadingListNotificationDelegate {
 public:
  ReadingListBridge() = default;
  ~ReadingListBridge() override = default;

 private:
  // ReadingListNotificationDelegate implementation.
  base::string16 getNotificationTitle() override;
  base::string16 getNotificationSubTitle(int unread_size) override;
  void OpenReadingListPage() override;
};

#endif  // CHROME_BROWSER_ANDROID_READING_LIST_READING_LIST_BRIDGE_H_
