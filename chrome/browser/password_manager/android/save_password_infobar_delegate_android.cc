// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_password_infobar_delegate_android.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/values.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/android/infobars/save_password_infobar.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

// static
void SavePasswordInfoBarDelegate::Create(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  base::Optional<AccountInfo> account_info =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id);
  bool is_single_account_user =
      identity_manager->GetAccountsWithRefreshTokens().size() == 1;

  // is_smartlock_branding_enabled indicates whether the user is syncing
  // passwords to their Google Account.
  bool is_smartlock_branding_enabled =
      password_bubble_experiment::IsSmartLockUser(sync_service);
  bool should_show_account_footer =
      (is_smartlock_branding_enabled &&
       base::FeatureList::IsEnabled(
           autofill::features::
               kAutofillEnableInfoBarAccountIndicationFooterForSyncUsers)) &&
      (!is_single_account_user ||
       base::FeatureList::IsEnabled(
           autofill::features::
               kAutofillEnableInfoBarAccountIndicationFooterForSingleAccountUsers)) &&
      account_info.has_value();
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->AddInfoBar(std::make_unique<SavePasswordInfoBar>(
      base::WrapUnique(
          new SavePasswordInfoBarDelegate(web_contents, std::move(form_to_save),
                                          is_smartlock_branding_enabled)),
      should_show_account_footer ? account_info : base::nullopt));
}

SavePasswordInfoBarDelegate::~SavePasswordInfoBarDelegate() {
  password_manager::metrics_util::LogSaveUIDismissalReason(
      infobar_response_, /*user_state=*/base::nullopt);
  if (form_to_save_->WasUnblacklisted()) {
    password_manager::metrics_util::LogSaveUIDismissalReasonAfterUnblacklisting(
        infobar_response_);
  }
  if (auto* recorder = form_to_save_->GetMetricsRecorder()) {
    recorder->RecordUIDismissalReason(infobar_response_);
  }
}

SavePasswordInfoBarDelegate::SavePasswordInfoBarDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool is_smartlock_branding_enabled)
    : PasswordManagerInfoBarDelegate(),
      form_to_save_(std::move(form_to_save)),
      infobar_response_(password_manager::metrics_util::NO_DIRECT_INTERACTION) {
  PasswordTitleType type =
      form_to_save_->GetPendingCredentials().federation_origin.opaque()
          ? PasswordTitleType::SAVE_PASSWORD
          : PasswordTitleType::SAVE_ACCOUNT;
  SetMessage(GetSavePasswordDialogTitleText(
      web_contents->GetVisibleURL(),
      url::Origin::Create(form_to_save_->GetURL()), type));

  if (type == PasswordTitleType::SAVE_PASSWORD &&
      is_smartlock_branding_enabled) {
    SetDetailsMessage(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD_FOOTER));
  }

  if (auto* recorder = form_to_save_->GetMetricsRecorder()) {
    recorder->RecordPasswordBubbleShown(
        form_to_save_->GetCredentialSource(),
        password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
SavePasswordInfoBarDelegate::GetIdentifier() const {
  return SAVE_PASSWORD_INFOBAR_DELEGATE_MOBILE;
}

void SavePasswordInfoBarDelegate::InfoBarDismissed() {
  DCHECK(form_to_save_.get());
  infobar_response_ = password_manager::metrics_util::CLICKED_CANCEL;
}

base::string16 SavePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_PASSWORD_MANAGER_SAVE_BUTTON
                                       : IDS_PASSWORD_MANAGER_BLOCKLIST_BUTTON);
}

bool SavePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save_.get());
  form_to_save_->Save();
  infobar_response_ = password_manager::metrics_util::CLICKED_ACCEPT;
  return true;
}

bool SavePasswordInfoBarDelegate::Cancel() {
  DCHECK(form_to_save_.get());
  form_to_save_->PermanentlyBlacklist();
  infobar_response_ = password_manager::metrics_util::CLICKED_NEVER;
  return true;
}
