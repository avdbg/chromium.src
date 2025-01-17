// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

namespace {

base::test::ScopedFeatureList::FeatureAndParams probability_zero{
    features::kHappinessTrackingSurveysForDesktop,
    {{"probability", "0.000"}}};
base::test::ScopedFeatureList::FeatureAndParams probability_one{
    features::kHappinessTrackingSurveysForDesktop,
    {{"probability", "1.000"},
     {"survey", kHatsSurveyTriggerSatisfaction},
     {"en_site_id", "test_site_id"}}};
base::test::ScopedFeatureList::FeatureAndParams settings_probability_one{
    features::kHappinessTrackingSurveysForDesktopSettings,
    {{"probability", "1.000"},
     {"survey", kHatsSurveyTriggerSettings},
     {"en_site_id", "test_site_id"}}};

class ScopedSetMetricsConsent {
 public:
  // Enables or disables metrics consent based off of |consent|.
  explicit ScopedSetMetricsConsent(bool consent) : consent_(consent) {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &consent_);
  }

  ~ScopedSetMetricsConsent() {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

 private:
  const bool consent_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetMetricsConsent);
};

class HatsServiceBrowserTestBase : public InProcessBrowserTest {
 protected:
  explicit HatsServiceBrowserTestBase(
      std::vector<base::test::ScopedFeatureList::FeatureAndParams>
          enabled_features)
      : enabled_features_(enabled_features) {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features_, {});
  }

  HatsServiceBrowserTestBase() = default;

  ~HatsServiceBrowserTestBase() override = default;

  HatsService* GetHatsService() {
    HatsService* service =
        HatsServiceFactory::GetForProfile(browser()->profile(), true);
    return service;
  }

  void SetMetricsConsent(bool consent) {
    scoped_metrics_consent_.emplace(consent);
  }

  bool HatsNextDialogCreated() {
    return GetHatsService()->hats_next_dialog_exists_for_testing();
  }

 private:
  base::Optional<ScopedSetMetricsConsent> scoped_metrics_consent_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::vector<base::test::ScopedFeatureList::FeatureAndParams>
      enabled_features_;

  DISALLOW_COPY_AND_ASSIGN(HatsServiceBrowserTestBase);
};

class HatsServiceProbabilityZero : public HatsServiceBrowserTestBase {
 protected:
  HatsServiceProbabilityZero()
      : HatsServiceBrowserTestBase({probability_zero}) {}

  ~HatsServiceProbabilityZero() override = default;

  DISALLOW_COPY_AND_ASSIGN(HatsServiceProbabilityZero);
};

class HatsServiceProbabilityOne : public HatsServiceBrowserTestBase {
 protected:
  HatsServiceProbabilityOne()
      : HatsServiceBrowserTestBase(
            {probability_one, settings_probability_one}) {}

  ~HatsServiceProbabilityOne() override = default;

 private:
  void SetUpOnMainThread() override {
    // Set the profile creation time to be old enough to ensure triggering.
    browser()->profile()->SetCreationTimeForTesting(
        base::Time::Now() - base::TimeDelta::FromDays(45));
  }

  void TearDownOnMainThread() override {
    GetHatsService()->SetSurveyMetadataForTesting({});
  }

  DISALLOW_COPY_AND_ASSIGN(HatsServiceProbabilityOne);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HatsServiceBrowserTestBase, BubbleNotShownOnDefault) {
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityZero, NoShow) {
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, NoShowConsentNotGiven) {
  SetMetricsConsent(false);
  ASSERT_FALSE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, TriggerMismatchNoShow) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey("nonexistent-trigger");
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, AlwaysShow) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, AlsoShowsSettingsSurvey) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SameMajorVersionNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_major_version = version_info::GetVersion().components()[0];
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, DifferentMajorVersionShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_major_version = 42;
  ASSERT_NE(42u, version_info::GetVersion().components()[0]);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       SurveyStartedBeforeRequiredElapsedTimeNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_survey_started_time = base::Time::Now();
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       SurveyStartedBeforeElapsedTimeBetweenAnySurveys) {
  SetMetricsConsent(true);
  base::HistogramTester histogram_tester;
  HatsService::SurveyMetadata metadata;
  metadata.any_last_survey_started_time = base::Time::Now();
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsNextDialogCreated());
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsService::ShouldShowSurveyReasons::kNoAnyLastSurveyTooRecent, 1);
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, ProfileTooYoungToShow) {
  SetMetricsConsent(true);
  // Set creation time to only 15 days.
  static_cast<ProfileImpl*>(browser()->profile())
      ->SetCreationTimeForTesting(base::Time::Now() -
                                  base::TimeDelta::FromDays(15));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, ProfileOldEnoughToShow) {
  SetMetricsConsent(true);
  // Set creation time to 31 days. This is just past the threshold.
  static_cast<ProfileImpl*>(browser()->profile())
      ->SetCreationTimeForTesting(base::Time::Now() -
                                  base::TimeDelta::FromDays(31));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, IncognitoModeDisabledNoShow) {
  SetMetricsConsent(true);
  // Disable incognito mode for this profile.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(prefs::kIncognitoModeAvailability,
                           IncognitoModePrefs::DISABLED);
  EXPECT_EQ(IncognitoModePrefs::DISABLED,
            IncognitoModePrefs::GetAvailability(pref_service));

  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, CheckedWithinADayNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_survey_check_time =
      base::Time::Now() - base::TimeDelta::FromHours(23);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, CheckedAfterADayToShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_survey_check_time =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SurveyAlreadyFullNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.is_survey_full = true;
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, LaunchDelayedSurvey) {
  SetMetricsConsent(true);
  EXPECT_TRUE(
      GetHatsService()->LaunchDelayedSurvey(kHatsSurveyTriggerSatisfaction, 0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       LaunchDelayedSurveyForWebContents) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, DisallowsEmptyWebContents) {
  SetMetricsConsent(true);
  EXPECT_FALSE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, nullptr, 0));
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(
    HatsServiceProbabilityOne,
    AllowsMultipleDelayedSurveyRequestsDifferentWebContents) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0));
  base::RunLoop().RunUntilIdle();
  chrome::AddTabAt(browser(), GURL(), -1, true);
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction,
      browser()->tab_strip_model()->GetActiveWebContents(), 0));
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       DisallowsSameDelayedSurveyForWebContentsRequests) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0));
  EXPECT_FALSE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       ReleasesPendingTaskAfterFulfilling) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetHatsService()->HasPendingTasks());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, VisibleWebContentsShow) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, InvisibleWebContentsNoShow) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  EXPECT_FALSE(HatsNextDialogCreated());
}

// Check that once a HaTS Next dialog has been created, ShouldShowSurvey
// returns false until the service has been informed the dialog was closed.
IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SingleHatsNextDialog) {
  SetMetricsConsent(true);
  EXPECT_TRUE(
      GetHatsService()->ShouldShowSurvey(kHatsSurveyTriggerSatisfaction));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);

  // Clear any metadata that would prevent another survey from being displayed.
  GetHatsService()->SetSurveyMetadataForTesting({});

  // At this point a HaTS Next dialog is created and is attempting to contact
  // the wrapper website (which will fail as requests to non-localhost addresses
  // are disallowed in browser tests). Regardless of the outcome of the network
  // request, the dialog waits for a timeout posted to the UI thread before
  // closing itself. Since this test is also on the UI thread, these checks,
  // which rely on the dialog still being open, will not race.
  EXPECT_FALSE(
      GetHatsService()->ShouldShowSurvey(kHatsSurveyTriggerSatisfaction));

  // Inform the service directly that the dialog has been closed.
  GetHatsService()->HatsNextDialogClosed();
  EXPECT_TRUE(
      GetHatsService()->ShouldShowSurvey(kHatsSurveyTriggerSatisfaction));
}

// Check that launching a HaTS Next survey records a survey check time
IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SurveyCheckTimeRecorded) {
  SetMetricsConsent(true);

  // Clear any existing survey metadata.
  GetHatsService()->SetSurveyMetadataForTesting({});

  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);

  HatsService::SurveyMetadata metadata;
  GetHatsService()->GetSurveyMetadataForTesting(&metadata);
  EXPECT_TRUE(metadata.last_survey_check_time.has_value());
}
