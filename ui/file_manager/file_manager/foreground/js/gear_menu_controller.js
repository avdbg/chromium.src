// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {ProvidersModel} from './providers_model.m.js';
// #import {CommandHandler} from './file_manager_commands.m.js';
// #import {DirectoryModel} from './directory_model.m.js';
// #import {ProvidersMenu} from './ui/providers_menu.m.js';
// #import {GearMenu} from './ui/gear_menu.m.js';
// #import {MultiMenuButton} from './ui/multi_menu_button.m.js';
// #import {VolumeManagerCommon} from '../../../base/js/volume_manager_types.m.js';
// #import {DirectoryChangeEvent} from '../../../externs/directory_change_event.m.js';
// #import {str, util} from '../../common/js/util.m.js';
// clang-format on


/* #export */ class GearMenuController {
  /**
   * @param {!cr.ui.MultiMenuButton} gearButton
   * @param {!FilesToggleRippleElement} toggleRipple
   * @param {!GearMenu} gearMenu
   * @param {!ProvidersMenu} providersMenu
   * @param {!DirectoryModel} directoryModel
   * @param {!CommandHandler} commandHandler
   * @param {!ProvidersModel} providersModel
   */
  constructor(
      gearButton, toggleRipple, gearMenu, providersMenu, directoryModel,
      commandHandler, providersModel) {
    /** @private @const {!cr.ui.MultiMenuButton} */
    this.gearButton_ = gearButton;

    /** @private @const {!FilesToggleRippleElement} */
    this.toggleRipple_ = toggleRipple;

    /** @private @const {!GearMenu} */
    this.gearMenu_ = gearMenu;

    /** @private @const {!ProvidersMenu} */
    this.providersMenu_ = providersMenu;

    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!CommandHandler} */
    this.commandHandler_ = commandHandler;

    /** @private @const {!ProvidersModel} */
    this.providersModel_ = providersModel;

    gearButton.addEventListener('menushow', this.onShowGearMenu_.bind(this));
    gearButton.addEventListener('menuhide', this.onHideGearMenu_.bind(this));
    directoryModel.addEventListener(
        'directory-changed', this.onDirectoryChanged_.bind(this));
    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        this.onPreferencesChanged_.bind(this));
    this.onPreferencesChanged_();
  }

  /**
   * @private
   */
  onShowGearMenu_() {
    this.toggleRipple_.activated = true;
    this.refreshRemainingSpace_(false); /* Without loading caption. */

    this.providersModel_.getMountableProviders().then(providers => {
      const shouldHide = providers.length == 0;
      if (!shouldHide) {
        // Trigger an update of the providers submenu.
        this.providersMenu_.updateSubMenu();
      }
      this.gearMenu_.updateShowProviders(shouldHide);
    });
  }

  /**
   * @private
   */
  onHideGearMenu_() {
    this.toggleRipple_.activated = false;
  }

  /**
   * @param {Event} event
   * @private
   */
  onDirectoryChanged_(event) {
    event = /** @type {DirectoryChangeEvent} */ (event);
    if (event.volumeChanged) {
      this.refreshRemainingSpace_(true);
    }  // Show loading caption.

    if (this.gearButton_.isMenuShown()) {
      this.gearButton_.menu.updateCommands(this.gearButton_);
    }
  }

  /**
   * Refreshes space info of the current volume.
   * @param {boolean} showLoadingCaption Whether show loading caption or not.
   * @private
   */
  refreshRemainingSpace_(showLoadingCaption) {
    const currentDirectory = this.directoryModel_.getCurrentDirEntry();
    if (!currentDirectory || util.isRecentRoot(currentDirectory)) {
      this.gearMenu_.setSpaceInfo(null, false);
      return;
    }

    const currentVolumeInfo = this.directoryModel_.getCurrentVolumeInfo();
    if (!currentVolumeInfo) {
      return;
    }

    // TODO(mtomasz): Add support for remaining space indication for provided
    // file systems.
    if (currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.PROVIDED ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.MEDIA_VIEW ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.ARCHIVE) {
      this.gearMenu_.setSpaceInfo(null, false);
      return;
    }

    // TODO(crbug.com/1177203): Remove once Drive sends proper quota info to
    // Chrome.
    if (currentVolumeInfo.volumeType == VolumeManagerCommon.VolumeType.DRIVE) {
      this.gearMenu_.setSpaceInfo(null, false);
      return;
    }

    this.gearMenu_.setSpaceInfo(
        new Promise(fulfill => {
          chrome.fileManagerPrivate.getSizeStats(
              currentVolumeInfo.volumeId, fulfill);
        }),
        true);
  }

  /**
   * Handles preferences change and updates menu.
   * @private
   */
  onPreferencesChanged_() {
    chrome.fileManagerPrivate.getPreferences(prefs => {
      if (chrome.runtime.lastError) {
        return;
      }

      if (prefs.cellularDisabled) {
        this.gearMenu_.syncButton.setAttribute('checked', '');
      } else {
        this.gearMenu_.syncButton.removeAttribute('checked');
      }
    });
  }
}
