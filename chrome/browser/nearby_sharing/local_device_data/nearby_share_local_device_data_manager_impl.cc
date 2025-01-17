// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_profile_info_provider.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater_impl.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

// Using the alphanumeric characters below, this provides 36^10 unique device
// IDs. Note that the uniqueness requirement is not global; the IDs are only
// used to differentiate between devices associated with a single GAIA account.
// This ID length agrees with the GmsCore implementation.
const size_t kDeviceIdLength = 10;

// Possible characters used in a randomly generated device ID. This agrees with
// the GmsCore implementation.
constexpr std::array<char, 36> kAlphaNumericChars = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

constexpr base::TimeDelta kUpdateDeviceDataTimeout =
    base::TimeDelta::FromSeconds(30);
constexpr base::TimeDelta kDeviceDataDownloadPeriod =
    base::TimeDelta::FromHours(12);

// Returns a truncated version of |name| that is |overflow_length| characters
// too long. For example, name="Reallylongname" with overflow_length=5 will
// return "Really...".
std::string GetTruncatedName(std::string name, size_t overflow_length) {
  std::string ellipsis("...");
  size_t max_name_length = name.length() - overflow_length - ellipsis.length();
  DCHECK_GT(max_name_length, 0u);
  std::string truncated;
  base::TruncateUTF8ToByteSize(name, max_name_length, &truncated);
  truncated.append(ellipsis);
  return truncated;
}

}  // namespace

// static
NearbyShareLocalDeviceDataManagerImpl::Factory*
    NearbyShareLocalDeviceDataManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareLocalDeviceDataManager>
NearbyShareLocalDeviceDataManagerImpl::Factory::Create(
    PrefService* pref_service,
    NearbyShareClientFactory* http_client_factory,
    NearbyShareProfileInfoProvider* profile_info_provider) {
  if (test_factory_) {
    return test_factory_->CreateInstance(pref_service, http_client_factory,
                                         profile_info_provider);
  }

  return base::WrapUnique(new NearbyShareLocalDeviceDataManagerImpl(
      pref_service, http_client_factory, profile_info_provider));
}

// static
void NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareLocalDeviceDataManagerImpl::Factory::~Factory() = default;

NearbyShareLocalDeviceDataManagerImpl::NearbyShareLocalDeviceDataManagerImpl(
    PrefService* pref_service,
    NearbyShareClientFactory* http_client_factory,
    NearbyShareProfileInfoProvider* profile_info_provider)
    : pref_service_(pref_service),
      profile_info_provider_(profile_info_provider),
      device_data_updater_(NearbyShareDeviceDataUpdaterImpl::Factory::Create(
          GetId(),
          kUpdateDeviceDataTimeout,
          http_client_factory)),
      download_device_data_scheduler_(
          NearbyShareSchedulerFactory::CreatePeriodicScheduler(
              kDeviceDataDownloadPeriod,
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerDownloadDeviceDataPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareLocalDeviceDataManagerImpl::
                                      OnDownloadDeviceDataRequested,
                                  base::Unretained(this)))) {}

NearbyShareLocalDeviceDataManagerImpl::
    ~NearbyShareLocalDeviceDataManagerImpl() = default;

std::string NearbyShareLocalDeviceDataManagerImpl::GetId() {
  std::string id =
      pref_service_->GetString(prefs::kNearbySharingDeviceIdPrefName);
  if (!id.empty())
    return id;

  for (size_t i = 0; i < kDeviceIdLength; ++i)
    id += kAlphaNumericChars[base::RandGenerator(kAlphaNumericChars.size())];

  pref_service_->SetString(prefs::kNearbySharingDeviceIdPrefName, id);

  return id;
}

std::string NearbyShareLocalDeviceDataManagerImpl::GetDeviceName() const {
  std::string device_name =
      pref_service_->GetString(prefs::kNearbySharingDeviceNamePrefName);
  return device_name.empty() ? GetDefaultDeviceName() : device_name;
}

base::Optional<std::string> NearbyShareLocalDeviceDataManagerImpl::GetFullName()
    const {
  std::string name =
      pref_service_->GetString(prefs::kNearbySharingFullNamePrefName);
  if (name.empty())
    return base::nullopt;

  return name;
}

base::Optional<std::string> NearbyShareLocalDeviceDataManagerImpl::GetIconUrl()
    const {
  std::string url =
      pref_service_->GetString(prefs::kNearbySharingIconUrlPrefName);
  if (url.empty())
    return base::nullopt;

  return url;
}

nearby_share::mojom::DeviceNameValidationResult
NearbyShareLocalDeviceDataManagerImpl::ValidateDeviceName(
    const std::string& name) {
  if (name.empty())
    return nearby_share::mojom::DeviceNameValidationResult::kErrorEmpty;

  if (!base::IsStringUTF8(name))
    return nearby_share::mojom::DeviceNameValidationResult::kErrorNotValidUtf8;

  if (name.length() > kNearbyShareDeviceNameMaxLength)
    return nearby_share::mojom::DeviceNameValidationResult::kErrorTooLong;

  return nearby_share::mojom::DeviceNameValidationResult::kValid;
}

nearby_share::mojom::DeviceNameValidationResult
NearbyShareLocalDeviceDataManagerImpl::SetDeviceName(const std::string& name) {
  if (name == GetDeviceName())
    return nearby_share::mojom::DeviceNameValidationResult::kValid;

  auto error = ValidateDeviceName(name);
  if (error != nearby_share::mojom::DeviceNameValidationResult::kValid)
    return error;

  pref_service_->SetString(prefs::kNearbySharingDeviceNamePrefName, name);

  NotifyLocalDeviceDataChanged(/*did_device_name_change=*/true,
                               /*did_full_name_change=*/false,
                               /*did_icon_url_change=*/false);

  return nearby_share::mojom::DeviceNameValidationResult::kValid;
}

void NearbyShareLocalDeviceDataManagerImpl::DownloadDeviceData() {
  download_device_data_scheduler_->MakeImmediateRequest();
}

void NearbyShareLocalDeviceDataManagerImpl::UploadContacts(
    std::vector<nearbyshare::proto::Contact> contacts,
    UploadCompleteCallback callback) {
  device_data_updater_->UpdateDeviceData(
      std::move(contacts),
      /*certificates=*/base::nullopt,
      base::BindOnce(
          &NearbyShareLocalDeviceDataManagerImpl::OnUploadContactsFinished,
          base::Unretained(this), std::move(callback)));
}

void NearbyShareLocalDeviceDataManagerImpl::UploadCertificates(
    std::vector<nearbyshare::proto::PublicCertificate> certificates,
    UploadCompleteCallback callback) {
  device_data_updater_->UpdateDeviceData(
      /*contacts=*/base::nullopt, std::move(certificates),
      base::BindOnce(
          &NearbyShareLocalDeviceDataManagerImpl::OnUploadCertificatesFinished,
          base::Unretained(this), std::move(callback)));
}

void NearbyShareLocalDeviceDataManagerImpl::OnStart() {
  // This schedules an immediate download of the full name and icon URL from the
  // server if that has never happened before.
  download_device_data_scheduler_->Start();
}

void NearbyShareLocalDeviceDataManagerImpl::OnStop() {
  download_device_data_scheduler_->Stop();
}

std::string NearbyShareLocalDeviceDataManagerImpl::GetDefaultDeviceName()
    const {
  base::string16 device_type = ui::GetChromeOSDeviceName();
  base::Optional<base::string16> given_name =
      profile_info_provider_->GetGivenName();
  if (!given_name)
    return base::UTF16ToUTF8(device_type);

  std::string device_name = l10n_util::GetStringFUTF8(
      IDS_NEARBY_DEFAULT_DEVICE_NAME, *given_name, device_type);
  if (device_name.length() <= kNearbyShareDeviceNameMaxLength)
    return device_name;

  std::string truncated_name =
      GetTruncatedName(base::UTF16ToUTF8(*given_name),
                       device_name.length() - kNearbyShareDeviceNameMaxLength);
  return l10n_util::GetStringFUTF8(IDS_NEARBY_DEFAULT_DEVICE_NAME,
                                   base::UTF8ToUTF16(truncated_name),
                                   device_type);
}

void NearbyShareLocalDeviceDataManagerImpl::OnDownloadDeviceDataRequested() {
  device_data_updater_->UpdateDeviceData(
      /*contacts=*/base::nullopt,
      /*certificates=*/base::nullopt,
      base::BindOnce(
          &NearbyShareLocalDeviceDataManagerImpl::OnDownloadDeviceDataFinished,
          base::Unretained(this)));
}

void NearbyShareLocalDeviceDataManagerImpl::OnDownloadDeviceDataFinished(
    const base::Optional<nearbyshare::proto::UpdateDeviceResponse>& response) {
  if (response)
    HandleUpdateDeviceResponse(response);

  download_device_data_scheduler_->HandleResult(
      /*success=*/response.has_value());
}

void NearbyShareLocalDeviceDataManagerImpl::OnUploadContactsFinished(
    UploadCompleteCallback callback,
    const base::Optional<nearbyshare::proto::UpdateDeviceResponse>& response) {
  if (response)
    HandleUpdateDeviceResponse(response);

  std::move(callback).Run(/*success=*/response.has_value());
}

void NearbyShareLocalDeviceDataManagerImpl::OnUploadCertificatesFinished(
    UploadCompleteCallback callback,
    const base::Optional<nearbyshare::proto::UpdateDeviceResponse>& response) {
  if (response)
    HandleUpdateDeviceResponse(response);

  std::move(callback).Run(/*success=*/response.has_value());
}

void NearbyShareLocalDeviceDataManagerImpl::HandleUpdateDeviceResponse(
    const base::Optional<nearbyshare::proto::UpdateDeviceResponse>& response) {
  if (!response)
    return;

  bool did_full_name_change = response->person_name() != GetFullName();
  bool did_icon_url_change = response->image_url() != GetIconUrl();
  if (!did_full_name_change && !did_icon_url_change)
    return;

  if (did_full_name_change) {
    pref_service_->SetString(prefs::kNearbySharingFullNamePrefName,
                             response->person_name());
  }
  if (did_icon_url_change) {
    pref_service_->SetString(prefs::kNearbySharingIconUrlPrefName,
                             response->image_url());
  }

  NotifyLocalDeviceDataChanged(/*did_device_name_change=*/false,
                               did_full_name_change, did_icon_url_change);
}
