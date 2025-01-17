// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_healthd/fake_cros_healthd_service.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {
namespace cros_healthd {

FakeCrosHealthdService::RoutineUpdateParams::RoutineUpdateParams(
    int32_t id,
    mojom::DiagnosticRoutineCommandEnum command,
    bool include_output)
    : id(id), command(command), include_output(include_output) {}

FakeCrosHealthdService::FakeCrosHealthdService() = default;
FakeCrosHealthdService::~FakeCrosHealthdService() = default;

void FakeCrosHealthdService::GetProbeService(
    mojom::CrosHealthdProbeServiceRequest service) {
  probe_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::GetDiagnosticsService(
    mojom::CrosHealthdDiagnosticsServiceRequest service) {
  diagnostics_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::GetEventService(
    mojom::CrosHealthdEventServiceRequest service) {
  event_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::SendNetworkHealthService(
    mojo::PendingRemote<chromeos::network_health::mojom::NetworkHealthService>
        remote) {
  network_health_remote_.Bind(std::move(remote));
}

void FakeCrosHealthdService::SendNetworkDiagnosticsRoutines(
    mojo::PendingRemote<
        chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
        network_diagnostics_routines) {
  network_diagnostics_routines_.Bind(std::move(network_diagnostics_routines));
}

void FakeCrosHealthdService::GetSystemService(
    mojom::CrosHealthdSystemServiceRequest service) {
  system_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::GetServiceStatus(
    GetServiceStatusCallback callback) {
  auto response = mojom::ServiceStatus::New();
  response->network_health_bound = network_health_remote_.is_bound();
  response->network_diagnostics_bound = network_health_remote_.is_bound();
  std::move(callback).Run(std::move(response));
}

void FakeCrosHealthdService::GetAvailableRoutines(
    GetAvailableRoutinesCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), available_routines_),
      callback_delay_);
}

void FakeCrosHealthdService::GetRoutineUpdate(
    int32_t id,
    mojom::DiagnosticRoutineCommandEnum command,
    bool include_output,
    GetRoutineUpdateCallback callback) {
  routine_update_params_ =
      FakeCrosHealthdService::RoutineUpdateParams(id, command, include_output);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          mojom::RoutineUpdate::New(
              routine_update_response_->progress_percent,
              std::move(routine_update_response_->output),
              std::move(routine_update_response_->routine_update_union))),
      callback_delay_);
}

void FakeCrosHealthdService::RunUrandomRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunUrandomRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryCapacityRoutine(
    RunBatteryCapacityRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryHealthRoutine(
    RunBatteryHealthRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunSmartctlCheckRoutine(
    RunSmartctlCheckRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunAcPowerRoutine(
    mojom::AcPowerStatusEnum expected_status,
    const base::Optional<std::string>& expected_power_type,
    RunAcPowerRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunCpuCacheRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunCpuCacheRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunCpuStressRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunCpuStressRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunFloatingPointAccuracyRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunFloatingPointAccuracyRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunNvmeWearLevelRoutine(
    uint32_t wear_level_threshold,
    RunNvmeWearLevelRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunNvmeSelfTestRoutine(
    mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
    RunNvmeSelfTestRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunDiskReadRoutine(
    mojom::DiskReadRoutineTypeEnum type,
    uint32_t length_seconds,
    uint32_t file_size_mb,
    RunDiskReadRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunPrimeSearchRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunPrimeSearchRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryDischargeRoutine(
    uint32_t length_seconds,
    uint32_t maximum_discharge_percent_allowed,
    RunBatteryDischargeRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunBatteryChargeRoutine(
    uint32_t length_seconds,
    uint32_t minimum_charge_percent_required,
    RunBatteryChargeRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunMemoryRoutine(
    RunMemoryRoutineCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::RunLanConnectivityRoutine(
    RunLanConnectivityRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunSignalStrengthRoutine(
    RunSignalStrengthRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunGatewayCanBePingedRoutine(
    RunGatewayCanBePingedRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunHasSecureWiFiConnectionRoutine(
    RunHasSecureWiFiConnectionRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunDnsResolverPresentRoutine(
    RunDnsResolverPresentRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunDnsLatencyRoutine(
    RunDnsLatencyRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunDnsResolutionRoutine(
    RunDnsResolutionRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunCaptivePortalRoutine(
    RunCaptivePortalRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunHttpFirewallRoutine(
    RunHttpFirewallRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunHttpsFirewallRoutine(
    RunHttpsFirewallRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunHttpsLatencyRoutine(
    RunHttpsLatencyRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunVideoConferencingRoutine(
    const base::Optional<std::string>& stun_server_hostname,
    RunVideoConferencingRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::AddBluetoothObserver(
    mojom::CrosHealthdBluetoothObserverPtr observer) {
  bluetooth_observers_.Add(observer.PassInterface());
}

void FakeCrosHealthdService::AddLidObserver(
    mojom::CrosHealthdLidObserverPtr observer) {
  lid_observers_.Add(observer.PassInterface());
}

void FakeCrosHealthdService::AddPowerObserver(
    mojom::CrosHealthdPowerObserverPtr observer) {
  power_observers_.Add(observer.PassInterface());
}

void FakeCrosHealthdService::AddNetworkObserver(
    mojo::PendingRemote<chromeos::network_health::mojom::NetworkEventsObserver>
        observer) {
  network_observers_.Add(std::move(observer));
}

void FakeCrosHealthdService::ProbeTelemetryInfo(
    const std::vector<mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), telemetry_response_info_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::ProbeProcessInfo(
    const uint32_t process_id,
    ProbeProcessInfoCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), process_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthdService::SetAvailableRoutinesForTesting(
    const std::vector<mojom::DiagnosticRoutineEnum>& available_routines) {
  available_routines_ = available_routines;
}

void FakeCrosHealthdService::SetRunRoutineResponseForTesting(
    mojom::RunRoutineResponsePtr& response) {
  run_routine_response_.Swap(&response);
}

void FakeCrosHealthdService::SetGetRoutineUpdateResponseForTesting(
    mojom::RoutineUpdatePtr& response) {
  routine_update_response_.Swap(&response);
}

void FakeCrosHealthdService::SetProbeTelemetryInfoResponseForTesting(
    mojom::TelemetryInfoPtr& response_info) {
  telemetry_response_info_.Swap(&response_info);
}

void FakeCrosHealthdService::SetProbeProcessInfoResponseForTesting(
    mojom::ProcessResultPtr& result) {
  process_response_.Swap(&result);
}

void FakeCrosHealthdService::SetCallbackDelay(base::TimeDelta delay) {
  callback_delay_ = delay;
}

void FakeCrosHealthdService::EmitAcInsertedEventForTesting() {
  for (auto& observer : power_observers_)
    observer->OnAcInserted();
}

void FakeCrosHealthdService::EmitAcRemovedEventForTesting() {
  for (auto& observer : power_observers_)
    observer->OnAcRemoved();
}

void FakeCrosHealthdService::EmitOsSuspendEventForTesting() {
  for (auto& observer : power_observers_)
    observer->OnOsSuspend();
}

void FakeCrosHealthdService::EmitOsResumeEventForTesting() {
  for (auto& observer : power_observers_)
    observer->OnOsResume();
}

void FakeCrosHealthdService::EmitAdapterAddedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnAdapterAdded();
}

void FakeCrosHealthdService::EmitAdapterRemovedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnAdapterRemoved();
}

void FakeCrosHealthdService::EmitAdapterPropertyChangedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnAdapterPropertyChanged();
}

void FakeCrosHealthdService::EmitDeviceAddedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnDeviceAdded();
}

void FakeCrosHealthdService::EmitDeviceRemovedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnDeviceRemoved();
}

void FakeCrosHealthdService::EmitDevicePropertyChangedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnDevicePropertyChanged();
}

void FakeCrosHealthdService::EmitLidClosedEventForTesting() {
  for (auto& observer : lid_observers_)
    observer->OnLidClosed();
}

void FakeCrosHealthdService::EmitLidOpenedEventForTesting() {
  for (auto& observer : lid_observers_)
    observer->OnLidOpened();
}

void FakeCrosHealthdService::EmitConnectionStateChangedEventForTesting(
    const std::string& network_guid,
    chromeos::network_health::mojom::NetworkState state) {
  for (auto& observer : network_observers_) {
    observer->OnConnectionStateChanged(network_guid, state);
  }
}

void FakeCrosHealthdService::EmitSignalStrengthChangedEventForTesting(
    const std::string& network_guid,
    chromeos::network_health::mojom::UInt32ValuePtr signal_strength) {
  for (auto& observer : network_observers_) {
    observer->OnSignalStrengthChanged(
        network_guid, chromeos::network_health::mojom::UInt32Value::New(
                          signal_strength->value));
  }
}

void FakeCrosHealthdService::RequestNetworkHealthForTesting(
    chromeos::network_health::mojom::NetworkHealthService::
        GetHealthSnapshotCallback callback) {
  network_health_remote_->GetHealthSnapshot(std::move(callback));
}

void FakeCrosHealthdService::RunLanConnectivityRoutineForTesting(
    chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines::
        LanConnectivityCallback callback) {
  network_diagnostics_routines_->LanConnectivity(std::move(callback));
}

base::Optional<FakeCrosHealthdService::RoutineUpdateParams>
FakeCrosHealthdService::GetRoutineUpdateParams() const {
  return routine_update_params_;
}

}  // namespace cros_healthd
}  // namespace chromeos
