// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertNotNull;

import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoFeatureList;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/**
 * Tests for PageInfoView. Uses pixel tests to ensure the UI handles different
 * configurations correctly. These tests are not batched because theme changes
 * don't seem to work with batched tests even with RequiresRestart as it results
 * in the current {@link Tab} in the {@link ChromeTabbedActivityTestRule} to be null.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_STARTUP_PROMOS,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
public class PageInfoViewDarkModeTest {
    private static final String sSimpleHtml = "/chrome/test/data/android/simple.html";

    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsTestRule =
            new DisableAnimationsTestRule();

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus().setRevision(4).build();

    private void loadUrlAndOpenPageInfo(String url) {
        mActivityTestRule.loadUrl(url);
        openPageInfo();
    }

    private void openPageInfo() {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PageInfoController.show(mActivityTestRule.getActivity(), tab.getWebContents(), null,
                    PageInfoController.OpenedFromSource.TOOLBAR,
                    new ChromePageInfoControllerDelegate(mActivityTestRule.getActivity(),
                            tab.getWebContents(),
                            mActivityTestRule.getActivity().getModalDialogManagerSupplier(),
                            /*offlinePageLoadUrlDelegate=*/
                            new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab)),
                    new ChromePermissionParamsListBuilderDelegate(),
                    PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        });

        if (PageInfoFeatureList.isEnabled(PageInfoFeatureList.PAGE_INFO_V2)) {
            onViewWaiting(allOf(withId(R.id.page_info_url_wrapper), isDisplayed()));
        } else {
            onViewWaiting(allOf(withId(R.id.page_info_url), isDisplayed()));
        }
    }

    private View getPageInfoView() {
        PageInfoController controller = PageInfoController.getLastPageInfoControllerForTesting();
        assertNotNull(controller);
        View view = controller.getPageInfoViewForTesting();
        assertNotNull(view);
        return view;
    }

    @Before
    public void setUp() throws InterruptedException {
        // Choose a fixed, "random" port to create stable screenshots.
        mTestServerRule.setServerPort(424242);
        mTestServerRule.setServerUsesHttps(true);
    }

    /**
     * Tests the new PageInfo UI on a secure website in dark mode.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.EnableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowOnSecureWebsiteDarkV2() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeNightModeTestUtils.setUpNightModeForChromeActivity(/*nightModeEnabled=*/true);
        });

        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_SecureWebsiteDarkV2");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeNightModeTestUtils.setUpNightModeForChromeActivity(/*nightModeEnabled=*/false);
        });
    }
}
