// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.TranslateUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the translate info included in onProvideAssistContent.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TranslateAssistContentTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TRANSLATE_PAGE = "/chrome/test/data/translate/fr_test.html";
    private static final String NON_TRANSLATE_PAGE = "/chrome/test/data/android/test.html";

    /**
     * Returns true if a test that requires internet access should be skipped due to an
     * out-of-process NetworkService. When the NetworkService is run out-of-process, a fake DNS
     * resolver is used that will fail to resolve any non-local names. crbug.com/1134812 is tracking
     * the changes to make the translate service mockable and remove the internet requirement.
     */
    private boolean shouldSkipDueToNetworkService() {
        return !ChromeFeatureList.isEnabled("NetworkServiceInProcess");
    }

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> TranslateBridge.setIgnoreMissingKeyForTesting(true));
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    public void testAssistContentDisabled() throws TimeoutException, ExecutionException {
        if (shouldSkipDueToNetworkService()) return;
        // Load a page that triggers the translate recommendation.
        final String url = mActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        mActivityTestRule.loadUrl(url);
        TranslateUtil.waitUntilTranslatable(mActivityTestRule.getActivity().getActivityTab());

        String structuredData = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateAssistContent.getTranslateDataForTab(
                                mActivityTestRule.getActivity().getActivityTab(),
                                /*isInOverviewMode=*/false));
        Assert.assertNull(structuredData);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    public void testAssistContentTranslatablePage()
            throws TimeoutException, ExecutionException, JSONException {
        if (shouldSkipDueToNetworkService()) return;
        // Load a page that triggers the translate recommendation.
        final String url = mActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        mActivityTestRule.loadUrl(url);
        TranslateUtil.waitUntilTranslatable(mActivityTestRule.getActivity().getActivityTab());

        String structuredData = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateAssistContent.getTranslateDataForTab(
                                mActivityTestRule.getActivity().getActivityTab(),
                                /*isInOverviewMode=*/false));

        JSONObject parsed = new JSONObject(structuredData);
        Assert.assertEquals(TranslateAssistContent.TYPE_VALUE,
                parsed.getString(TranslateAssistContent.TYPE_KEY));
        Assert.assertEquals(url, parsed.getString(TranslateAssistContent.URL_KEY));
        Assert.assertEquals("fr", parsed.getString(TranslateAssistContent.IN_LANGUAGE_KEY));
        Assert.assertEquals("en",
                parsed.getJSONObject(TranslateAssistContent.WORK_TRANSLATION_KEY)
                        .getString(TranslateAssistContent.IN_LANGUAGE_KEY));
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    public void testAssistContentTranslatedPage()
            throws TimeoutException, ExecutionException, JSONException {
        if (shouldSkipDueToNetworkService()) return;
        // Load a page that triggers the translate recommendation.
        final String url = mActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        mActivityTestRule.loadUrl(url);
        TranslateUtil.waitUntilTranslatable(mActivityTestRule.getActivity().getActivityTab());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateBridge.translateTabWhenReady(
                                mActivityTestRule.getActivity().getActivityTab()));

        // Can't wait on the Translate infobar state here because the target language tab is
        // selected before the translation is complete. Wait for the language to change instead.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(TranslateBridge.getCurrentLanguage(
                                       mActivityTestRule.getActivity().getActivityTab()),
                    Matchers.is("en"));
        });

        String structuredData = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateAssistContent.getTranslateDataForTab(
                                mActivityTestRule.getActivity().getActivityTab(),
                                /*isInOverviewMode=*/false));

        JSONObject parsed = new JSONObject(structuredData);
        Assert.assertEquals(TranslateAssistContent.TYPE_VALUE,
                parsed.getString(TranslateAssistContent.TYPE_KEY));
        Assert.assertEquals(url, parsed.getString(TranslateAssistContent.URL_KEY));
        Assert.assertEquals("en", parsed.getString(TranslateAssistContent.IN_LANGUAGE_KEY));
        Assert.assertEquals("fr",
                parsed.getJSONObject(TranslateAssistContent.TRANSLATION_OF_WORK_KEY)
                        .getString(TranslateAssistContent.IN_LANGUAGE_KEY));
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    public void testAssistContentNonTranslatePage()
            throws TimeoutException, ExecutionException, JSONException {
        if (shouldSkipDueToNetworkService()) return;
        // Load a page that can't be translated.
        final String url = mActivityTestRule.getTestServer().getURL(NON_TRANSLATE_PAGE);
        mActivityTestRule.loadUrl(url);

        String structuredData = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateAssistContent.getTranslateDataForTab(
                                mActivityTestRule.getActivity().getActivityTab(),
                                /*isInOverviewMode=*/false));

        JSONObject parsed = new JSONObject(structuredData);
        Assert.assertEquals(TranslateAssistContent.TYPE_VALUE,
                parsed.getString(TranslateAssistContent.TYPE_KEY));
        Assert.assertEquals(url, parsed.getString(TranslateAssistContent.URL_KEY));
        Assert.assertFalse(parsed.has(TranslateAssistContent.IN_LANGUAGE_KEY));
        Assert.assertFalse(parsed.has(TranslateAssistContent.TRANSLATION_OF_WORK_KEY));
        Assert.assertFalse(parsed.has(TranslateAssistContent.WORK_TRANSLATION_KEY));
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    public void testAssistContentOverviewMode() throws TimeoutException, ExecutionException {
        if (shouldSkipDueToNetworkService()) return;
        // Load a page that triggers the translate recommendation.
        final String url = mActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        mActivityTestRule.loadUrl(url);
        TranslateUtil.waitUntilTranslatable(mActivityTestRule.getActivity().getActivityTab());

        // Pretend we're in overview mode.
        String structuredData = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateAssistContent.getTranslateDataForTab(
                                mActivityTestRule.getActivity().getActivityTab(),
                                /*isInOverviewMode=*/true));
        Assert.assertNull(structuredData);
    }
}
