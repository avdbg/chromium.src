// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.accounts.Account;
import android.content.Context;
import android.os.Build;
import android.telephony.TelephonyManager;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill_assistant.onboarding.BaseOnboardingCoordinator;
import org.chromium.chrome.browser.autofill_assistant.trigger_scripts.AssistantTriggerScriptBridge;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * An Autofill Assistant client, associated with a specific WebContents.
 *
 * This mainly a bridge to autofill_assistant::ClientAndroid.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantClient {
    /** OAuth2 scope that RPCs require. */
    private static final String AUTH_TOKEN_TYPE =
            "oauth2:https://www.googleapis.com/auth/userinfo.profile";

    /**
     * Pointer to the corresponding native autofill_assistant::ClientAndroid instance. Might be 0 if
     * the native instance has been deleted. Always check before use.
     */
    private long mNativeClientAndroid;

    /**
     * Indicates whether account initialization was started.
     */
    private boolean mAccountInitializationStarted;

    /**
     * Indicates whether {@link mAccount} has been initialized.
     */
    private boolean mAccountInitialized;

    /**
     * Account that was used to initiate AutofillAssistant.
     *
     * <p>This account is used to  authenticate when sending RPCs and as default account for Payment
     * Request. Not relevant until the accounts have been fetched, and mAccountInitialized set to
     * true. Can still be null after the accounts are fetched, in which case authentication is
     * disabled.
     */
    @Nullable
    private Account mAccount;

    /** If set, fetch the access token once the accounts are fetched. */
    private boolean mShouldFetchAccessToken;

    /** Returns the client for the given web contents, creating it if necessary. */
    public static AutofillAssistantClient fromWebContents(WebContents webContents) {
        return AutofillAssistantClientJni.get().fromWebContents(webContents);
    }

    /**
     * Notifies that an onboarding UI is shown or hidden.
     */
    public static void onOnboardingUiChange(WebContents webContents, boolean shown) {
        AutofillAssistantClientJni.get().onOnboardingUiChange(webContents, shown);
    }

    private AutofillAssistantClient(long nativeClientAndroid) {
        mNativeClientAndroid = nativeClientAndroid;
    }

    private void checkNativeClientIsAliveOrThrow() {
        if (mNativeClientAndroid == 0) {
            throw new IllegalStateException("Native instance is dead");
        }
    }

    /**
     * Start a flow on the current URL, autostarting scripts defined for that URL.
     *
     * <p>This immediately shows the UI, with a loading message, then fetches scripts
     * from the server and autostarts one of them.
     *
     * @param initialUrl the original deep link, if known. When started from CCT, this
     * is the URL included into the intent
     * @param parameters Autofill Assistant parameters to set during the whole flow
     * @param experimentIds comma-separated set of experiments to use while running the flow
     * @param callerAccount the account calling the flow
     * @param userName the user name associated with this flow
     * @param isChromeCustomTab whether this was started from a {@link CustomTabActivity} or a
     *         normal Chrome tab.
     * @param onboardingCoordinator if non-null, reuse existing UI elements, usually created to show
     *         onboarding.
     *
     * @return true if the flow was started, false if the controller is in a state where
     * autostarting is not possible, such as can happen if a script is already running. The flow can
     * still fail after this method returns true; the failure will be displayed on the UI.
     */
    boolean start(String initialUrl, Map<String, String> parameters, String experimentIds,
            @Nullable String callerAccount, @Nullable String userName, boolean isChromeCustomTab,
            @Nullable BaseOnboardingCoordinator onboardingCoordinator) {
        if (mNativeClientAndroid == 0) return false;

        checkNativeClientIsAliveOrThrow();
        chooseAccountAsyncIfNecessary(userName);
        return AutofillAssistantClientJni.get().start(mNativeClientAndroid, this, initialUrl,
                experimentIds, callerAccount,
                parameters.keySet().toArray(new String[parameters.size()]),
                parameters.values().toArray(new String[parameters.size()]), isChromeCustomTab,
                onboardingCoordinator,
                /* onboardingShown= */
                onboardingCoordinator != null && onboardingCoordinator.getOnboardingShown(),
                AutofillAssistantServiceInjector.getServiceToInject(mNativeClientAndroid));
    }

    public void startTriggerScript(AssistantTriggerScriptBridge delegate, String initialUrl,
            Map<String, String> parameters, String experimentIds) {
        if (mNativeClientAndroid == 0) {
            return;
        }
        checkNativeClientIsAliveOrThrow();
        AutofillAssistantClientJni.get().startTriggerScript(mNativeClientAndroid, this, delegate,
                initialUrl, experimentIds,
                parameters.keySet().toArray(new String[parameters.size()]),
                parameters.values().toArray(new String[parameters.size()]),
                AutofillAssistantServiceInjector.getServiceRequestSenderToInject());
    }

    /**
     * Gets rid of the UI, if there is one. Leaves Autofill Assistant running.
     */
    public void destroyUi() {
        if (mNativeClientAndroid == 0) return;

        AutofillAssistantClientJni.get().onJavaDestroyUI(
                mNativeClientAndroid, AutofillAssistantClient.this);
    }

    /**
     * Transfers ownership of the UI to an instance of Autofill Assistant running on
     * the given tab/WebContents. Leaves Autofill Assistant running.
     *
     * <p>If Autofill Assistant is not running on the given WebContents, the UI is destroyed, as if
     * {@link #destroyUi} was called.
     */
    public void transferUiTo(WebContents otherWebContents) {
        if (mNativeClientAndroid == 0) return;

        AutofillAssistantClientJni.get().transferUITo(
                mNativeClientAndroid, AutofillAssistantClient.this, otherWebContents);
    }

    /** Starts the controller and fetching scripts for websites. */
    public void fetchWebsiteActions(String userName, String experimentIds,
            Map<String, String> arguments, Callback<Boolean> callback) {
        if (mNativeClientAndroid == 0) {
            callback.onResult(false);
            return;
        }

        chooseAccountAsyncIfNecessary(userName.isEmpty() ? null : userName);

        // The native side calls sendDirectActionList() on the callback once the controller has
        // results.
        AutofillAssistantClientJni.get().fetchWebsiteActions(mNativeClientAndroid,
                AutofillAssistantClient.this, experimentIds,
                arguments.keySet().toArray(new String[arguments.size()]),
                arguments.values().toArray(new String[arguments.size()]), callback);
    }

    /** Return true if the controller exists and is in tracking mode. */
    public boolean hasRunFirstCheck() {
        if (mNativeClientAndroid == 0) {
            return false;
        }

        ThreadUtils.assertOnUiThread();
        return AutofillAssistantClientJni.get().hasRunFirstCheck(
                mNativeClientAndroid, AutofillAssistantClient.this);
    }

    /** Lists available direct actions. */
    public List<AutofillAssistantDirectAction> getDirectActions() {
        if (mNativeClientAndroid == 0) {
            return Collections.emptyList();
        }
        AutofillAssistantDirectAction[] actions = AutofillAssistantClientJni.get().getDirectActions(
                mNativeClientAndroid, AutofillAssistantClient.this);
        return Arrays.asList(actions);
    }

    /**
     * Performs a direct action.
     *
     * @param actionId id of the action
     * @param experimentIds comma-separated set of experiments to use while running the flow
     * @param arguments report these as script parameters while performing this specific action
     * @param onboardingCoordinator if non-null, reuse existing UI elements, usually created to show
     *         onboarding.
     * @return true if the action was found started, false otherwise. The action can still fail
     * after this method returns true; the failure will be displayed on the UI.
     */
    public boolean performDirectAction(String actionId, String experimentIds,
            Map<String, String> arguments,
            @Nullable BaseOnboardingCoordinator onboardingCoordinator) {
        if (mNativeClientAndroid == 0) return false;

        // Note that only fetchWebsiteActions can start AA, so only it needs
        // chooseAccountAsyncIfNecessary.
        return AutofillAssistantClientJni.get().performDirectAction(mNativeClientAndroid,
                AutofillAssistantClient.this, actionId, experimentIds,
                arguments.keySet().toArray(new String[arguments.size()]),
                arguments.values().toArray(new String[arguments.size()]), onboardingCoordinator);
    }

    @CalledByNative
    private static AutofillAssistantClient create(long nativeClientAndroid) {
        return new AutofillAssistantClient(nativeClientAndroid);
    }

    private void chooseAccountAsyncIfNecessary(@Nullable String userName) {
        if (mAccountInitializationStarted) return;
        mAccountInitializationStarted = true;

        AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts(accounts -> {
            if (mNativeClientAndroid == 0) return;
            if (accounts.size() == 1) {
                // If there's only one account, there aren't any doubts.
                onAccountChosen(accounts.get(0));
                return;
            }
            Account signedIn = findAccountByName(accounts,
                    AutofillAssistantClientJni.get().getPrimaryAccountName(
                            mNativeClientAndroid, AutofillAssistantClient.this));
            if (signedIn != null) {
                // TODO(crbug.com/806868): Compare against account name from extras and complain if
                // they don't match.
                onAccountChosen(signedIn);
                return;
            }

            if (userName != null) {
                Account account = findAccountByName(accounts, userName);
                if (account != null) {
                    onAccountChosen(account);
                    return;
                }
            }
            onAccountChosen(null);
        });
    }

    private void onAccountChosen(@Nullable Account account) {
        mAccount = account;
        mAccountInitialized = true;
        // TODO(crbug.com/806868): Consider providing a way of signing in this case, to enforce
        // that all calls are authenticated.

        if (mShouldFetchAccessToken) {
            mShouldFetchAccessToken = false;
            fetchAccessToken();
        }
    }

    private static Account findAccountByName(List<Account> accounts, String name) {
        for (int i = 0; i < accounts.size(); i++) {
            Account account = accounts.get(i);
            if (account.name.equals(name)) {
                return account;
            }
        }
        return null;
    }

    @CalledByNative
    private void fetchAccessToken() {
        if (!mAccountInitialized) {
            // Still getting the account list. Fetch the token as soon as an account is available.
            mShouldFetchAccessToken = true;
            return;
        }
        if (mAccount == null) {
            if (mNativeClientAndroid != 0) {
                AutofillAssistantClientJni.get().onAccessToken(
                        mNativeClientAndroid, AutofillAssistantClient.this, true, "");
            }
            return;
        }

        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                AutofillAssistantUiController.getProfile());
        identityManager.getAccessToken(
                mAccount, AUTH_TOKEN_TYPE, new IdentityManager.GetAccessTokenCallback() {
                    @Override
                    public void onGetTokenSuccess(AccessTokenData token) {
                        if (mNativeClientAndroid != 0) {
                            AutofillAssistantClientJni.get().onAccessToken(mNativeClientAndroid,
                                    AutofillAssistantClient.this, true, token.getToken());
                        }
                    }

                    @Override
                    public void onGetTokenFailure(boolean isTransientError) {
                        if (!isTransientError && mNativeClientAndroid != 0) {
                            AutofillAssistantClientJni.get().onAccessToken(
                                    mNativeClientAndroid, AutofillAssistantClient.this, false, "");
                        }
                    }
                });
    }

    @CalledByNative
    private void invalidateAccessToken(String accessToken) {
        if (mAccount == null) {
            return;
        }

        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                AutofillAssistantUiController.getProfile());
        identityManager.invalidateAccessToken(accessToken);
    }

    /** Returns the e-mail address that corresponds to the access token or an empty string. */
    @CalledByNative
    private String getEmailAddressForAccessTokenAccount() {
        return mAccount != null ? mAccount.name : "";
    }

    /**
     * Returns the country that the device is currently located in. This currently only works
     * for devices with active SIM cards. For a more general solution, we should probably use
     * the LocationManager together with the Geocoder.
     */
    @CalledByNative
    @Nullable
    private String getCountryCode() {
        TelephonyManager telephonyManager =
                (TelephonyManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.TELEPHONY_SERVICE);

        // According to API, location for CDMA networks is unreliable
        if (telephonyManager != null
                && telephonyManager.getPhoneType() != TelephonyManager.PHONE_TYPE_CDMA) {
            return telephonyManager.getNetworkCountryIso();
        }

        return null;
    }

    /** Returns the android version of the device. */
    @CalledByNative
    private int getSdkInt() {
        return Build.VERSION.SDK_INT;
    }

    /** Returns the manufacturer of the device. */
    @CalledByNative
    private String getDeviceManufacturer() {
        return Build.MANUFACTURER;
    }

    /** Returns the model of the device. */
    @CalledByNative
    private String getDeviceModel() {
        return Build.MODEL;
    }

    /** Returns whether a11y is enabled or not. */
    @CalledByNative
    private boolean isAccessibilityEnabled() {
        return ChromeAccessibilityUtil.get().isAccessibilityEnabled();
    }

    /** Adds a dynamic action to the given reporter. */
    @CalledByNative
    private void onFetchWebsiteActions(Callback<Boolean> callback, boolean success) {
        callback.onResult(success);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeClientAndroid = 0;
    }

    @NativeMethods
    interface Natives {
        AutofillAssistantClient fromWebContents(WebContents webContents);
        void onOnboardingUiChange(WebContents webContents, boolean shown);
        boolean start(long nativeClientAndroid, AutofillAssistantClient caller, String initialUrl,
                String experimentIds, String callerAccount, String[] parameterNames,
                String[] parameterValues, boolean isChromeCustomTab,
                @Nullable BaseOnboardingCoordinator onboardingCoordinator, boolean onboardingShown,
                long nativeService);
        void startTriggerScript(long nativeClientAndroid, AutofillAssistantClient caller,
                AssistantTriggerScriptBridge delegate, String initialUrl, String experimentIds,
                String[] parameterNames, String[] parameterValues, long nativeServiceRequestSender);
        void onAccessToken(long nativeClientAndroid, AutofillAssistantClient caller,
                boolean success, String accessToken);
        String getPrimaryAccountName(long nativeClientAndroid, AutofillAssistantClient caller);
        void onJavaDestroyUI(long nativeClientAndroid, AutofillAssistantClient caller);
        void transferUITo(
                long nativeClientAndroid, AutofillAssistantClient caller, Object otherWebContents);
        void fetchWebsiteActions(long nativeClientAndroid, AutofillAssistantClient caller,
                String experimentIds, String[] argumentNames, String[] argumentValues,
                Object callback);
        boolean hasRunFirstCheck(long nativeClientAndroid, AutofillAssistantClient caller);

        AutofillAssistantDirectAction[] getDirectActions(
                long nativeClientAndroid, AutofillAssistantClient caller);

        boolean performDirectAction(long nativeClientAndroid, AutofillAssistantClient caller,
                String actionId, String experimentId, String[] argumentNames,
                String[] argumentValues, @Nullable BaseOnboardingCoordinator onboardingCoordinator);
    }
}
