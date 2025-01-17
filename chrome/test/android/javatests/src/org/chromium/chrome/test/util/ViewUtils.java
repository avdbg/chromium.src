// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static java.lang.annotation.RetentionPolicy.SOURCE;

import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.ViewAssertion;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.ViewMatchers;

import org.hamcrest.Matcher;
import org.junit.Assert;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;

import java.lang.annotation.Retention;
import java.util.ArrayList;
import java.util.List;

/**
 * Collection of utilities helping to clarify expectations on views in tests.
 */
public class ViewUtils {
    @Retention(SOURCE)
    @IntDef(flag = true, value = {VIEW_VISIBLE, VIEW_INVISIBLE, VIEW_GONE, VIEW_NULL})
    public @interface ExpectedViewState {}
    public static final int VIEW_VISIBLE = 1;
    public static final int VIEW_INVISIBLE = 1 << 1;
    public static final int VIEW_GONE = 1 << 2;
    public static final int VIEW_NULL = 1 << 3;

    private static class ExpectedViewCriteria implements Runnable {
        private final Matcher<View> mViewMatcher;
        private final @ExpectedViewState int mViewState;
        private final ViewGroup mRootView;

        ExpectedViewCriteria(
                Matcher<View> viewMatcher, @ExpectedViewState int viewState, ViewGroup rootView) {
            mViewMatcher = viewMatcher;
            mViewState = viewState;
            mRootView = rootView;
        }

        @Override
        public void run() {
            List<View> matchedViews = new ArrayList<>();
            findMatchingChildren(mRootView, matchedViews);
            Assert.assertThat(matchedViews, not(hasSize(greaterThan(1))));
            assertViewExpectedState(matchedViews.size() == 0 ? null : matchedViews.get(0));
        }

        private void findMatchingChildren(ViewGroup root, List<View> matchedViews) {
            for (int i = 0; i < root.getChildCount(); i++) {
                View view = root.getChildAt(i);
                if (mViewMatcher.matches(view)) matchedViews.add(view);
                if (view instanceof ViewGroup) {
                    findMatchingChildren((ViewGroup) view, matchedViews);
                }
            }
        }

        private void assertViewExpectedState(View view) {
            if (view == null) {
                Criteria.checkThat("No view found to match: " + mViewMatcher.toString(),
                        (mViewState & VIEW_NULL) != 0, is(true));
                return;
            }

            switch (view.getVisibility()) {
                case View.VISIBLE:
                    Criteria.checkThat("View matching '" + mViewMatcher.toString()
                                    + "' is unexpectedly visible!",
                            (mViewState & VIEW_VISIBLE) != 0, is(true));
                    break;
                case View.INVISIBLE:
                    Criteria.checkThat("View matching '" + mViewMatcher.toString()
                                    + "' is unexpectedly invisible!",
                            (mViewState & VIEW_INVISIBLE) != 0, is(true));
                    break;
                case View.GONE:
                    Criteria.checkThat(
                            "View matching '" + mViewMatcher.toString() + "' is unexpectedly gone!",
                            (mViewState & VIEW_GONE) != 0, is(true));
                    break;
            }
        }
    }

    private ViewUtils() {}

    /**
     * Waits until a view matching the given matches any of the given {@link ExpectedViewState}s.
     * Fails if the matcher applies to multiple views. Times out if no view was found while waiting
     * up to {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     * @param root The view group to search in.
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @param viewState State that the matching view should be in. If multiple states are passed,
     *                  the waiting will stop if at least one applies.
     */
    public static void waitForView(
            ViewGroup root, Matcher<View> viewMatcher, @ExpectedViewState int viewState) {
        CriteriaHelper.pollUiThread(new ExpectedViewCriteria(viewMatcher, viewState, root));
    }

    /**
     * Waits until a view matching the given matches any of the given {@link ExpectedViewState}s.
     * Fails if the matcher applies to multiple views. Times out if no view was found while waiting
     * up to {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     * This should be used on {@link ViewInteraction#check} with a {@link ViewGroup}. For example,
     * the following usage assumes the root view is a {@link ViewGroup}.
     * <pre>
     *   onView(isRoot()).check(waitForView(withId(R.id.example_id), VIEW_GONE));
     * </pre>
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @param viewState State that the matching view should be in. If multiple states are passed,
     *                  the waiting will stop if at least one applies.
     */
    public static ViewAssertion waitForView(
            Matcher<View> viewMatcher, @ExpectedViewState int viewState) {
        return (View view, NoMatchingViewException noMatchException) -> {
            if (noMatchException != null) throw noMatchException;
            CriteriaHelper.pollUiThreadNested(
                    new ExpectedViewCriteria(viewMatcher, viewState, (ViewGroup) view));
        };
    }

    /**
     * Waits until a visible view matching the given matcher appears. Fails if the matcher applies
     * to multiple views.  Times out if no view was found while waiting up to
     * {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     * @param root The view group to search in.
     * @param viewMatcher The matcher matching the view that should be waited for.
     */
    public static void waitForView(ViewGroup root, Matcher<View> viewMatcher) {
        waitForView(root, viewMatcher, VIEW_VISIBLE);
    }

    /**
     * Waits until a visible view matching the given matcher appears. Fails if the matcher applies
     * to multiple views.  Times out if no view was found while waiting up to
     * {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     * @param viewMatcher The matcher matching the view that should be waited for.
     */
    public static ViewAssertion waitForView(Matcher<View> viewMatcher) {
        return waitForView(viewMatcher, VIEW_VISIBLE);
    }

    /**
     * Waits until a visible view matching the given matcher appears. Fails if the matcher applies
     * to multiple views.  Times out if no view was found while waiting up to
     * {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @return An interaction on the matching view.
     */
    public static ViewInteraction onViewWaiting(Matcher<View> viewMatcher) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Espresso.onView(ViewMatchers.isRoot())
                    .check((View view, NoMatchingViewException noMatchException) -> {
                        if (noMatchException != null) throw noMatchException;
                        new ExpectedViewCriteria(viewMatcher, VIEW_VISIBLE, (ViewGroup) view).run();
                    });
        });
        return Espresso.onView(viewMatcher);
    }

    /**
     * Wait until the specified view has finished layout updates.
     * @param view The specified view.
     */
    public static void waitForStableView(final View view) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("The view is dirty.", view.isDirty(), is(false));
            Criteria.checkThat(
                    "The view has layout requested.", view.isLayoutRequested(), is(false));
        });
    }

    public static MotionEvent createMotionEvent(float x, float y) {
        return MotionEvent.obtain(0, 0, 0, x, y, 0);
    }
}
