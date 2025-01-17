// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.provider.Browser;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Java side of Android implementation of the page info UI.
 */
public class ConnectionInfoView implements OnClickListener {
    private static final String TAG = "ConnectionInfoView";

    private static final String HELP_URL =
            "https://support.google.com/chrome?p=android_connection_info";

    private final Context mContext;
    private ConnectionInfoDelegate mDelegate;
    private final LinearLayout mContainer;
    private final WebContents mWebContents;
    private final int mPaddingWide;
    private final int mPaddingThin;
    private final long mNativeConnectionInfoView;
    private final CertificateViewer mCertificateViewer;
    private TextView mCertificateViewerTextView;
    private TextView mMoreInfoLink;
    private ViewGroup mCertificateLayout;
    private ViewGroup mDescriptionLayout;
    private Button mResetCertDecisionsButton;
    private String mLinkUrl;
    private VrHandler mVrHandler;
    private final boolean mIsV2Enabled;

    /**
     * Delegate that embeds the ConnectionInfoView. Must call ConnectionInfoView::onDismiss when
     * the embedding view is removed.
     */
    interface ConnectionInfoDelegate {
        /**
         * Called when the ConnectionInfoView is initialized
         */
        void onReady(ConnectionInfoView popup);

        /**
         * Called in order to dismiss the dialog or page that is showing the ConnectionInfoView.
         */
        void dismiss(int actionOnContent);
    }

    private ConnectionInfoView(Context context, WebContents webContents,
            ConnectionInfoDelegate delegate, VrHandler vrHandler) {
        mIsV2Enabled = PageInfoFeatureList.isEnabled(PageInfoFeatureList.PAGE_INFO_V2);
        mContext = context;
        mDelegate = delegate;
        mWebContents = webContents;
        mVrHandler = vrHandler;

        mCertificateViewer = new CertificateViewer(mContext);

        mContainer = new LinearLayout(mContext);
        mContainer.setOrientation(LinearLayout.VERTICAL);
        // TODO(crbug.com/1077766): Rename mPaddingWide and mPaddingThin to mPaddingSides and
        // mPaddingVertical respectively when cleaning up V2 enabled tags
        if (mIsV2Enabled) {
            mPaddingWide = context.getResources().getDimensionPixelSize(
                    R.dimen.page_info_popup_padding_sides);
            mPaddingThin = context.getResources().getDimensionPixelSize(
                    R.dimen.page_info_popup_padding_vertical);
            mContainer.setPadding(mPaddingWide, mPaddingThin, mPaddingWide, 0);

        } else {
            mPaddingWide =
                    (int) context.getResources().getDimension(R.dimen.connection_info_padding_wide);
            mPaddingThin =
                    (int) context.getResources().getDimension(R.dimen.connection_info_padding_thin);
            mContainer.setPadding(
                    mPaddingWide, mPaddingWide, mPaddingWide, mPaddingWide - mPaddingThin);
        }

        // This needs to come after other member initialization.
        mNativeConnectionInfoView = ConnectionInfoViewJni.get().init(this, mWebContents);
    }

    /**
     * Adds certificate section, which contains an icon, a headline, a
     * description and a label for certificate info link.
     */
    @CalledByNative
    private void addCertificateSection(
            int iconId, String headline, String description, String label, int iconColorId) {
        View section = addSection(iconId, headline, description, iconColorId);
        assert mCertificateLayout == null;
        mCertificateLayout = (ViewGroup) section.findViewById(R.id.connection_info_text_layout);
        if (label != null && !label.isEmpty()) {
            setCertificateViewer(label);
        }
    }

    /**
     * Adds Description section, which contains an icon, a headline, and a
     * description. Most likely headline for description is empty
     */
    @CalledByNative
    private void addDescriptionSection(
            int iconId, String headline, String description, int iconColorId) {
        View section = addSection(iconId, headline, description, iconColorId);
        assert mDescriptionLayout == null;
        mDescriptionLayout = section.findViewById(R.id.connection_info_text_layout);
    }

    private View addSection(int iconId, String headline, String description, int iconColorId) {
        View section = mIsV2Enabled
                ? LayoutInflater.from(mContext).inflate(R.layout.connection_info_v2, null)
                : LayoutInflater.from(mContext).inflate(R.layout.connection_info, null);
        ImageView i = section.findViewById(R.id.connection_info_icon);
        i.setImageResource(iconId);
        if (mIsV2Enabled) {
            ApiCompatibilityUtils.setImageTintList(
                    i, ColorStateList.valueOf(mContext.getResources().getColor(iconColorId)));
        }

        if (!mIsV2Enabled) {
            TextView h = section.findViewById(R.id.connection_info_headline);
            h.setText(headline);
            if (TextUtils.isEmpty(headline)) h.setVisibility(View.GONE);
        }

        TextView d = section.findViewById(R.id.connection_info_description);
        d.setText(description);
        if (TextUtils.isEmpty(description)) d.setVisibility(View.GONE);

        mContainer.addView(section);
        return section;
    }

    private void setCertificateViewer(String label) {
        assert mCertificateViewerTextView == null;
        mCertificateViewerTextView = new TextView(mContext);
        mCertificateViewerTextView.setText(label);
        ApiCompatibilityUtils.setTextAppearance(
                mCertificateViewerTextView, R.style.TextAppearance_TextSmall_Blue);
        mCertificateViewerTextView.setOnClickListener(this);
        mCertificateViewerTextView.setPadding(0, mPaddingThin, 0, 0);
        mCertificateLayout.addView(mCertificateViewerTextView);
    }

    @CalledByNative
    private void addResetCertDecisionsButton(String label) {
        assert mResetCertDecisionsButton == null;

        mResetCertDecisionsButton = new ButtonCompat(mContext, R.style.FilledButtonThemeOverlay);
        mResetCertDecisionsButton.setText(label);
        mResetCertDecisionsButton.setOnClickListener(this);

        LinearLayout container = new LinearLayout(mContext);
        container.setOrientation(LinearLayout.VERTICAL);
        container.addView(mResetCertDecisionsButton);
        container.setPadding(0, 0, 0, mPaddingWide);
        mContainer.addView(container);
    }

    @CalledByNative
    private void addMoreInfoLink(String linkText) {
        mMoreInfoLink = new TextView(mContext);
        mLinkUrl = HELP_URL;
        mMoreInfoLink.setText(linkText);
        ApiCompatibilityUtils.setTextAppearance(
                mMoreInfoLink, R.style.TextAppearance_TextSmall_Blue);
        mMoreInfoLink.setPadding(0, mPaddingThin, 0, 0);
        mMoreInfoLink.setOnClickListener(this);
        mDescriptionLayout.addView(mMoreInfoLink);
    }

    /** Displays the ConnectionInfoView. */
    @CalledByNative
    private void onReady() {
        mDelegate.onReady(this);
    }

    @Override
    public void onClick(View v) {
        if (mResetCertDecisionsButton == v) {
            ConnectionInfoViewJni.get().resetCertDecisions(
                    mNativeConnectionInfoView, ConnectionInfoView.this, mWebContents);
            mDelegate.dismiss(DialogDismissalCause.ACTION_ON_CONTENT);
        } else if (mCertificateViewerTextView == v) {
            byte[][] certChain = CertificateChainHelper.getCertificateChain(mWebContents);
            if (certChain == null) {
                // The WebContents may have been destroyed/invalidated. If so,
                // ignore this request.
                return;
            }
            if (mVrHandler != null && mVrHandler.isInVr()) {
                mVrHandler.exitVrAndRun(() -> {
                    mCertificateViewer.showCertificateChain(certChain);
                }, VrHandler.UiType.CERTIFICATE_INFO);
                return;
            }
            mCertificateViewer.showCertificateChain(certChain);
        } else if (mMoreInfoLink == v) {
            if (mVrHandler != null && mVrHandler.isInVr()) {
                mVrHandler.exitVrAndRun(this::showConnectionSecurityInfo,
                        VrHandler.UiType.CONNECTION_SECURITY_INFO);
                return;
            }
            showConnectionSecurityInfo();
        }
    }

    /**
     * @return The view containing connection info.
     */
    public View getView() {
        return mContainer;
    }

    /**
     * Called when the embedding view is removed.
     */
    public void onDismiss() {
        assert mNativeConnectionInfoView != 0;
        org.chromium.components.page_info.ConnectionInfoViewJni.get().destroy(
                mNativeConnectionInfoView, ConnectionInfoView.this);
    }

    private void showConnectionSecurityInfo() {
        // TODO(crbug.com/1077766): We probably don't want to dismiss the new PageInfo UI here?
        mDelegate.dismiss(DialogDismissalCause.ACTION_ON_CONTENT);
        try {
            Intent i = Intent.parseUri(mLinkUrl, Intent.URI_INTENT_SCHEME);
            i.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
            i.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
            mContext.startActivity(i);
        } catch (Exception ex) {
            // Do nothing intentionally.
            Log.w(TAG, "Bad URI %s", mLinkUrl, ex);
        }
    }

    static class ConnectionInfoDialogDelegate
            implements ConnectionInfoDelegate, ModalDialogProperties.Controller {
        private ConnectionInfoView mPopup;
        private PropertyModel mDialogModel;
        private final ModalDialogManager mModalDialogManager;
        private WebContents mWebContents;
        private final WebContentsObserver mWebContentsObserver;

        ConnectionInfoDialogDelegate(
                ModalDialogManager modalDialogManager, WebContents webContents) {
            mModalDialogManager = modalDialogManager;
            mWebContents = webContents;
            mWebContentsObserver = new WebContentsObserver(mWebContents) {
                @Override
                public void navigationEntryCommitted(LoadCommittedDetails details) {
                    // If a navigation is committed (e.g. from in-page redirect), the data we're
                    // showing is stale so dismiss the dialog.
                    dismiss(DialogDismissalCause.UNKNOWN);
                }

                @Override
                public void destroy() {
                    super.destroy();
                    dismiss(DialogDismissalCause.UNKNOWN);
                }
            };
        }

        @Override
        public void onClick(PropertyModel model, int buttonType) {}

        @Override
        public void dismiss(@DialogDismissalCause int dismissalCause) {
            mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
        }

        @Override
        public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
            mPopup.onDismiss();
            mWebContentsObserver.destroy();
            mDialogModel = null;
        }

        @Override
        public void onReady(ConnectionInfoView popup) {
            mPopup = popup;
            ScrollView scrollView = new ScrollView(popup.mContext);
            scrollView.addView(popup.getView());

            mDialogModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                   .with(ModalDialogProperties.CONTROLLER, this)
                                   .with(ModalDialogProperties.CUSTOM_VIEW, scrollView)
                                   .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                                   .build();

            mModalDialogManager.showDialog(
                    mDialogModel, ModalDialogManager.ModalDialogType.APP, true);
        }
    }

    /**
     * Shows a connection info dialog for the provided WebContents.
     *
     * The popup adds itself to the view hierarchy which owns the reference while it's
     * visible.
     *
     * @param context Context which is used for launching a dialog.
     * @param webContents The WebContents for which to show website information
     */
    public static void show(Context context, WebContents webContents,
            ModalDialogManager modalDialogManager, VrHandler vrHandler) {
        new ConnectionInfoView(context, webContents,
                new ConnectionInfoDialogDelegate(modalDialogManager, webContents), vrHandler);
    }

    public static ConnectionInfoView create(Context context, WebContents webContents,
            ConnectionInfoDelegate delegate, VrHandler vrHandler) {
        return new ConnectionInfoView(context, webContents, delegate, vrHandler);
    }

    @NativeMethods
    interface Natives {
        long init(ConnectionInfoView popup, WebContents webContents);
        void destroy(long nativeConnectionInfoViewAndroid, ConnectionInfoView caller);
        void resetCertDecisions(long nativeConnectionInfoViewAndroid, ConnectionInfoView caller,
                WebContents webContents);
    }
}
