// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Filter;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.DeveloperModeUtils;
import org.chromium.android_webview.common.Flag;
import org.chromium.android_webview.common.ProductionSupportedFlagList;
import org.chromium.android_webview.common.services.IDeveloperUiService;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * A fragment to toggle experimental WebView flags/features.
 */
@SuppressLint("SetTextI18n")
public class FlagsFragment extends DevUiBaseFragment {
    private static final String TAG = "WebViewDevTools";

    private static final String STATE_DEFAULT = "Default";
    private static final String STATE_ENABLED = "Enabled";
    private static final String STATE_DISABLED = "Disabled";
    private static final String[] sFlagStates = {
            STATE_DEFAULT,
            STATE_ENABLED,
            STATE_DISABLED,
    };

    private Map<String, Boolean> mOverriddenFlags = new HashMap<>();
    private FlagsListAdapter mListAdapter;

    private Context mContext;
    private EditText mSearchBar;

    private static volatile @Nullable Runnable sFilterListener;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_flags, null);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        Activity activity = (Activity) mContext;
        activity.setTitle("WebView Flags");

        ListView flagsListView = view.findViewById(R.id.flags_list);

        // Restore flag overrides from the service process to repopulate the UI, if developer mode
        // is enabled.
        if (DeveloperModeUtils.isDeveloperModeEnabled(mContext.getPackageName())) {
            mOverriddenFlags = DeveloperModeUtils.getFlagOverrides(mContext.getPackageName());
        }

        Flag[] sortedFlags = sortFlagList(ProductionSupportedFlagList.sFlagList);
        Flag[] flagsAndWarningText = new Flag[ProductionSupportedFlagList.sFlagList.length + 1];
        flagsAndWarningText[0] = null; // the first entry is the warning text
        for (int i = 0; i < ProductionSupportedFlagList.sFlagList.length; i++) {
            flagsAndWarningText[i + 1] = sortedFlags[i];
        }
        mListAdapter = new FlagsListAdapter(flagsAndWarningText);
        flagsListView.setAdapter(mListAdapter);

        Button resetFlagsButton = view.findViewById(R.id.reset_flags_button);
        resetFlagsButton.setOnClickListener((View flagButton) -> { resetAllFlags(); });

        mSearchBar = view.findViewById(R.id.flag_search_bar);
        mSearchBar.addTextChangedListener(new TextWatcher() {
            private boolean mPreviouslyHadText;
            @Override
            public void onTextChanged(CharSequence cs, int start, int before, int count) {
                mListAdapter.getFilter().filter(cs);
                boolean currentlyHasText = !cs.toString().isEmpty();
                // As an optimization, only change the clear text button if the search bar just now
                // became empty or non-empty.
                if (mPreviouslyHadText != currentlyHasText) {
                    setClearTextButtonEnabled(mSearchBar, currentlyHasText);
                }
                mPreviouslyHadText = currentlyHasText;
            }

            @Override
            public void beforeTextChanged(CharSequence cs, int start, int count, int after) {}

            @Override
            public void afterTextChanged(Editable e) {}
        });

        mSearchBar.setOnFocusChangeListener((View v, boolean hasFocus) -> {
            if (!hasFocus) hideKeyboard(mContext, v);
        });
    }

    private static void hideKeyboard(Context context, View view) {
        InputMethodManager inputMethodManager =
                (InputMethodManager) context.getSystemService(Activity.INPUT_METHOD_SERVICE);
        inputMethodManager.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }

    private void setClearTextButtonEnabled(EditText editText, boolean enabled) {
        int iconColor = getResources().getColor(R.color.navigation_unselected);
        Drawable clearTextIcon = getResources().getDrawable(R.drawable.ic_clear_text);
        clearTextIcon.mutate();
        clearTextIcon.setColorFilter(new PorterDuffColorFilter(iconColor, PorterDuff.Mode.SRC_IN));

        // Overwrite only the end drawable (index = 2), since there's already a drawable at the
        // start.
        Drawable[] compoundDrawables = editText.getCompoundDrawablesRelative();
        compoundDrawables[2] = enabled ? clearTextIcon : null;
        editText.setCompoundDrawablesRelativeWithIntrinsicBounds(compoundDrawables[0],
                compoundDrawables[1], compoundDrawables[2], compoundDrawables[3]);

        // Set (or remove) the onTouchListener
        if (enabled) {
            editText.setOnTouchListener((View v, MotionEvent event) -> {
                int x = (int) event.getX();
                int iconStart = editText.getWidth() - clearTextIcon.getIntrinsicWidth();
                int iconEnd = editText.getWidth();

                boolean didTapIcon = x >= iconStart && x <= iconEnd;
                if (didTapIcon) {
                    if (event.getAction() == MotionEvent.ACTION_UP) {
                        editText.setText("");
                    }
                    return true;
                }
                return false;
            });
        } else {
            editText.setOnTouchListener(null);
        }
    }

    /**
     * Notifies the caller when ListView filtering is complete, in response to modifying the text in
     * {@code R.id.flag_search_bar}.
     */
    @VisibleForTesting
    public static void setFilterListener(@Nullable Runnable listener) {
        sFilterListener = listener;
    }

    private void onFilterDone() {
        if (sFilterListener != null) sFilterListener.run();
    }

    /**
     * Sorts the flag list so enabled/disabled flags are at the beginning and default flags are at
     * the end.
     */
    private Flag[] sortFlagList(Flag[] unsorted) {
        Flag[] sortedFlags = new Flag[unsorted.length];
        int i = 0;
        for (Flag flag : unsorted) {
            if (mOverriddenFlags.containsKey(flag.getName())) {
                sortedFlags[i++] = flag;
            }
        }
        for (Flag flag : unsorted) {
            if (!mOverriddenFlags.containsKey(flag.getName())) {
                sortedFlags[i++] = flag;
            }
        }
        assert sortedFlags.length == unsorted.length : "arrays should be same length";
        return sortedFlags;
    }

    private static int booleanToState(Boolean b) {
        if (b == null) {
            return /* STATE_DEFAULT */ 0;
        } else if (b) {
            return /* STATE_ENABLED */ 1;
        }
        return /* STATE_DISABLED */ 2;
    }

    private class FlagStateSpinnerSelectedListener implements AdapterView.OnItemSelectedListener {
        private Flag mFlag;

        FlagStateSpinnerSelectedListener(Flag flag) {
            mFlag = flag;
        }

        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
            String flagName = mFlag.getName();
            int oldState = booleanToState(mOverriddenFlags.get(flagName));
            int newState = position;

            switch (sFlagStates[newState]) {
                case STATE_DEFAULT:
                    mOverriddenFlags.remove(flagName);
                    break;
                case STATE_ENABLED:
                    mOverriddenFlags.put(flagName, true);
                    break;
                case STATE_DISABLED:
                    mOverriddenFlags.put(flagName, false);
                    break;
            }

            // Update UI and Service. Only communicate with the service if the map actually updated.
            // This optimizes the number of IPCs we make, but this also allows for atomic batch
            // updates by updating mOverriddenFlags prior to updating the Spinner state.
            if (oldState != newState) {
                sendFlagsToService();

                ViewParent grandparent = parent.getParent();
                if (grandparent instanceof View) {
                    formatListEntry((View) grandparent, newState);
                }

                boolean hasSearchQuery = !mSearchBar.getText().toString().isEmpty();
                RecordHistogram.recordBooleanHistogram(
                        "Android.WebView.DevUi.FlagsUi.ToggledFromSearch", hasSearchQuery);
            }
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {}
    }

    @IntDef({LayoutType.WARNING_MESSAGE, LayoutType.TOGGLEABLE_FLAG})
    private @interface LayoutType {
        int WARNING_MESSAGE = 0;
        int TOGGLEABLE_FLAG = 1;
        int COUNT = 2;
    }

    private static boolean flagMatchesQuery(Flag flag, String lowerCaseQuery) {
        assert lowerCaseQuery.equals(lowerCaseQuery.toLowerCase(Locale.getDefault()))
            : "lowerCaseQuery should already be converted to lower case";

        // If empty query, match every everything (including the warning text)
        if (lowerCaseQuery.isEmpty()) {
            return true;
        }

        // If the user is searching for something and flag represents the warning text, don't
        // match the warning text
        if (flag == null) {
            return false;
        }

        // Match if the flag name contains the query as a substring (case-insensitive)
        String lowerCaseName = flag.getName().toLowerCase(Locale.getDefault());
        if (lowerCaseName.contains(lowerCaseQuery)) return true;

        // Or if the flag description contains the query as a substring (case-insensitive)
        String lowerCaseDescription = flag.getDescription().toLowerCase(Locale.getDefault());
        if (lowerCaseDescription.contains(lowerCaseQuery)) return true;

        return false;
    }

    /**
     * Adapter to create rows of toggleable Flags.
     */
    private class FlagsListAdapter extends ArrayAdapter<Flag> {
        private List<Flag> mItems;
        private final Filter mFilter;

        public FlagsListAdapter(Flag[] flagsAndWarningText) {
            super(mContext, 0);
            mItems = Arrays.asList(flagsAndWarningText);
            mFilter = new Filter() {
                @Override
                protected FilterResults performFiltering(CharSequence constraint) {
                    List<Flag> matches = new ArrayList<>();

                    String lowerCaseQuery = constraint.toString().toLowerCase(Locale.getDefault());
                    for (Flag flag : flagsAndWarningText) {
                        if (flagMatchesQuery(flag, lowerCaseQuery)) matches.add(flag);
                    }

                    FilterResults filterResults = new FilterResults();
                    filterResults.values = matches;
                    filterResults.count = matches.size();
                    return filterResults;
                }

                @Override
                protected void publishResults(CharSequence constraint, FilterResults results) {
                    mItems = (List<Flag>) results.values;
                    notifyDataSetChanged();
                    onFilterDone();
                }
            };
        }

        private View getToggleableFlag(@NonNull Flag flag, View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.toggleable_flag, null);
            }

            TextView flagName = view.findViewById(R.id.flag_name);
            TextView flagDescription = view.findViewById(R.id.flag_description);
            Spinner flagToggle = view.findViewById(R.id.flag_toggle);

            String label = flag.getName();
            if (flag.getEnabledStateValue() != null) {
                label += "=" + flag.getEnabledStateValue();
            }
            flagName.setText(label);
            flagDescription.setText(flag.getDescription());
            ArrayAdapter<String> adapter =
                    new ArrayAdapter<>(mContext, R.layout.flag_states, sFlagStates);
            adapter.setDropDownViewResource(android.R.layout.select_dialog_singlechoice);
            flagToggle.setAdapter(adapter);

            // Populate spinner state from map and update indicators.
            int state = booleanToState(mOverriddenFlags.get(flag.getName()));
            flagToggle.setSelection(state);
            flagToggle.setOnItemSelectedListener(new FlagStateSpinnerSelectedListener(flag));
            formatListEntry(view, state);

            return view;
        }

        private View getWarningMessage(View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.flag_ui_warning, null);
            }

            TextView flagsDescriptionView = view.findViewById(R.id.flags_description);
            flagsDescriptionView.setText("By enabling these features, you could "
                    + "lose app data or compromise your security or privacy. Enabled features "
                    + "apply to WebViews across all apps on the device.");

            return view;
        }

        @Override
        public int getCount() {
            return mItems.size();
        }

        @Override
        public Flag getItem(int position) {
            return mItems.get(position);
        }

        @Override
        @LayoutType
        public int getItemViewType(int position) {
            if (getItem(position) == null) return LayoutType.WARNING_MESSAGE;
            return LayoutType.TOGGLEABLE_FLAG;
        }

        @Override
        public int getViewTypeCount() {
            return LayoutType.COUNT;
        }

        @Override
        public View getView(int position, View view, ViewGroup parent) {
            Flag flag = getItem(position);
            if (getItemViewType(position) == LayoutType.WARNING_MESSAGE) {
                return getWarningMessage(view, parent);
            } else {
                return getToggleableFlag(flag, view, parent);
            }
        }

        @Override
        public Filter getFilter() {
            return mFilter;
        }
    }

    /**
     * Formats a flag list entry. {@code toggleableFlag} should be the View which holds the {@link
     * Spinner}, flag title, flag description, etc. as children.
     *
     * @param toggleableFlag a View representing an entire flag entry.
     * @param state the state of the flag.
     */
    private void formatListEntry(View toggleableFlag, int state) {
        TextView flagName = toggleableFlag.findViewById(R.id.flag_name);
        if (state == /* STATE_DEFAULT */ 0) {
            // Unset the compound drawable.
            flagName.setCompoundDrawablesRelativeWithIntrinsicBounds(0, 0, 0, 0);
        } else { // STATE_ENABLED or STATE_DISABLED
            // Draws a blue circle to the left of the text.
            flagName.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    R.drawable.blue_circle, 0, 0, 0);
        }
    }

    private class FlagsServiceConnection implements ServiceConnection {
        public void start() {
            Intent intent = new Intent();
            intent.setClassName(mContext.getPackageName(), ServiceNames.DEVELOPER_UI_SERVICE);
            if (!mContext.bindService(intent, this, Context.BIND_AUTO_CREATE)) {
                Log.e(TAG, "Failed to bind to Developer UI service");
            }
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            try {
                IDeveloperUiService.Stub.asInterface(service).setFlagOverrides(mOverriddenFlags);
            } catch (RemoteException e) {
                Log.e(TAG, "Failed to send flag overrides to service", e);
            } finally {
                // Unbind when we've sent the flags overrides, since we can always rebind later. The
                // service will manage its own lifetime.
                mContext.unbindService(this);
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    private void sendFlagsToService() {
        FlagsServiceConnection connection = new FlagsServiceConnection();
        connection.start();
    }

    private void resetAllFlags() {
        // Clear the map, then update the Spinners from the map value.
        mOverriddenFlags.clear();
        mListAdapter.notifyDataSetChanged();
        sendFlagsToService();
    }
}
