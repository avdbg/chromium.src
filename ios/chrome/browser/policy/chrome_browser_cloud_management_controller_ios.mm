// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/chrome_browser_cloud_management_controller_ios.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/enterprise/browser/reporting/report_generator.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/features.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/policy/browser_dm_token_storage_ios.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#include "ios/chrome/browser/policy/policy_features.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace policy {

ChromeBrowserCloudManagementControllerIOS::
    ChromeBrowserCloudManagementControllerIOS() = default;
ChromeBrowserCloudManagementControllerIOS::
    ~ChromeBrowserCloudManagementControllerIOS() = default;

void ChromeBrowserCloudManagementControllerIOS::SetDMTokenStorageDelegate() {
  BrowserDMTokenStorage::SetDelegate(
      std::make_unique<BrowserDMTokenStorageIOS>());
}

int ChromeBrowserCloudManagementControllerIOS::GetUserDataDirKey() {
  return ios::DIR_USER_DATA;
}

base::FilePath
ChromeBrowserCloudManagementControllerIOS::GetExternalPolicyPath() {
  // External policies are not supported on iOS.
  return base::FilePath();
}

ChromeBrowserCloudManagementController::Delegate::NetworkConnectionTrackerGetter
ChromeBrowserCloudManagementControllerIOS::
    CreateNetworkConnectionTrackerGetter() {
  return base::BindRepeating(&ApplicationContext::GetNetworkConnectionTracker,
                             base::Unretained(GetApplicationContext()));
}

void ChromeBrowserCloudManagementControllerIOS::InitializeOAuthTokenFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_state) {
  // Policy invalidations aren't currently supported on iOS.
}

void ChromeBrowserCloudManagementControllerIOS::StartWatchingRegistration(
    ChromeBrowserCloudManagementController* controller) {
  // Enrollment isn't blocking or mandatory on iOS.
}

bool ChromeBrowserCloudManagementControllerIOS::
    WaitUntilPolicyEnrollmentFinished() {
  // Enrollment currently isn't blocking or mandatory on iOS, so this method
  // isn't used. Always report success.
  return true;
}

bool ChromeBrowserCloudManagementControllerIOS::
    IsEnterpriseStartupDialogShowing() {
  // There is no enterprise startup dialog on iOS.
  return false;
}

void ChromeBrowserCloudManagementControllerIOS::OnServiceAccountSet(
    CloudPolicyClient* client,
    const std::string& account_email) {
  // Policy invalidations aren't currently supported on iOS.
}

void ChromeBrowserCloudManagementControllerIOS::ShutDown() {
  // No additional shutdown to perform on iOS.
}

MachineLevelUserCloudPolicyManager* ChromeBrowserCloudManagementControllerIOS::
    GetMachineLevelUserCloudPolicyManager() {
  return GetApplicationContext()
      ->GetBrowserPolicyConnector()
      ->machine_level_user_cloud_policy_manager();
}

DeviceManagementService*
ChromeBrowserCloudManagementControllerIOS::GetDeviceManagementService() {
  return GetApplicationContext()
      ->GetBrowserPolicyConnector()
      ->device_management_service();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeBrowserCloudManagementControllerIOS::GetSharedURLLoaderFactory() {
  return GetApplicationContext()->GetSharedURLLoaderFactory();
}

std::unique_ptr<enterprise_reporting::ReportScheduler>
ChromeBrowserCloudManagementControllerIOS::CreateReportScheduler(
    CloudPolicyClient* client) {
  auto generator = std::make_unique<enterprise_reporting::ReportGenerator>(
      &reporting_delegate_factory_);
  return std::make_unique<enterprise_reporting::ReportScheduler>(
      client, std::move(generator), &reporting_delegate_factory_);
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeBrowserCloudManagementControllerIOS::GetBestEffortTaskRunner() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return base::CreateSingleThreadTaskRunner(
      {web::WebThread::UI, base::TaskPriority::BEST_EFFORT});
}

void ChromeBrowserCloudManagementControllerIOS::SetGaiaURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // Policy invalidations aren't currently supported on iOS.
}

}  // namespace policy