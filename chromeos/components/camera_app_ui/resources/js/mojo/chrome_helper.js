// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {windowController} from '../window_controller.js';

import {closeWhenUnload} from './util.js';

/**
 * The singleton instance of ChromeHelper. Initialized by the first
 * invocation of getInstance().
 * @type {?ChromeHelper}
 */
let instance = null;

/**
 * Communicates with Chrome.
 */
export class ChromeHelper {
  /**
   * @public
   */
  constructor() {
    /**
     * An interface remote that is used to communicate with Chrome.
     * @type {!chromeosCamera.mojom.CameraAppHelperRemote}
     */
    this.remote_ = chromeosCamera.mojom.CameraAppHelper.getRemote();

    closeWhenUnload(this.remote_);
  }

  /**
   * Starts tablet mode monitor monitoring tablet mode state of device.
   * @param {function(boolean): void} onChange Callback called each time when
   *     tablet mode state of device changes with boolean parameter indecating
   *     whether device is entering tablet mode.
   * @return {!Promise<boolean>} Resolved to initial state of whether device is
   *     is in tablet mode.
   */
  async initTabletModeMonitor(onChange) {
    const monitorCallbackRouter =
        new chromeosCamera.mojom.TabletModeMonitorCallbackRouter();
    closeWhenUnload(monitorCallbackRouter);
    monitorCallbackRouter.update.addListener(onChange);

    return (await this.remote_.setTabletMonitor(
                monitorCallbackRouter.$.bindNewPipeAndPassRemote()))
        .isTabletMode;
  }

  /**
   * Starts monitor monitoring system screen state of device.
   * @param {function(!chromeosCamera.mojom.ScreenState): void} onChange
   *     Callback called each time when device screen state changes with
   *     parameter of newly changed value.
   * @return {!Promise<!chromeosCamera.mojom.ScreenState>} Resolved to initial
   *     system screen state.
   */
  async initScreenStateMonitor(onChange) {
    const monitorCallbackRouter =
        new chromeosCamera.mojom.ScreenStateMonitorCallbackRouter();
    closeWhenUnload(monitorCallbackRouter);
    monitorCallbackRouter.update.addListener(onChange);

    return (await this.remote_.setScreenStateMonitor(
                monitorCallbackRouter.$.bindNewPipeAndPassRemote()))
        .initialState;
  }

  /**
   * Starts monitor monitoring the existence of external screens.
   * @param {function(boolean): void} onChange Callback called when the
   *     existence of external screens changes.
   * @return {!Promise<boolean>} Resolved to the initial state.
   */
  async initExternalScreenMonitor(onChange) {
    const monitorCallbackRouter =
        new chromeosCamera.mojom.ExternalScreenMonitorCallbackRouter();
    closeWhenUnload(monitorCallbackRouter);
    monitorCallbackRouter.update.addListener(onChange);

    return (await this.remote_.setExternalScreenMonitor(
                monitorCallbackRouter.$.bindNewPipeAndPassRemote()))
        .hasExternalScreen;
  }

  /**
   * Checks if the device is under tablet mode currently.
   * @return {!Promise<boolean>}
   */
  async isTabletMode() {
    const {isTabletMode} = await this.remote_.isTabletMode();
    return isTabletMode;
  }

  /**
   * Starts camera usage monitor.
   * @param {function(): !Promise} exploitUsage
   * @param {function(): !Promise} releaseUsage
   * @return {!Promise}
   */
  async initCameraUsageMonitor(exploitUsage, releaseUsage) {
    const usageCallbackRouter =
        new chromeosCamera.mojom.CameraUsageOwnershipMonitorCallbackRouter();
    closeWhenUnload(usageCallbackRouter);

    usageCallbackRouter.onCameraUsageOwnershipChanged.addListener(
        async (hasUsage) => {
          if (hasUsage) {
            await exploitUsage();
          } else {
            await releaseUsage();
          }
        });

    await this.remote_.setCameraUsageMonitor(
        usageCallbackRouter.$.bindNewPipeAndPassRemote());

    const {controller} = await this.remote_.getWindowStateController();
    await windowController.bind(controller);
  }

  /**
   * Triggers the begin of event tracing in Chrome.
   * @param {string} event Name of the event.
   */
  startTracing(event) {
    this.remote_.startPerfEventTrace(event);
  }

  /**
   * Triggers the end of event tracing in Chrome.
   * @param {string} event Name of the event.
   */
  stopTracing(event) {
    this.remote_.stopPerfEventTrace(event);
  }

  /**
   * Opens the file in Downloads folder by its |name| in gallery.
   * @param {string} name Name of the target file.
   */
  openFileInGallery(name) {
    this.remote_.openFileInGallery(name);
  }

  /**
   * Opens the chrome feedback dialog.
   * @param {string} placeholder The text of the placeholder in the description
   *     field.
   */
  openFeedbackDialog(placeholder) {
    this.remote_.openFeedbackDialog(placeholder);
  }

  /**
   * Checks return value from |handleCameraResult|.
   * @param {string} caller Caller identifier.
   * @param {!Promise<{isSuccess: boolean}>|null} value
   * @return {!Promise}
   */
  async checkReturn_(caller, value) {
    const ret = await value;
    if (ret === null) {
      console.error(`Return null from calling intent ${caller}.`);
      return;
    }
    if (!ret.isSuccess) {
      console.error(`Return not isSuccess from calling intent ${caller}.`);
    }
  }

  /**
   * Notifies ARC++ to finish the intent.
   * @param {number} intentId Intent id of the intent to be finished.
   * @return {!Promise}
   */
  async finish(intentId) {
    const ret = this.remote_.handleCameraResult(
        intentId, arc.mojom.CameraIntentAction.FINISH, []);
    await this.checkReturn_('finish()', ret);
  }

  /**
   * Notifies ARC++ to cancel the intent.
   * @param {number} intentId Intent id of the intent to be canceled.
   * @return {!Promise}
   */
  async cancel(intentId) {
    const ret = this.remote_.handleCameraResult(
        intentId, arc.mojom.CameraIntentAction.CANCEL, []);
    await this.checkReturn_('cancel()', ret);
  }

  /**
   * Forces casting type from Uint8Array to !Array<number>.
   * @param {!Uint8Array} data
   * @return {!Array<number>}
   * @suppress {checkTypes}
   * @private
   */
  static castResultType_(data) {
    return data;
  }

  /**
   * Notifies ARC++ to append data to intent result.
   * @param {number} intentId Intent id of the intent to be appended data to.
   * @param {!Uint8Array} data The data to be appended to intent result.
   * @return {!Promise}
   */
  async appendData(intentId, data) {
    const ret = this.remote_.handleCameraResult(
        intentId, arc.mojom.CameraIntentAction.APPEND_DATA,
        this.constructor.castResultType_(data));
    await this.checkReturn_('appendData()', ret);
  }

  /**
   * Notifies ARC++ to clear appended intent result data.
   * @param {number} intentId Intent id of the intent to be cleared its result.
   * @return {!Promise}
   */
  async clearData(intentId) {
    const ret = this.remote_.handleCameraResult(
        intentId, arc.mojom.CameraIntentAction.CLEAR_DATA, []);
    await this.checkReturn_('clearData()', ret);
  }

  /**
   * Adds listener for screen locked event.
   * @param {function(boolean): void} callback Callback for screen locked status
   *     changed. Called with the latest status of whether screen is locked.
   */
  async addOnLockListener(callback) {
    const monitorCallbackRouter = new blink.mojom.IdleMonitorCallbackRouter();
    closeWhenUnload(monitorCallbackRouter);
    monitorCallbackRouter.update.addListener((newState) => {
      callback(newState.screen === blink.mojom.ScreenIdleState.kLocked);
    });

    const idleManager = blink.mojom.IdleManager.getRemote();
    closeWhenUnload(idleManager);
    // Set a large threshold since we don't care about user idle. Note that
    // ESLint does not yet seem to know about BigInt, so it complains about an
    // uppercase "function" being used as something other than a constructor,
    // and about BigInt not existing.
    // eslint-disable-next-line new-cap, no-undef
    const threshold = {microseconds: BigInt(86400000000)};
    const {state} = await idleManager.addMonitor(
        threshold, monitorCallbackRouter.$.bindNewPipeAndPassRemote());
    callback(state.screen === blink.mojom.ScreenIdleState.kLocked);
  }

  /**
   * Checks if the logging consent option is enabled.
   * @return {!Promise<boolean>}
   */
  async isMetricsAndCrashReportingEnabled() {
    const {isEnabled} = await this.remote_.isMetricsAndCrashReportingEnabled();
    return isEnabled;
  }

  /**
   * Sends the broadcast to ARC to notify the new photo/video is captured.
   * @param {{isVideo: boolean, name: string}} info
   * @return {!Promise}
   */
  async sendNewCaptureBroadcast({isVideo, name}) {
    this.remote_.sendNewCaptureBroadcast(isVideo, name);
  }

  /**
   * Creates a new instance of ChromeHelper if it is not set. Returns the
   *     exist instance.
   * @return {!ChromeHelper} The singleton instance.
   */
  static getInstance() {
    if (instance === null) {
      instance = new ChromeHelper();
    }
    return instance;
  }
}
