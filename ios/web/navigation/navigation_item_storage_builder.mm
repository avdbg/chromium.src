// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_item_storage_builder.h"

#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

int NavigationItemStorageBuilder::ItemStoredSize(
    NavigationItemImpl* navigation_item) const {
  DCHECK(navigation_item);
  int size = 0;
  size += navigation_item->virtual_url_.spec().size();
  size += navigation_item->url_.spec().size();
  size += navigation_item->referrer_.url.spec().size();
  size += navigation_item->title_.size();
  for (NSString* key in navigation_item->http_request_headers_) {
    NSString* value = navigation_item->http_request_headers_[key];
    size += key.length + value.length;
  }
  return size;
}

CRWNavigationItemStorage* NavigationItemStorageBuilder::BuildStorage(
    NavigationItemImpl* navigation_item) const {
  DCHECK(navigation_item);
  CRWNavigationItemStorage* storage = [[CRWNavigationItemStorage alloc] init];
  storage.virtualURL = navigation_item->GetVirtualURL();
  storage.URL = navigation_item->GetURL();
  // Use default referrer if URL is longer than allowed. Navigation items with
  // these long URLs will not be serialized, so there is no point in keeping
  // referrer URL.
  if (navigation_item->GetReferrer().url.spec().size() <= url::kMaxURLChars) {
    storage.referrer = navigation_item->GetReferrer();
  }
  storage.timestamp = navigation_item->GetTimestamp();
  storage.title = navigation_item->GetTitle();
  storage.displayState = navigation_item->GetPageDisplayState();
  storage.shouldSkipRepostFormConfirmation =
      navigation_item->ShouldSkipRepostFormConfirmation();
  storage.userAgentType = navigation_item->GetUserAgentType();
  storage.HTTPRequestHeaders = navigation_item->GetHttpRequestHeaders();
  return storage;
}

std::unique_ptr<NavigationItemImpl>
NavigationItemStorageBuilder::BuildNavigationItemImpl(
    CRWNavigationItemStorage* navigation_item_storage) const {
  std::unique_ptr<NavigationItemImpl> item(new web::NavigationItemImpl());
  // While the virtual URL is persisted, we still need the original request URL
  // and the non-virtual URL to be set upon NavigationItem creation.  Since
  // GetVirtualURL() returns |url_| for the non-overridden case, this will also
  // update the virtual URL reported by this object.
  item->original_request_url_ = navigation_item_storage.URL;

  // In the cases where the URL to be restored is not an HTTP URL, it very
  // probable that we can't restore the page (for example for files, either
  // because it is already a session restoration item or because it is an
  // external PDF), don't restore it to avoid issues. See
  // http://crbug.com/1017147 , 1076851 and 1065433.
  bool should_use_url = navigation_item_storage.URL.SchemeIsHTTPOrHTTPS() ||
                        web::GetWebClient()->IsEmbedderBlockRestoreUrlEnabled();
  if (should_use_url) {
    item->SetURL(navigation_item_storage.URL);
    item->SetVirtualURL(navigation_item_storage.virtualURL);
  } else {
    item->SetURL(navigation_item_storage.virtualURL);
  }

  item->referrer_ = navigation_item_storage.referrer;
  item->timestamp_ = navigation_item_storage.timestamp;
  item->title_ = navigation_item_storage.title;
  item->page_display_state_ = navigation_item_storage.displayState;
  item->should_skip_repost_form_confirmation_ =
      navigation_item_storage.shouldSkipRepostFormConfirmation;
  // Use reload transition type to avoid incorrect increase for typed count.
  item->transition_type_ = ui::PAGE_TRANSITION_RELOAD;
  item->user_agent_type_ = navigation_item_storage.userAgentType;
  item->http_request_headers_ =
      [navigation_item_storage.HTTPRequestHeaders mutableCopy];
  return item;
}

}  // namespace web
