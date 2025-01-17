// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_cloud_management_controller_desktop.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_identity_provider.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/enterprise/remote_commands/cbcm_remote_commands_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_register_watcher.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "chrome/browser/policy/cloud/remote_commands_invalidator_impl.h"
#include "chrome/browser/policy/device_account_initializer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/base_paths_win.h"
#include "chrome/install_static/install_modes.h"
#else
#include "chrome/common/chrome_switches.h"
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if defined(OS_MAC)
#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/policy/browser_dm_token_storage_mac.h"
#endif  // defined(OS_MAC)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "chrome/browser/policy/browser_dm_token_storage_linux.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include "chrome/browser/policy/browser_dm_token_storage_win.h"
#include "chrome/install_static/install_util.h"
#endif  // defined(OS_WIN)

namespace policy {

namespace {

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr base::FilePath::StringPieceType kCachedPolicyDirname =
    FILE_PATH_LITERAL("Policies");
constexpr base::FilePath::StringPieceType kCachedPolicyFilename =
    FILE_PATH_LITERAL("PolicyFetchResponse");
#endif

}  // namespace

// A helper class to make the appropriate calls into the device account
// initializer and manage the ChromeBrowserCloudManagementRegistrar callback's
// lifetime.
class MachineLevelDeviceAccountInitializerHelper
    : public DeviceAccountInitializer::Delegate {
 public:
  using Callback = base::OnceCallback<void(bool)>;

  // |policy_client| should be registered and outlive this object.
  MachineLevelDeviceAccountInitializerHelper(
      policy::CloudPolicyClient* policy_client,
      Callback callback,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : policy_client_(std::move(policy_client)),
        callback_(std::move(callback)),
        url_loader_factory_(url_loader_factory) {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations));

    DCHECK(url_loader_factory_);

    device_account_initializer_ =
        std::make_unique<DeviceAccountInitializer>(policy_client_, this);
    device_account_initializer_->FetchToken();
  }

  MachineLevelDeviceAccountInitializerHelper& operator=(
      MachineLevelDeviceAccountInitializerHelper&) = delete;
  MachineLevelDeviceAccountInitializerHelper(
      MachineLevelDeviceAccountInitializerHelper&) = delete;
  MachineLevelDeviceAccountInitializerHelper(
      MachineLevelDeviceAccountInitializerHelper&&) = delete;

  ~MachineLevelDeviceAccountInitializerHelper() override = default;

  // DeviceAccountInitializer::Delegate:
  void OnDeviceAccountTokenFetched(bool empty_token) override {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
    if (empty_token) {
      // Not being able to obtain a token isn't a showstopper for machine
      // level policies: the browser will fallback to fetching policies on a
      // regular schedule and won't support remote commands. Getting a refresh
      // token will be reattempted on the next successful policy fetch.
      std::move(callback_).Run(false);
      return;
    }

    device_account_initializer_->StoreToken();
  }

  void OnDeviceAccountTokenStored() override {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
    std::move(callback_).Run(true);
  }

  void OnDeviceAccountTokenError(EnrollmentStatus status) override {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
    std::move(callback_).Run(false);
  }

  void OnDeviceAccountClientError(DeviceManagementStatus status) override {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
    std::move(callback_).Run(false);
  }

  enterprise_management::DeviceServiceApiAccessRequest::DeviceType
  GetRobotAuthCodeDeviceType() override {
    return enterprise_management::DeviceServiceApiAccessRequest::CHROME_BROWSER;
  }

  std::set<std::string> GetRobotOAuthScopes() override {
    return {
        GaiaConstants::kGoogleUserInfoEmail,
        GaiaConstants::kFCMOAuthScope,
    };
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return url_loader_factory_;
  }

  policy::CloudPolicyClient* policy_client_;
  std::unique_ptr<DeviceAccountInitializer> device_account_initializer_;
  Callback callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

ChromeBrowserCloudManagementControllerDesktop::
    ChromeBrowserCloudManagementControllerDesktop() = default;
ChromeBrowserCloudManagementControllerDesktop::
    ~ChromeBrowserCloudManagementControllerDesktop() = default;

void ChromeBrowserCloudManagementControllerDesktop::
    SetDMTokenStorageDelegate() {
  std::unique_ptr<BrowserDMTokenStorage::Delegate> storage_delegate;

#if defined(OS_MAC)
  storage_delegate = std::make_unique<BrowserDMTokenStorageMac>();
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  storage_delegate = std::make_unique<BrowserDMTokenStorageLinux>();
#elif defined(OS_WIN)
  storage_delegate = std::make_unique<BrowserDMTokenStorageWin>();
#else
  NOT_REACHED();
#endif

  BrowserDMTokenStorage::SetDelegate(std::move(storage_delegate));
}

int ChromeBrowserCloudManagementControllerDesktop::GetUserDataDirKey() {
  return chrome::DIR_USER_DATA;
}

base::FilePath
ChromeBrowserCloudManagementControllerDesktop::GetExternalPolicyPath() {
  base::FilePath external_policy_path;
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::PathService::Get(base::DIR_PROGRAM_FILESX86, &external_policy_path);

  external_policy_path =
      external_policy_path.Append(install_static::kCompanyPathName)
          .Append(kCachedPolicyDirname)
          .AppendASCII(
              policy::dm_protocol::kChromeMachineLevelUserCloudPolicyTypeBase64)
          .Append(kCachedPolicyFilename);
#endif

  return external_policy_path;
}

ChromeBrowserCloudManagementController::Delegate::NetworkConnectionTrackerGetter
ChromeBrowserCloudManagementControllerDesktop::
    CreateNetworkConnectionTrackerGetter() {
  return base::BindRepeating(&content::GetNetworkConnectionTracker);
}

void ChromeBrowserCloudManagementControllerDesktop::InitializeOAuthTokenFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_state) {
  DeviceOAuth2TokenServiceFactory::Initialize(url_loader_factory, local_state);
}

void ChromeBrowserCloudManagementControllerDesktop::StartWatchingRegistration(
    ChromeBrowserCloudManagementController* controller) {
  cloud_management_register_watcher_ =
      std::make_unique<ChromeBrowserCloudManagementRegisterWatcher>(controller);
}

bool ChromeBrowserCloudManagementControllerDesktop::
    WaitUntilPolicyEnrollmentFinished() {
  if (cloud_management_register_watcher_) {
    switch (cloud_management_register_watcher_
                ->WaitUntilCloudPolicyEnrollmentFinished()) {
      case ChromeBrowserCloudManagementController::RegisterResult::
          kNoEnrollmentNeeded:
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentSuccessBeforeDialogDisplayed:
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentFailedSilentlyBeforeDialogDisplayed:
        return true;
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentSuccess:
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentFailedSilently:
#if defined(OS_MAC)
        app_controller_mac::EnterpriseStartupDialogClosed();
#endif
        return true;
      case ChromeBrowserCloudManagementController::RegisterResult::
          kRestartDueToFailure:
        chrome::AttemptRestart();
        return false;
      case ChromeBrowserCloudManagementController::RegisterResult::
          kQuitDueToFailure:
        chrome::AttemptExit();
        return false;
    }
  }
  return true;
}

bool ChromeBrowserCloudManagementControllerDesktop::
    IsEnterpriseStartupDialogShowing() {
  return cloud_management_register_watcher_ &&
         cloud_management_register_watcher_->IsDialogShowing();
}

void ChromeBrowserCloudManagementControllerDesktop::OnServiceAccountSet(
    CloudPolicyClient* client,
    const std::string& account_email) {
  if (!base::FeatureList::IsEnabled(
          policy::features::kCBCMPolicyInvalidations)) {
    return;
  }

  // No need to get a refresh token if there is one present already.
  if (!DeviceOAuth2TokenServiceFactory::Get()->RefreshTokenIsAvailable()) {
    if (account_initializer_helper_) {
      // Bail out early if there's already an active account initializer,
      // otherwise multiple auth requests might race to completion and attempt
      // to initiate multiple invalidations service instances.
      NOTREACHED() << "Trying to start an account initializer when there's "
                      "already one. Please see crbug.com/1186159.";
      return;
    }

    // If this feature is enabled, we need to ensure the device service
    // account is initialized and fetch auth codes to exchange for a refresh
    // token. Creating this object starts that process and the callback will
    // be called from it whether it succeeds or not.
    DeviceOAuth2TokenServiceFactory::Get()->SetServiceAccountEmail(
        account_email);
    account_initializer_helper_ = std::make_unique<
        MachineLevelDeviceAccountInitializerHelper>(
        std::move(client),
        base::BindOnce(
            &ChromeBrowserCloudManagementControllerDesktop::AccountInitCallback,
            base::Unretained(this), account_email),
        gaia_url_loader_factory_
            ? gaia_url_loader_factory_
            : g_browser_process->system_network_context_manager()
                  ->GetSharedURLLoaderFactory());
  } else if (!policy_invalidator_) {
    // There's already a refresh token available but no |policy_invalidator_|
    // which means this is browser startup and the refresh token was retrieved
    // from local storage. It's OK to start invalidations now.
    StartInvalidations();
  }
}

void ChromeBrowserCloudManagementControllerDesktop::ShutDown() {
  if (policy_invalidator_)
    policy_invalidator_->Shutdown();
  if (commands_invalidator_)
    commands_invalidator_->Shutdown();
}

MachineLevelUserCloudPolicyManager*
ChromeBrowserCloudManagementControllerDesktop::
    GetMachineLevelUserCloudPolicyManager() {
  return g_browser_process->browser_policy_connector()
      ->machine_level_user_cloud_policy_manager();
}

DeviceManagementService*
ChromeBrowserCloudManagementControllerDesktop::GetDeviceManagementService() {
  return g_browser_process->browser_policy_connector()
      ->device_management_service();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeBrowserCloudManagementControllerDesktop::GetSharedURLLoaderFactory() {
  return g_browser_process->system_network_context_manager()
      ->GetSharedURLLoaderFactory();
}

std::unique_ptr<enterprise_reporting::ReportScheduler>
ChromeBrowserCloudManagementControllerDesktop::CreateReportScheduler(
    CloudPolicyClient* client) {
  auto generator = std::make_unique<enterprise_reporting::ReportGenerator>(
      &reporting_delegate_factory_);
  return std::make_unique<enterprise_reporting::ReportScheduler>(
      client, std::move(generator), &reporting_delegate_factory_);
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeBrowserCloudManagementControllerDesktop::GetBestEffortTaskRunner() {
  // ChromeBrowserCloudManagementControllerDesktop is bound to BrowserThread::UI
  // and so must its best-effort task runner.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT});
}

void ChromeBrowserCloudManagementControllerDesktop::SetGaiaURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  gaia_url_loader_factory_ = url_loader_factory;
}

void ChromeBrowserCloudManagementControllerDesktop::StartInvalidations() {
  DCHECK(
      base::FeatureList::IsEnabled(policy::features::kCBCMPolicyInvalidations));

  if (invalidation_service_) {
    NOTREACHED() << "Trying to start an invalidation service when there's "
                    "already one. Please see crbug.com/1186159.";
    return;
  }

  identity_provider_ = std::make_unique<DeviceIdentityProvider>(
      DeviceOAuth2TokenServiceFactory::Get());
  device_instance_id_driver_ = std::make_unique<instance_id::InstanceIDDriver>(
      g_browser_process->gcm_driver());

  invalidation_service_ =
      std::make_unique<invalidation::FCMInvalidationService>(
          identity_provider_.get(),
          base::BindRepeating(&invalidation::FCMNetworkHandler::Create,
                              g_browser_process->gcm_driver(),
                              device_instance_id_driver_.get()),
          base::BindRepeating(
              &invalidation::PerUserTopicSubscriptionManager::Create,
              identity_provider_.get(), g_browser_process->local_state(),
              base::RetainedRef(
                  g_browser_process->shared_url_loader_factory())),
          device_instance_id_driver_.get(), g_browser_process->local_state(),
          policy::kPolicyFCMInvalidationSenderID);
  invalidation_service_->Init();

  policy_invalidator_ = std::make_unique<CloudPolicyInvalidator>(
      PolicyInvalidationScope::kCBCM,
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager()
          ->core(),
      base::ThreadTaskRunnerHandle::Get(), base::DefaultClock::GetInstance(),
      0 /* highest_handled_invalidation_version */);
  policy_invalidator_->Initialize(invalidation_service_.get());

  if (base::FeatureList::IsEnabled(policy::features::kCBCMRemoteCommands)) {
    g_browser_process->browser_policy_connector()
        ->machine_level_user_cloud_policy_manager()
        ->core()
        ->StartRemoteCommandsService(
            std::make_unique<enterprise_commands::CBCMRemoteCommandsFactory>(),
            PolicyInvalidationScope::kCBCM);

    commands_invalidator_ = std::make_unique<RemoteCommandsInvalidatorImpl>(
        g_browser_process->browser_policy_connector()
            ->machine_level_user_cloud_policy_manager()
            ->core(),
        base::DefaultClock::GetInstance(), PolicyInvalidationScope::kCBCM);
    commands_invalidator_->Initialize(invalidation_service_.get());
  }
}

void ChromeBrowserCloudManagementControllerDesktop::AccountInitCallback(
    const std::string& account_email,
    bool success) {
  account_initializer_helper_.reset();
  if (success)
    StartInvalidations();
}

}  // namespace policy
