// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_INTERACTION_HANDLER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_INTERACTION_HANDLER_H_

#include <stdint.h>
#include "chromeos/components/phonehub/notification_interaction_handler.h"

namespace chromeos {
namespace phonehub {

class FakeNotificationInteractionHandler
    : public NotificationInteractionHandler {
 public:
  FakeNotificationInteractionHandler();
  ~FakeNotificationInteractionHandler() override;

  size_t handled_notification_count() const {
    return handled_notification_count_;
  }

 private:
  void HandleNotificationClicked(int64_t notification_id) override;
  size_t handled_notification_count_ = 0;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_INTERACTION_HANDLER_H_
