// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_bubble_handler_impl.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/autofill/payments/save_upi_bubble.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_failure_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_manage_cards_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_offer_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_upi_offer_bubble_views.h"
#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace autofill {

AutofillBubbleHandlerImpl::AutofillBubbleHandlerImpl(
    Browser* browser,
    ToolbarButtonProvider* toolbar_button_provider)
    : browser_(browser), toolbar_button_provider_(toolbar_button_provider) {
  if (browser->profile()) {
    personal_data_manager_observation_.Observe(
        PersonalDataManagerFactory::GetForProfile(
            browser->profile()->GetOriginalProfile()));
  }
  if (toolbar_button_provider_->GetAvatarToolbarButton()) {
    avatar_toolbar_button_observation_.Observe(
        toolbar_button_provider_->GetAvatarToolbarButton());
  }
}

AutofillBubbleHandlerImpl::~AutofillBubbleHandlerImpl() = default;

// TODO(crbug.com/1061633): Clean up this two functions and add helper for
// shared code.
AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    SaveCardBubbleController* controller,
    bool is_user_gesture) {
  BubbleType bubble_type = controller->GetBubbleType();
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kSaveCard);
  views::View* anchor_view =
      toolbar_button_provider_->GetAnchorView(PageActionIconType::kSaveCard);

  SaveCardBubbleViews* bubble = nullptr;
  switch (bubble_type) {
    case BubbleType::LOCAL_SAVE:
    case BubbleType::UPLOAD_SAVE:
      bubble =
          new SaveCardOfferBubbleViews(anchor_view, web_contents, controller);
      break;
    case BubbleType::MANAGE_CARDS:
      bubble = new SaveCardManageCardsBubbleViews(anchor_view, web_contents,
                                                  controller);
      break;
    case BubbleType::FAILURE:
      bubble =
          new SaveCardFailureBubbleViews(anchor_view, web_contents, controller);
      break;
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::INACTIVE:
      break;
  }
  DCHECK(bubble);

  DCHECK(icon_view);
  bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                               : LocationBarBubbleDelegateView::AUTOMATIC);
  return bubble;
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowLocalCardMigrationBubble(
    content::WebContents* web_contents,
    LocalCardMigrationBubbleController* controller,
    bool is_user_gesture) {
  LocalCardMigrationBubbleViews* bubble = new LocalCardMigrationBubbleViews(
      toolbar_button_provider_->GetAnchorView(
          PageActionIconType::kLocalCardMigration),
      web_contents, controller);

  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kLocalCardMigration);
  DCHECK(icon_view);
  bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                               : LocationBarBubbleDelegateView::AUTOMATIC);
  return bubble;
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowOfferNotificationBubble(
    content::WebContents* web_contents,
    OfferNotificationBubbleController* controller,
    bool is_user_gesture) {
  views::View* anchor_view = toolbar_button_provider_->GetAnchorView(
      PageActionIconType::kPaymentsOfferNotification);
  OfferNotificationBubbleViews* bubble =
      new OfferNotificationBubbleViews(anchor_view, web_contents, controller);

  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kPaymentsOfferNotification);
  DCHECK(icon_view);
  bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->ShowForReason(is_user_gesture
                            ? OfferNotificationBubbleViews::USER_GESTURE
                            : OfferNotificationBubbleViews::AUTOMATIC);
  return bubble;
}

SaveUPIBubble* AutofillBubbleHandlerImpl::ShowSaveUPIBubble(
    content::WebContents* web_contents,
    SaveUPIBubbleController* controller) {
  views::View* anchor_view =
      toolbar_button_provider_->GetAnchorView(PageActionIconType::kSaveCard);
  SaveUPIOfferBubbleViews* bubble =
      new SaveUPIOfferBubbleViews(anchor_view, web_contents, controller);

  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kSaveCard);
  DCHECK(icon_view);
  bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show();
  return bubble;
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowSaveAddressProfileBubble(
    content::WebContents* web_contents,
    SaveAddressProfileBubbleController* controller,
    bool is_user_gesture) {
  views::View* anchor_view = toolbar_button_provider_->GetAnchorView(
      PageActionIconType::kSaveAutofillAddress);
  SaveAddressProfileView* bubble =
      new SaveAddressProfileView(anchor_view, web_contents, controller);
  DCHECK(bubble);
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kSaveAutofillAddress);
  DCHECK(icon_view);
  bubble->SetHighlightedButton(icon_view);
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                               : LocationBarBubbleDelegateView::AUTOMATIC);
  return bubble;
}

void AutofillBubbleHandlerImpl::OnPasswordSaved() {
  if (base::FeatureList::IsEnabled(
          features::kAutofillCreditCardUploadFeedback)) {
    ShowAvatarHighlightAnimation();
  }
}

void AutofillBubbleHandlerImpl::OnCreditCardSaved(
    bool should_show_sign_in_promo_if_applicable) {
  should_show_sign_in_promo_if_applicable_ =
      should_show_sign_in_promo_if_applicable;
  ShowAvatarHighlightAnimation();
}

void AutofillBubbleHandlerImpl::OnAvatarHighlightAnimationFinished() {
  if (should_show_sign_in_promo_if_applicable_) {
    should_show_sign_in_promo_if_applicable_ = false;
    chrome::ExecuteCommand(
        browser_, IDC_SHOW_SAVE_LOCAL_CARD_SIGN_IN_PROMO_IF_APPLICABLE);
  }
}

void AutofillBubbleHandlerImpl::ShowAvatarHighlightAnimation() {
  AvatarToolbarButton* avatar =
      toolbar_button_provider_->GetAvatarToolbarButton();
  if (avatar)
    avatar->ShowAvatarHighlightAnimation();
}

}  // namespace autofill
