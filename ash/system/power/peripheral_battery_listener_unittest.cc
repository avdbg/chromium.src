// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/peripheral_battery_listener.h"

#include <memory>

#include "ash/shell.h"
#include "ash/system/power/peripheral_battery_tests.h"
#include "ash/test/ash_test_base.h"
#include "base/scoped_observation.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/message_center/public/cpp/notification.h"

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Field;
using testing::InSequence;
using testing::NiceMock;
using testing::Optional;
using testing::StrictMock;

using BI = ash::PeripheralBatteryListener::BatteryInfo;

// Annotate testing::Field invocations to improve feedback.
#define AFIELD(element, test) testing::Field(#element, element, test)

namespace {

class MockPeripheralBatteryObserver
    : public ash::PeripheralBatteryListener::Observer {
 public:
  MockPeripheralBatteryObserver() {}

  // ash::PeripheralBatteryListener::Observer:
  MOCK_METHOD(void,
              OnAddingBattery,
              (const ash::PeripheralBatteryListener::BatteryInfo& battery));
  MOCK_METHOD(void,
              OnRemovingBattery,
              (const ash::PeripheralBatteryListener::BatteryInfo& battery));
  MOCK_METHOD(void,
              OnUpdatedBatteryLevel,
              (const ash::PeripheralBatteryListener::BatteryInfo& battery));
};

}  // namespace

namespace ash {

class PeripheralBatteryListenerTest : public AshTestBase {
 public:
  // Constants for active field of PeripheralBatteryStylusReceived().
  const bool kBluetoothBatteryUpdate = true;
  const bool kBatteryPolledUpdate = false;
  const bool kBatteryEventUpdate = true;

  PeripheralBatteryListenerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  PeripheralBatteryListenerTest(const PeripheralBatteryListenerTest&) = delete;
  PeripheralBatteryListenerTest& operator=(
      const PeripheralBatteryListenerTest&) = delete;
  ~PeripheralBatteryListenerTest() override = default;

  void SetUp() override {
    ui::DeviceDataManager::CreateInstance();

    AshTestBase::SetUp();

    // Simulate the complete listing of input devices, required by the listener.
    if (complete_devices_)
      ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    mock_device_1_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        mock_adapter_.get(), /*bluetooth_class=*/0, kBluetoothDeviceName1,
        kBluetoothDeviceAddress1, /*paired=*/true, /*connected=*/true);
    mock_device_2_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        mock_adapter_.get(), /*bluetooth_class=*/0, kBluetoothDeviceName2,
        kBluetoothDeviceAddress2, /*paired=*/true, /*connected=*/true);

    battery_listener_ = std::make_unique<PeripheralBatteryListener>();
  }

  void TearDown() override {
    battery_listener_.reset();
    AshTestBase::TearDown();
    ui::DeviceDataManager::DeleteInstance();
  }

  base::TimeTicks GetTestingClock() { return base::TimeTicks::Now(); }

  void ClockAdvance(base::TimeDelta delta) {
    task_environment()->AdvanceClock(delta);
  }

 protected:
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_1_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_2_;
  std::unique_ptr<PeripheralBatteryListener> battery_listener_;

  void set_complete_devices(bool complete_devices) {
    complete_devices_ = complete_devices;
  }

  // SetUp() doesn't complete devices if this is set to false.
  bool complete_devices_ = true;
};

TEST_F(PeripheralBatteryListenerTest, Basic) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  // Level 50 at time 100, listener should be notified.
  ClockAdvance(base::TimeDelta::FromSeconds(100));

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp, Eq(base::nullopt)),
                  AFIELD(&BI::level, Eq(50)),
                  AFIELD(&BI::charge_status, Eq(kTestBatteryStatusOut)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  // Level 5 at time 110, listener should be notified.
  ClockAdvance(base::TimeDelta::FromSeconds(10));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp, Eq(base::nullopt)),
                  AFIELD(&BI::level, Eq(5)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  // Level -1 at time 115, listener should be notified.
  ClockAdvance(base::TimeDelta::FromSeconds(5));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp, Eq(base::nullopt)),
                  AFIELD(&BI::level, Eq(base::nullopt)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, -1, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  // Level 50 at time 120, listener should be notified.
  ClockAdvance(base::TimeDelta::FromSeconds(5));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp, Eq(base::nullopt)),
                  AFIELD(&BI::level, Eq(50)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      kBatteryPolledUpdate);
}

TEST_F(PeripheralBatteryListenerTest, ActiveUpdates) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  // Level 50 at time 100, listener should be notified.
  ClockAdvance(base::TimeDelta::FromSeconds(100));

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kTestBatteryId)),
                AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                AFIELD(&BI::charge_status, Eq(kTestBatteryStatusOut)),
                AFIELD(&BI::last_active_update_timestamp, Eq(base::nullopt)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  // Level 5 at time 110, listener should be notified.
  ClockAdvance(base::TimeDelta::FromSeconds(10));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp,
                         Optional(GetTestingClock())))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      kBatteryEventUpdate);

  // Level -1 at time 115, listener should be notified.
  ClockAdvance(base::TimeDelta::FromSeconds(5));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp,
                         Optional(GetTestingClock())))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, -1, kTestBatteryStatusIn,
      kBatteryEventUpdate);

  auto prior_active_update_time = GetTestingClock();

  // Level 50 at time 120, listener should be notified.
  ClockAdvance(base::TimeDelta::FromSeconds(5));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp,
                         Optional(prior_active_update_time)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      kBatteryPolledUpdate);
}

TEST_F(PeripheralBatteryListenerTest, FirstActiveUpdates) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  // Level 50 at time 100, listener should be notified.
  ClockAdvance(base::TimeDelta::FromSeconds(100));

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp,
                         Optional(GetTestingClock())))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      kBatteryEventUpdate);
}

TEST_F(PeripheralBatteryListenerTest, InvalidBatteryInfo) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  const std::string invalid_path1 = "invalid-path";
  const std::string invalid_path2 = "/sys/class/power_supply/hid-battery";

  EXPECT_CALL(listener_observer_mock, OnAddingBattery(_)).Times(0);
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_)).Times(0);

  battery_listener_->PeripheralBatteryStatusReceived(
      invalid_path1, kTestDeviceName, 10, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  battery_listener_->PeripheralBatteryStatusReceived(
      invalid_path2, kTestDeviceName, 10, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, -2, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 101, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  // Note that -1 is a valid battery level for the Listener, so not checked.
}

// Verify that for Bluetooth devices, the correct address gets stored in the
// BatteryInfo's bluetooth_address member, and for non-Bluetooth devices, that
// bluetooth_address member is empty.
TEST_F(PeripheralBatteryListenerTest, ExtractBluetoothAddress) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  const std::string bluetooth_path =
      "/sys/class/power_supply/hid-A0:b1:C2:d3:E4:f5-battery";
  const std::string expected_bluetooth_address = "a0:b1:c2:d3:e4:f5";
  const std::string expected_bluetooth_id =
      "battery_bluetooth-a0:b1:c2:d3:e4:f5";
  const std::string non_bluetooth_path =
      "/sys/class/power_supply/hid-notbluetooth-battery";

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(expected_bluetooth_id))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(expected_bluetooth_id)),
                                  AFIELD(&BI::level, Eq(10)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      bluetooth_path, kTestDeviceName, 10, kTestBatteryStatusIn,
      kBluetoothBatteryUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(non_bluetooth_path))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(non_bluetooth_path)),
                                  AFIELD(&BI::bluetooth_address, Eq("")))));

  battery_listener_->PeripheralBatteryStatusReceived(
      non_bluetooth_path, kTestDeviceName, 10, kTestBatteryStatusIn,
      kBatteryPolledUpdate);
}

TEST_F(PeripheralBatteryListenerTest, DeviceRemove) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
}

TEST_F(PeripheralBatteryListenerTest, StylusNotification) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  const std::string kTestStylusBatteryPath =
      "/sys/class/power_supply/hid-AAAA:BBBB:CCCC.DDDD-battery";
  const std::string kTestStylusName = "test_stylus";
  const auto kTestStylusBatteryStatusIn = power_manager::
      PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING;
  const auto kTestStylusBatteryStatusOut = BI::ChargeStatus::kDischarging;

  // Add an external stylus to our test device manager.
  ui::TouchscreenDevice stylus(/*id=*/0, ui::INPUT_DEVICE_USB, kTestStylusName,
                               gfx::Size(),
                               /*touch_points=*/1, /*has_stylus=*/true);
  stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                  AFIELD(&BI::level, Eq(50)),
                  AFIELD(&BI::charge_status, Eq(kTestStylusBatteryStatusOut)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                  AFIELD(&BI::bluetooth_address, Eq("")))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50, kTestStylusBatteryStatusIn,
      kBatteryPolledUpdate);

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                                  AFIELD(&BI::level, Eq(5)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 5, kTestStylusBatteryStatusIn,
      kBatteryEventUpdate);

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                                  AFIELD(&BI::level, Eq(base::nullopt)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, -1, kTestStylusBatteryStatusIn,
      kBatteryEventUpdate);
}

TEST_F(PeripheralBatteryListenerTest,
       Bluetooth_CreatesANotificationForEachDevice) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId1)), AFIELD(&BI::level, Eq(5)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kOther)),
          AFIELD(&BI::name, Eq(base::ASCIIToUTF16(kBluetoothDeviceName1))),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId2))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId2)), AFIELD(&BI::level, Eq(0)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kOther)),
          AFIELD(&BI::name, Eq(base::ASCIIToUTF16(kBluetoothDeviceName2))),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress2)))));

  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/5);
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_2_.get(),
                                          /*new_battery_percentage=*/0);
}

TEST_F(PeripheralBatteryListenerTest,
       Bluetooth_RemovesNotificationForDisconnectedDevices) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId1)), AFIELD(&BI::level, Eq(5)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kOther)),
          AFIELD(&BI::name, Eq(base::ASCIIToUTF16(kBluetoothDeviceName1))),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId2))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId2)), AFIELD(&BI::level, Eq(0)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kOther)),
          AFIELD(&BI::name, Eq(base::ASCIIToUTF16(kBluetoothDeviceName2))),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress2)))));

  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/5);
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_2_.get(),
                                          /*new_battery_percentage=*/0);

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));

  // Verify only the notification for device 1 gets removed.
  battery_listener_->DeviceConnectedStateChanged(mock_adapter_.get(),
                                                 mock_device_1_.get(), false);

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId2))));

  // Remove the second notification.
  battery_listener_->DeviceRemoved(mock_adapter_.get(), mock_device_2_.get());
}

TEST_F(PeripheralBatteryListenerTest,
       Bluetooth_RemovesNotificationForDisconnectedDevicesInOtherOrder) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId2))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId2)),
                AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress2)))));

  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/5);
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_2_.get(),
                                          /*new_battery_percentage=*/0);

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId2))));

  // Remove the second notification.
  battery_listener_->DeviceRemoved(mock_adapter_.get(), mock_device_2_.get());

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));

  // Verify only the notification for device 1 gets removed.
  battery_listener_->DeviceConnectedStateChanged(mock_adapter_.get(),
                                                 mock_device_1_.get(), false);
}

TEST_F(PeripheralBatteryListenerTest, Bluetooth_RemoveAndReconnect) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/5);

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));

  battery_listener_->DeviceConnectedStateChanged(mock_adapter_.get(),
                                                 mock_device_1_.get(), false);

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/5);
}

TEST_F(PeripheralBatteryListenerTest,
       Bluetooth_CancelNotificationForInvalidBatteryLevel) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                                  AFIELD(&BI::level, Eq(1)))));

  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/1);

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                                  AFIELD(&BI::level, Eq(base::nullopt)))));

  battery_listener_->DeviceBatteryChanged(
      mock_adapter_.get(), mock_device_1_.get(),
      /*new_battery_percentage=*/base::nullopt);
}

// Do notify observer if the battery level drops again under the
// threshold before kNotificationInterval is completed.
TEST_F(PeripheralBatteryListenerTest, EnsureUpdatesWithinSmallTimeIntervals) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  ClockAdvance(base::TimeDelta::FromSeconds(100));

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(1)))));

  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/1);
  ClockAdvance(base::TimeDelta::FromSeconds(1));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(base::nullopt)))));
  battery_listener_->DeviceBatteryChanged(
      mock_adapter_.get(), mock_device_1_.get(),
      /*new_battery_percentage=*/base::nullopt);

  ClockAdvance(base::TimeDelta::FromSeconds(1));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(1)))));
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/1);
}

// Notify observer if the battery is under threshold, then unknown level and
// then is again under the threshold after kNotificationInterval is completed.
// (Listener should not pay attention to kNotificationInterval anyway.)
TEST_F(PeripheralBatteryListenerTest,
       PostNotificationIfBatteryGoesFromUnknownLevelToBelowThreshold) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  ClockAdvance(base::TimeDelta::FromSeconds(100));

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(1)))));
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/1);

  ClockAdvance(base::TimeDelta::FromSeconds(1));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(base::nullopt)))));
  battery_listener_->DeviceBatteryChanged(
      mock_adapter_.get(), mock_device_1_.get(),
      /*new_battery_percentage=*/base::nullopt);

  ClockAdvance(base::TimeDelta::FromSeconds(100));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(1)))));
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/1);
}

// If there is an existing notification and the battery level remains low,
// update its content.
TEST_F(PeripheralBatteryListenerTest, UpdateNotificationIfVisible) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  ClockAdvance(base::TimeDelta::FromSeconds(100));

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(5)))));
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/5);

  // The battery level remains low, should update the notification.
  ClockAdvance(base::TimeDelta::FromSeconds(100));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(3)))));
  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/3);
}

TEST_F(PeripheralBatteryListenerTest, MultipleObserversCoexist) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock_1;
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock_2;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs_1{&listener_observer_mock_1};
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs_2{&listener_observer_mock_2};

  scoped_listener_obs_1.Observe(battery_listener_.get());
  scoped_listener_obs_2.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock_1,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock_2,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock_1,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kTestBatteryId)),
                                          AFIELD(&BI::level, Eq(50)))));
  EXPECT_CALL(listener_observer_mock_2,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kTestBatteryId)),
                                          AFIELD(&BI::level, Eq(50)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      kBatteryPolledUpdate);
}

TEST_F(PeripheralBatteryListenerTest, ObserverationLifetimeObeyed) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  testing::InSequence sequence;

  // Connect observer, add and remove battery
  scoped_listener_obs.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);

  // Disconnect observer, add and remove battery

  scoped_listener_obs.Reset();

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);

  // Reconnect observer, add and remove battery

  scoped_listener_obs.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
}

// Check that observers only see events occuring while they are connected.
TEST_F(PeripheralBatteryListenerTest, PartialObserverationLifetimeObeyed) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  testing::InSequence sequence;

  // Connect observer, add and remove battery.
  scoped_listener_obs.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  // Disconnect observer before we remove battery.

  scoped_listener_obs.Reset();

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);

  // Reconnect battery.

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn, true);

  // Reconnect observer, add and remove battery.

  EXPECT_CALL(listener_observer_mock, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));
  scoped_listener_obs.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock, OnRemovingBattery(_));
  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
}

// Check that observers will get events to 'catch up' on batteries they missed.
TEST_F(PeripheralBatteryListenerTest, PartialObserverationLifetimeCatchUp) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  testing::InSequence sequence;

  // Connect battery.

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      kBatteryEventUpdate);

  EXPECT_CALL(listener_observer_mock, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));
  scoped_listener_obs.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock, OnRemovingBattery(_));
  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
}

TEST_F(PeripheralBatteryListenerTest, MultipleObserverationLifetimeObeyed) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock_1;
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock_2;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs_1{&listener_observer_mock_1};
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs_2{&listener_observer_mock_2};

  testing::InSequence sequence;

  scoped_listener_obs_1.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock_1, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock_1, OnUpdatedBatteryLevel(_));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  EXPECT_CALL(listener_observer_mock_2, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock_2, OnUpdatedBatteryLevel(_));
  scoped_listener_obs_2.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock_1, OnRemovingBattery(_));
  EXPECT_CALL(listener_observer_mock_2, OnRemovingBattery(_));
  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);

  scoped_listener_obs_1.Reset();

  EXPECT_CALL(listener_observer_mock_2, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock_2, OnUpdatedBatteryLevel(_));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      kBatteryPolledUpdate);

  EXPECT_CALL(listener_observer_mock_2, OnRemovingBattery(_));
  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
}

TEST_F(PeripheralBatteryListenerTest, Charger) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestChargerId))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kTestChargerId)),
                AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                AFIELD(&BI::level, Eq(50)),
                AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kCharging)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING,
      kBatteryEventUpdate);
}

TEST_F(PeripheralBatteryListenerTest, ChargerError) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestChargerId))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kTestChargerId)), AFIELD(&BI::level, Eq(50)),
          AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kError)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50,
      power_manager::PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_ERROR,
      kBatteryPolledUpdate);
}

TEST_F(PeripheralBatteryListenerTest, ChargerErrorTransition) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestChargerId))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kTestChargerId)), AFIELD(&BI::level, Eq(50)),
          AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kCharging)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING,
      kBatteryPolledUpdate);

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kTestChargerId)), AFIELD(&BI::level, Eq(50)),
          AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kError)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50,
      power_manager::PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_ERROR,
      kBatteryEventUpdate);
}

// Stylus-via-screen updates of level zero should be translated to
// nullopt as zero is not a valid level, but may come through during
// boot or other device creation scenarios.
TEST_F(PeripheralBatteryListenerTest, StylusDiscardsZeros) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  const std::string kTestStylusBatteryPath =
      "/sys/class/power_supply/hid-AAAA:BBBB:CCCC.DDDD-battery";
  const std::string kTestStylusName = "test_stylus";
  const auto kTestStylusBatteryStatusIn = power_manager::
      PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING;
  const auto kTestStylusBatteryStatusOut = BI::ChargeStatus::kDischarging;

  // Add an external stylus to our test device manager.
  ui::TouchscreenDevice stylus(/*id=*/0, ui::INPUT_DEVICE_USB, kTestStylusName,
                               gfx::Size(),
                               /*touch_points=*/1, /*has_stylus=*/true);
  stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                AFIELD(&BI::level, Eq(base::nullopt)),
                AFIELD(&BI::charge_status, Eq(kTestStylusBatteryStatusOut)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 0, kTestStylusBatteryStatusIn,
      kBatteryEventUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::level, Eq(50)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50, kTestStylusBatteryStatusIn,
      kBatteryEventUpdate);

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::level, Eq(base::nullopt)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 0, kTestStylusBatteryStatusIn,
      kBatteryEventUpdate);
}

// Stylus-via-charger updates of level zero should come through as expected, as
// the numeric value may be relevant to the charge state.
TEST_F(PeripheralBatteryListenerTest, StylusChargerDoesNotDiscardZeros) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestChargerId))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestChargerId)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                  AFIELD(&BI::level, Eq(0)),
                  AFIELD(&BI::charge_status, Eq(kTestBatteryStatusOut)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 0, kTestBatteryStatusIn,
      kBatteryEventUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::level, Eq(50)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50, kTestBatteryStatusIn,
      kBatteryEventUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::level, Eq(0)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 0, kTestBatteryStatusIn,
      kBatteryEventUpdate);
}

// Bluetooth/other HID updates of level zero should come through as expected, as
// we don't know that 0 is invalid.
TEST_F(PeripheralBatteryListenerTest, BluetoothDoesNotDiscardZeros) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId1)), AFIELD(&BI::level, Eq(0)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kOther)),
          AFIELD(&BI::name, Eq(base::ASCIIToUTF16(kBluetoothDeviceName1))),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/0);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::level, Eq(5)))));

  battery_listener_->DeviceBatteryChanged(mock_adapter_.get(),
                                          mock_device_1_.get(),
                                          /*new_battery_percentage=*/5);
}

}  // namespace ash
