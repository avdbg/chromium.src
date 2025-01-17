// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_access_setup_operation.h"

#include <array>

#include "base/check.h"
#include "base/containers/contains.h"

namespace chromeos {
namespace phonehub {
namespace {

// Status values which are considered "final" - i.e., once the status of an
// operation changes to one of these values, the operation has completed. These
// status values indicate either a success or a fatal error.
constexpr std::array<NotificationAccessSetupOperation::Status, 4>
    kOperationFinishedStatus{
        NotificationAccessSetupOperation::Status::kTimedOutConnecting,
        NotificationAccessSetupOperation::Status::kConnectionDisconnected,
        NotificationAccessSetupOperation::Status::kCompletedSuccessfully,
        NotificationAccessSetupOperation::Status::
            kProhibitedFromProvidingAccess,
    };

}  // namespace

// static
bool NotificationAccessSetupOperation::IsFinalStatus(Status status) {
  return base::Contains(kOperationFinishedStatus, status);
}

NotificationAccessSetupOperation::NotificationAccessSetupOperation(
    Delegate* delegate,
    base::OnceClosure destructor_callback)
    : delegate_(delegate),
      destructor_callback_(std::move(destructor_callback)) {
  DCHECK(delegate_);
  DCHECK(destructor_callback_);
}

NotificationAccessSetupOperation::~NotificationAccessSetupOperation() {
  std::move(destructor_callback_).Run();
}

void NotificationAccessSetupOperation::NotifyStatusChanged(Status new_status) {
  delegate_->OnStatusChange(new_status);
}

std::ostream& operator<<(std::ostream& stream,
                         NotificationAccessSetupOperation::Status status) {
  switch (status) {
    case NotificationAccessSetupOperation::Status::kConnecting:
      stream << "[Connecting]";
      break;
    case NotificationAccessSetupOperation::Status::kTimedOutConnecting:
      stream << "[Timed out connecting]";
      break;
    case NotificationAccessSetupOperation::Status::kConnectionDisconnected:
      stream << "[Connection disconnected]";
      break;
    case NotificationAccessSetupOperation::Status::
        kSentMessageToPhoneAndWaitingForResponse:
      stream << "[Sent message to phone; waiting for response]";
      break;
    case NotificationAccessSetupOperation::Status::kCompletedSuccessfully:
      stream << "[Completed successfully]";
      break;
    case NotificationAccessSetupOperation::Status::
        kProhibitedFromProvidingAccess:
      stream << "[Prohibited from providing access]";
      break;
  }

  return stream;
}

}  // namespace phonehub
}  // namespace chromeos
