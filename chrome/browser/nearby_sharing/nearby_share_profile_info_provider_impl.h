// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_PROFILE_INFO_PROVIDER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_PROFILE_INFO_PROVIDER_IMPL_H_

#include "chrome/browser/nearby_sharing/common/nearby_share_profile_info_provider.h"

class Profile;

// An implementation of NearbyShareProfileInfoProfider that accesses the actual
// profile data.
class NearbyShareProfileInfoProviderImpl
    : public NearbyShareProfileInfoProvider {
 public:
  explicit NearbyShareProfileInfoProviderImpl(Profile* profile);
  NearbyShareProfileInfoProviderImpl(
      const NearbyShareProfileInfoProviderImpl&) = delete;
  NearbyShareProfileInfoProviderImpl& operator=(
      const NearbyShareProfileInfoProviderImpl&) = delete;
  ~NearbyShareProfileInfoProviderImpl() override;

  // NearbyShareProfileInfoProvider:
  base::Optional<base::string16> GetGivenName() const override;
  base::Optional<std::string> GetProfileUserName() const override;

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_PROFILE_INFO_PROVIDER_IMPL_H_
