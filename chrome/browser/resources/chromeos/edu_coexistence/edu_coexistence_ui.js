// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_coexistence_css.js';
import './edu_coexistence_template.js';
import './edu_coexistence_button.js';
import './gaia_action_buttons.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AuthParams} from '../../gaia_auth_host/authenticator.m.js';
import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';
import {EduCoexistenceController, EduCoexistenceParams} from './edu_coexistence_controller.js';

Polymer({
  is: 'edu-coexistence-ui',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Indicates whether the page is loading.
     * @private {boolean}
     */
    loading_: {
      type: Boolean,
      value: true,
    },

    /**
     * Indicates whether the GAIA buttons should be shown.
     * @private {boolean}
     */
    showGaiaButtons_: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates whether the GAIA "Next" button should be shown.
     * @private {boolean}
     */
    showGaiaNextButton_: {
      type: Boolean,
      value: false,
    },



    /**
     * The EDU Ceoxistence controller instance.
     * @private {?EduCoexistenceController}
     */
    controller_: Object,
  },

  /** Attempts to close the dialog */
  closeDialog_() {
    EduCoexistenceBrowserProxyImpl.getInstance().dialogClose();
  },

  loadAuthExtension_(data) {
    // Set up the controller.
    this.controller_.loadAuthExtension(data);

    this.webview_.addEventListener('contentload', () => {
      this.loading_ = false;
      this.configureUiForGaiaFlow();
    });
  },

  handleGaiaLoginGoBack_(e) {
    e.stopPropagation();
    let backButton = this.root.getElementById('gaia-back-button');
    if (backButton.disabled) {
      // This is a safeguard against this method getting called somehow
      // despite the button being disabled.
      return;
    }
    backButton.disabled = true;

    this.webview_.back((success /* ignored */) => {
      // Wait a full second after the callback fires before processing another
      // click on the back button.  This delay is needed because the callback
      // fires before the content finishes navigating to the previous page.
      setTimeout(() => {
        backButton.disabled = false;
      }, 1000 /* 1 second */);
      this.webview_.focus();
    });
  },

  /**
   * Configures the UI for showing/hiding the GAIA login flow.
   */
  configureUiForGaiaFlow() {
    var currentUrl = new URL(this.webview_.src);
    var mainDiv = this.$$('edu-coexistence-template').$$('div.main');

    if (currentUrl.hostname !== this.controller_.getFlowOriginHostname()) {
      // Show the GAIA Buttons.
      this.showGaiaButtons_ = true;
      // Shrink the main div so that the buttons line up more closely with the
      // server rendered buttons.
      mainDiv.style.height = 'calc(100% - 90px)';

      // Don't show the "Next" button if the EDU authentication got forwarded to
      // a non-Google SSO page.
      this.showGaiaNextButton_ = currentUrl.hostname.endsWith('.google.com');

    } else {
      // Hide the GAIA Buttons.
      this.showGaiaButtons_ = false;

      // Hide the GAIA Next button.
      this.showGaiaNextButton_ = false;

      // Restore the main div to 100%
      mainDiv.style.height = '100%';
    }
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'load-auth-extension', data => this.loadAuthExtension_(data));

    EduCoexistenceBrowserProxyImpl.getInstance().initializeEduArgs().then(
        (data) => {
          this.webview_ =
              /** @type {!WebView} */ (this.$.signinFrame);
          this.controller_ =
              new EduCoexistenceController(this, this.webview_, data);

          EduCoexistenceBrowserProxyImpl.getInstance().initializeLogin();
        },
        (err) => {
          this.fire('go-error');
          EduCoexistenceBrowserProxyImpl.getInstance().onError(
              ['There was an error getting edu coexistence data']);
        });
  },
});
