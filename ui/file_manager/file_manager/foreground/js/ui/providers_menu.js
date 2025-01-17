// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {Menu} from 'chrome://resources/js/cr/ui/menu.m.js';
// #import {ProvidersModel} from '../providers_model.m.js';
// #import {util} from '../../../common/js/util.m.js';
// #import {FilesMenuItem} from './files_menu.m.js';
// #import {decorate} from 'chrome://resources/js/cr/ui.m.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';

/**
 * Fills out the menu for mounting or installing new providers.
 */
/* #export */ class ProvidersMenu {
  /**
   * @param {!ProvidersModel} model
   * @param {!cr.ui.Menu} menu
   */
  constructor(model, menu) {
    /**
     * @private {!ProvidersModel}}
     * @const
     */
    this.model_ = model;

    /**
     * @private {!cr.ui.Menu}
     * @const
     */
    this.menu_ = menu;

    this.menu_.addEventListener('update', this.onUpdate_.bind(this));
  }

  /**
   * @private
   */
  clearProviders_() {
    while (this.menu_.firstChild) {
      this.menu_.removeChild(this.menu_.lastChild);
    }
  }

  /**
   * @return {!cr.ui.FilesMenuItem}
   * @private
   */
  addMenuItem_() {
    const menuItem = this.menu_.addMenuItem({});
    cr.ui.decorate(/** @type {!Element} */ (menuItem), cr.ui.FilesMenuItem);
    return /** @type {!cr.ui.FilesMenuItem} */ (menuItem);
  }

  /**
   * @param {string} providerId ID of the provider.
   * @param {!chrome.fileManagerPrivate.IconSet} iconSet Set of icons for the
   * provider.
   * @param {string} name Already localized name of the provider.
   * @private
   */
  addProvider_(providerId, iconSet, name) {
    const item = this.addMenuItem_();
    item.label = name;

    const iconImage = util.iconSetToCSSBackgroundImageValue(iconSet);
    item.iconStartImage = iconImage;

    item.addEventListener(
        'activate', this.onItemActivate_.bind(this, providerId));
  }

  /**
   * @param {!Event} event
   * @private
   */
  onUpdate_(event) {
    this.model_.getMountableProviders().then(providers => {
      this.clearProviders_();
      providers.forEach(provider => {
        this.addProvider_(provider.providerId, provider.iconSet, provider.name);
      });
    });
  }

  /**
   * @param {string} providerId
   * @param {!Event} event
   * @private
   */
  onItemActivate_(providerId, event) {
    this.model_.requestMount(providerId);
  }

  /**
   *  Sends an 'update' event to the sub menu to trigger
   *  a reload of its content.
   */
  updateSubMenu() {
    const updateEvent = new Event('update');
    this.menu_.dispatchEvent(updateEvent);
  }
}
