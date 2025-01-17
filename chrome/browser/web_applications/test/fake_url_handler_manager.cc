// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_url_handler_manager.h"

#include "chrome/browser/profiles/profile.h"

namespace web_app {

FakeUrlHandlerManager::FakeUrlHandlerManager(Profile* profile)
    : UrlHandlerManager(profile) {}

FakeUrlHandlerManager::~FakeUrlHandlerManager() = default;

void FakeUrlHandlerManager::RegisterUrlHandlers(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  std::move(callback).Run(true);
}

bool FakeUrlHandlerManager::UnregisterUrlHandlers(const AppId& app_id) {
  return true;
}

bool FakeUrlHandlerManager::UpdateUrlHandlers(const AppId& app_id) {
  return true;
}

}  // namespace web_app
