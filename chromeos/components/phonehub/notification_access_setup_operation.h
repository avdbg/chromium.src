// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_SETUP_OPERATION_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_SETUP_OPERATION_H_

#include <ostream>

#include "base/callback.h"

namespace chromeos {
namespace phonehub {

// Implements the notification access setup flow. This flow involves:
// (1) Creating a connection to the phone if one does not already exist.
// (2) Sending a message to the phone which asks it to begin the setup flow;
//     upon receipt of the message, the phone displays a UI which asks the
//     user to enable notification access for Phone Hub.
// (3) Waiting for the user to complete the flow; once the flow is complete, the
//     phone sends a message back to this device which indicates that
//     notification access has been granted.
//
// If an instance of this class exists, the flow continues until the status
// changes to a "final" status (i.e., a success or a fatal error). To cancel the
// ongoing setup operation, simply delete the instance of this class.
class NotificationAccessSetupOperation {
 public:
  // Note: Numerical values should not be changed because they must stay in
  // sync with multidevice_notification_access_setup_dialog.js, with the
  // exception of NOT_STARTED, which has a value of 0.
  enum class Status {
    // Connecting to the phone in order to set up notification access.
    kConnecting = 1,

    // No connection was able to be made to the phone within the expected time
    // period.
    kTimedOutConnecting = 2,

    // A connection to the phone was successful, but it unexpectedly became
    // disconnected before the setup flow could complete.
    kConnectionDisconnected = 3,

    // A connection to the phone has succeeded, and a message has been sent to
    // the phone to start the notification access opt-in flow. However, the user
    // has not yet completed the flow phone-side.
    kSentMessageToPhoneAndWaitingForResponse = 4,

    // The user has completed the phone-side opt-in flow.
    kCompletedSuccessfully = 5,

    // The user's phone is prohibited from granting notification access (e.g.,
    // the user could be using a Work Profile).
    kProhibitedFromProvidingAccess = 6
  };

  // Returns true if the provided status is the final one for this operation,
  // indicating either success or failure.
  static bool IsFinalStatus(Status status);

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when status of the setup flow has changed.
    virtual void OnStatusChange(Status new_status) = 0;
  };

  NotificationAccessSetupOperation(const NotificationAccessSetupOperation&) =
      delete;
  NotificationAccessSetupOperation& operator=(
      const NotificationAccessSetupOperation&) = delete;
  virtual ~NotificationAccessSetupOperation();

 private:
  friend class NotificationAccessManager;

  NotificationAccessSetupOperation(Delegate* delegate,
                                   base::OnceClosure destructor_callback);

  void NotifyStatusChanged(Status new_status);

  Delegate* const delegate_;
  base::OnceClosure destructor_callback_;
};

std::ostream& operator<<(std::ostream& stream,
                         NotificationAccessSetupOperation::Status status);

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_SETUP_OPERATION_H_
