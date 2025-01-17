// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-content-dialog',

  behaviors: [OobeFocusBehavior],

  properties: {
    /**
     * Hide the box shadow on the top of oobe-bottom
     */
    hideShadow: {
      type: Boolean,
      value: false,
    },

    /**
     * Removes footer padding.
     */
    noFooterPadding: {
      type: Boolean,
      value: false,
    },

    /**
     * If true footer would be shrunk as much as possible to fit container.
     */
    footerShrinkable: {
      type: Boolean,
      value: false,
    },

    /**
     * If set, prevents lazy instantiation of the dialog.
     */
    noLazy: {
      type: Boolean,
      value: false,
    },

    /**
     * Supports loading dialog which is shown without buttons.
     */
    noButtons: {
      type: Boolean,
      value: false,
    },
  },

  focus() {
    /**
     * TODO (crbug.com/1159721): Fix this once event flow of showing step in
     * display_manager is updated.
     */
    this.show();
  },

  show() {
    this.focusMarkedElement(this);
  },

  onBeforeShow() {
    this.$.dialog.onBeforeShow();
  },

  /**
   * Scroll to the bottom of footer container.
   */
  scrollToBottom() {
    this.$.dialog.scrollToBottom();
  },
});
