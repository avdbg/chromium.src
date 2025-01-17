// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/core/password_protection/metrics_util.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#import "components/safe_browsing/ios/password_protection/password_protection_service.h"
#include "components/sync/protocol/gaia_password_reuse.pb.h"

class ChromeBrowserState;
class GURL;
class PrefService;
class SafeBrowsingService;

namespace password_manager {
class PasswordStore;
}  // namespace password_manager

namespace safe_browsing {
class PasswordProtectionRequest;
}  // namespace safe_browsing

namespace web {
class WebState;
}  // namespace web

class ChromePasswordProtectionService
    : public safe_browsing::PasswordProtectionService,
      public KeyedService {
 public:
  ChromePasswordProtectionService(SafeBrowsingService* sb_service,
                                  ChromeBrowserState* browser_state);
  ~ChromePasswordProtectionService() override;

  // PasswordProtectionServiceBase:
  void RequestFinished(
      safe_browsing::PasswordProtectionRequest* request,
      safe_browsing::RequestOutcome outcome,
      std::unique_ptr<safe_browsing::LoginReputationClientResponse> response)
      override;

  void ShowModalWarning(
      safe_browsing::PasswordProtectionRequest* request,
      safe_browsing::LoginReputationClientResponse::VerdictType verdict_type,
      const std::string& verdict_token,
      safe_browsing::ReusedPasswordAccountType password_type) override;

  void MaybeReportPasswordReuseDetected(
      safe_browsing::PasswordProtectionRequest* request,
      const std::string& username,
      safe_browsing::PasswordType password_type,
      bool is_phishing_url) override;

  void ReportPasswordChanged() override;

  void FillReferrerChain(
      const GURL& event_url,
      SessionID event_tab_id,  // SessionID::InvalidValue()
                               // if tab not available.
      safe_browsing::LoginReputationClientRequest::Frame* frame) override;

  void SanitizeReferrerChain(
      safe_browsing::ReferrerChain* referrer_chain) override;

  void PersistPhishedSavedPasswordCredential(
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials) override;

  void RemovePhishedSavedPasswordCredential(
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials) override;

  safe_browsing::RequestOutcome GetPingNotSentReason(
      safe_browsing::LoginReputationClientRequest::TriggerType trigger_type,
      const GURL& url,
      safe_browsing::ReusedPasswordAccountType password_type) override;

  void RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
      bool all_history,
      const history::URLRows& deleted_rows) override;

  bool UserClickedThroughSBInterstitial(
      safe_browsing::PasswordProtectionRequest* request) override;

  safe_browsing::PasswordProtectionTrigger
  GetPasswordProtectionWarningTriggerPref(
      safe_browsing::ReusedPasswordAccountType password_type) const override;

  safe_browsing::LoginReputationClientRequest::UrlDisplayExperiment
  GetUrlDisplayExperiment() const override;

  AccountInfo GetAccountInfo() const override;

  AccountInfo GetSignedInNonSyncAccount(
      const std::string& username) const override;

  safe_browsing::LoginReputationClientRequest::PasswordReuseEvent::
      SyncAccountType
      GetSyncAccountType() const override;

  bool CanShowInterstitial(
      safe_browsing::ReusedPasswordAccountType password_type,
      const GURL& main_frame_url) override;

  bool IsURLAllowlistedForPasswordEntry(const GURL& url) const override;

  bool IsInPasswordAlertMode(
      safe_browsing::ReusedPasswordAccountType password_type) override;

  bool CanSendSamplePing() override;

  bool IsPingingEnabled(
      safe_browsing::LoginReputationClientRequest::TriggerType trigger_type,
      safe_browsing::ReusedPasswordAccountType password_type) override;

  bool IsIncognito() override;

  bool IsExtendedReporting() override;

  bool IsPrimaryAccountSyncing() const override;

  bool IsPrimaryAccountSignedIn() const override;

  bool IsPrimaryAccountGmail() const override;

  bool IsOtherGaiaAccountGmail(const std::string& username) const override;

  bool IsInExcludedCountry() override;

  // PasswordProtectionService override.
  void MaybeStartProtectedPasswordEntryRequest(
      web::WebState* web_state,
      const GURL& main_frame_url,
      const std::string& username,
      safe_browsing::PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      bool password_field_exists,
      safe_browsing::PasswordProtectionService::ShowWarningCallback
          show_warning_callback) override;

  // PasswordProtectionService override.
  void MaybeLogPasswordReuseLookupEvent(
      web::WebState* web_state,
      safe_browsing::RequestOutcome outcome,
      safe_browsing::PasswordType password_type,
      const safe_browsing::LoginReputationClientResponse* response) override;

  // PasswordProtectionService override.
  void MaybeLogPasswordReuseDetectedEvent(web::WebState* web_state) override;

  // Records a Chrome Sync event with the result of the user's interaction with
  // the warning dialog.
  void MaybeLogPasswordReuseDialogInteraction(
      int64_t navigation_id,
      sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction::
          InteractionResult interaction_result);

  // Gets the detailed warning text that should show in the modal warning
  // dialog. |placeholder_offsets| are the start points/indices of the
  // placeholders that are passed into the resource string. It is only set for
  // saved passwords.
  base::string16 GetWarningDetailText(
      safe_browsing::ReusedPasswordAccountType password_type,
      std::vector<size_t>* placeholder_offsets) const;

  // Gets the warning text for saved password reuse warnings.
  // |placeholder_offsets| are the start points/indices of the placeholders that
  // are passed into the resource string.
  base::string16 GetWarningDetailTextForSavedPasswords(
      std::vector<size_t>* placeholder_offsets) const;

  // Gets the warning text of the saved password reuse warnings that tells the
  // user to check their saved passwords. |placeholder_offsets| are the start
  // points/indices of the placeholders that are passed into the resource
  // string.
  base::string16 GetWarningDetailTextToCheckSavedPasswords(
      std::vector<size_t>* placeholder_offsets) const;

  // Get placeholders for the warning detail text for saved password reuse
  // warnings.
  std::vector<base::string16> GetPlaceholdersForSavedPasswordWarningText()
      const;

  // Creates, starts, and tracks a new request.
  void StartRequest(
      web::WebState* web_state,
      const GURL& main_frame_url,
      const std::string& username,
      safe_browsing::PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      safe_browsing::LoginReputationClientRequest::TriggerType trigger_type,
      bool password_field_exists,
      safe_browsing::PasswordProtectionService::ShowWarningCallback
          show_warning_callback);

  // Called when user interacts with password protection UIs.
  void OnUserAction(web::WebState* web_state,
                    safe_browsing::ReusedPasswordAccountType password_type,
                    safe_browsing::WarningAction action);

 protected:
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifySendsPingForAboutBlank);

  void FillUserPopulation(
      safe_browsing::LoginReputationClientRequest* request_proto) override;

 private:
  // Returns true if the |web_state| is already showing a warning dialog.
  bool IsModalWarningShowingInWebState(web::WebState* web_state);
  // Removes all warning requests for |web_state|.
  void RemoveWarningRequestsByWebState(web::WebState* web_state);

  password_manager::PasswordStore* GetStoreForReusedCredential(
      const password_manager::MatchingReusedCredential& reused_credential);

  // Returns the profile PasswordStore associated with this instance.
  password_manager::PasswordStore* GetProfilePasswordStore() const;

  // Returns the GAIA-account-scoped PasswordStore associated with this
  // instance. The account password store contains passwords stored in the
  // account and is accessible only when the user is signed in and non syncing.
  password_manager::PasswordStore* GetAccountPasswordStore() const;

  // Gets prefs associated with |browser_state_|.
  PrefService* GetPrefs() const;

  // Returns whether |browser_state_| has safe browsing service enabled.
  bool IsSafeBrowsingEnabled();

  // Lookup for a callback for showing a warning for a given request.
  std::map<safe_browsing::PasswordProtectionRequest*,
           safe_browsing::PasswordProtectionService::ShowWarningCallback>
      show_warning_callbacks_;

  ChromeBrowserState* browser_state_;

  base::WeakPtrFactory<ChromePasswordProtectionService> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
