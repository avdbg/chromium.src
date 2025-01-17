// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link SafeBrowsingSettingsFragment}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@Features.EnableFeatures({ChromeFeatureList.SAFE_BROWSING_SECTION_UI})
public class SafeBrowsingSettingsFragmentTest {
    // clang-format on
    private static final String ASSERT_SAFE_BROWSING_STATE_RADIO_BUTTON_GROUP =
            "Incorrect Safe Browsing state in the radio button group.";
    private static final String ASSERT_RADIO_BUTTON_CHECKED =
            "Incorrect radio button checked state.";
    private static final String ASSERT_SAFE_BROWSING_STATE_NATIVE =
            "Incorrect Safe Browsing state from native.";

    @Rule
    public SettingsActivityTestRule<SafeBrowsingSettingsFragment> mTestRule =
            new SettingsActivityTestRule<>(SafeBrowsingSettingsFragment.class);

    @Mock
    private SettingsLauncher mSettingsLauncher;

    @Mock
    private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    private SafeBrowsingSettingsFragment mSafeBrowsingSettingsFragment;
    private RadioButtonGroupSafeBrowsingPreference mSafeBrowsingPreference;
    private TextMessagePreference mManagedTextPreference;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    private void launchSettingsActivity() {
        mTestRule.startSettingsActivity();
        mSafeBrowsingSettingsFragment = mTestRule.getFragment();
        mSafeBrowsingPreference = mSafeBrowsingSettingsFragment.findPreference(
                SafeBrowsingSettingsFragment.PREF_SAFE_BROWSING);
        mManagedTextPreference = mSafeBrowsingSettingsFragment.findPreference(
                SafeBrowsingSettingsFragment.PREF_TEXT_MANAGED);
        Assert.assertNotNull(
                "Safe Browsing preference should not be null.", mSafeBrowsingPreference);
        Assert.assertNotNull("Text managed preference should not be null.", mManagedTextPreference);
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    public void testOnStartup() {
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            @SafeBrowsingState
            int currentState = SafeBrowsingBridge.getSafeBrowsingState();
            boolean enhanced_protection_checked =
                    currentState == SafeBrowsingState.ENHANCED_PROTECTION;
            boolean standard_protection_checked =
                    currentState == SafeBrowsingState.STANDARD_PROTECTION;
            boolean no_protection_checked = currentState == SafeBrowsingState.NO_SAFE_BROWSING;
            Assert.assertEquals(ASSERT_RADIO_BUTTON_CHECKED, enhanced_protection_checked,
                    getEnhancedProtectionButton().isChecked());
            Assert.assertEquals(ASSERT_RADIO_BUTTON_CHECKED, standard_protection_checked,
                    getStandardProtectionButton().isChecked());
            Assert.assertEquals(ASSERT_RADIO_BUTTON_CHECKED, no_protection_checked,
                    getNoProtectionButton().isChecked());
            Assert.assertFalse(mManagedTextPreference.isVisible());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    public void testCheckRadioButtons() {
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mManagedTextPreference.isVisible());
            // Click the Enhanced Protection button.
            getEnhancedProtectionButton().onClick(null);
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_RADIO_BUTTON_GROUP,
                    SafeBrowsingState.ENHANCED_PROTECTION, getSafeBrowsingState());
            Assert.assertTrue(
                    ASSERT_RADIO_BUTTON_CHECKED, getEnhancedProtectionButton().isChecked());
            Assert.assertFalse(
                    ASSERT_RADIO_BUTTON_CHECKED, getStandardProtectionButton().isChecked());
            Assert.assertFalse(ASSERT_RADIO_BUTTON_CHECKED, getNoProtectionButton().isChecked());
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_NATIVE,
                    SafeBrowsingState.ENHANCED_PROTECTION,
                    SafeBrowsingBridge.getSafeBrowsingState());

            // Click the Standard Protection button.
            getStandardProtectionButton().onClick(null);
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_RADIO_BUTTON_GROUP,
                    SafeBrowsingState.STANDARD_PROTECTION, getSafeBrowsingState());
            Assert.assertFalse(
                    ASSERT_RADIO_BUTTON_CHECKED, getEnhancedProtectionButton().isChecked());
            Assert.assertTrue(
                    ASSERT_RADIO_BUTTON_CHECKED, getStandardProtectionButton().isChecked());
            Assert.assertFalse(ASSERT_RADIO_BUTTON_CHECKED, getNoProtectionButton().isChecked());
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_NATIVE,
                    SafeBrowsingState.STANDARD_PROTECTION,
                    SafeBrowsingBridge.getSafeBrowsingState());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    public void testCheckNoProtectionRadioButtonsCancel() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        });
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getNoProtectionButton().onClick(null);

            // Checked button hasn't changed yet, because confirmation is pending.
            Assert.assertTrue(
                    ASSERT_RADIO_BUTTON_CHECKED, getEnhancedProtectionButton().isChecked());
            Assert.assertFalse(ASSERT_RADIO_BUTTON_CHECKED, getNoProtectionButton().isChecked());
        });

        // The dialog is displayed.
        onView(withText(R.string.safe_browsing_no_protection_confirmation_dialog_title))
                .check(matches(isDisplayed()));
        // Don't confirm.
        onView(withText(R.string.cancel)).perform(click());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // It should stay in enhanced protection mode.
            Assert.assertTrue(
                    ASSERT_RADIO_BUTTON_CHECKED, getEnhancedProtectionButton().isChecked());
            Assert.assertFalse(ASSERT_RADIO_BUTTON_CHECKED, getNoProtectionButton().isChecked());
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_NATIVE,
                    SafeBrowsingState.ENHANCED_PROTECTION,
                    SafeBrowsingBridge.getSafeBrowsingState());
        });

        // The confirmation dialog should be gone.
        onView(withText(R.string.safe_browsing_no_protection_confirmation_dialog_title))
                .check(doesNotExist());
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    public void testCheckNoProtectionRadioButtonsConfirm() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        });
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getNoProtectionButton().onClick(null);

            // Checked button is not changed yet, because confirmation is pending.
            Assert.assertTrue(
                    ASSERT_RADIO_BUTTON_CHECKED, getEnhancedProtectionButton().isChecked());
            Assert.assertFalse(ASSERT_RADIO_BUTTON_CHECKED, getNoProtectionButton().isChecked());
        });

        // The dialog is displayed.
        onView(withText(R.string.safe_browsing_no_protection_confirmation_dialog_title))
                .check(matches(isDisplayed()));
        // Confirm.
        onView(withText(R.string.safe_browsing_no_protection_confirmation_dialog_confirm))
                .perform(click());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(
                    ASSERT_RADIO_BUTTON_CHECKED, getEnhancedProtectionButton().isChecked());
            Assert.assertTrue(ASSERT_RADIO_BUTTON_CHECKED, getNoProtectionButton().isChecked());
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_NATIVE,
                    SafeBrowsingState.NO_SAFE_BROWSING, SafeBrowsingBridge.getSafeBrowsingState());
        });

        // The confirmation dialog should be gone.
        onView(withText(R.string.safe_browsing_no_protection_confirmation_dialog_title))
                .check(doesNotExist());
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    public void testCheckNoProtectionConfirmationIfAlreadyInNoProtectionMode() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.NO_SAFE_BROWSING);
        });
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> { getNoProtectionButton().onClick(null); });

        // Since it is already in no protection mode, the dialog shouldn't be shown.
        onView(withText(R.string.safe_browsing_no_protection_confirmation_dialog_title))
                .check(doesNotExist());
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.DisableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    public void testEnhancedProtectionDisabled() {
        launchSettingsActivity();
        Assert.assertNull(getEnhancedProtectionButton());
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    public void testEnhancedProtectionAuxButtonClicked() {
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSafeBrowsingSettingsFragment.setSettingsLauncher(mSettingsLauncher);
            getEnhancedProtectionButton().getAuxButtonForTests().performClick();
            Mockito.verify(mSettingsLauncher)
                    .launchSettingsActivity(mSafeBrowsingSettingsFragment.getContext(),
                            EnhancedProtectionSettingsFragment.class);
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    public void testStandardProtectionAuxButtonClicked() {
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSafeBrowsingSettingsFragment.setSettingsLauncher(mSettingsLauncher);
            getStandardProtectionButton().getAuxButtonForTests().performClick();
            Mockito.verify(mSettingsLauncher)
                    .launchSettingsActivity(mSafeBrowsingSettingsFragment.getContext(),
                            StandardProtectionSettingsFragment.class);
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    @Policies.Add({ @Policies.Item(key = "SafeBrowsingEnabled", string = "true") })
    public void testSafeBrowsingManaged() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeBrowserInitializer.getInstance().handleSynchronousStartup(); });
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(SafeBrowsingBridge.isSafeBrowsingManaged());
            Assert.assertTrue(mManagedTextPreference.isVisible());
            Assert.assertFalse(getEnhancedProtectionButton().isEnabled());
            Assert.assertFalse(getStandardProtectionButton().isEnabled());
            Assert.assertFalse(getNoProtectionButton().isEnabled());
            Assert.assertEquals(SafeBrowsingState.STANDARD_PROTECTION, getSafeBrowsingState());
            // To disclose information, aux buttons should be enabled under managed mode.
            Assert.assertTrue(getEnhancedProtectionButton().getAuxButtonForTests().isEnabled());
            Assert.assertTrue(getStandardProtectionButton().getAuxButtonForTests().isEnabled());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    @Policies.Add({ @Policies.Item(key = "SafeBrowsingProtectionLevel", string = "2") })
    public void testSafeBrowsingProtectionLevelManagedEnhanced() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeBrowserInitializer.getInstance().handleSynchronousStartup(); });
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(SafeBrowsingBridge.isSafeBrowsingManaged());
            Assert.assertTrue(mManagedTextPreference.isVisible());
            Assert.assertFalse(getEnhancedProtectionButton().isEnabled());
            Assert.assertFalse(getStandardProtectionButton().isEnabled());
            Assert.assertFalse(getNoProtectionButton().isEnabled());
            Assert.assertEquals(SafeBrowsingState.ENHANCED_PROTECTION, getSafeBrowsingState());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    @Policies.Add({ @Policies.Item(key = "SafeBrowsingProtectionLevel", string = "1") })
    public void testSafeBrowsingProtectionLevelManagedStandard() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeBrowserInitializer.getInstance().handleSynchronousStartup(); });
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(SafeBrowsingBridge.isSafeBrowsingManaged());
            Assert.assertEquals(SafeBrowsingState.STANDARD_PROTECTION, getSafeBrowsingState());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    @Policies.Add({ @Policies.Item(key = "SafeBrowsingProtectionLevel", string = "0") })
    public void testSafeBrowsingProtectionLevelManagedDisabled() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeBrowserInitializer.getInstance().handleSynchronousStartup(); });
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(SafeBrowsingBridge.isSafeBrowsingManaged());
            Assert.assertEquals(SafeBrowsingState.NO_SAFE_BROWSING, getSafeBrowsingState());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    public void testHelpButtonClicked() {
        launchSettingsActivity();
        mSafeBrowsingSettingsFragment.setHelpAndFeedbackLauncher(mHelpAndFeedbackLauncher);
        onView(withId(R.id.menu_id_targeted_help)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Mockito.verify(mHelpAndFeedbackLauncher)
                    .show(mSafeBrowsingSettingsFragment.getActivity(),
                            mSafeBrowsingSettingsFragment.getString(
                                    R.string.help_context_safe_browsing),
                            Profile.getLastUsedRegularProfile(), null);
        });
    }

    private @SafeBrowsingState int getSafeBrowsingState() {
        return mSafeBrowsingPreference.getSafeBrowsingStateForTesting();
    }

    private RadioButtonWithDescriptionAndAuxButton getEnhancedProtectionButton() {
        return mSafeBrowsingPreference.getEnhancedProtectionButtonForTesting();
    }

    private RadioButtonWithDescriptionAndAuxButton getStandardProtectionButton() {
        return mSafeBrowsingPreference.getStandardProtectionButtonForTesting();
    }

    private RadioButtonWithDescription getNoProtectionButton() {
        return mSafeBrowsingPreference.getNoProtectionButtonForTesting();
    }
}
