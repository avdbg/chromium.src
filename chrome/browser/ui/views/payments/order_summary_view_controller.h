// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_ORDER_SUMMARY_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_ORDER_SUMMARY_VIEW_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"

namespace payments {

class PaymentRequestDialogView;

// The PaymentRequestSheetController subtype for the Order Summary screen of the
// Payment Request flow.
class OrderSummaryViewController : public PaymentRequestSheetController,
                                   public PaymentRequestSpec::Observer,
                                   public PaymentRequestState::Observer {
 public:
  // Does not take ownership of the arguments, which should outlive this object.
  // The `spec` and `state` parameters should not be null.
  OrderSummaryViewController(base::WeakPtr<PaymentRequestSpec> spec,
                             base::WeakPtr<PaymentRequestState> state,
                             base::WeakPtr<PaymentRequestDialogView> dialog);
  ~OrderSummaryViewController() override;

  // PaymentRequestSpec::Observer:
  void OnSpecUpdated() override;

  // PaymentRequestState::Observer:
  void OnGetAllPaymentAppsFinished() override {}
  void OnSelectedInformationChanged() override;

 private:
  // PaymentRequestSheetController:
  bool ShouldShowSecondaryButton() override;
  base::string16 GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;

  base::WeakPtrFactory<OrderSummaryViewController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OrderSummaryViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_ORDER_SUMMARY_VIEW_CONTROLLER_H_
