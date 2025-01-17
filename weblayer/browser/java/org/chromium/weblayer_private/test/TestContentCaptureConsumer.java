// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.test;

import org.chromium.components.content_capture.ContentCaptureConsumer;
import org.chromium.components.content_capture.ContentCaptureFrame;
import org.chromium.components.content_capture.FrameSession;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;

/**
 * A test ContentCaptureConsumer for ContentCaptureTest.
 */
public class TestContentCaptureConsumer extends ContentCaptureConsumer {
    public static final int CONTENT_CAPTURED = 1;
    public static final int CONTENT_UPDATED = 2;
    public static final int CONTENT_REMOVED = 3;
    public static final int SESSION_REMOVED = 4;
    public static final int TITLE_UPDATED = 5;

    public TestContentCaptureConsumer(
            WebContents webContents, Runnable onNewEvents, ArrayList<Integer> eventsObserved) {
        super(webContents);
        mOnNewEvents = onNewEvents;
        mEventsObserved = eventsObserved;
    }

    @Override
    public void onContentCaptured(
            FrameSession parentFrame, ContentCaptureFrame contentCaptureData) {
        mEventsObserved.add(CONTENT_CAPTURED);
        mOnNewEvents.run();
    }

    @Override
    public void onContentUpdated(FrameSession parentFrame, ContentCaptureFrame contentCaptureData) {
        mEventsObserved.add(CONTENT_UPDATED);
        mOnNewEvents.run();
    }

    @Override
    public void onSessionRemoved(FrameSession session) {
        mEventsObserved.add(SESSION_REMOVED);
        mOnNewEvents.run();
    }

    @Override
    public void onContentRemoved(FrameSession session, long[] removedIds) {
        mEventsObserved.add(CONTENT_REMOVED);
        mOnNewEvents.run();
    }

    @Override
    public void onTitleUpdated(ContentCaptureFrame mainFrame) {
        mEventsObserved.add(TITLE_UPDATED);
        mOnNewEvents.run();
    }

    @Override
    public boolean shouldCapture(String[] urls) {
        return true;
    }

    private ArrayList<Integer> mEventsObserved;
    private Runnable mOnNewEvents;
}
