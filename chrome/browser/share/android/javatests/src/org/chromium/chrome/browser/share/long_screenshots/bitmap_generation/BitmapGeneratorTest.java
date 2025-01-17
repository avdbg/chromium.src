// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.paint_preview.PaintPreviewCompositorUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests for the LongScreenshotsEntryTest. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BitmapGeneratorTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    private Tab mTab;
    private BitmapGenerator mGenerator;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
    }

    @After
    public void tearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mGenerator.destroy(); });
    }

    /**
     * Verifies that a Tab's contents are captured.
     */
    @Test
    @LargeTest
    @Feature({"LongScreenshots"})
    @DisabledTest(message = "https://crbug.com/1183524")
    public void testCapturedNewOne() throws Exception {
        Runnable onErrorCallback = new Runnable() {
            @Override
            public void run() {
                Assert.fail("Error should not be thrown");
            }
        };

        Callback<Bitmap> onBitmapGenerated = new Callback<Bitmap>() {
            @Override
            public void onResult(Bitmap result) {
                Assert.assertNotNull(result);
            }
        };

        class Listener implements BitmapGenerator.GeneratorCallBack {
            @Override
            public void onCompositorResult(@CompositorStatus int status) {
                Assert.assertEquals(CompositorStatus.OK, status);
                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    mGenerator.compositeBitmap(
                            new Rect(0, 0, 100, 100), onErrorCallback, onBitmapGenerated);
                });
            }

            @Override
            public void onCaptureResult(@Status int status) {
                Assert.assertEquals(Status.OK, status);
            }
        }

        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        final String url = testServer.getURL("/chrome/test/data/android/about.html");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mGenerator = new BitmapGenerator(mActivityTestRule.getActivity(), mTab,
                    new Rect(0, 0, 100, 100), new Listener());
            PaintPreviewCompositorUtils.warmupCompositor();
            mTab.loadUrl(new LoadUrlParams(url));
            mGenerator.captureTab();
        });
    }
}
