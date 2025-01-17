// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"

#include "base/notreached.h"

namespace autofill {

TestAutofillBubbleHandler::TestAutofillBubbleHandler() = default;

TestAutofillBubbleHandler::~TestAutofillBubbleHandler() = default;

AutofillBubbleBase* TestAutofillBubbleHandler::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    SaveCardBubbleController* controller,
    bool is_user_gesture) {
  if (!save_card_bubble_view_)
    save_card_bubble_view_ = std::make_unique<TestAutofillBubble>();
  return save_card_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowLocalCardMigrationBubble(
    content::WebContents* web_contents,
    LocalCardMigrationBubbleController* controller,
    bool is_user_gesture) {
  if (!local_card_migration_bubble_view_) {
    local_card_migration_bubble_view_ = std::make_unique<TestAutofillBubble>();
  }
  return local_card_migration_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowOfferNotificationBubble(
    content::WebContents* web_contents,
    OfferNotificationBubbleController* controller,
    bool is_user_gesture) {
  if (!offer_notification_bubble_view_)
    offer_notification_bubble_view_ = std::make_unique<TestAutofillBubble>();
  return offer_notification_bubble_view_.get();
}

SaveUPIBubble* TestAutofillBubbleHandler::ShowSaveUPIBubble(
    content::WebContents* contents,
    SaveUPIBubbleController* controller) {
  if (!save_upi_bubble_)
    save_upi_bubble_ = std::make_unique<TestSaveUPIBubble>();
  return save_upi_bubble_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowSaveAddressProfileBubble(
    content::WebContents* contents,
    SaveAddressProfileBubbleController* controller,
    bool is_user_gesture) {
  if (!save_address_profile_bubble_view_)
    save_address_profile_bubble_view_ = std::make_unique<TestAutofillBubble>();
  return save_address_profile_bubble_view_.get();
}

void TestAutofillBubbleHandler::OnPasswordSaved() {}

}  // namespace autofill
