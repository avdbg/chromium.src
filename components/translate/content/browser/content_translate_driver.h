// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class NavigationController;
class WebContents;
}  // namespace content

namespace language {
class UrlLanguageHistogram;
}  // namespace language

namespace translate {

struct LanguageDetectionDetails;
class TranslateManager;
class TranslateModelService;

// Content implementation of TranslateDriver.
class ContentTranslateDriver : public TranslateDriver,
                               public translate::mojom::ContentTranslateDriver,
                               public content::WebContentsObserver {
 public:
  class TranslationObserver : public base::CheckedObserver {
   public:
    // Handles when the value of IsPageTranslated is changed.
    virtual void OnIsPageTranslatedChanged(content::WebContents* source) {}

    // Handles when the value of translate_enabled is changed.
    virtual void OnTranslateEnabledChanged(content::WebContents* source) {}

    // Called when the page has been translated.
    virtual void OnPageTranslated(const std::string& original_lang,
                                  const std::string& translated_lang,
                                  translate::TranslateErrors::Type error_type) {
    }
  };

  ContentTranslateDriver(content::NavigationController* nav_controller,
                         language::UrlLanguageHistogram* url_language_histogram,
                         TranslateModelService* translate_model_service);
  ~ContentTranslateDriver() override;

  // Adds or removes observers.
  void AddTranslationObserver(TranslationObserver* observer);
  void RemoveTranslationObserver(TranslationObserver* observer);

  // Number of attempts before waiting for a page to be fully reloaded.
  void set_translate_max_reload_attempts(int attempts) {
    max_reload_check_attempts_ = attempts;
  }

  // Sets the TranslateManager associated with this driver.
  void set_translate_manager(TranslateManager* manager) {
    translate_manager_ = manager;
  }

  // Initiates translation once the page is finished loading.
  void InitiateTranslation(const std::string& page_lang, int attempt);

  // TranslateDriver methods.
  void OnIsPageTranslatedChanged() override;
  void OnTranslateEnabledChanged() override;
  bool IsLinkNavigation() override;
  void TranslatePage(int page_seq_no,
                     const std::string& translate_script,
                     const std::string& source_lang,
                     const std::string& target_lang) override;
  void RevertTranslation(int page_seq_no) override;
  bool IsIncognito() override;
  const std::string& GetContentsMimeType() override;
  const GURL& GetLastCommittedURL() override;
  const GURL& GetVisibleURL() override;
  ukm::SourceId GetUkmSourceId() override;
  bool HasCurrentPage() override;
  void OpenUrlInNewTab(const GURL& url) override;

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void OnPageTranslated(bool cancelled,
                        const std::string& original_lang,
                        const std::string& translated_lang,
                        TranslateErrors::Type error_type);

  // Adds a receiver in |receivers_| for the passed |receiver|.
  void AddReceiver(
      mojo::PendingReceiver<translate::mojom::ContentTranslateDriver> receiver);

  // Called when a page has been loaded and can be potentially translated.
  void RegisterPage(
      mojo::PendingRemote<translate::mojom::TranslateAgent> translate_agent,
      const translate::LanguageDetectionDetails& details,
      bool page_level_translation_critiera_met) override;

  // translate::mojom::ContentTranslateDriver implementation:
  void GetLanguageDetectionModel(
      GetLanguageDetectionModelCallback callback) override;

 protected:
  const base::ObserverList<TranslationObserver, true>& translation_observers()
      const {
    return translation_observers_;
  }

  TranslateManager* translate_manager() const { return translate_manager_; }

  language::UrlLanguageHistogram* language_histogram() const {
    return language_histogram_;
  }

  bool IsAutoHrefTranslateAllOriginsEnabled() const;

 private:
  void OnPageAway(int page_seq_no);

  void InitiateTranslationIfReload(
      content::NavigationHandle* navigation_handle);

  // Runs the provided callback with the loaded model file
  // to pass it to the connected translate agent.
  void OnLanguageDetectionModelFile(GetLanguageDetectionModelCallback callback,
                                    base::File model_file);

  // The navigation controller of the tab we are associated with.
  content::NavigationController* navigation_controller_;

  TranslateManager* translate_manager_;

  base::ObserverList<TranslationObserver, true> translation_observers_;

  // Max number of attempts before checking if a page has been reloaded.
  int max_reload_check_attempts_;

  // Records mojo connections with all current alive pages.
  int next_page_seq_no_;
  // mojo::Remote<TranslateAgent> is the connection between this driver and a
  // TranslateAgent (which are per RenderFrame). Each TranslateAgent has a
  // |binding_| member, representing the other end of this pipe.
  std::map<int, mojo::Remote<mojom::TranslateAgent>> translate_agents_;

  // Histogram to be notified about detected language of every page visited. Not
  // owned here.
  language::UrlLanguageHistogram* const language_histogram_;

  // ContentTranslateDriver is a singleton per web contents but multiple render
  // frames may be contained in a single web contents. TranslateAgents get the
  // other end of this receiver in the form of a ContentTranslateDriver.
  mojo::ReceiverSet<translate::mojom::ContentTranslateDriver> receivers_;

  // Time when the navigation was finished (i.e., DidFinishNavigation
  // in the main frame). This is used to know a duration time to when the
  // page language is determined.
  base::TimeTicks finish_navigation_time_;

  // The service that provides the model files needed for translate. Not owned
  // but guaranteed to outlive |this|.
  TranslateModelService* const translate_model_service_;

  base::WeakPtrFactory<ContentTranslateDriver> weak_pointer_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ContentTranslateDriver);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_
