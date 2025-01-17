// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_

#include <map>

#include "chrome/browser/web_applications/components/web_app_origin_association_manager.h"

namespace web_app {

// Fake implementation of WebAppOriginAssociationManager.
class FakeWebAppOriginAssociationManager
    : public WebAppOriginAssociationManager {
 public:
  FakeWebAppOriginAssociationManager();
  ~FakeWebAppOriginAssociationManager() override;

  // Sends back |url_handlers| as is.
  void GetWebAppOriginAssociations(
      const GURL& manifest_url,
      apps::UrlHandlers url_handlers,
      OnDidGetWebAppOriginAssociations callback) override;

  void SetData(std::map<apps::UrlHandlerInfo, apps::UrlHandlerInfo> data);

 private:
  // Maps a url handler to the corresponding result to send back in the
  // callback.
  std::map<apps::UrlHandlerInfo, apps::UrlHandlerInfo> data_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_
