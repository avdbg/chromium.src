// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {alphabeticalCompare, getColorModeString} from './scanning_app_util.js';
import {SelectBehavior} from './select_behavior.js';

/** @type {chromeos.scanning.mojom.ColorMode} */
const DEFAULT_COLOR_MODE = chromeos.scanning.mojom.ColorMode.kColor;

/**
 * @fileoverview
 * 'color-mode-select' displays the available scanner color modes in a dropdown.
 */
Polymer({
  is: 'color-mode-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

  /**
   * @param {number} index
   * @return {string}
   */
  getOptionAtIndex(index) {
    assert(index < this.options.length);

    return this.options[index].toString();
  },

  /**
   * @param {!chromeos.scanning.mojom.ColorMode} mojoColorMode
   * @return {string}
   * @private
   */
  getColorModeString_(mojoColorMode) {
    return getColorModeString(mojoColorMode);
  },

  sortOptions() {
    this.options.sort((a, b) => {
      return alphabeticalCompare(getColorModeString(a), getColorModeString(b));
    });
  },

  /**
   * @param {!chromeos.scanning.mojom.ColorMode} option
   * @return {boolean}
   */
  isDefaultOption(option) {
    return option === DEFAULT_COLOR_MODE;
  },
});
