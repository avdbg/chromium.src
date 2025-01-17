// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ColorModeRestriction, Destination, DestinationConnectionStatus, DestinationOrigin, DestinationStore, DestinationType, DuplexModeRestriction, InvitationStore, NativeLayer, NativeLayerCrosImpl, NativeLayerImpl} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertNotEquals} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';

import {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getCddTemplate, setupTestListenerElement} from './print_preview_test_utils.js';

window.destination_search_test_chromeos = {};
const destination_search_test_chromeos =
    window.destination_search_test_chromeos;
destination_search_test_chromeos.suiteName = 'DestinationSearchTest';
/** @enum {string} */
destination_search_test_chromeos.TestNames = {
  ReceiveSuccessfulSetup: 'receive successful setup',
  ResolutionFails: 'resolution fails',
  ReceiveFailedSetup: 'receive failed setup',
  CloudKioskPrinter: 'cloud kiosk printer',
  ReceiveSuccessfulSetupWithPolicies: 'receive successful setup with policies',
};

suite(destination_search_test_chromeos.suiteName, function() {
  /** @type {PrintPreviewDestinationDialogCrosElement} */
  let dialog;

  /** @type {DestinationStore} */
  let destinationStore;

  /** @type {NativeLayerStub} */
  let nativeLayer;

  /** @type {NativeLayerCrosStub} */
  let nativeLayerCros;

  /** @override */
  suiteSetup(function() {
    setupTestListenerElement();
  });

  /** @override */
  setup(function() {
    // Create data classes
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.instance_ = nativeLayer;
    nativeLayerCros = new NativeLayerCrosStub();
    NativeLayerCrosImpl.instance_ = nativeLayerCros;
    destinationStore = createDestinationStore();
    nativeLayer.setLocalDestinationCapabilities(
        getCddTemplate('FooDevice', 'FooName'));
    destinationStore.init(
        false /* pdfPrinterDisabled */, true /* isDriveMounted */,
        'FooDevice' /* printerName */,
        '' /* serializedDefaultDestinationSelectionRulesStr */,
        [] /* recentDestinations */);

    // Set up dialog
    dialog = /** @type {!PrintPreviewDestinationDialogCrosElement} */ (
        document.createElement('print-preview-destination-dialog-cros'));
    dialog.users = [];
    dialog.activeUser = '';
    dialog.destinationStore = destinationStore;
    dialog.invitationStore = new InvitationStore();
    document.body.innerHTML = '';
    document.body.appendChild(dialog);
    return nativeLayer.whenCalled('getPrinterCapabilities').then(function() {
      dialog.show();
      flush();
      nativeLayer.reset();
    });
  });

  /**
   * @param {!Destination} destination The destination to
   *     simulate selection of.
   */
  function simulateDestinationSelect(destination) {
    // Fake destinationListItem.
    const item = document.createElement('print-preview-destination-list-item');
    item.destination = destination;

    // Get print list and fire event.
    const list = dialog.$$('print-preview-destination-list');
    list.fire('destination-selected', item);
  }

  /**
   * Adds a destination to the dialog and simulates selection of the
   * destination.
   * @param {string} destId The ID for the destination.
   */
  function requestSetup(destId) {
    const dest = new Destination(
        destId, DestinationType.LOCAL, DestinationOrigin.CROS, 'displayName',
        DestinationConnectionStatus.ONLINE);

    // Add the destination to the list.
    simulateDestinationSelect(dest);
  }

  // Tests that a destination is selected if the user clicks on it and setup
  // (for CrOS) or capabilities fetch (for non-Cros) succeeds.
  test(
      assert(destination_search_test_chromeos.TestNames.ReceiveSuccessfulSetup),
      function() {
        const destId = '00112233DEADBEEF';
        const response = {
          printerId: destId,
          capabilities: getCddTemplate(destId).capabilities,
          success: true,
        };
        nativeLayerCros.setSetupPrinterResponse(response);

        const waiter = eventToPromise(
            DestinationStore.EventType.DESTINATION_SELECT,
            /** @type {!EventTarget} */ (destinationStore));
        requestSetup(destId);
        return Promise.all([nativeLayerCros.whenCalled('setupPrinter'), waiter])
            .then(function(results) {
              const actualId = results[0];
              assertEquals(destId, actualId);
              // After setup or capabilities fetch succeeds, the destination
              // should be selected.
              assertNotEquals(null, destinationStore.selectedDestination);
              assertEquals(destId, destinationStore.selectedDestination.id);
            });
      });

  // Test what happens when the setupPrinter request is rejected.
  test(
      assert(destination_search_test_chromeos.TestNames.ResolutionFails),
      function() {
        const destId = '001122DEADBEEF';
        const originalDestination = destinationStore.selectedDestination;
        nativeLayerCros.setSetupPrinterResponse(
            {
              printerId: destId,
              success: false,
              capabilities: {printer: {}, version: '1'}
            },
            true);
        requestSetup(destId);
        return nativeLayerCros.whenCalled('setupPrinter')
            .then(function(actualId) {
              assertEquals(destId, actualId);
              // The selected printer should not have changed, since a printer
              // cannot be selected until setup succeeds.
              assertEquals(
                  originalDestination, destinationStore.selectedDestination);
            });
      });

  // Test what happens when the setupPrinter request is resolved with a
  // failed status. Chrome OS only.
  test(
      assert(destination_search_test_chromeos.TestNames.ReceiveFailedSetup),
      function() {
        const originalDestination = destinationStore.selectedDestination;
        const destId = '00112233DEADBEEF';
        const response = {
          printerId: destId,
          capabilities: getCddTemplate(destId).capabilities,
          success: false,
        };
        nativeLayerCros.setSetupPrinterResponse(response);
        requestSetup(destId);
        return nativeLayerCros.whenCalled('setupPrinter')
            .then(function(actualDestId) {
              assertEquals(destId, actualDestId);
              // The selected printer should not have changed, since a printer
              // cannot be selected until setup succeeds.
              assertEquals(
                  originalDestination, destinationStore.selectedDestination);
            });
      });

  // Test what happens when a simulated cloud kiosk printer is selected.
  test(
      assert(destination_search_test_chromeos.TestNames.CloudKioskPrinter),
      function() {
        const printerId = 'cloud-printer-id';

        // Create cloud destination.
        const cloudDest = new Destination(
            printerId, DestinationType.GOOGLE, DestinationOrigin.DEVICE,
            'displayName', DestinationConnectionStatus.ONLINE);
        cloudDest.capabilities =
            getCddTemplate(printerId, 'displayName').capabilities;

        // Place destination in the local list as happens for Kiosk printers.
        simulateDestinationSelect(cloudDest);

        // Verify that the destination has been selected.
        assertEquals(printerId, destinationStore.selectedDestination.id);
      });

  // Tests that if policies are set correctly if they are present
  // for a destination.
  test(
      assert(destination_search_test_chromeos.TestNames
                 .ReceiveSuccessfulSetupWithPolicies),
      function() {
        const destId = '00112233DEADBEEF';
        const response = {
          printerId: destId,
          capabilities: getCddTemplate(destId).capabilities,
          policies: {
            allowedColorModes: ColorModeRestriction.MONOCHROME,
            allowedDuplexModes: DuplexModeRestriction.DUPLEX,
            allowedPinMode: null,
            defaultColorMode: null,
            defaultDuplexMode: null,
            defaultPinMode: null,
          },
          success: true,
        };
        nativeLayerCros.setSetupPrinterResponse(response);
        requestSetup(destId);
        return nativeLayerCros.whenCalled('setupPrinter')
            .then(function(actualId) {
              assertEquals(destId, actualId);
              const selectedDestination = destinationStore.selectedDestination;
              assertNotEquals(null, selectedDestination);
              assertEquals(destId, selectedDestination.id);
              assertNotEquals(null, selectedDestination.capabilities);
              assertNotEquals(null, selectedDestination.policies);
              assertEquals(
                  ColorModeRestriction.MONOCHROME,
                  selectedDestination.policies.allowedColorModes);
              assertEquals(
                  DuplexModeRestriction.DUPLEX,
                  selectedDestination.policies.allowedDuplexModes);
            });
      });
});
