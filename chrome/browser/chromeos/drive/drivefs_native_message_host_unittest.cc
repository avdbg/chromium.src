// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drivefs_native_message_host.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace {

using base::test::RunOnceClosure;
using testing::_;
using testing::InvokeWithoutArgs;

class MockClient : public extensions::NativeMessageHost::Client {
 public:
  MockClient() {}

  MOCK_METHOD(void,
              PostMessageFromNativeHost,
              (const std::string& message),
              (override));
  MOCK_METHOD(void,
              CloseChannel,
              (const std::string& error_message),
              (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClient);
};

class DriveFsNativeMessageHostTest
    : public testing::Test,
      public drivefs::mojom::DriveFsInterceptorForTesting,
      public drivefs::mojom::NativeMessagingHost {
 public:
  DriveFsNativeMessageHostTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kDriveFsBidirectionalNativeMessaging);
  }

  DriveFs* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  void CreateNativeHostSession(
      drivefs::mojom::ExtensionConnectionParamsPtr params,
      mojo::PendingReceiver<drivefs::mojom::NativeMessagingHost> session,
      mojo::PendingRemote<drivefs::mojom::NativeMessagingPort> port) {
    params_ = std::move(params);
    extension_port_.Bind(std::move(port));
    receiver_.Bind(std::move(session));
  }

  MOCK_METHOD(void,
              HandleMessageFromExtension,
              (const std::string& message),
              (override));

  base::test::TaskEnvironment task_environment_;
  drivefs::mojom::ExtensionConnectionParamsPtr params_;
  mojo::Receiver<drivefs::mojom::NativeMessagingHost> receiver_{this};
  mojo::Remote<drivefs::mojom::NativeMessagingPort> extension_port_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsNativeMessageHostTest);
};

TEST_F(DriveFsNativeMessageHostTest, DriveFsInitiatedMessaging) {
  base::RunLoop run_loop;

  std::unique_ptr<extensions::NativeMessageHost> host =
      CreateDriveFsInitiatedNativeMessageHost(
          extension_port_.BindNewPipeAndPassReceiver(),
          receiver_.BindNewPipeAndPassRemote());
  MockClient client;
  EXPECT_CALL(client, PostMessageFromNativeHost("foo"))
      .WillOnce(InvokeWithoutArgs([&host]() { host->OnMessage("bar"); }));
  EXPECT_CALL(*this, HandleMessageFromExtension("bar"))
      .WillOnce(InvokeWithoutArgs(
          [this]() { extension_port_->PostMessageToExtension("baz"); }));
  EXPECT_CALL(client, PostMessageFromNativeHost("baz"))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  host->Start(&client);
  extension_port_->PostMessageToExtension("foo");
  run_loop.Run();
}

TEST_F(DriveFsNativeMessageHostTest, ExtensionInitiatedMessaging) {
  base::RunLoop run_loop;

  std::unique_ptr<extensions::NativeMessageHost> host =
      CreateDriveFsNativeMessageHostForTesting(this);
  MockClient client;
  host->Start(&client);
  EXPECT_CALL(*this, HandleMessageFromExtension("foo"))
      .WillOnce(InvokeWithoutArgs(
          [this]() { extension_port_->PostMessageToExtension("bar"); }));
  EXPECT_CALL(client, PostMessageFromNativeHost("bar"))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  host->OnMessage("foo");
  run_loop.Run();
}

TEST_F(DriveFsNativeMessageHostTest, NativeHostSendsMessageBeforeStart) {
  std::unique_ptr<extensions::NativeMessageHost> host =
      CreateDriveFsInitiatedNativeMessageHost(
          extension_port_.BindNewPipeAndPassReceiver(),
          receiver_.BindNewPipeAndPassRemote());
  MockClient client;

  EXPECT_CALL(client, PostMessageFromNativeHost(_)).Times(0);
  extension_port_->PostMessageToExtension("foo");
  base::RunLoop().RunUntilIdle();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(client, PostMessageFromNativeHost("foo"))
        .WillOnce(InvokeWithoutArgs([&host]() { host->OnMessage("bar"); }));
    EXPECT_CALL(*this, HandleMessageFromExtension("bar"))
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
    host->Start(&client);
    run_loop.Run();
  }
}

TEST_F(DriveFsNativeMessageHostTest, Error) {
  base::RunLoop run_loop;

  std::unique_ptr<extensions::NativeMessageHost> host =
      CreateDriveFsInitiatedNativeMessageHost(
          extension_port_.BindNewPipeAndPassReceiver(),
          receiver_.BindNewPipeAndPassRemote());
  MockClient client;
  EXPECT_CALL(*this, HandleMessageFromExtension).Times(0);
  EXPECT_CALL(client, PostMessageFromNativeHost).Times(0);
  EXPECT_CALL(client, CloseChannel("FILE_ERROR_FAILED: foo"));
  receiver_.set_disconnect_handler(run_loop.QuitClosure());

  host->Start(&client);
  extension_port_.ResetWithReason(1u, "foo");

  run_loop.Run();

  host->OnMessage("bar");
  base::RunLoop().RunUntilIdle();
}

class DriveFsNativeMessageHostTestWithoutFlag
    : public DriveFsNativeMessageHostTest {
 public:
  DriveFsNativeMessageHostTestWithoutFlag() {
    scoped_feature_list_.InitAndDisableFeature(
        chromeos::features::kDriveFsBidirectionalNativeMessaging);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsNativeMessageHostTestWithoutFlag);
};

TEST_F(DriveFsNativeMessageHostTestWithoutFlag,
       DriveFsCannotInitiateMessaging) {
  ASSERT_FALSE(CreateDriveFsInitiatedNativeMessageHost(
      extension_port_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote()));
}

}  // namespace
}  // namespace drive
