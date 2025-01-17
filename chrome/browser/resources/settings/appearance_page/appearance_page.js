// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import '../controls/controlled_radio_button.m.js';
import '../controls/extension_controlled_indicator.m.js';
import '../controls/settings_radio_group.m.js';
import '../controls/settings_toggle_button.m.js';
import '../settings_page/settings_animated_pages.m.js';
import '../settings_page/settings_subpage.m.js';
import '../settings_shared_css.m.js';
import '../settings_vars_css.m.js';
import './home_url_input.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.m.js';
import {loadTimeData} from '../i18n_setup.js';
import {AppearancePageVisibility} from '../page_visibility.js';
import {routes} from '../route.js';
import {Router} from '../router.m.js';

import {AppearanceBrowserProxy, AppearanceBrowserProxyImpl} from './appearance_browser_proxy.js';


/**
 * This is the absolute difference maintained between standard and
 * fixed-width font sizes. http://crbug.com/91922.
 * @type {number}
 */
const SIZE_DIFFERENCE_FIXED_STANDARD = 3;

/**
 * ID for autogenerated themes. Should match
 * |ThemeService::kAutogeneratedThemeID|.
 */
const AUTOGENERATED_THEME_ID = 'autogenerated_theme_id';

/**
 * 'settings-appearance-page' is the settings page containing appearance
 * settings.
 */
Polymer({
  is: 'settings-appearance-page',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Dictionary defining page visibility.
     * @type {!AppearancePageVisibility}
     */
    pageVisibility: Object,

    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    defaultZoom_: Number,

    /** @private */
    isWallpaperPolicyControlled_: {type: Boolean, value: true},

    /**
     * List of options for the font size drop-down menu.
     * @type {!DropdownMenuOptionList}
     */
    fontSizeOptions_: {
      readOnly: true,
      type: Array,
      value() {
        return [
          {value: 9, name: loadTimeData.getString('verySmall')},
          {value: 12, name: loadTimeData.getString('small')},
          {value: 16, name: loadTimeData.getString('medium')},
          {value: 20, name: loadTimeData.getString('large')},
          {value: 24, name: loadTimeData.getString('veryLarge')},
        ];
      },
    },

    /**
     * Predefined zoom factors to be used when zooming in/out. These are in
     * ascending order. Values are displayed in the page zoom drop-down menu
     * as percentages.
     * @private {!Array<number>}
     */
    pageZoomLevels_: Array,

    /** @private */
    themeSublabel_: String,

    /** @private */
    themeUrl_: String,

    /** @private */
    useSystemTheme_: {
      type: Boolean,
      value: false,  // Can only be true on Linux, but value exists everywhere.
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (routes.FONTS) {
          map.set(routes.FONTS.path, '#customize-fonts-subpage-trigger');
        }
        return map;
      },
    },

    /** @private */
    showReaderModeOption_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showReaderModeOption');
      },
    },
  },

  /** @private {?AppearanceBrowserProxy} */
  appearanceBrowserProxy_: null,

  observers: [
    'defaultFontSizeChanged_(prefs.webkit.webprefs.default_font_size.value)',
    'themeChanged_(prefs.extensions.theme.id.value, useSystemTheme_)',

    // <if expr="is_linux and not chromeos">
    // NOTE: this pref only exists on Linux.
    'useSystemThemePrefChanged_(prefs.extensions.theme.use_system.value)',
    // </if>
  ],

  /** @override */
  created() {
    this.appearanceBrowserProxy_ = AppearanceBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.$.defaultFontSize.menuOptions = this.fontSizeOptions_;
    // TODO(dschuyler): Look into adding a listener for the
    // default zoom percent.
    this.appearanceBrowserProxy_.getDefaultZoom().then(zoom => {
      this.defaultZoom_ = zoom;
    });

    this.pageZoomLevels_ = /** @type {!Array<number>} */ (
        JSON.parse(loadTimeData.getString('presetZoomFactors')));
  },

  /**
   * @param {number} zoom
   * @return {number} A zoom easier read by users.
   * @private
   */
  formatZoom_(zoom) {
    return Math.round(zoom * 100);
  },

  /**
   * @param {boolean} showHomepage Whether to show home page.
   * @param {boolean} isNtp Whether to use the NTP as the home page.
   * @param {string} homepageValue If not using NTP, use this URL.
   * @return {string} The sub-label.
   * @private
   */
  getShowHomeSubLabel_(showHomepage, isNtp, homepageValue) {
    if (!showHomepage) {
      return this.i18n('homeButtonDisabled');
    }
    if (isNtp) {
      return this.i18n('homePageNtp');
    }
    return homepageValue || this.i18n('customWebAddress');
  },

  /** @private */
  onCustomizeFontsTap_() {
    Router.getInstance().navigateTo(routes.FONTS);
  },

  /** @private */
  onDisableExtension_() {
    this.fire('refresh-pref', 'homepage');
  },

  /**
   * @param {number} value The changed font size slider value.
   * @private
   */
  defaultFontSizeChanged_(value) {
    // This pref is handled separately in some extensions, but here it is tied
    // to default_font_size (to simplify the UI).
    this.set(
        'prefs.webkit.webprefs.default_fixed_font_size.value',
        value - SIZE_DIFFERENCE_FIXED_STANDARD);
  },

  /**
   * Open URL for either current theme or the theme gallery.
   * @private
   */
  openThemeUrl_() {
    window.open(this.themeUrl_ || loadTimeData.getString('themesGalleryUrl'));
  },

  /** @private */
  onUseDefaultTap_() {
    this.appearanceBrowserProxy_.useDefaultTheme();
  },

  // <if expr="is_linux and not chromeos">
  /**
   * @param {boolean} useSystemTheme
   * @private
   */
  useSystemThemePrefChanged_(useSystemTheme) {
    this.useSystemTheme_ = useSystemTheme;
  },

  /**
   * @param {string} themeId
   * @param {boolean} useSystemTheme
   * @return {boolean} Whether to show the "USE CLASSIC" button.
   * @private
   */
  showUseClassic_(themeId, useSystemTheme) {
    return !!themeId || useSystemTheme;
  },

  /**
   * @param {string} themeId
   * @param {boolean} useSystemTheme
   * @return {boolean} Whether to show the "USE GTK+" button.
   * @private
   */
  showUseSystem_(themeId, useSystemTheme) {
    return (!!themeId || !useSystemTheme) &&
        !this.appearanceBrowserProxy_.isSupervised();
  },

  /**
   * @param {string} themeId
   * @param {boolean} useSystemTheme
   * @return {boolean} Whether to show the secondary area where "USE CLASSIC"
   *     and "USE GTK+" buttons live.
   * @private
   */
  showThemesSecondary_(themeId, useSystemTheme) {
    return this.showUseClassic_(themeId, useSystemTheme) ||
        this.showUseSystem_(themeId, useSystemTheme);
  },

  /** @private */
  onUseSystemTap_() {
    this.appearanceBrowserProxy_.useSystemTheme();
  },
  // </if>

  /**
   * @param {string} themeId
   * @param {boolean} useSystemTheme
   * @private
   */
  themeChanged_(themeId, useSystemTheme) {
    if (this.prefs === undefined || useSystemTheme === undefined) {
      return;
    }

    if (themeId.length > 0 && themeId !== AUTOGENERATED_THEME_ID) {
      assert(!useSystemTheme);

      this.appearanceBrowserProxy_.getThemeInfo(themeId).then(info => {
        this.themeSublabel_ = info.name;
      });

      this.themeUrl_ = 'https://chrome.google.com/webstore/detail/' + themeId;
      return;
    }

    this.themeUrl_ = '';

    if (themeId === AUTOGENERATED_THEME_ID) {
      this.themeSublabel_ = this.i18n('chromeColors');
      return;
    }

    let i18nId;
    // <if expr="is_linux and not chromeos and not lacros">
    i18nId = useSystemTheme ? 'systemTheme' : 'classicTheme';
    // </if>
    // <if expr="not is_linux or chromeos or lacros">
    i18nId = 'chooseFromWebStore';
    // </if>
    this.themeSublabel_ = this.i18n(i18nId);
  },

  /** @private */
  onZoomLevelChange_() {
    chrome.settingsPrivate.setDefaultZoom(parseFloat(this.$.zoomLevel.value));
  },

  /**
   * @see blink::PageZoomValuesEqual().
   * @param {number} zoom1
   * @param {number} zoom2
   * @return {boolean}
   * @private
   */
  zoomValuesEqual_(zoom1, zoom2) {
    return Math.abs(zoom1 - zoom2) <= 0.001;
  },

  /**
   * @param {boolean} previousIsVisible
   * @param {boolean} nextIsVisible
   * @return {boolean}
   */
  showHr_(previousIsVisible, nextIsVisible) {
    return previousIsVisible && nextIsVisible;
  },
});
