// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/local_state_mixin.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"

namespace chromeos {

namespace {

class TestMainExtraPart : public ChromeBrowserMainExtraParts {
 public:
  explicit TestMainExtraPart(LocalStateMixin::Delegate* delegate)
      : delegate_(delegate) {}
  ~TestMainExtraPart() override = default;

  // ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override {
    // SaveKnownUser depends on UserManager to get the local state that has to
    // be updated, and do ephemeral user checks.
    // Given that user manager does not exist yet (by design), create a
    // temporary fake user manager instance.
    auto user_manager = std::make_unique<user_manager::FakeUserManager>();
    user_manager->set_local_state(g_browser_process->local_state());
    user_manager::ScopedUserManager scoper(std::move(user_manager));
    delegate_->SetUpLocalState();
  }

 private:
  LocalStateMixin::Delegate* const delegate_;
};

}  // namespace

LocalStateMixin::LocalStateMixin(InProcessBrowserTestMixinHost* host,
                                 Delegate* delegate)
    : InProcessBrowserTestMixin(host), delegate_(delegate) {}

LocalStateMixin::~LocalStateMixin() = default;

void LocalStateMixin::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  // `browser_main_parts` take ownership of TestUserRegistrationMainExtra.
  static_cast<ChromeBrowserMainParts*>(browser_main_parts)
      ->AddParts(std::make_unique<TestMainExtraPart>(delegate_));
}

}  // namespace chromeos
