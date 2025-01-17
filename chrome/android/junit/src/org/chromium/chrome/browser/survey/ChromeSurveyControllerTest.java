// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.WebContents;

/**
 * Unit tests for ChromeSurveyController.java.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeSurveyControllerTest {
    private static final String STUDY_NAME = "HorizontalTabSwitcherStudyName";
    private static final String GROUP_NAME = "HorizontalTabSwitcherGroupName";

    private TestChromeSurveyController mTestController;
    private RiggedSurveyController mRiggedController;
    private SharedPreferencesManager mSharedPreferences;

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    Tab mTab;

    @Mock
    WebContents mWebContents;

    @Mock
    TabModelSelector mSelector;

    @Before
    public void before() {
        MockitoAnnotations.initMocks(this);

        mTestController = new TestChromeSurveyController();
        mTestController.setTabModelSelector(mSelector);
        mSharedPreferences = SharedPreferencesManager.getInstance();
        Assert.assertNull("Tab should be null", mTestController.getLastTabInfobarShown());
    }

    @Test
    public void testInfoBarDisplayedBefore() {
        Assert.assertFalse(
                mSharedPreferences.contains(ChromePreferenceKeys.SURVEY_INFO_BAR_DISPLAYED));
        Assert.assertFalse(mTestController.hasInfoBarBeenDisplayed());
        mSharedPreferences.writeLong(
                ChromePreferenceKeys.SURVEY_INFO_BAR_DISPLAYED, System.currentTimeMillis());
        Assert.assertTrue(mTestController.hasInfoBarBeenDisplayed());
    }

    @Test
    public void testValidTab() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isIncognito();

        Assert.assertTrue(mTestController.isValidTabForSurvey(mTab));

        verify(mTab, atLeastOnce()).getWebContents();
        verify(mTab, atLeastOnce()).isIncognito();
    }

    @Test
    public void testNullTab() {
        Assert.assertFalse(mTestController.isValidTabForSurvey(null));
    }

    @Test
    public void testIncognitoTab() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(true).when(mTab).isIncognito();

        Assert.assertFalse(mTestController.isValidTabForSurvey(mTab));

        verify(mTab, atLeastOnce()).isIncognito();
    }

    @Test
    public void testTabWithNoWebContents() {
        doReturn(null).when(mTab).getWebContents();

        Assert.assertFalse(mTestController.isValidTabForSurvey(mTab));

        verify(mTab, atLeastOnce()).getWebContents();
        verify(mTab, never()).isIncognito();
    }

    @Test
    public void testSurveyAvailableWebContentsLoaded() {
        doReturn(mTab).when(mSelector).getCurrentTab();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isIncognito();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(false).when(mWebContents).isLoading();

        mTestController.onSurveyAvailable(null);
        Assert.assertEquals("Tabs should be equal", mTab, mTestController.getLastTabInfobarShown());

        verify(mSelector, atLeastOnce()).getCurrentTab();
        verify(mTab, atLeastOnce()).isIncognito();
        verify(mTab, atLeastOnce()).isUserInteractable();
        verify(mTab, atLeastOnce()).isLoading();
    }

    @Test
    public void testShowInfoBarTabApplicable() {
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(false).when(mTab).isLoading();

        mTestController.showInfoBarIfApplicable(mTab, null, null);
        Assert.assertEquals("Tabs should be equal", mTab, mTestController.getLastTabInfobarShown());
        verify(mTab, atLeastOnce()).isUserInteractable();
        verify(mTab, atLeastOnce()).isLoading();
    }

    @Test
    public void testShowInfoBarTabNotApplicable() {
        doReturn(false).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isLoading();

        mTestController.showInfoBarIfApplicable(mTab, null, null);
        Assert.assertNull("Tab should be null", mTestController.getLastTabInfobarShown());
        verify(mTab, atLeastOnce()).isUserInteractable();
    }

    @Test
    public void testSurveyAvailableNullTab() {
        doReturn(null).when(mSelector).getCurrentTab();

        mTestController.onSurveyAvailable(null);
        Assert.assertNull("Tab should be null", mTestController.getLastTabInfobarShown());
        verify(mSelector).addObserver(any());
    }

    @Test
    public void testEligibilityRolledYesterday() {
        mRiggedController = new RiggedSurveyController(0, 5, 10);
        mSharedPreferences.writeInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, 4);
        Assert.assertTrue(
                "Random selection should be true", mRiggedController.isRandomlySelectedForSurvey());
    }

    @Test
    public void testEligibilityRollingTwiceSameDay() {
        mRiggedController = new RiggedSurveyController(0, 5, 10);
        mSharedPreferences.writeInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, 5);
        Assert.assertFalse("Random selection should be false",
                mRiggedController.isRandomlySelectedForSurvey());
    }

    @Test
    public void testEligibilityFirstTimeRollingQualifies() {
        mRiggedController = new RiggedSurveyController(0, 5, 10);
        Assert.assertFalse(
                mSharedPreferences.contains(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED));
        Assert.assertTrue(
                "Random selection should be true", mRiggedController.isRandomlySelectedForSurvey());
        Assert.assertEquals("Numbers should match", 5,
                mSharedPreferences.readInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, -1));
    }

    @Test
    public void testEligibilityFirstTimeRollingDoesNotQualify() {
        mRiggedController = new RiggedSurveyController(5, 1, 10);
        Assert.assertFalse(
                mSharedPreferences.contains(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED));
        Assert.assertFalse(
                "Random selection should be true", mRiggedController.isRandomlySelectedForSurvey());
        Assert.assertEquals("Numbers should match", 1,
                mSharedPreferences.readInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, -1));
    }

    class RiggedSurveyController extends ChromeSurveyController {
        private int mRandomNumberToReturn;
        private int mDayOfYear;
        private int mMaxNumber;

        RiggedSurveyController(int randomNumberToReturn, int dayOfYear, int maxNumber) {
            super();
            mRandomNumberToReturn = randomNumberToReturn;
            mDayOfYear = dayOfYear;
            mMaxNumber = maxNumber;
        }

        @Override
        int getRandomNumberUpTo(int max) {
            return mRandomNumberToReturn;
        }

        @Override
        int getDayOfYear() {
            return mDayOfYear;
        }

        @Override
        int getMaxNumber() {
            return mMaxNumber;
        }
    }

    class TestChromeSurveyController extends ChromeSurveyController {
        private Tab mTab;

        public TestChromeSurveyController() {
            super();
        }

        @Override
        void showSurveyInfoBar(Tab tab, String siteId) {
            mTab = tab;
        }

        public Tab getLastTabInfobarShown() {
            return mTab;
        }
    }
}
