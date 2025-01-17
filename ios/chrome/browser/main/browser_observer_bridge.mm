// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserObserverBridge::BrowserObserverBridge(id<BrowserObserving> observer)
    : observer_(observer) {}

BrowserObserverBridge::~BrowserObserverBridge() {}

void BrowserObserverBridge::BrowserDestroyed(Browser* browser) {
  [observer_ browserDestroyed:browser];
}
