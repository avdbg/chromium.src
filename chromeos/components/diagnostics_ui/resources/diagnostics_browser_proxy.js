// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the Diagnostics App UI in chromeos/ to
 * provide access to the SessionLogHandler which invokes functions that only
 * exist in chrome/.
 */

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class DiagnosticsBrowserProxy {
  /** Initialize SessionLogHandler. */
  initialize() {}

  /**
   * Saves the session log to the selected location.
   * @return {!Promise<boolean>}
   */
  saveSessionLog() {}
}

/** @implements {DiagnosticsBrowserProxy} */
export class DiagnosticsBrowserProxyImpl {
  /** @override */
  initialize() {
    chrome.send('initialize');
  }

  /** @override */
  saveSessionLog() {
    return sendWithPromise('saveSessionLog');
  }
}

// The singleton instance_ can be replaced with a test version of this wrapper
// during testing.
addSingletonGetter(DiagnosticsBrowserProxyImpl);
