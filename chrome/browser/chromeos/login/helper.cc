// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/helper.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/login/signin_partition_manager.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

gfx::Rect CalculateScreenBounds(const gfx::Size& size) {
  gfx::Rect bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  if (!size.IsEmpty()) {
    int horizontal_diff = bounds.width() - size.width();
    int vertical_diff = bounds.height() - size.height();
    bounds.Inset(horizontal_diff / 2, vertical_diff / 2);
  }
  return bounds;
}

int GetCurrentUserImageSize() {
  // The biggest size that the profile picture is displayed at is currently
  // 220px, used for the big preview on OOBE and Change Picture options page.
  static const int kBaseUserImageSize = 220;
  float scale_factor = display::Display::GetForcedDeviceScaleFactor();
  if (scale_factor > 1.0f)
    return static_cast<int>(scale_factor * kBaseUserImageSize);
  return kBaseUserImageSize * gfx::ImageSkia::GetMaxSupportedScale();
}

namespace login {

NetworkStateHelper::NetworkStateHelper() {}
NetworkStateHelper::~NetworkStateHelper() {}

base::string16 NetworkStateHelper::GetCurrentNetworkName() const {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  const NetworkState* network =
      nsh->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());
  if (network) {
    if (network->Matches(NetworkTypePattern::Ethernet()))
      return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    return base::UTF8ToUTF16(network->name());
  }

  network = nsh->ConnectingNetworkByType(NetworkTypePattern::NonVirtual());
  if (network) {
    if (network->Matches(NetworkTypePattern::Ethernet()))
      return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    return base::UTF8ToUTF16(network->name());
  }
  return base::string16();
}

bool NetworkStateHelper::IsConnected() const {
  chromeos::NetworkStateHandler* nsh =
      chromeos::NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectedNetworkByType(chromeos::NetworkTypePattern::Default()) !=
         nullptr;
}

bool NetworkStateHelper::IsConnecting() const {
  chromeos::NetworkStateHandler* nsh =
      chromeos::NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectingNetworkByType(
             chromeos::NetworkTypePattern::Default()) != nullptr;
}

void NetworkStateHelper::OnCreateConfiguration(
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback,
    const std::string& service_path,
    const std::string& guid) const {
  // Connect to the network.
  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path, std::move(success_callback), std::move(error_callback),
      false /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
}

content::StoragePartition* GetSigninPartition() {
  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  SigninPartitionManager* signin_partition_manager =
      SigninPartitionManager::Factory::GetForBrowserContext(signin_profile);
  if (!signin_partition_manager->IsInSigninSession())
    return nullptr;
  return signin_partition_manager->GetCurrentStoragePartition();
}

network::mojom::NetworkContext* GetSigninNetworkContext() {
  content::StoragePartition* signin_partition = GetSigninPartition();

  if (!signin_partition)
    return nullptr;

  return signin_partition->GetNetworkContext();
}

scoped_refptr<network::SharedURLLoaderFactory> GetSigninURLLoaderFactory() {
  content::StoragePartition* signin_partition = GetSigninPartition();

  // Special case for unit tests. There's no LoginDisplayHost thus no
  // webview instance. See http://crbug.com/477402
  if (!signin_partition && !LoginDisplayHost::default_host())
    return ProfileHelper::GetSigninProfile()->GetURLLoaderFactory();

  if (!signin_partition)
    return nullptr;

  return signin_partition->GetURLLoaderFactoryForBrowserProcess();
}

void SaveSyncPasswordDataToProfile(const UserContext& user_context,
                                   Profile* profile) {
  DCHECK(user_context.GetSyncPasswordData().has_value());
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS);
  if (password_store) {
    password_store->SaveSyncPasswordHash(
        user_context.GetSyncPasswordData().value(),
        password_manager::metrics_util::GaiaPasswordHashChange::
            SAVED_ON_CHROME_SIGNIN);
  }
}

base::TimeDelta TimeToOnlineSignIn(base::Time last_online_signin,
                                   base::TimeDelta offline_signin_limit) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  // Time left to the next forced online signin.
  return offline_signin_limit - (now - last_online_signin);
}

}  // namespace login

}  // namespace chromeos
