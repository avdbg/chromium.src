// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_coexistence_css.js';
import './edu_coexistence_template.js';
import './edu_coexistence_button.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';


Polymer({
  is: 'edu-coexistence-error',

  _template: html`{__html_template__}`,

  listeners: {
    'go-action': 'closeDialog_',
  },

  /**
   * Attempts to close the dialog.  In OOBE, this will move on
   * to the next screen of OOBE (not the next screen of this flow).
   * @private
   */
  closeDialog_() {
    EduCoexistenceBrowserProxyImpl.getInstance().dialogClose();
  },
});
