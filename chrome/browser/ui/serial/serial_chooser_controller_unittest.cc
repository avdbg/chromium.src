// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/serial/serial_chooser_controller.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/chooser_controller/mock_chooser_controller_view.h"
#include "chrome/browser/serial/serial_blocklist.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/serial/serial_chooser_histograms.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"

using testing::_;
using testing::Invoke;

class SerialChooserControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mojo::PendingRemote<device::mojom::SerialPortManager> port_manager;
    port_manager_.AddReceiver(port_manager.InitWithNewPipeAndPassReceiver());
    SerialChooserContextFactory::GetForProfile(profile())
        ->SetPortManagerForTesting(std::move(port_manager));
  }

  void TearDown() override {
    // Because SerialBlocklist is a singleton it must be cleared after tests run
    // to prevent leakage between tests.
    feature_list_.Reset();
    SerialBlocklist::Get().ResetToDefaultValuesForTesting();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  base::UnguessableToken AddPort(
      const std::string& display_name,
      const base::FilePath& path,
      base::Optional<uint16_t> vendor_id = base::nullopt,
      base::Optional<uint16_t> product_id = base::nullopt) {
    auto port = device::mojom::SerialPortInfo::New();
    port->token = base::UnguessableToken::Create();
    port->display_name = display_name;
    port->path = path;
    if (vendor_id) {
      port->has_vendor_id = true;
      port->vendor_id = *vendor_id;
    }
    if (product_id) {
      port->has_product_id = true;
      port->product_id = *product_id;
    }
    base::UnguessableToken port_token = port->token;
    port_manager().AddPort(std::move(port));
    return port_token;
  }

  void SetDynamicBlocklist(base::StringPiece value) {
    feature_list_.Reset();

    std::map<std::string, std::string> parameters;
    parameters[kWebSerialBlocklistAdditions.name] = std::string(value);
    feature_list_.InitWithFeaturesAndParameters(
        {{kWebSerialBlocklist, parameters}}, {});

    SerialBlocklist::Get().ResetToDefaultValuesForTesting();
  }

  device::FakeSerialPortManager& port_manager() { return port_manager_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  device::FakeSerialPortManager port_manager_;
};

TEST_F(SerialChooserControllerTest, GetPortsLateResponse) {
  std::vector<blink::mojom::SerialPortFilterPtr> filters;

  bool callback_run = false;
  auto callback = base::BindLambdaForTesting(
      [&](device::mojom::SerialPortInfoPtr port_info) {
        EXPECT_FALSE(port_info);
        callback_run = true;
      });

  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters), std::move(callback));
  controller.reset();

  // Allow any tasks posted by |controller| to run, such as asynchronous
  // requests to the Device Service to get the list of available serial ports.
  // These should be safely discarded since |controller| was destroyed.
  base::RunLoop().RunUntilIdle();

  // Even if |controller| is destroyed without user interaction the callback
  // should be run.
  EXPECT_TRUE(callback_run);
}

TEST_F(SerialChooserControllerTest, PortsAddedAndRemoved) {
  base::HistogramTester histogram_tester;

  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters), base::DoNothing());

  MockChooserControllerView view;
  controller->set_view(&view);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(0u, controller->NumOptions());

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->display_name = "Test Port 1";
  port->path = base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0"));
#if defined(OS_MAC)
  // This path will be ignored and not generate additional chooser entries or
  // be displayed in the device name.
  port->alternate_path = base::FilePath(FILE_PATH_LITERAL("/dev/alternateS0"));
#endif
  base::UnguessableToken port1_token = port->token;
  port_manager().AddPort(std::move(port));
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionAdded(_)).WillOnce(Invoke([&](size_t index) {
      EXPECT_EQ(0u, index);
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(1u, controller->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("Test Port 1 (ttyS0)"),
            controller->GetOption(0));

  AddPort("Test Port 2", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS1")));
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionAdded(_)).WillOnce(Invoke([&](size_t index) {
      EXPECT_EQ(1u, index);
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(2u, controller->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("Test Port 1 (ttyS0)"),
            controller->GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("Test Port 2 (ttyS1)"),
            controller->GetOption(1));

  port_manager().RemovePort(port1_token);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionRemoved(_)).WillOnce(Invoke([&](size_t index) {
      EXPECT_EQ(0u, index);
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(1u, controller->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("Test Port 2 (ttyS1)"),
            controller->GetOption(0));

  controller.reset();
  histogram_tester.ExpectUniqueSample("Permissions.Serial.ChooserClosed",
                                      SerialChooserOutcome::kCancelled, 1);
}

TEST_F(SerialChooserControllerTest, PortSelected) {
  base::HistogramTester histogram_tester;

  base::UnguessableToken port_token =
      AddPort("Test Port", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0")));

  base::MockCallback<content::SerialChooser::Callback> callback;
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters), callback.Get());

  MockChooserControllerView view;
  controller->set_view(&view);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      EXPECT_EQ(1u, controller->NumOptions());
      EXPECT_EQ(base::ASCIIToUTF16("Test Port (ttyS0)"),
                controller->GetOption(0));
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  EXPECT_CALL(callback, Run(_))
      .WillOnce(Invoke([&](device::mojom::SerialPortInfoPtr port) {
        EXPECT_EQ(port_token, port->token);

        // Regression test for https://crbug.com/1069057. Ensure that the set of
        // options is still valid after the callback is run.
        EXPECT_EQ(1u, controller->NumOptions());
        EXPECT_EQ(base::ASCIIToUTF16("Test Port (ttyS0)"),
                  controller->GetOption(0));
      }));
  controller->Select({0});
  histogram_tester.ExpectUniqueSample(
      "Permissions.Serial.ChooserClosed",
      SerialChooserOutcome::kEphemeralPermissionGranted, 1);
}

TEST_F(SerialChooserControllerTest, PortFiltered) {
  base::HistogramTester histogram_tester;

  // Create two ports from the same vendor with different product IDs.
  base::UnguessableToken port_1 =
      AddPort("Test Port 1", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0")),
              0x1234, 0x0001);
  base::UnguessableToken port_2 =
      AddPort("Test Port 2", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS1")),
              0x1234, 0x0002);

  // Create a filter which will select only the first port.
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto filter = blink::mojom::SerialPortFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x1234;
  filter->has_product_id = true;
  filter->product_id = 0x0001;
  filters.push_back(std::move(filter));

  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters), base::DoNothing());

  MockChooserControllerView view;
  controller->set_view(&view);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      // Expect that only the first port is shown thanks to the filter.
      EXPECT_EQ(1u, controller->NumOptions());
      EXPECT_EQ(base::ASCIIToUTF16("Test Port 1 (ttyS0)"),
                controller->GetOption(0));
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  // Removing the second port should be a no-op since it is filtered out.
  EXPECT_CALL(view, OnOptionRemoved).Times(0);
  port_manager().RemovePort(port_2);
  base::RunLoop().RunUntilIdle();

  // Adding it back should be a no-op as well.
  EXPECT_CALL(view, OnOptionAdded).Times(0);
  AddPort("Test Port 2", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS1")),
          0x1234, 0x0002);
  base::RunLoop().RunUntilIdle();

  // Removing the first port should trigger a change in the UI. This also acts
  // as a synchronization point to make sure that the changes above were
  // processed.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionRemoved(0)).WillOnce(Invoke([&]() {
      run_loop.Quit();
    }));
    port_manager().RemovePort(port_1);
    run_loop.Run();
  }
}

TEST_F(SerialChooserControllerTest, Blocklist) {
  base::HistogramTester histogram_tester;

  // Create two ports from the same vendor with different product IDs.
  base::UnguessableToken port_1 =
      AddPort("Test Port 1", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0")),
              0x1234, 0x0001);
  base::UnguessableToken port_2 =
      AddPort("Test Port 2", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS1")),
              0x1234, 0x0002);

  // Add the second port to the blocklist.
  SetDynamicBlocklist("usb:1234:0002");

  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters), base::DoNothing());

  MockChooserControllerView view;
  controller->set_view(&view);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      // Expect that only the first port is shown thanks to the filter.
      EXPECT_EQ(1u, controller->NumOptions());
      EXPECT_EQ(base::ASCIIToUTF16("Test Port 1 (ttyS0)"),
                controller->GetOption(0));
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  // Removing the second port should be a no-op since it is filtered out.
  EXPECT_CALL(view, OnOptionRemoved).Times(0);
  port_manager().RemovePort(port_2);
  base::RunLoop().RunUntilIdle();

  // Adding it back should be a no-op as well.
  EXPECT_CALL(view, OnOptionAdded).Times(0);
  AddPort("Test Port 2", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS1")),
          0x1234, 0x0002);
  base::RunLoop().RunUntilIdle();

  // Removing the first port should trigger a change in the UI. This also acts
  // as a synchronization point to make sure that the changes above were
  // processed.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionRemoved(0)).WillOnce(Invoke([&]() {
      run_loop.Quit();
    }));
    port_manager().RemovePort(port_1);
    run_loop.Run();
  }
}
