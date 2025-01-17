// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Route} from './router.m.js';

/**
 * Specifies all possible routes in settings.
 *
 * @typedef {{
 *   ABOUT: !Route,
 *   ACCESSIBILITY: !Route,
 *   ADVANCED: !Route,
 *   ADDRESSES: !Route,
 *   APPEARANCE: !Route,
 *   AUTOFILL: !Route,
 *   BASIC: !Route,
 *   CAPTIONS: !Route,
 *   CERTIFICATES: !Route,
 *   CHECK_PASSWORDS: !Route,
 *   CHROME_CLEANUP: !Route,
 *   CLEAR_BROWSER_DATA: !Route,
 *   COOKIES: !Route,
 *   DEFAULT_BROWSER: !Route,
 *   DOWNLOADS: !Route,
 *   EDIT_DICTIONARY: !Route,
 *   FONTS: !Route,
 *   IMPORT_DATA: !Route,
 *   INCOMPATIBLE_APPLICATIONS: !Route,
 *   LANGUAGES: !Route,
 *   MANAGE_PROFILE: !Route,
 *   ON_STARTUP: !Route,
 *   PASSWORDS: !Route,
 *   DEVICE_PASSWORDS: !Route,
 *   PAYMENTS: !Route,
 *   PEOPLE: !Route,
 *   PRIVACY: !Route,
 *   RESET: !Route,
 *   RESET_DIALOG: !Route,
 *   SEARCH: !Route,
 *   SEARCH_ENGINES: !Route,
 *   SECURITY: !Route,
 *   SECURITY_KEYS: !Route,
 *   SIGN_OUT: !Route,
 *   SITE_SETTINGS: !Route,
 *   SITE_SETTINGS_ADS: !Route,
 *   SITE_SETTINGS_ALL: !Route,
 *   SITE_SETTINGS_AR: !Route,
 *   SITE_SETTINGS_AUTOMATIC_DOWNLOADS: !Route,
 *   SITE_SETTINGS_BACKGROUND_SYNC: !Route,
 *   SITE_SETTINGS_BLUETOOTH_DEVICES: !Route,
 *   SITE_SETTINGS_BLUETOOTH_SCANNING: !Route,
 *   SITE_SETTINGS_CAMERA: !Route,
 *   SITE_SETTINGS_CLIPBOARD: !Route,
 *   SITE_SETTINGS_COOKIES: !Route,
 *   SITE_SETTINGS_DATA_DETAILS: !Route,
 *   SITE_SETTINGS_FONT_ACCESS: !Route,
 *   SITE_SETTINGS_HANDLERS: !Route,
 *   SITE_SETTINGS_HID_DEVICES: !Route,
 *   SITE_SETTINGS_IDLE_DETECTION: !Route,
 *   SITE_SETTINGS_IMAGES: !Route,
 *   SITE_SETTINGS_MIXEDSCRIPT: !Route,
 *   SITE_SETTINGS_JAVASCRIPT: !Route,
 *   SITE_SETTINGS_SENSORS: !Route,
 *   SITE_SETTINGS_SOUND: !Route,
 *   SITE_SETTINGS_LOCATION: !Route,
 *   SITE_SETTINGS_MICROPHONE: !Route,
 *   SITE_SETTINGS_MIDI_DEVICES: !Route,
 *   SITE_SETTINGS_FILE_SYSTEM_WRITE: !Route,
 *   SITE_SETTINGS_NOTIFICATIONS: !Route,
 *   SITE_SETTINGS_PAYMENT_HANDLER: !Route,
 *   SITE_SETTINGS_PDF_DOCUMENTS: !Route,
 *   SITE_SETTINGS_POPUPS: !Route,
 *   SITE_SETTINGS_PROTECTED_CONTENT: !Route,
 *   SITE_SETTINGS_SITE_DATA: !Route,
 *   SITE_SETTINGS_SITE_DETAILS: !Route,
 *   SITE_SETTINGS_USB_DEVICES: !Route,
 *   SITE_SETTINGS_SERIAL_PORTS: !Route,
 *   SITE_SETTINGS_VR: !Route,
 *   SITE_SETTINGS_WINDOW_PLACEMENT: !Route,
 *   SITE_SETTINGS_ZOOM_LEVELS: !Route,
 *   SYNC: !Route,
 *   SYNC_ADVANCED: !Route,
 *   SYSTEM: !Route,
 *   TRIGGERED_RESET_DIALOG: !Route,
 * }}
 */
export let SettingsRoutes;
