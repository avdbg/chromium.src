// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

UNDEFINED_INTERVAL_DELAY = -1;

/** Test fixture for auto scan manager. */
SwitchAccessAutoScanManagerTest = class extends SwitchAccessE2ETest {
  /** @override */
  setUp() {
    var runTest = this.deferRunTest(WhenTestDone.EXPECT);
    (async () => {
      let module = await import('/switch_access/nodes/back_button_node.js');
      window.BackButtonNode = module.BackButtonNode;

      module = await import('/switch_access/nodes/basic_node.js');
      window.BasicNode = module.BasicNode;
      window.BasicRootNode = module.BasicRootNode;

      module = await import('/switch_access/auto_scan_manager.js');
      window.AutoScanManager = module.AutoScanManager;

      module = await import('/switch_access/navigator.js');
      window.Navigator = module.Navigator;

      AutoScanManager.instance.primaryScanTime_ = 1000;
      // Use intervalCount and intervalDelay to check how many intervals are
      // currently running (should be no more than 1) and the current delay.
      window.intervalCount = 0;
      window.intervalDelay = UNDEFINED_INTERVAL_DELAY;
      window.defaultSetInterval = window.setInterval;
      window.defaultClearInterval = window.clearInterval;
      this.defaultMoveForward =
          Navigator.byItem.moveForward.bind(Navigator.byItem);
      this.moveForwardCount = 0;

      window.setInterval = function(func, delay) {
        window.intervalCount++;
        window.intervalDelay = delay;

        // Override the delay for testing.
        return window.defaultSetInterval(func, 0);
      };

      window.clearInterval = function(intervalId) {
        if (intervalId) {
          window.intervalCount--;
        }
        window.defaultClearInterval(intervalId);
      };

      Navigator.byItem.moveForward = () => {
        this.moveForwardCount++;
        this.onMoveForward_ && this.onMoveForward_();
        this.defaultMoveForward();
      };

      this.onMoveForward_ = null;

      runTest();
    })();
  }
};

TEST_F('SwitchAccessAutoScanManagerTest', 'SetEnabled', function() {
  this.runWithLoadedTree('', () => {
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is running prematurely');
    assertEquals(
        0, this.moveForwardCount,
        'Incorrect initialization of moveForwardCount');
    assertEquals(0, intervalCount, 'Incorrect initialization of intervalCount');

    this.onMoveForward_ = this.newCallback(() => {
      assertTrue(
          AutoScanManager.instance.isRunning_(),
          'Auto scan manager has stopped running');
      assertGT(this.moveForwardCount, 0, 'Switch Access has not moved forward');
      assertEquals(
          1, intervalCount, 'The number of intervals is no longer exactly 1');
    });

    AutoScanManager.setEnabled(true);
    assertTrue(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is not running');
    assertEquals(1, intervalCount, 'There is not exactly 1 interval');
  });
});

TEST_F('SwitchAccessAutoScanManagerTest', 'SetEnabledMultiple', function() {
  this.runWithLoadedDesktop(() => {
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is running prematurely');
    assertEquals(0, intervalCount, 'Incorrect initialization of intervalCount');

    AutoScanManager.setEnabled(true);
    AutoScanManager.setEnabled(true);
    AutoScanManager.setEnabled(true);

    assertTrue(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is not running');
    assertEquals(1, intervalCount, 'There is not exactly 1 interval');
  });
});

TEST_F('SwitchAccessAutoScanManagerTest', 'EnableAndDisable', function() {
  this.runWithLoadedDesktop(() => {
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is running prematurely');
    assertEquals(0, intervalCount, 'Incorrect initialization of intervalCount');

    AutoScanManager.setEnabled(true);
    assertTrue(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is not running');
    assertEquals(1, intervalCount, 'There is not exactly 1 interval');

    AutoScanManager.setEnabled(false);
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager did not stop running');
    assertEquals(0, intervalCount, 'Interval was not removed');
  });
});

TEST_F(
    'SwitchAccessAutoScanManagerTest', 'RestartIfRunningMultiple', function() {
      this.runWithLoadedDesktop(() => {
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is running prematurely');
        assertEquals(
            0, this.moveForwardCount,
            'Incorrect initialization of moveForwardCount');
        assertEquals(
            0, intervalCount, 'Incorrect initialization of intervalCount');

        AutoScanManager.setEnabled(true);
        AutoScanManager.restartIfRunning();
        AutoScanManager.restartIfRunning();
        AutoScanManager.restartIfRunning();

        assertTrue(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is not running');
        assertEquals(1, intervalCount, 'There is not exactly 1 interval');
      });
    });

TEST_F(
    'SwitchAccessAutoScanManagerTest', 'RestartIfRunningWhenOff', function() {
      this.runWithLoadedDesktop(() => {
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is running at start.');
        AutoScanManager.restartIfRunning();
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager enabled by restartIfRunning');
      });
    });

TEST_F('SwitchAccessAutoScanManagerTest', 'SetPrimaryScanTime', function() {
  this.runWithLoadedDesktop(() => {
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is running prematurely');
    assertEquals(
        UNDEFINED_INTERVAL_DELAY, intervalDelay,
        'Interval delay improperly initialized');

    AutoScanManager.setPrimaryScanTime(2);
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Setting default scan time started auto-scanning');
    assertEquals(
        2, AutoScanManager.instance.primaryScanTime_,
        'Default scan time set improperly');
    assertEquals(
        UNDEFINED_INTERVAL_DELAY, intervalDelay,
        'Interval delay set prematurely');

    AutoScanManager.setEnabled(true);
    assertTrue(
        AutoScanManager.instance.isRunning_(), 'Auto scan did not start');
    assertEquals(
        2, AutoScanManager.instance.primaryScanTime_,
        'Default scan time has changed');
    assertEquals(2, intervalDelay, 'Interval delay not set');

    AutoScanManager.setPrimaryScanTime(5);
    assertTrue(AutoScanManager.instance.isRunning_(), 'Auto scan stopped');
    assertEquals(
        5, AutoScanManager.instance.primaryScanTime_,
        'Default scan time did not change when set a second time');
    assertEquals(5, intervalDelay, 'Interval delay did not update');
  });
});
