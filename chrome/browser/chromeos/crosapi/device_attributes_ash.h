// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_DEVICE_ATTRIBUTES_ASH_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_DEVICE_ATTRIBUTES_ASH_H_

#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The ash-chrome implementation of the DeviceAttributes crosapi interface.
// This class must only be used from the main thread.
class DeviceAttributesAsh : public mojom::DeviceAttributes {
 public:
  DeviceAttributesAsh();
  DeviceAttributesAsh(const DeviceAttributesAsh&) = delete;
  DeviceAttributesAsh& operator=(const DeviceAttributesAsh&) = delete;
  ~DeviceAttributesAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::DeviceAttributes> receiver);

  // crosapi::mojom::DeviceAttributes:
  void GetDirectoryDeviceId(GetDirectoryDeviceIdCallback callback) override;
  void GetDeviceSerialNumber(GetDeviceSerialNumberCallback callback) override;
  void GetDeviceAssetId(GetDeviceAssetIdCallback callback) override;
  void GetDeviceAnnotatedLocation(
      GetDeviceAnnotatedLocationCallback callback) override;
  void GetDeviceHostname(GetDeviceHostnameCallback callback) override;

 private:
  using StringResult = mojom::DeviceAttributesStringResult;

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::DeviceAttributes> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_DEVICE_ATTRIBUTES_ASH_H_
