// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

class AutofillBubbleBase;
class CreditCard;

// Interface that exposes controller functionality to offer notification related
// UIs.
class OfferNotificationBubbleController {
 public:
  OfferNotificationBubbleController() = default;
  virtual ~OfferNotificationBubbleController() = default;
  OfferNotificationBubbleController(const OfferNotificationBubbleController&) =
      delete;
  OfferNotificationBubbleController& operator=(
      const OfferNotificationBubbleController&) = delete;

  // Returns a reference to the OfferNotificationBubbleController associated
  // with the given |web_contents|. If controller does not exist, this will
  // create the controller from the |web_contents| then return the reference.
  static OfferNotificationBubbleController* GetOrCreate(
      content::WebContents* web_contents);

  // Returns a reference to the OfferNotificationBubbleController associated
  // with the given |web_contents|. If controller does not exist, this will
  // return nullptr.
  static OfferNotificationBubbleController* Get(
      content::WebContents* web_contents);

  // Returns the title that should be displayed in the bubble.
  virtual base::string16 GetWindowTitle() const = 0;

  // Returns the label text for the Ok button.
  virtual base::string16 GetOkButtonLabel() const = 0;

  // Returns the reference to the offer notification bubble view.
  virtual AutofillBubbleBase* GetOfferNotificationBubbleView() const = 0;

  // Returns the related card if the offer is a card linked offer.
  virtual const CreditCard* GetLinkedCard() const = 0;

  // Returns whether the omnibox icon should be visible.
  virtual bool IsIconVisible() const = 0;

  // Removes the reference the controller has to the bubble.
  virtual void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_H_
