// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/device_attribute_api.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr char kAnnotatedAssetId[] = "annotated_asset_id";
constexpr char kAnnotatedLocation[] = "annotated_location";
constexpr char kDirectoryApiId[] = "directory_api_id";
constexpr char kSerialNumber[] = "serial_number";

}  // namespace

// This test class provides unset device policy values and statistic data used
// by device attributes APIs.
class DeviceAttributeAPIUnsetTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Init machine statistic.
    fake_statistics_provider_.SetMachineStatistic(
        chromeos::system::kSerialNumberKeyForTest, std::string());
  }

 private:
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_F(DeviceAttributeAPIUnsetTest, AllAttributes) {
  device_attribute_api::GetDirectoryId(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_FALSE(result->get_attribute().has_value());
      }));

  device_attribute_api::GetAnnotatedAssetId(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_FALSE(result->get_attribute().has_value());
      }));

  device_attribute_api::GetAnnotatedLocation(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_FALSE(result->get_attribute().has_value());
      }));

  device_attribute_api::GetSerialNumber(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_FALSE(result->get_attribute().has_value());
      }));

  base::RunLoop().RunUntilIdle();
}

// This test class provides regular device policy values and statistic data used
// by device attributes APIs.
class DeviceAttributeAPITest : public policy::DevicePolicyCrosBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Init the device policy.
    device_policy()->SetDefaultSigningKey();
    device_policy()->policy_data().set_annotated_asset_id(kAnnotatedAssetId);
    device_policy()->policy_data().set_annotated_location(kAnnotatedLocation);
    device_policy()->policy_data().set_directory_api_id(kDirectoryApiId);
    device_policy()->Build();
    RefreshDevicePolicy();

    // Init machine statistic.
    fake_statistics_provider_.SetMachineStatistic(
        chromeos::system::kSerialNumberKeyForTest, kSerialNumber);
  }

 private:
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_F(DeviceAttributeAPITest, AllAttributes) {
  device_attribute_api::GetDirectoryId(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_EQ(result->get_attribute(), kDirectoryApiId);
      }));

  device_attribute_api::GetAnnotatedAssetId(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_EQ(result->get_attribute(), kAnnotatedAssetId);
      }));

  device_attribute_api::GetAnnotatedLocation(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_EQ(result->get_attribute(), kAnnotatedLocation);
      }));

  device_attribute_api::GetSerialNumber(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_EQ(result->get_attribute(), kSerialNumber);
      }));

  base::RunLoop().RunUntilIdle();
}
