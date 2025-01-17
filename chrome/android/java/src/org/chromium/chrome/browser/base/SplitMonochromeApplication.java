// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;

import org.chromium.android_webview.nonembedded.WebViewApkApplication;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.browser.version.ChromeVersionInfo;
import org.chromium.content_public.browser.ChildProcessCreationParams;

/**
 * Application class to use for Monochrome when //chrome code is in an isolated split. See {@link
 * SplitChromeApplication} for more info.
 */
public class SplitMonochromeApplication extends SplitChromeApplication {
    private static class NonBrowserMonochromeApplication extends Impl {
        @Override
        public void onCreate() {
            super.onCreate();
            // TODO(crbug.com/1126301): This matches logic in MonochromeApplication.java.
            // Deduplicate if chrome split launches.
            if (!ChromeVersionInfo.isStableBuild() && getApplication().isWebViewProcess()) {
                WebViewApkApplication.postDeveloperUiLauncherIconTask();
            }
        }
    }

    public SplitMonochromeApplication() {
        super(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.MonochromeApplication$MonochromeApplicationImpl"));
    }

    @Override
    public void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        initializeMonochromeProcessCommon(getPackageName());
    }

    @Override
    protected Impl createNonBrowserApplication() {
        return new NonBrowserMonochromeApplication();
    }

    public static void initializeMonochromeProcessCommon(String packageName) {
        WebViewApkApplication.maybeInitProcessGlobals();

        // ChildProcessCreationParams is only needed for browser process, though it is
        // created and set in all processes. We must set isExternalService to true for
        // Monochrome because Monochrome's renderer services are shared with WebView
        // and are external, and will fail to bind otherwise.
        boolean bindToCaller = false;
        boolean ignoreVisibilityForImportance = false;
        ChildProcessCreationParams.set(packageName, null /* privilegedServicesName */, packageName,
                null /* sandboxedServicesName */, true /* isExternalService */,
                LibraryProcessType.PROCESS_CHILD, bindToCaller, ignoreVisibilityForImportance);
    }

    @Override
    public boolean isWebViewProcess() {
        return WebViewApkApplication.isWebViewProcess();
    }
}
