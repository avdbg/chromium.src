// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/vm_plugin_dispatcher_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom-forward.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

class CrosUsbDetectorTest;

namespace chromeos {

const uint8_t kInvalidUsbPortNumber = 0xff;

// Reasons the notification may be closed. These are used in histograms so do
// not remove/reorder entries. Only add at the end just before kMaxValue. Also
// remember to update the enum listing in
// tools/metrics/histograms/histograms.xml.
enum class CrosUsbNotificationClosed {
  // The notification was dismissed but not by the user (either automatically
  // or because the device was unplugged).
  kUnknown,
  // The user closed the notification via the close box.
  kByUser,
  // The user clicked on the Connect to Linux button of the notification.
  kConnectToLinux,
  // Maximum value for the enum.
  kMaxValue = kConnectToLinux
};

// Represents a USB device tracked by a CrosUsbDetector instance. The
// CrosUsbDetector only exposes devices which can be shared with Guest OSes.
struct CrosUsbDeviceInfo {
  CrosUsbDeviceInfo(std::string guid,
                    base::string16 label,
                    base::Optional<std::string> shared_vm_name,
                    bool prompt_before_sharing);
  CrosUsbDeviceInfo(const CrosUsbDeviceInfo&);
  ~CrosUsbDeviceInfo();

  std::string guid;
  base::string16 label;
  // Name of VM shared with. Unset if not shared. The device may be shared but
  // not yet attached.
  base::Optional<std::string> shared_vm_name;
  // Devices shared with other devices or otherwise in use by the system
  // should have a confirmation prompt shown prior to sharing.
  bool prompt_before_sharing;
};

class CrosUsbDeviceObserver : public base::CheckedObserver {
 public:
  // Called when the available USB devices change.
  virtual void OnUsbDevicesChanged() = 0;
};

// Detects USB Devices for Chrome OS and manages UI for controlling their use
// with CrOS, Web or GuestOSs.
class CrosUsbDetector : public device::mojom::UsbDeviceManagerClient,
                        public chromeos::ConciergeClient::VmObserver,
                        public chromeos::VmPluginDispatcherClient::Observer,
                        public disks::DiskMountManager::Observer {
 public:
  // Used to namespace USB notifications to avoid clashes with WebUsbDetector.
  static std::string MakeNotificationId(const std::string& guid);

  // Can return nullptr.
  static CrosUsbDetector* Get();

  CrosUsbDetector();
  ~CrosUsbDetector() override;

  void SetDeviceManagerForTesting(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager);

  // Connect to the device manager to be notified of connection/removal.
  // Used during browser startup, after connection errors and to setup a fake
  // device manager during testing.
  void ConnectToDeviceManager();

  // Called when a VM starts, to attach USB devices marked as shared to the VM.
  void ConnectSharedDevicesOnVmStartup(const std::string& vm_name);

  // device::mojom::UsbDeviceManagerClient
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device) override;

  // Attaches the device identified by |guid| into the VM identified by
  // |vm_name|. Will unmount filesystems and detach any already shared devices.
  void AttachUsbDeviceToVm(const std::string& vm_name,
                           const std::string& guid,
                           base::OnceCallback<void(bool success)> callback);

  // Detaches the device identified by |guid| from the VM identified by
  // |vm_name|.
  void DetachUsbDeviceFromVm(const std::string& vm_name,
                             const std::string& guid,
                             base::OnceCallback<void(bool success)> callback);

  void AddUsbDeviceObserver(CrosUsbDeviceObserver* observer);
  void RemoveUsbDeviceObserver(CrosUsbDeviceObserver* observer);
  void SignalUsbDeviceObservers();

  // Returns all the USB devices that are shareable with Guest OSes. This may
  // not include all connected devices.
  std::vector<CrosUsbDeviceInfo> GetShareableDevices() const;

 private:
  friend class ::CrosUsbDetectorTest;

  // Internal representation of a USB device.
  struct UsbDevice {
    UsbDevice();
    UsbDevice(const UsbDevice&) = delete;
    UsbDevice(UsbDevice&&);
    ~UsbDevice();

    // Device information from the USB manager.
    device::mojom::UsbDeviceInfoPtr info;

    base::string16 label;

    // Whether the device can be shared with guest OSes.
    bool shareable = false;
    // Name of VM shared with. Unset if not shared. The device may be shared but
    // not yet attached.
    base::Optional<std::string> shared_vm_name;
    // Non-empty only when device is attached to a VM.
    base::Optional<uint8_t> guest_port;
    // Interfaces shareable with guest OSes
    uint32_t allowed_interfaces_mask = 0;
    // For a mass storage device, the mount points for active mounts.
    std::set<std::string> mount_points;
    // An internal flag to suppress observer events as mount_points empties.
    bool is_unmounting = false;
    // TODO(nverne): Add current state and errors etc.
  };

  // chromeos::ConciergeClient::VmObserver:
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;

  // chromeos::VmPluginDispatcherClient::Observer:
  void OnVmToolsStateChanged(
      const vm_tools::plugin_dispatcher::VmToolsStateChangedSignal& signal)
      override;
  void OnVmStateChanged(
      const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) override;

  // disks::DiskMountManager::Observer:
  void OnMountEvent(
      disks::DiskMountManager::MountEvent event,
      MountError error_code,
      const disks::DiskMountManager::MountPointInfo& mount_info) override;

  // Called after USB device access has been checked.
  void OnDeviceChecked(device::mojom::UsbDeviceInfoPtr device,
                       bool hide_notification,
                       bool allowed);

  // Allows the notification to be hidden (OnDeviceAdded without the flag calls
  // this).
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device,
                     bool hide_notification);
  void OnDeviceManagerConnectionError();

  // Callback listing devices attached to the machine.
  void OnListAttachedDevices(
      std::vector<device::mojom::UsbDeviceInfoPtr> devices);

  // Attaching a device goes through the flow:
  // AttachUsbDeviceToVm() -> UnmountFilesystems() -> OnUnmountFilesystems()
  //  -> AttachAfterDetach() -> OnAttachUsbDeviceOpened() -> DoVmAttach()
  //  -> OnUsbDeviceAttachFinished().
  // Unmounting filesystems and detaching devices is only needed in some cases,
  // usually we will skip these.

  // This prevents data corruption and suppresses the notification about
  // ejecting USB drives. A corresponding mount step when detaching from a VM is
  // not necessary as PermissionBroker reattaches the usb-storage drivers,
  // causing the drive to get mounted as usual.
  void UnmountFilesystems(const std::string& vm_name,
                          const std::string& guid,
                          base::OnceCallback<void(bool success)> callback);

  void OnUnmountFilesystems(const std::string& vm_name,
                            const std::string& guid,
                            base::OnceCallback<void(bool success)> callback,
                            bool unmount_success);

  // Devices will be auto-detached if they are attached to another VM.
  void AttachAfterDetach(const std::string& vm_name,
                         const std::string& guid,
                         uint32_t allowed_interfaces_mask,
                         base::OnceCallback<void(bool success)> callback,
                         bool detach_success);

  // Callback for AttachUsbDeviceToVm after opening a file handler.
  void OnAttachUsbDeviceOpened(const std::string& vm_name,
                               device::mojom::UsbDeviceInfoPtr device,
                               base::OnceCallback<void(bool success)> callback,
                               base::File file);

  void DoVmAttach(const std::string& vm_name,
                  device::mojom::UsbDeviceInfoPtr device_info,
                  base::ScopedFD fd,
                  base::OnceCallback<void(bool success)> callback);

  // Callbacks for when the USB device state has been updated.
  void OnUsbDeviceAttachFinished(
      const std::string& vm_name,
      const std::string& guid,
      base::OnceCallback<void(bool success)> callback,
      base::Optional<vm_tools::concierge::AttachUsbDeviceResponse> response);

  void OnUsbDeviceDetachFinished(
      const std::string& vm_name,
      const std::string& guid,
      base::OnceCallback<void(bool success)> callback,
      base::Optional<vm_tools::concierge::DetachUsbDeviceResponse> response);

  // Returns true when a device should show a notification when attached.
  bool ShouldShowNotification(const UsbDevice& device);

  void RelinquishDeviceClaim(const std::string& guid);

  mojo::Remote<device::mojom::UsbDeviceManager> device_manager_;
  mojo::AssociatedReceiver<device::mojom::UsbDeviceManagerClient>
      client_receiver_{this};

  std::vector<device::mojom::UsbDeviceFilterPtr> guest_os_classes_blocked_;
  std::vector<device::mojom::UsbDeviceFilterPtr>
      guest_os_classes_without_notif_;
  device::mojom::UsbDeviceFilterPtr adb_device_filter_;
  device::mojom::UsbDeviceFilterPtr fastboot_device_filter_;

  // GUID -> UsbDevice map for all connected USB devices.
  std::map<std::string, UsbDevice> usb_devices_;

  // Populated when we open the device path on the host. Acts as a claim on the
  // device even if the intended VM has not started yet. Removed when the device
  // is shared successfully with the VM. When an file is closed (here or by the
  // VM,  PermissionBroker will reattach the previous host drivers (if any).
  struct DeviceClaim {
    base::File device_file;
    base::File lifeline_file;
  };
  std::map<std::string, DeviceClaim> devices_claimed_;

  base::ObserverList<CrosUsbDeviceObserver> usb_device_observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrosUsbDetector> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrosUsbDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_
