// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_policy_connector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/command_line_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "components/policy/core/common/policy_loader_win.h"
#elif defined(OS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "components/policy/core/common/config_dir_policy_loader.h"
#elif defined(OS_ANDROID)
#include "components/policy/core/common/android/android_combined_policy_provider.h"
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/policy/chrome_browser_cloud_management_controller_desktop.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/browser_switcher/browser_switcher_policy_migrator.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

namespace policy {
namespace {
bool command_line_enabled_for_testing = false;
}  // namespace

ChromeBrowserPolicyConnector::ChromeBrowserPolicyConnector()
    : BrowserPolicyConnector(base::BindRepeating(&BuildHandlerList)) {
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  chrome_browser_cloud_management_controller_ =
      std::make_unique<ChromeBrowserCloudManagementController>(
          std::make_unique<ChromeBrowserCloudManagementControllerDesktop>());
#endif
}

ChromeBrowserPolicyConnector::~ChromeBrowserPolicyConnector() {}

void ChromeBrowserPolicyConnector::OnResourceBundleCreated() {
  BrowserPolicyConnectorBase::OnResourceBundleCreated();
}

void ChromeBrowserPolicyConnector::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  std::unique_ptr<DeviceManagementService::Configuration> configuration(
      new DeviceManagementServiceConfiguration(GetDeviceManagementUrl(),
                                               GetRealtimeReportingUrl(),
                                               GetEncryptedReportingUrl()));
  std::unique_ptr<DeviceManagementService> device_management_service(
      new DeviceManagementService(std::move(configuration)));
  device_management_service->ScheduleInitialization(
      kServiceInitializationStartupDelay);

#if defined(OS_ANDROID)
  pollicy_cache_updater_ = std::make_unique<android::PolicyCacheUpdater>(
      GetPolicyService(), GetHandlerList());
#endif

  InitInternal(local_state, std::move(device_management_service));
}

bool ChromeBrowserPolicyConnector::IsEnterpriseManaged() const {
  NOTREACHED() << "This method is only defined for Chrome OS";
  return false;
}

bool ChromeBrowserPolicyConnector::HasMachineLevelPolicies() {
  if (ProviderHasPolicies(GetPlatformProvider()))
    return true;
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (ProviderHasPolicies(machine_level_user_cloud_policy_manager_))
    return true;
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (ProviderHasPolicies(command_line_provider_))
    return true;
  return false;
}

void ChromeBrowserPolicyConnector::Shutdown() {
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Reset the controller before calling base class so that
  // shutdown occurs in correct sequence.
  chrome_browser_cloud_management_controller_.reset();
#endif

  BrowserPolicyConnector::Shutdown();
}

ConfigurationPolicyProvider*
ChromeBrowserPolicyConnector::GetPlatformProvider() {
  ConfigurationPolicyProvider* provider =
      BrowserPolicyConnectorBase::GetPolicyProviderForTesting();
  return provider ? provider : platform_provider_;
}

bool ChromeBrowserPolicyConnector::IsCommandLineSwitchSupported() const {
  if (command_line_enabled_for_testing)
    return true;

  version_info::Channel channel = chrome::GetChannel();
  return channel != version_info::Channel::STABLE &&
         channel != version_info::Channel::BETA;
}

// static
void ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting() {
  command_line_enabled_for_testing = true;
}

std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
ChromeBrowserPolicyConnector::CreatePolicyProviders() {
  auto providers = BrowserPolicyConnector::CreatePolicyProviders();
  std::unique_ptr<ConfigurationPolicyProvider> platform_provider =
      CreatePlatformProvider();
  if (platform_provider) {
    platform_provider_ = platform_provider.get();
    // PlatformProvider should be before all other providers (highest priority).
    providers.insert(providers.begin(), std::move(platform_provider));
  }

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<MachineLevelUserCloudPolicyManager>
      machine_level_user_cloud_policy_manager =
          chrome_browser_cloud_management_controller_->CreatePolicyManager(
              platform_provider_);
  if (machine_level_user_cloud_policy_manager) {
    machine_level_user_cloud_policy_manager_ =
        machine_level_user_cloud_policy_manager.get();
    providers.push_back(std::move(machine_level_user_cloud_policy_manager));
  }
#endif

  std::unique_ptr<CommandLinePolicyProvider> command_line_provider =
      CommandLinePolicyProvider::CreateIfAllowed(
          *base::CommandLine::ForCurrentProcess(), chrome::GetChannel());
  if (command_line_provider) {
    command_line_provider_ = command_line_provider.get();
    providers.push_back(std::move(command_line_provider));
  }

  return providers;
}

std::unique_ptr<ConfigurationPolicyProvider>
ChromeBrowserPolicyConnector::CreatePlatformProvider() {
#if defined(OS_WIN)
  std::unique_ptr<AsyncPolicyLoader> loader(PolicyLoaderWin::Create(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      kRegistryChromePolicyKey));
  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#elif defined(OS_MAC)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Explicitly watch the "com.google.Chrome" bundle ID, no matter what this
  // app's bundle ID actually is. All channels of Chrome should obey the same
  // policies.
  CFStringRef bundle_id = CFSTR("com.google.Chrome");
#else
  base::ScopedCFTypeRef<CFStringRef> bundle_id(
      base::SysUTF8ToCFStringRef(base::mac::BaseBundleID()));
#endif
  auto loader = std::make_unique<PolicyLoaderMac>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      policy::PolicyLoaderMac::GetManagedPolicyPath(bundle_id),
      new MacPreferences(), bundle_id);
  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto loader = std::make_unique<PolicyLoaderLacros>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  base::FilePath config_dir_path;
  if (base::PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    std::unique_ptr<AsyncPolicyLoader> loader(new ConfigDirPolicyLoader(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
        config_dir_path, POLICY_SCOPE_MACHINE));
    return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                                 std::move(loader));
  } else {
    return nullptr;
  }
#elif defined(OS_ANDROID)
  return std::make_unique<policy::android::AndroidCombinedPolicyProvider>(
      GetSchemaRegistry());
#else
  return nullptr;
#endif
}

}  // namespace policy
