// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_profile_info_provider_impl.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"

NearbyShareProfileInfoProviderImpl::NearbyShareProfileInfoProviderImpl(
    Profile* profile)
    : profile_(profile) {}

NearbyShareProfileInfoProviderImpl::~NearbyShareProfileInfoProviderImpl() =
    default;

base::Optional<base::string16>
NearbyShareProfileInfoProviderImpl::GetGivenName() const {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (!user)
    return base::nullopt;

  base::string16 name = user->GetGivenName();
  return name.empty() ? base::nullopt : base::make_optional(name);
}

base::Optional<std::string>
NearbyShareProfileInfoProviderImpl::GetProfileUserName() const {
  std::string name = profile_->GetProfileUserName();
  return name.empty() ? base::nullopt : base::make_optional(name);
}
