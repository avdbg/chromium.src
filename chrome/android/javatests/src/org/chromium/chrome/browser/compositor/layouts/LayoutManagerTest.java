// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static android.os.Build.VERSION_CODES.N_MR1;

import static org.hamcrest.Matchers.is;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.tab.TabCreationState.LIVE_IN_BACKGROUND;
import static org.chromium.chrome.test.util.ViewUtils.createMotionEvent;

import android.content.Context;
import android.graphics.PointF;
import android.support.test.InstrumentationRegistry;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.accessibility_tab_switcher.OverviewListLayout;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayout;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.Stack;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackTab;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel.MockTabModelDelegate;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Unit tests for {@link org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome}
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class LayoutManagerTest implements MockTabModelDelegate {
    private static final String TAG = "LayoutManagerTest";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private ActivityTabProvider mTabSupplier;

    @Mock
    private TopUiThemeColorProvider mTopUiThemeColorProvider;

    private long mLastDownTime;

    private TabModelSelector mTabModelSelector;
    private LayoutManagerChrome mManager;
    private LayoutManagerChromePhone mManagerPhone;

    private final PointerProperties[] mProperties = new PointerProperties[2];
    private final PointerCoords[] mPointerCoords = new PointerCoords[2];

    private float mDpToPx;

    private OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier;

    public static class GridTabSwitcherParams implements ParameterProvider {
        private static List<ParameterSet> sGridTabSwitcherParams =
                Arrays.asList(new ParameterSet().value(false).name("GridTabSwicherDisabled"),
                        new ParameterSet().value(true).name("GridTabSwicherEnabled"));

        @Override
        public List<ParameterSet> getParameters() {
            return sGridTabSwitcherParams;
        }
    }

    class LayoutObserverCallbackHelper extends CallbackHelper {
        @LayoutType
        public int layoutType;
    }

    private void initializeMotionEvent() {
        mProperties[0] = new PointerProperties();
        mProperties[0].id = 0;
        mProperties[0].toolType = MotionEvent.TOOL_TYPE_FINGER;
        mProperties[1] = new PointerProperties();
        mProperties[1].id = 1;
        mProperties[1].toolType = MotionEvent.TOOL_TYPE_FINGER;

        mPointerCoords[0] = new PointerCoords();
        mPointerCoords[0].x = 0;
        mPointerCoords[0].y = 0;
        mPointerCoords[0].pressure = 1;
        mPointerCoords[0].size = 1;
        mPointerCoords[1] = new PointerCoords();
        mPointerCoords[1].x = 0;
        mPointerCoords[1].y = 0;
        mPointerCoords[1].pressure = 1;
        mPointerCoords[1].size = 1;
    }

    private void setAccessibilityEnabledForTesting(Boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(value));
    }

    /**
     * Simulates time so the animation updates.
     * @param layoutManager The {@link LayoutManagerChrome} to update.
     * @param maxFrameCount The maximum number of frames to simulate before the motion ends.
     * @return              Whether the maximum number of frames was enough for the
     *                      {@link LayoutManagerChrome} to reach the end of the animations.
     */
    private static boolean simulateTime(LayoutManagerChrome layoutManager, int maxFrameCount) {
        // Simulating time
        int frame = 0;
        long time = 0;
        final long dt = 16;
        while (layoutManager.onUpdate(time, dt) && frame < maxFrameCount) {
            time += dt;
            frame++;
        }
        Log.w(TAG, "simulateTime frame " + frame);
        return frame < maxFrameCount;
    }

    private void initializeLayoutManagerPhone(int standardTabCount, int incognitoTabCount) {
        initializeLayoutManagerPhone(standardTabCount, incognitoTabCount,
                TabModel.INVALID_TAB_INDEX, TabModel.INVALID_TAB_INDEX, false);
    }

    private void initializeLayoutManagerPhone(int standardTabCount, int incognitoTabCount,
            int standardIndexSelected, int incognitoIndexSelected, boolean incognitoSelected) {
        Context context = new MockContextForLayout(InstrumentationRegistry.getContext());

        mDpToPx = context.getResources().getDisplayMetrics().density;

        mTabModelSelector = new MockTabModelSelector(standardTabCount, incognitoTabCount, this);
        if (standardIndexSelected != TabModel.INVALID_TAB_INDEX) {
            TabModelUtils.setIndex(mTabModelSelector.getModel(false), standardIndexSelected);
        }
        if (incognitoIndexSelected != TabModel.INVALID_TAB_INDEX) {
            TabModelUtils.setIndex(mTabModelSelector.getModel(true), incognitoIndexSelected);
        }
        mTabModelSelector.selectModel(incognitoSelected);
        Assert.assertNotNull(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter());

        LayoutManagerHost layoutManagerHost = new MockLayoutHost(context);
        TabContentManager tabContentManager = new TabContentManager(context, null, false, null);
        tabContentManager.initWithNative();

        // Build a fake content container
        FrameLayout parentContainer = new FrameLayout(context);
        FrameLayout container = new FrameLayout(context);
        parentContainer.addView(container);

        ObservableSupplierImpl<TabContentManager> tabContentManagerSupplier =
                new ObservableSupplierImpl<>();
        OneshotSupplierImpl<OverviewModeBehavior> overviewModeBehaviorSupplier =
                new OneshotSupplierImpl<>();

        if (mLayoutStateProviderSupplier == null) {
            mLayoutStateProviderSupplier = new OneshotSupplierImpl<>();
        }

        mManagerPhone = new LayoutManagerChromePhone(layoutManagerHost, container, null,
                tabContentManagerSupplier, null, overviewModeBehaviorSupplier,
                mLayoutStateProviderSupplier, () -> mTopUiThemeColorProvider);
        tabContentManagerSupplier.set(tabContentManager);
        mManager = mManagerPhone;
        CompositorAnimationHandler.setTestingMode(true);
        mManager.init(mTabModelSelector, null, null, null);
        initializeMotionEvent();
    }

    private void eventDown(long time, PointF p) {
        mLastDownTime = time;

        mPointerCoords[0].x = p.x * mDpToPx;
        mPointerCoords[0].y = p.y * mDpToPx;

        MotionEvent event = MotionEvent.obtain(mLastDownTime, time, MotionEvent.ACTION_DOWN,
                1, mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0);
        Assert.assertTrue(
                "Down event not intercepted", mManager.onInterceptTouchEvent(event, false));
        Assert.assertTrue("Down event not handled", mManager.onTouchEvent(event));
    }

    private void eventDown1(long time, PointF p) {
        mPointerCoords[1].x = p.x * mDpToPx;
        mPointerCoords[1].y = p.y * mDpToPx;

        Assert.assertTrue("Down_1 event not handled",
                mManager.onTouchEvent(MotionEvent.obtain(mLastDownTime, time,
                        MotionEvent.ACTION_POINTER_DOWN
                                | (0x1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                        2, mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0)));
    }

    private void eventMove(long time, PointF p) {
        mPointerCoords[0].x = p.x * mDpToPx;
        mPointerCoords[0].y = p.y * mDpToPx;

        Assert.assertTrue("Move event not handled",
                mManager.onTouchEvent(
                        MotionEvent.obtain(mLastDownTime, time, MotionEvent.ACTION_MOVE, 1,
                                mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0)));
    }

    private void eventUp(long time, PointF p) {
        mPointerCoords[0].x = p.x * mDpToPx;
        mPointerCoords[0].y = p.y * mDpToPx;

        Assert.assertTrue("Up event not handled",
                mManager.onTouchEvent(MotionEvent.obtain(mLastDownTime, time, MotionEvent.ACTION_UP,
                        1, mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0)));
    }

    private void eventUp1(long time, PointF p) {
        mPointerCoords[1].x = p.x * mDpToPx;
        mPointerCoords[1].y = p.y * mDpToPx;

        Assert.assertTrue("Up_1 event not handled",
                mManager.onTouchEvent(MotionEvent.obtain(mLastDownTime, time,
                        MotionEvent.ACTION_POINTER_UP
                                | (0x1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                        2, mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0)));
    }

    private void eventMoveBoth(long time, PointF p0, PointF p1) {
        mPointerCoords[0].x = p0.x * mDpToPx;
        mPointerCoords[0].y = p0.y * mDpToPx;
        mPointerCoords[1].x = p1.x * mDpToPx;
        mPointerCoords[1].y = p1.y * mDpToPx;

        Assert.assertTrue("Move event not handled",
                mManager.onTouchEvent(
                        MotionEvent.obtain(mLastDownTime, time, MotionEvent.ACTION_MOVE, 2,
                                mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0)));
    }

    @Test
    @SmallTest
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testCreation() {
        initializeLayoutManagerPhone(0, 0);
    }

    @Test
    @SmallTest
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @DisabledTest(message = "https://crbug.com/965250")
    public void testStack() {
        initializeLayoutManagerPhone(3, 0);
        mManagerPhone.showOverview(true);
        Assert.assertTrue(
                "layoutManager is way too long to end motion", simulateTime(mManager, 1000));
        Assert.assertTrue("The activate layout type is expected to be StackLayout",
                mManager.getActiveLayout() instanceof StackLayout);
        mManagerPhone.hideOverview(true);
        Assert.assertTrue(
                "layoutManager is way too long to end motion", simulateTime(mManager, 1000));
    }

    @Test
    @SmallTest
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testStackNoAnimation() {
        initializeLayoutManagerPhone(1, 0);
        mManagerPhone.showOverview(false);
        Assert.assertTrue("The activate layout type is expected to be StackLayout",
                mManager.getActiveLayout() instanceof StackLayout);
        mManagerPhone.hideOverview(false);
    }

    /**
     * Tests the tab pinching behavior with two finger.
     * This test is still under development.
     */
    @Test
    @SmallTest
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testStackPinch() {
        initializeLayoutManagerPhone(5, 0);
        // Setting the index to the second to last element ensure the stack can be scrolled in both
        // directions.
        mManager.tabSelected(mTabModelSelector.getCurrentModel().getTabAt(3).getId(),
                Tab.INVALID_TAB_ID, false);

        mManagerPhone.showOverview(false);
        // Basic verifications
        Assert.assertTrue("The activate layout type is expected to be StackLayout",
                mManager.getActiveLayout() instanceof StackLayout);

        StackLayout layout = (StackLayout) mManager.getActiveLayout();
        Stack stack = layout.getTabStackAtIndex(StackLayout.NORMAL_STACK_INDEX);
        StackTab[] tabs = stack.getTabs();

        long time = 0;
        // At least one update is necessary to get updated positioning of LayoutTabs.
        mManager.onUpdate(time, 16);
        time++;

        LayoutTab tab1 = tabs[1].getLayoutTab();
        LayoutTab tab3 = tabs[3].getLayoutTab();

        float fingerOffset = Math.min(tab1.getClippedHeight() / 2, tab3.getClippedHeight() / 2);

        PointF finger0 = new PointF(tab1.getX() + tab1.getFinalContentWidth() / 2,
                tab1.getY() + fingerOffset);

        // Initiate finger 0
        eventDown(time, finger0);
        mManager.onUpdate(time, 16);
        time++;

        // Move finger 0: Y to simulate a scroll
        final float scrollOffset1 = (tab3.getY() - tab1.getY()) / 8.0f;
        finger0.y += scrollOffset1;

        eventMove(time, finger0);
        mManager.onUpdate(time, 16);
        time++;

        finger0.y -= scrollOffset1;

        eventMove(time, finger0);
        mManager.onUpdate(time, 16);
        time++;

        float expectedTab1X = tab1.getX();
        float expectedTab1Y = tab1.getY();

        // Initiate the pinch with finger 1
        PointF finger1 = new PointF(tab3.getX() + tab3.getFinalContentWidth() / 2,
                tab3.getY() + fingerOffset);
        eventDown1(time, finger1);
        mManager.onUpdate(time, 16);
        time++;

        final float delta = 0.001f;
        Assert.assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        Assert.assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        float expectedTab3X = tab3.getX();
        float expectedTab3Y = tab3.getY();

        // Move Finger 0: Y only
        finger0.y += scrollOffset1;
        expectedTab1Y += scrollOffset1;

        eventMoveBoth(time, finger0, finger1);
        mManager.onUpdate(time, 16);
        time++;

        Assert.assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        Assert.assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        Assert.assertEquals("Wrong x offset for tab 3", expectedTab3X, tab3.getX(), delta);
        Assert.assertEquals("Wrong y offset for tab 3", expectedTab3Y, tab3.getY(), delta);

        // Move finger 0: Y and X
        finger0.y += scrollOffset1;
        finger0.x += tab1.getFinalContentWidth() / 8.0f;
        expectedTab1Y += scrollOffset1;

        eventMoveBoth(time, finger0, finger1);
        mManager.onUpdate(time, 16);
        time++;

        Assert.assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        Assert.assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        Assert.assertEquals("Wrong x offset for tab 3", expectedTab3X, tab3.getX(), delta);
        Assert.assertEquals("Wrong y offset for tab 3", expectedTab3Y, tab3.getY(), delta);

        // Move finger 1: Y and X
        final float scrollOffset3 = (tab3.getY() - layout.getHeight()) / 8.0f;
        finger1.y += scrollOffset3;
        finger1.x += tab3.getFinalContentWidth() / 8.0f;
        expectedTab3Y += scrollOffset3;

        eventMoveBoth(time, finger0, finger1);
        mManager.onUpdate(time, 16);
        time++;

        Assert.assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        Assert.assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        Assert.assertEquals("Wrong x offset for tab 3", expectedTab3X, tab3.getX(), delta);
        Assert.assertEquals("Wrong y offset for tab 3", expectedTab3Y, tab3.getY(), delta);

        // Move finger 0 and 1: Y and X
        finger0.y += scrollOffset1;
        finger0.x += tab1.getFinalContentWidth() / 8.0f;
        expectedTab1Y += scrollOffset1;
        finger1.y += scrollOffset3;
        finger1.x += tab3.getFinalContentWidth() / 8.0f;
        expectedTab3Y += scrollOffset3;

        eventMoveBoth(time, finger0, finger1);
        mManager.onUpdate(time, 16);
        time++;

        Assert.assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        Assert.assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        Assert.assertEquals("Wrong x offset for tab 3", expectedTab3X, tab3.getX(), delta);
        Assert.assertEquals("Wrong y offset for tab 3", expectedTab3Y, tab3.getY(), delta);

        // Done
        eventUp1(time, finger1);
        eventUp(time, finger0);

        Assert.assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        Assert.assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        Assert.assertEquals("Wrong x offset for tab 3", expectedTab3X, tab3.getX(), delta);
        Assert.assertEquals("Wrong y offset for tab 3", expectedTab3Y, tab3.getY(), delta);

        mManagerPhone.hideOverview(false);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeOnlyTab() {
        initializeLayoutManagerPhone(1, 0, 0, TabModel.INVALID_TAB_INDEX, false);
        Assert.assertEquals(mTabModelSelector.getModel(false).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeOnlyTabIncognito() {
        initializeLayoutManagerPhone(0, 1, TabModel.INVALID_TAB_INDEX, 0, true);
        Assert.assertEquals(mTabModelSelector.getModel(true).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeNextTab() {
        initializeLayoutManagerPhone(2, 0, 0, TabModel.INVALID_TAB_INDEX, false);
        Assert.assertEquals(mTabModelSelector.getModel(false).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipePrevTab() {
        initializeLayoutManagerPhone(2, 0, 1, TabModel.INVALID_TAB_INDEX, false);
        Assert.assertEquals(mTabModelSelector.getModel(false).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeNextTabNone() {
        initializeLayoutManagerPhone(2, 0, 1, TabModel.INVALID_TAB_INDEX, false);
        Assert.assertEquals(mTabModelSelector.getModel(false).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipePrevTabNone() {
        initializeLayoutManagerPhone(2, 0, 0, TabModel.INVALID_TAB_INDEX, false);
        Assert.assertEquals(mTabModelSelector.getModel(false).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeNextTabIncognito() {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 0, true);
        Assert.assertEquals(mTabModelSelector.getModel(true).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipePrevTabIncognito() {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 1, true);
        Assert.assertEquals(mTabModelSelector.getModel(true).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeNextTabNoneIncognito() {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 1, true);
        Assert.assertEquals(mTabModelSelector.getModel(true).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipePrevTabNoneIncognito() {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 0, true);
        Assert.assertEquals(mTabModelSelector.getModel(true).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testStartSurfaceLayout_Disabled_LowEndPhone() throws Exception {
        // clang-format on
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, true);
        verifyOverviewListLayoutEnabled();

        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GROUPS_ANDROID, false);
        verifyOverviewListLayoutEnabled();

        // Test accessibility
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        setAccessibilityEnabledForTesting(true);
        verifyOverviewListLayoutEnabled();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    public void testStartSurfaceLayout_Disabled_HighEndPhone() throws Exception {
        // clang-format on
        verifyStackLayoutEnabled();

        // Verify accessibility
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        setAccessibilityEnabledForTesting(true);
        verifyOverviewListLayoutEnabled();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @Feature({"Android-TabSwitcher"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testStartSurfaceLayout_Disabled_AllPhone_Accessibility_WithoutContinuationFlag() {
        // clang-format on
        setAccessibilityEnabledForTesting(true);
        verifyOverviewListLayoutEnabled();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testStartSurfaceLayout_Enabled_HighEndPhone() throws Exception {
        // clang-format on
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, true);
        verifyStartSurfaceLayoutEnable(TabListCoordinator.TabListMode.GRID);

        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, false);
        verifyStartSurfaceLayoutEnable(TabListCoordinator.TabListMode.GRID);

        // Verify accessibility
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        setAccessibilityEnabledForTesting(true);
        verifyStartSurfaceLayoutEnable(TabListCoordinator.TabListMode.GRID);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testStartSurfaceLayout_Enabled_LowEndPhone() throws Exception {
        // clang-format on
        verifyStartSurfaceLayoutEnable(TabListCoordinator.TabListMode.LIST);

        // Test Accessibility
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        setAccessibilityEnabledForTesting(true);
        verifyStartSurfaceLayoutEnable(TabListCoordinator.TabListMode.LIST);
    }

    // TODO(crbug.com/1108496): Update the test to use Assert.assertThat for better failure message.
    @Test
    @MediumTest
    public void testLayoutObserverNotification_ShowAndHide_ToolbarSwipe() throws TimeoutException {
        LayoutObserverCallbackHelper startedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper startedHidingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedHidingCallback = new LayoutObserverCallbackHelper();

        setUpShowAndHideLayoutObserverNotification(startedShowingCallback, finishedShowingCallback,
                startedHidingCallback, finishedHidingCallback);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            performToolbarSideSwipe(ScrollDirection.RIGHT);
            Assert.assertEquals(
                    LayoutType.TOOLBAR_SWIPE, mManager.getActiveLayout().getLayoutType());
            Assert.assertTrue(
                    mLayoutStateProviderSupplier.get().isLayoutVisible(LayoutType.TOOLBAR_SWIPE));
        });

        // The |startedShowingCallback| callCount 0 is reserved for the default layout during
        // initialization. Because LayoutManager does not explicitly hide the old layout when a new
        // layout is forced to show, the callCount for |finishedShowingCallback|,
        // |startedHidingCallback|, and |finishedHidingCallback| are still 0.
        // TODO(crbug.com/1108496): update the callCount when LayoutManager explicitly hide the old
        // layout.
        startedShowingCallback.waitForCallback(1);
        Assert.assertEquals(LayoutType.TOOLBAR_SWIPE, startedShowingCallback.layoutType);

        finishedShowingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TOOLBAR_SWIPE, finishedShowingCallback.layoutType);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            finishToolbarSideSwipe();
            Assert.assertEquals(LayoutType.BROWSING, mManager.getActiveLayout().getLayoutType());
            Assert.assertTrue(
                    mLayoutStateProviderSupplier.get().isLayoutVisible(LayoutType.BROWSING));
        });

        startedHidingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TOOLBAR_SWIPE, startedHidingCallback.layoutType);

        finishedHidingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TOOLBAR_SWIPE, finishedHidingCallback.layoutType);

        startedShowingCallback.waitForCallback(2);
        Assert.assertEquals(LayoutType.BROWSING, startedShowingCallback.layoutType);

        finishedShowingCallback.waitForCallback(1);
        Assert.assertEquals(LayoutType.BROWSING, finishedShowingCallback.layoutType);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = N_MR1, message = "crbug.com/1139943")
    @ParameterAnnotations.UseMethodParameter(GridTabSwitcherParams.class)
    public void testLayoutObserverNotification_ShowAndHide_TabSwitcher(boolean isGridTabSwitcher)
            throws TimeoutException {
        CachedFeatureFlags.setForTesting(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, isGridTabSwitcher);
        LayoutObserverCallbackHelper startedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper startedHidingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedHidingCallback = new LayoutObserverCallbackHelper();

        setUpShowAndHideLayoutObserverNotification(startedShowingCallback, finishedShowingCallback,
                startedHidingCallback, finishedHidingCallback);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManager.showOverview(true);

            Assert.assertTrue(
                    "layoutManager is way too long to end motion", simulateTime(mManager, 1000));
            Assert.assertEquals(
                    LayoutType.TAB_SWITCHER, mManager.getActiveLayout().getLayoutType());
            Assert.assertTrue(
                    mLayoutStateProviderSupplier.get().isLayoutVisible(LayoutType.TAB_SWITCHER));
        });

        // The |startedShowingCallback| callCount 0 is reserved for the default layout during
        // initialization. Because LayoutManager does not explicitly hide the old layout when a new
        // layout is forced to show, the callCount for |finishedShowingCallback|,
        // |startedHidingCallback|, and |finishedHidingCallback| are still 0.
        // TODO(crbug.com/1108496): update the callCount when LayoutManager explicitly hide the old
        // layout.
        startedShowingCallback.waitForCallback(1);
        Assert.assertEquals(LayoutType.TAB_SWITCHER, startedShowingCallback.layoutType);

        finishedShowingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TAB_SWITCHER, finishedShowingCallback.layoutType);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManagerPhone.hideOverview(true);
            Assert.assertTrue(
                    "layoutManager is way too long to end motion", simulateTime(mManager, 1000));

            Assert.assertTrue(
                    mLayoutStateProviderSupplier.get().isLayoutVisible(LayoutType.BROWSING));
        });

        startedHidingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TAB_SWITCHER, startedHidingCallback.layoutType);

        finishedHidingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TAB_SWITCHER, finishedHidingCallback.layoutType);

        startedShowingCallback.waitForCallback(2);
        Assert.assertEquals(LayoutType.BROWSING, startedShowingCallback.layoutType);

        finishedShowingCallback.waitForCallback(1);
        Assert.assertEquals(LayoutType.BROWSING, finishedShowingCallback.layoutType);
    }

    @Test
    @MediumTest
    public void testLayoutObserverNotification_ShowAndHide_SimpleAnimation()
            throws TimeoutException {
        LayoutObserverCallbackHelper startedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper startedHidingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedHidingCallback = new LayoutObserverCallbackHelper();

        setUpShowAndHideLayoutObserverNotification(startedShowingCallback, finishedShowingCallback,
                startedHidingCallback, finishedHidingCallback);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = createTab(123, false);
            mTabModelSelector.getModel(false).addTab(
                    tab, -1, TabLaunchType.FROM_LONGPRESS_BACKGROUND, LIVE_IN_BACKGROUND);
            Assert.assertTrue("LayoutManager took too long to finish the animations",
                    simulateTime(mManager, 1000));
            Assert.assertThat("Incorrect active LayoutType",
                    mManager.getActiveLayout().getLayoutType(), is(LayoutType.SIMPLE_ANIMATION));
            Assert.assertThat("Incorrect active Layout",
                    mLayoutStateProviderSupplier.get().isLayoutVisible(LayoutType.SIMPLE_ANIMATION),
                    is(true));
        });

        // The |startedShowingCallback| callCount 0 is reserved for the default layout during
        // initialization. Because LayoutManager does not explicitly hide the old layout when a new
        // layout is forced to show, the callCount for |finishedShowingCallback|,
        // |startedHidingCallback|, and |finishedHidingCallback| are still 0.
        // TODO(crbug.com/1108496): update the callCount when LayoutManager explicitly hide the old
        // layout.
        startedShowingCallback.waitForCallback(1);
        Assert.assertThat("startedShowingCallback with incorrect LayoutType",
                startedShowingCallback.layoutType, is(LayoutType.SIMPLE_ANIMATION));

        finishedShowingCallback.waitForCallback(0);
        Assert.assertThat("finishedShowingCallback with incorrect LayoutType",
                finishedShowingCallback.layoutType, is(LayoutType.SIMPLE_ANIMATION));

        CriteriaHelper.pollUiThread(() -> {
            return mManagerPhone.getActiveLayout().getLayoutType() == LayoutType.SIMPLE_ANIMATION
                    && mManagerPhone.getActiveLayout().isStartingToHide();
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Simulate hiding animation.
            Assert.assertTrue("LayoutManager took too long to finish the animations",
                    simulateTime(mManager, 1000));
        });

        startedHidingCallback.waitForCallback(0);
        Assert.assertThat("startedHidingCallback with incorrect LayoutType",
                startedHidingCallback.layoutType, is(LayoutType.SIMPLE_ANIMATION));

        finishedHidingCallback.waitForCallback(0);
        Assert.assertThat("finishedHidingCallback with incorrectLayoutType",
                finishedHidingCallback.layoutType, is(LayoutType.SIMPLE_ANIMATION));

        startedShowingCallback.waitForCallback(2);
        Assert.assertThat("startedShowingCallback with incorrectLayoutType",
                startedShowingCallback.layoutType, is(LayoutType.BROWSING));

        finishedShowingCallback.waitForCallback(1);
        Assert.assertThat("finishedShowingCallback with incorrectLayoutType",
                finishedShowingCallback.layoutType, is(LayoutType.BROWSING));
    }

    private void setUpShowAndHideLayoutObserverNotification(
            LayoutObserverCallbackHelper startedShowingCallback,
            LayoutObserverCallbackHelper finishedShowingCallback,
            LayoutObserverCallbackHelper startedHidingCallback,
            LayoutObserverCallbackHelper finishedHidingCallback) throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLayoutStateProviderSupplier = new OneshotSupplierImpl<>();

            mLayoutStateProviderSupplier.onAvailable((layoutStateProvider) -> {
                layoutStateProvider.addObserver(new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(int layoutType, boolean showToolbar) {
                        Log.d(TAG, "Started to show: " + layoutType);
                        startedShowingCallback.layoutType = layoutType;
                        startedShowingCallback.notifyCalled();
                    }

                    @Override
                    public void onFinishedShowing(int layoutType) {
                        Log.d(TAG, "finished to show: " + layoutType);
                        finishedShowingCallback.layoutType = layoutType;
                        finishedShowingCallback.notifyCalled();
                    }

                    @Override
                    public void onStartedHiding(
                            int layoutType, boolean showToolbar, boolean delayAnimation) {
                        Log.d(TAG, "Started to hide: " + layoutType);
                        startedHidingCallback.layoutType = layoutType;
                        startedHidingCallback.notifyCalled();
                    }

                    @Override
                    public void onFinishedHiding(int layoutType) {
                        Log.d(TAG, "finished to hide: " + layoutType);
                        finishedHidingCallback.layoutType = layoutType;
                        finishedHidingCallback.notifyCalled();
                    }
                });
            });

            initializeLayoutManagerPhone(2, 0);
            Assert.assertEquals(LayoutType.BROWSING, mManager.getActiveLayout().getLayoutType());
        });

        startedShowingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.BROWSING, startedShowingCallback.layoutType);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = N_MR1, message = "crbug.com/1139943")
    public void testLayoutObserverNotification_TabSelectionHinted() throws TimeoutException {
        CallbackHelper tabSelectionHintedCallback = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLayoutStateProviderSupplier = new OneshotSupplierImpl<>();

            mLayoutStateProviderSupplier.onAvailable((layoutStateProvider) -> {
                layoutStateProvider.addObserver(new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onTabSelectionHinted(int tabId) {
                        Log.d(TAG, "onTabSelectionHinted");
                        tabSelectionHintedCallback.notifyCalled();
                    }
                });
            });

            initializeLayoutManagerPhone(2, 0);
            mManager.showOverview(true);

            Assert.assertTrue(
                    "layoutManager is way too long to end motion", simulateTime(mManager, 1000));
            Assert.assertEquals(
                    LayoutType.TAB_SWITCHER, mManager.getActiveLayout().getLayoutType());

            mManagerPhone.hideOverview(true);
            Assert.assertTrue(
                    "layoutManager is way too long to end motion", simulateTime(mManager, 1000));

            Assert.assertEquals(LayoutType.BROWSING, mManager.getActiveLayout().getLayoutType());
        });

        tabSelectionHintedCallback.waitForCallback(0);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Load the browser process.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeBrowserInitializer.getInstance().handleSynchronousStartup(); });
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, null);
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GROUPS_ANDROID, null);
        setAccessibilityEnabledForTesting(null);
    }

    /**
     * Verify {@link StackLayout} is in used. The {@link StackLayout} is used when
     * ChromeFeatureList.TAB_GROUPS_ANDROID or ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID is disabled
     * in high end phone.
     */
    private void verifyStackLayoutEnabled() {
        launchedChromeAndEnterTabSwitcher();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Layout activeLayout = getActiveLayout();
            Assert.assertTrue(activeLayout instanceof StackLayout);
        });
    }

    /**
     * Verify the {@link OverviewListLayout} is in used. The {@link OverviewListLayout} is used when
     * accessibility is turned on. It is also used for low end device.
     */
    private void verifyOverviewListLayoutEnabled() {
        launchedChromeAndEnterTabSwitcher();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Layout activeLayout = getActiveLayout();
            Assert.assertTrue(activeLayout instanceof OverviewListLayout);
        });
    }

    private void verifyStartSurfaceLayoutEnable(
            @TabListCoordinator.TabListMode int expectedTabListMode) {
        launchedChromeAndEnterTabSwitcher();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Layout activeLayout = getActiveLayout();
            Assert.assertTrue(activeLayout instanceof StartSurfaceLayout);

            StartSurfaceLayout startSurfaceLayout = (StartSurfaceLayout) activeLayout;

            Assert.assertEquals(expectedTabListMode,
                    startSurfaceLayout.getStartSurfaceForTesting()
                            .getTabListDelegate()
                            .getListModeForTesting());
        });
    }

    private void launchedChromeAndEnterTabSwitcher() {
        mActivityTestRule.startMainActivityOnBlankPage();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);

        LayoutManagerChrome layoutManager = mActivityTestRule.getActivity().getLayoutManager();
        TestThreadUtils.runOnUiThreadBlocking(() -> layoutManager.showOverview(false));
        CriteriaHelper.pollUiThread(layoutManager::overviewVisible);
    }

    private Layout getActiveLayout() {
        LayoutManagerChrome layoutManager = mActivityTestRule.getActivity().getLayoutManager();
        return layoutManager.getActiveLayout();
    }

    private void runToolbarSideSwipeTestOnCurrentModel(
            @ScrollDirection int direction, int finalIndex) {
        final TabModel model = mTabModelSelector.getCurrentModel();
        final int finalId = model.getTabAt(finalIndex).getId();

        performToolbarSideSwipe(direction);
        finishToolbarSideSwipe();

        Assert.assertEquals("Unexpected model change after side swipe", model.isIncognito(),
                mTabModelSelector.isIncognitoSelected());

        Assert.assertEquals("Wrong index after side swipe", finalIndex, model.index());
        Assert.assertEquals(
                "Wrong current tab id", finalId, TabModelUtils.getCurrentTab(model).getId());
        Assert.assertTrue("LayoutManager#getActiveLayout() should be StaticLayout",
                mManager.getActiveLayout() instanceof StaticLayout);
    }

    private void performToolbarSideSwipe(@ScrollDirection int direction) {
        Assert.assertTrue("Unexpected direction for side swipe " + direction,
                direction == ScrollDirection.LEFT || direction == ScrollDirection.RIGHT);

        final Layout layout = mManager.getActiveLayout();
        final SwipeHandler eventHandler = mManager.getToolbarSwipeHandler();

        Assert.assertNotNull("LayoutManager#getToolbarSwipeHandler() returned null", eventHandler);
        Assert.assertNotNull("LayoutManager#getActiveLayout() returned null", layout);

        final float layoutWidth = layout.getWidth();
        final boolean scrollLeft = direction == ScrollDirection.LEFT;
        final float deltaX = MathUtils.flipSignIf(layoutWidth / 2.f, scrollLeft);

        eventHandler.onSwipeStarted(direction, createMotionEvent(layoutWidth * mDpToPx, 0));
        // Call swipeUpdated twice since the handler computes direction in that method.
        // TODO(mdjones): Update implementation of EdgeSwipeHandler to work this way by default.
        MotionEvent ev = createMotionEvent(deltaX * mDpToPx, 0.f);
        eventHandler.onSwipeUpdated(ev, deltaX * mDpToPx, 0.f, deltaX * mDpToPx, 0.f);
        eventHandler.onSwipeUpdated(ev, deltaX * mDpToPx, 0.f, deltaX * mDpToPx, 0.f);
        Assert.assertTrue("LayoutManager#getActiveLayout() should be ToolbarSwipeLayout",
                mManager.getActiveLayout() instanceof ToolbarSwipeLayout);
        Assert.assertTrue("LayoutManager took too long to finish the animations",
                simulateTime(mManager, 1000));
    }

    private void finishToolbarSideSwipe() {
        final SwipeHandler eventHandler = mManager.getToolbarSwipeHandler();
        Assert.assertNotNull("LayoutManager#getToolbarSwipeHandler() returned null", eventHandler);

        eventHandler.onSwipeFinished();
        Assert.assertTrue("LayoutManager took too long to finish the animations",
                simulateTime(mManager, 1000));
    }

    @Override
    public Tab createTab(int id, boolean incognito) {
        return MockTab.createAndInitialize(id, incognito);
    }
}
