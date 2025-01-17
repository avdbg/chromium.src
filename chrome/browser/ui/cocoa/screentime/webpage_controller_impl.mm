// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/webpage_controller_impl.h"

#include "net/base/mac/url_conversions.h"

#include <ScreenTime/ScreenTime.h>

@interface BlockedObserver : NSObject
@end

NS_AVAILABLE_MAC(11.0)
@implementation BlockedObserver {
  screentime::WebpageControllerImpl* _controller;
  STWebpageController* _nativeController;
}

- (instancetype)initWithController:
                    (screentime::WebpageControllerImpl*)controller
                  nativeController:(STWebpageController*)nativeController {
  if (self = [super init]) {
    _controller = controller;
    _nativeController = nativeController;
    [_nativeController addObserver:self
                        forKeyPath:@"URLIsBlocked"
                           options:0
                           context:nullptr];
  }
  return self;
}

- (void)dealloc {
  [_nativeController removeObserver:self forKeyPath:@"URLIsBlocked"];
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)forKeyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK([forKeyPath isEqualToString:@"URLIsBlocked"]);
  _controller->OnBlockedChanged(_nativeController.URLIsBlocked);
}

@end

namespace screentime {

WebpageControllerImpl::WebpageControllerImpl(
    const BlockedChangedCallback& blocked_changed_callback)
    : platform_controller_([[STWebpageController alloc] init]),
      blocked_changed_callback_(blocked_changed_callback) {}
WebpageControllerImpl::~WebpageControllerImpl() = default;

NSView* WebpageControllerImpl::GetView() {
  return [platform_controller_ view];
}

void WebpageControllerImpl::PageURLChangedTo(const GURL& url) {
  [platform_controller_ setURL:net::NSURLWithGURL(url)];
}

void WebpageControllerImpl::OnBlockedChanged(bool blocked) {
  blocked_changed_callback_.Run(blocked);
}

}  // namespace screentime
