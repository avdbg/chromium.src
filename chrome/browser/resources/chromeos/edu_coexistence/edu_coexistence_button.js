// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */
const ButtonTypes = {
  ACTION: 'action',
  BACK: 'back',
};

Polymer({
  is: 'edu-coexistence-button',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Set button type.
     * @type {!ButtonTypes}
     */
    buttonType: {
      type: String,
      value: ButtonTypes.ACTION,
    },
    /**
     * 'disabled' button attribute.
     * @type {Boolean}
     */
    disabled: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  ready() {
    this.assertButtonType_(this.buttonType);
  },

  /**
   * @param {!ButtonTypes} buttonType
   * @private
   */
  assertButtonType_(buttonType) {
    assert(Object.values(ButtonTypes).includes(buttonType));
  },

  /**
   * @param {!ButtonTypes} buttonType
   * @return {string} CSS class name
   * @private
   */
  getClass_(buttonType) {
    this.assertButtonType_(buttonType);
    if (buttonType === ButtonTypes.BACK) {
      return '';
    }

    return 'action-button';
  },

  /**
   * @param {!ButtonTypes} buttonType
   * @return {boolean} Whether the button should have an icon before text
   * @private
   */
  hasIconBeforeText_(buttonType) {
    this.assertButtonType_(buttonType);
    return buttonType === ButtonTypes.BACK;
  },

  /**
   * @param {!ButtonTypes} buttonType
   * @return {boolean} Whether the button should have an icon after text
   * @private
   */
  hasIconAfterText_(buttonType) {
    this.assertButtonType_(buttonType);
    return false;
  },

  /**
   * @param {!ButtonTypes} buttonType
   * @return {string} Icon
   * @private
   */
  getIcon_(buttonType) {
    this.assertButtonType_(buttonType);
    if (buttonType === ButtonTypes.BACK) {
      return isRTL() ? 'cr:chevron-right' : 'cr:chevron-left';
    }
    return '';
  },

  /**
   * @param {!ButtonTypes} buttonType
   * @return {string} Localized button text
   * @private
   */
  getDisplayName_(buttonType) {
    this.assertButtonType_(buttonType);

    if (buttonType === ButtonTypes.BACK) {
      return this.i18n('backButton');
    }
    if (buttonType === ButtonTypes.ACTION) {
      return this.i18n('nextButton');
    }
    return '';  // unreached
  },

  /**
   * @param {!Event} e
   * @private
   */
  onTap_(e) {
    if (this.disabled) {
      e.stopPropagation();
      return;
    }
    if (this.buttonType === ButtonTypes.BACK) {
      this.fire('go-back');
      return;
    }
    if (this.buttonType === ButtonTypes.ACTION) {
      this.fire('go-action');
      return;
    }
  }

});
