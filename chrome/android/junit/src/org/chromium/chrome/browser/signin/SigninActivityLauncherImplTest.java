// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * Tests {@link SigninActivityLauncherImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninActivityLauncherImplTest {
    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private Context mContextMock;

    @Mock
    private Profile mProfile;

    private final Context mContext = RuntimeEnvironment.application.getApplicationContext();

    @Before
    public void setUp() {
        initMocks(this);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        Profile.setLastUsedProfileForTesting(mProfile);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
    }

    @Test
    public void testLaunchActivityIfAllowedWhenSigninIsAllowed() {
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(true);
        Assert.assertTrue(SigninActivityLauncherImpl.get().launchActivityIfAllowed(
                mContextMock, SigninAccessPoint.SETTINGS));
        verify(mContextMock).startActivity(notNull());
    }

    @Test
    public void testLaunchActivityIfAllowedWhenSigninIsNotAllowed() {
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(false);
        Object toastBeforeCall = ShadowToast.getLatestToast();
        Assert.assertFalse(SigninActivityLauncherImpl.get().launchActivityIfAllowed(
                mContext, SigninAccessPoint.SETTINGS));
        Object toastAfterCall = ShadowToast.getLatestToast();
        Assert.assertEquals(
                "No new toast should be made during the call!", toastBeforeCall, toastAfterCall);
    }

    @Test
    public void testLaunchActivityIfAllowedWhenSigninIsDisabledByPolicy() {
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        Assert.assertFalse(SigninActivityLauncherImpl.get().launchActivityIfAllowed(
                mContext, SigninAccessPoint.SETTINGS));
        Assert.assertTrue(ShadowToast.showedCustomToast(
                mContext.getResources().getString(R.string.managed_by_your_organization),
                R.id.toast_text));
    }
}
