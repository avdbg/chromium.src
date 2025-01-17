// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/profile_sync_service.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_manager_impl.h"
#include "components/sync/driver/fake_data_type_controller.h"
#include "components/sync/driver/fake_sync_api_component_factory.h"
#include "components/sync/driver/profile_sync_service_bundle.h"
#include "components/sync/driver/sync_client_mock.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/invalidations/switches.h"
#include "components/sync/test/engine/fake_sync_engine.h"
#include "components/version_info/version_info_values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::Not;
using testing::Return;

namespace syncer {

namespace {

constexpr char kTestUser[] = "test_user@gmail.com";
constexpr char kTestCacheGuid[] = "test_cache_guid";

class TestSyncServiceObserver : public SyncServiceObserver {
 public:
  TestSyncServiceObserver()
      : setup_in_progress_(false), auth_error_(GoogleServiceAuthError()) {}

  void OnStateChanged(SyncService* sync) override {
    setup_in_progress_ = sync->IsSetupInProgress();
    auth_error_ = sync->GetAuthError();
  }

  bool setup_in_progress() const { return setup_in_progress_; }
  GoogleServiceAuthError auth_error() const { return auth_error_; }

 private:
  bool setup_in_progress_;
  GoogleServiceAuthError auth_error_;
};

// A test harness that uses a real ProfileSyncService and in most cases a
// FakeSyncEngine.
//
// This is useful if we want to test the ProfileSyncService and don't care about
// testing the SyncEngine.
class ProfileSyncServiceTest : public ::testing::Test {
 protected:
  ProfileSyncServiceTest() {}
  ~ProfileSyncServiceTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSyncDeferredStartupTimeoutSeconds, "0");
  }

  void TearDown() override {
    // Kill the service before the profile.
    ShutdownAndDeleteService();
  }

  void SignIn() { identity_test_env()->MakePrimaryAccountAvailable(kTestUser); }

  void CreateService(ProfileSyncService::StartBehavior behavior,
                     policy::PolicyService* policy_service = nullptr,
                     std::vector<std::pair<ModelType, bool>>
                         registered_types_and_transport_mode_support = {
                             {BOOKMARKS, false},
                             {DEVICE_INFO, true}}) {
    DCHECK(!service_);

    // Default includes a regular controller and a transport-mode controller.
    DataTypeController::TypeVector controllers;
    for (const auto& type_and_transport_mode_support :
         registered_types_and_transport_mode_support) {
      ModelType type = type_and_transport_mode_support.first;
      bool transport_mode_support = type_and_transport_mode_support.second;
      auto controller = std::make_unique<FakeDataTypeController>(
          type, transport_mode_support);
      // Hold a raw pointer to directly interact with the controller.
      controller_map_[type] = controller.get();
      controllers.push_back(std::move(controller));
    }

    std::unique_ptr<SyncClientMock> sync_client =
        profile_sync_service_bundle_.CreateSyncClientMock();
    sync_client_ = sync_client.get();
    ON_CALL(*sync_client, CreateDataTypeControllers)
        .WillByDefault(Return(ByMove(std::move(controllers))));

    auto init_params = profile_sync_service_bundle_.CreateBasicInitParams(
        behavior, std::move(sync_client));
    init_params.policy_service = policy_service;

    service_ = std::make_unique<ProfileSyncService>(std::move(init_params));
  }

  void CreateServiceWithLocalSyncBackend() {
    DCHECK(!service_);

    // Include a regular controller and a transport-mode controller.
    DataTypeController::TypeVector controllers;
    controllers.push_back(std::make_unique<FakeDataTypeController>(BOOKMARKS));
    controllers.push_back(std::make_unique<FakeDataTypeController>(
        DEVICE_INFO, /*enable_transport_only_modle=*/true));

    std::unique_ptr<SyncClientMock> sync_client =
        profile_sync_service_bundle_.CreateSyncClientMock();
    sync_client_ = sync_client.get();
    ON_CALL(*sync_client, CreateDataTypeControllers)
        .WillByDefault(Return(ByMove(std::move(controllers))));

    ProfileSyncService::InitParams init_params =
        profile_sync_service_bundle_.CreateBasicInitParams(
            ProfileSyncService::AUTO_START, std::move(sync_client));

    prefs()->SetBoolean(prefs::kEnableLocalSyncBackend, true);
    init_params.identity_manager = nullptr;

    service_ = std::make_unique<ProfileSyncService>(std::move(init_params));
  }

  void ShutdownAndDeleteService() {
    if (service_)
      service_->Shutdown();
    service_.reset();
  }

  void PopulatePrefsForNthSync() {
    // Set first sync time before initialize to simulate a complete sync setup.
    SyncTransportDataPrefs transport_data_prefs(prefs());
    SyncPrefs sync_prefs(prefs());
    transport_data_prefs.SetCacheGuid(kTestCacheGuid);
    transport_data_prefs.SetBirthday(FakeSyncEngine::kTestBirthday);
    transport_data_prefs.SetLastSyncedTime(base::Time::Now());
    component_factory()->set_first_time_sync_configure_done(true);
    sync_prefs.SetSyncRequested(true);
    sync_prefs.SetSelectedTypes(
        /*keep_everything_synced=*/true,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/UserSelectableTypeSet::All());
    sync_prefs.SetFirstSetupComplete();
  }

  void InitializeForNthSync() {
    PopulatePrefsForNthSync();
    service_->Initialize();
  }

  void InitializeForFirstSync() { service_->Initialize(); }

  void TriggerPassphraseRequired() {
    service_->GetEncryptionObserverForTest()->OnPassphraseRequired(
        KeyDerivationParams::CreateForPbkdf2(), sync_pb::EncryptedData());
  }

  void TriggerDataTypeStartRequest() {
    service_->OnDataTypeRequestsSyncStartup(BOOKMARKS);
  }

  signin::IdentityManager* identity_manager() {
    return profile_sync_service_bundle_.identity_manager();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return profile_sync_service_bundle_.identity_test_env();
  }

  ProfileSyncService* service() { return service_.get(); }

  SyncClientMock* sync_client() { return sync_client_; }

  TestingPrefServiceSimple* prefs() {
    return profile_sync_service_bundle_.pref_service();
  }

  FakeSyncApiComponentFactory* component_factory() {
    return profile_sync_service_bundle_.component_factory();
  }

  DataTypeManagerImpl* data_type_manager() {
    return component_factory()->last_created_data_type_manager();
  }

  FakeSyncEngine* engine() {
    return component_factory()->last_created_engine();
  }

  MockSyncInvalidationsService* sync_invalidations_service() {
    return profile_sync_service_bundle_.sync_invalidations_service();
  }

  FakeDataTypeController* get_controller(ModelType type) {
    return controller_map_[type];
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ProfileSyncServiceBundle profile_sync_service_bundle_;
  std::unique_ptr<ProfileSyncService> service_;
  SyncClientMock* sync_client_;  // Owned by |service_|.
  // The controllers are owned by |service_|.
  std::map<ModelType, FakeDataTypeController*> controller_map_;
};

class ProfileSyncServiceTestWithSyncInvalidationsServiceCreated
    : public ProfileSyncServiceTest {
 public:
  ProfileSyncServiceTestWithSyncInvalidationsServiceCreated() {
    override_features_.InitAndEnableFeature(
        switches::kSyncSendInterestedDataTypes);
  }

  ~ProfileSyncServiceTestWithSyncInvalidationsServiceCreated() override =
      default;

 private:
  base::test::ScopedFeatureList override_features_;
};

// Verify that the server URLs are sane.
TEST_F(ProfileSyncServiceTest, InitialState) {
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  const std::string& url = service()->GetSyncServiceUrlForDebugging().spec();
  EXPECT_TRUE(url == internal::kSyncServerUrl ||
              url == internal::kSyncDevServerUrl);
}

TEST_F(ProfileSyncServiceTest, SuccessfulInitialization) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

TEST_F(ProfileSyncServiceTest, SuccessfulLocalBackendInitialization) {
  CreateServiceWithLocalSyncBackend();
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// Verify that an initialization where first setup is not complete does not
// start up Sync-the-feature.
TEST_F(ProfileSyncServiceTest, NeedsConfirmation) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);

  SyncPrefs sync_prefs(prefs());
  sync_prefs.SetSyncRequested(true);
  sync_prefs.SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet::All());

  // Mimic a sync cycle (transport-only) having completed earlier.
  const base::Time kLastSyncedTime = base::Time::Now();
  SyncTransportDataPrefs transport_data_prefs(prefs());
  transport_data_prefs.SetLastSyncedTime(kLastSyncedTime);
  transport_data_prefs.SetCacheGuid(kTestCacheGuid);
  transport_data_prefs.SetBirthday(FakeSyncEngine::kTestBirthday);

  service()->Initialize();

  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());

  // Sync should immediately start up in transport mode.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  // The local sync data shouldn't be cleared.
  EXPECT_EQ(kTestCacheGuid, transport_data_prefs.GetCacheGuid());
  EXPECT_EQ(kLastSyncedTime, transport_data_prefs.GetLastSyncedTime());
}

TEST_F(ProfileSyncServiceTest, ModelTypesForTransportMode) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();

  // Disable sync-the-feature.
  service()->GetUserSettings()->SetSyncRequested(false);
  ASSERT_FALSE(service()->IsSyncFeatureActive());
  ASSERT_FALSE(service()->IsSyncFeatureEnabled());

  // Sync-the-transport is still active.
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // ModelTypes for sync-the-feature are not configured.
  EXPECT_FALSE(service()->GetActiveDataTypes().Has(BOOKMARKS));

  // ModelTypes for sync-the-transport are configured.
  EXPECT_TRUE(service()->GetActiveDataTypes().Has(DEVICE_INFO));
}

// Verify that the SetSetupInProgress function call updates state
// and notifies observers.
TEST_F(ProfileSyncServiceTest, SetupInProgress) {
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForFirstSync();

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  auto sync_blocker = service()->GetSetupInProgressHandle();
  EXPECT_TRUE(observer.setup_in_progress());
  sync_blocker.reset();
  EXPECT_FALSE(observer.setup_in_progress());

  service()->RemoveObserver(&observer);
}

// Verify that we wait for policies to load before starting the sync engine.
TEST_F(ProfileSyncServiceTest, WaitForPoliciesToStart) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(switches::kSyncRequiresPoliciesLoaded);
  std::unique_ptr<policy::PolicyServiceImpl> policy_service =
      policy::PolicyServiceImpl::CreateWithThrottledInitialization(
          policy::PolicyServiceImpl::Providers());

  SignIn();
  CreateService(ProfileSyncService::MANUAL_START, policy_service.get());
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::START_DEFERRED,
            service()->GetTransportState());

  EXPECT_EQ(
      syncer::UploadState::INITIALIZING,
      syncer::GetUploadToGoogleState(service(), syncer::ModelType::BOOKMARKS));

  policy_service->UnthrottleInitialization();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// Verify that disable by enterprise policy works.
TEST_F(ProfileSyncServiceTest, DisabledByPolicyBeforeInit) {
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  // Sync was disabled due to the policy, setting SyncRequested to false and
  // causing DISABLE_REASON_USER_CHOICE.
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
                SyncService::DISABLE_REASON_USER_CHOICE),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
}

TEST_F(ProfileSyncServiceTest, DisabledByPolicyBeforeInitThenPolicyRemoved) {
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  // Sync was disabled due to the policy, setting SyncRequested to false and
  // causing DISABLE_REASON_USER_CHOICE.
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
                SyncService::DISABLE_REASON_USER_CHOICE),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());

  // Remove the policy. Sync-the-feature is still disabled, sync-the-transport
  // can run.
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Once we mark first setup complete again (it was cleared by the policy) and
  // set SyncRequested to true, sync starts up.
  service()->GetUserSettings()->SetSyncRequested(true);
  service()->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_TRUE(service()->GetDisableReasons().Empty());
}

// Verify that disable by enterprise policy works even after the backend has
// been initialized.
TEST_F(ProfileSyncServiceTest, DisabledByPolicyAfterInit) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();

  ASSERT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));

  // Sync was disabled due to the policy, setting SyncRequested to false and
  // causing DISABLE_REASON_USER_CHOICE.
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
                SyncService::DISABLE_REASON_USER_CHOICE),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
}

TEST_F(ProfileSyncServiceTest,
       ShouldDisableSyncFeatureWhenSyncDisallowedByPlatform) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();

  ASSERT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  service()->SetSyncAllowedByPlatform(false);
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  // Sync-the-transport should be still active.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// Exercises the ProfileSyncService's code paths related to getting shut down
// before the backend initialize call returns.
TEST_F(ProfileSyncServiceTest, AbortedByShutdown) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  component_factory()->AllowFakeEngineInitCompletion(false);

  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  ShutdownAndDeleteService();
}

// Test SetSyncRequested(false) before we've initialized the backend.
TEST_F(ProfileSyncServiceTest, EarlyRequestStop) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  // Set up a fake sync engine that will not immediately finish initialization.
  component_factory()->AllowFakeEngineInitCompletion(false);
  InitializeForNthSync();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  // Request stop. This should immediately restart the service in standalone
  // transport mode.
  component_factory()->AllowFakeEngineInitCompletion(true);
  service()->GetUserSettings()->SetSyncRequested(false);
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  // Request start. Now Sync-the-feature should start again.
  service()->GetUserSettings()->SetSyncRequested(true);
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_TRUE(service()->IsSyncFeatureActive());
  EXPECT_TRUE(service()->IsSyncFeatureEnabled());
}

// Test SetSyncRequested(false) after we've initialized the backend.
TEST_F(ProfileSyncServiceTest, DisableAndEnableSyncTemporarily) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();

  SyncPrefs sync_prefs(prefs());

  ASSERT_TRUE(sync_prefs.IsSyncRequested());
  ASSERT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_TRUE(service()->IsSyncFeatureActive());
  ASSERT_TRUE(service()->IsSyncFeatureEnabled());

  service()->GetUserSettings()->SetSyncRequested(false);
  EXPECT_FALSE(sync_prefs.IsSyncRequested());
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  service()->GetUserSettings()->SetSyncRequested(true);
  EXPECT_TRUE(sync_prefs.IsSyncRequested());
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_TRUE(service()->IsSyncFeatureActive());
  EXPECT_TRUE(service()->IsSyncFeatureEnabled());
}

// Certain ProfileSyncService tests don't apply to Chrome OS, for example
// things that deal with concepts like "signing out".
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfileSyncServiceTest, SignOutDisablesSyncTransportAndSyncFeature) {
  // Sign-in and enable sync.
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Sign-out.
  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  // Wait for ProfileSyncService to be notified.
  base::RunLoop().RunUntilIdle();
  // SyncRequested was set to false, causing DISABLE_REASON_USER_CHOICE.
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_NOT_SIGNED_IN,
                                    SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
}

TEST_F(ProfileSyncServiceTest,
       SignOutClearsSyncTransportDataAndSyncTheFeaturePrefs) {
  // Sign-in and enable sync.
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  ASSERT_TRUE(service()->GetUserSettings()->IsFirstSetupComplete());
  ASSERT_TRUE(service()->GetUserSettings()->IsSyncRequested());
  ASSERT_EQ(0, component_factory()->clear_transport_data_call_count());

  // Sign-out.
  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  // Wait for ProfileSyncService to be notified.
  base::RunLoop().RunUntilIdle();
  // These are specific to sync-the-feature and should be cleared.
  EXPECT_FALSE(service()->GetUserSettings()->IsFirstSetupComplete());
  EXPECT_FALSE(service()->GetUserSettings()->IsSyncRequested());
  EXPECT_EQ(1, component_factory()->clear_transport_data_call_count());
}

TEST_F(ProfileSyncServiceTest, SyncRequestedSetToFalseIfStartsSignedOut) {
  // Set up bad state.
  SyncPrefs sync_prefs(prefs());
  sync_prefs.SetSyncRequested(true);

  CreateService(ProfileSyncService::MANUAL_START);
  service()->Initialize();

  // There's no signed-in user, so SyncRequested should have been set to false.
  EXPECT_FALSE(service()->GetUserSettings()->IsSyncRequested());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ProfileSyncServiceTest, GetSyncTokenStatus) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();

  // Initial status.
  SyncTokenStatus token_status = service()->GetSyncTokenStatusForDebugging();
  ASSERT_EQ(CONNECTION_NOT_ATTEMPTED, token_status.connection_status);
  ASSERT_TRUE(token_status.connection_status_update_time.is_null());
  ASSERT_FALSE(token_status.token_request_time.is_null());
  ASSERT_TRUE(token_status.token_response_time.is_null());
  ASSERT_FALSE(token_status.has_token);

  // The token request will take the form of a posted task.  Run it.
  base::RunLoop().RunUntilIdle();

  // Now we should have an access token.
  token_status = service()->GetSyncTokenStatusForDebugging();
  EXPECT_TRUE(token_status.connection_status_update_time.is_null());
  EXPECT_FALSE(token_status.token_request_time.is_null());
  EXPECT_FALSE(token_status.token_response_time.is_null());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            token_status.last_get_token_error);
  EXPECT_TRUE(token_status.next_token_request_time.is_null());
  EXPECT_TRUE(token_status.has_token);

  // Simulate an auth error.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  // This should get reflected in the status, and we should have dropped the
  // invalid access token.
  token_status = service()->GetSyncTokenStatusForDebugging();
  EXPECT_EQ(CONNECTION_AUTH_ERROR, token_status.connection_status);
  EXPECT_FALSE(token_status.connection_status_update_time.is_null());
  EXPECT_FALSE(token_status.token_request_time.is_null());
  EXPECT_FALSE(token_status.token_response_time.is_null());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            token_status.last_get_token_error);
  EXPECT_FALSE(token_status.next_token_request_time.is_null());
  EXPECT_FALSE(token_status.has_token);

  // Simulate successful connection.
  service()->OnConnectionStatusChange(CONNECTION_OK);
  token_status = service()->GetSyncTokenStatusForDebugging();
  EXPECT_EQ(CONNECTION_OK, token_status.connection_status);
}

TEST_F(ProfileSyncServiceTest, RevokeAccessTokenFromTokenService) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());

  AccountInfo secondary_account_info =
      identity_test_env()->MakeAccountAvailable("test_user2@gmail.com");
  identity_test_env()->RemoveRefreshTokenForAccount(
      secondary_account_info.account_id);
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  identity_test_env()->RemoveRefreshTokenForPrimaryAccount();
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}

// Checks that CREDENTIALS_REJECTED_BY_CLIENT resets the access token and stops
// Sync. Regression test for https://crbug.com/824791.
TEST_F(ProfileSyncServiceTest, CredentialsRejectedByClient_StopSync) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::AuthErrorNone(), service()->GetAuthError());
  ASSERT_EQ(GoogleServiceAuthError::AuthErrorNone(), observer.auth_error());

  // Simulate the credentials getting locally rejected by the client by setting
  // the refresh token to a special invalid value.
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  const GoogleServiceAuthError rejected_by_client =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT);
  ASSERT_EQ(rejected_by_client,
            identity_test_env()
                ->identity_manager()
                ->GetErrorStateOfRefreshTokenForAccount(primary_account_id));
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());

  // The observer should have been notified of the auth error state.
  EXPECT_EQ(rejected_by_client, observer.auth_error());
  // The Sync engine should have been shut down.
  EXPECT_FALSE(service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::TransportState::PAUSED,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

// CrOS does not support signout.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfileSyncServiceTest, SignOutRevokeAccessToken) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();

  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);

  account_mutator->ClearPrimaryAccount(
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}
#endif

TEST_F(ProfileSyncServiceTest,
       StopAndClearWillClearDataAndSwitchToTransportMode) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_EQ(0, component_factory()->clear_transport_data_call_count());

  service()->StopAndClear();

  // Even though Sync-the-feature is disabled, there's still an (unconsented)
  // signed-in account, so Sync-the-transport should still be running.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
  EXPECT_EQ(1, component_factory()->clear_transport_data_call_count());
}

// Verify that sync transport data is cleared when the service is initializing
// and account is signed out.
TEST_F(ProfileSyncServiceTest, ClearTransportDataOnInitializeWhenSignedOut) {
  // Don't sign-in before creating the service.
  CreateService(ProfileSyncService::MANUAL_START);

  ASSERT_EQ(0, component_factory()->clear_transport_data_call_count());

  // Initialize when signed out to trigger clearing of prefs.
  InitializeForNthSync();

  EXPECT_EQ(1, component_factory()->clear_transport_data_call_count());
}

TEST_F(ProfileSyncServiceTest, StopSyncAndClearTwiceDoesNotCrash) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Disable sync.
  service()->StopAndClear();
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  // Calling StopAndClear while already stopped should not crash. This may
  // (under some circumstances) happen when the user enables sync again but hits
  // the cancel button at the end of the process.
  ASSERT_FALSE(service()->GetUserSettings()->IsSyncRequested());
  service()->StopAndClear();
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
}

// Verify that credential errors get returned from GetAuthError().
TEST_F(ProfileSyncServiceTest, CredentialErrorReturned) {
  // This test needs to manually send access tokens (or errors), so disable
  // automatic replies to access token requests.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  // Wait for ProfileSyncService to send an access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      primary_account_id, "access token", base::Time::Max());
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());

  // Emulate Chrome receiving a new, invalid LST. This happens when the user
  // signs out of the content area.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Again, wait for ProfileSyncService to be notified.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Check that the invalid token is returned from sync.
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            observer.auth_error().state());
  // The overall state should remain ACTIVE.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

// Verify that credential errors get cleared when a new token is fetched
// successfully.
TEST_F(ProfileSyncServiceTest, CredentialErrorClearsOnNewToken) {
  // This test needs to manually send access tokens (or errors), so disable
  // automatic replies to access token requests.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  // Wait for ProfileSyncService to send an access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      primary_account_id, "access token", base::Time::Max());
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());

  // Emulate Chrome receiving a new, invalid LST. This happens when the user
  // signs out of the content area.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Wait for ProfileSyncService to be notified of the changed credentials and
  // send a new access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Check that the invalid token is returned from sync.
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());
  // The overall state should remain ACTIVE.
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Now emulate Chrome receiving a new, valid LST.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Again, wait for ProfileSyncService to be notified.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "this one works", base::Time::Now() + base::TimeDelta::FromDays(10));

  // Check that sync auth error state cleared.
  EXPECT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());
  EXPECT_EQ(GoogleServiceAuthError::NONE, observer.auth_error().state());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

// Verify that the disable sync flag disables sync.
TEST_F(ProfileSyncServiceTest, DisableSyncFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableSync);
  EXPECT_FALSE(switches::IsSyncAllowedByFlag());
}

// Verify that no disable sync flag enables sync.
TEST_F(ProfileSyncServiceTest, NoDisableSyncFlag) {
  EXPECT_TRUE(switches::IsSyncAllowedByFlag());
}

// Test that when ProfileSyncService receives actionable error
// RESET_LOCAL_SYNC_DATA it restarts sync.
TEST_F(ProfileSyncServiceTest, ResetSyncData) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  // Backend should get initialized two times: once during initialization and
  // once when handling actionable error.
  InitializeForNthSync();

  SyncProtocolError client_cmd;
  client_cmd.action = RESET_LOCAL_SYNC_DATA;
  service()->OnActionableError(client_cmd);
}

// Test that when ProfileSyncService receives actionable error
// DISABLE_SYNC_ON_CLIENT it disables sync and signs out.
TEST_F(ProfileSyncServiceTest, DisableSyncOnClient) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_call_count());

  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  service()->OnActionableError(client_cmd);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS does not support signout.
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  // Since ChromeOS doesn't support signout and so the account is still there
  // and available, Sync will restart in standalone transport mode.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
#else
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_NOT_SIGNED_IN,
                                    SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
  EXPECT_TRUE(service()->GetLastSyncedTimeForDebugging().is_null());
#endif

  EXPECT_EQ(1, get_controller(BOOKMARKS)->model()->clear_metadata_call_count());

  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
}

// Verify a that local sync mode isn't impacted by sync being disabled.
TEST_F(ProfileSyncServiceTest, LocalBackendUnimpactedByPolicy) {
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));
  CreateServiceWithLocalSyncBackend();
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));

  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Note: If standalone transport is enabled, then setting kSyncManaged to
  // false will immediately start up the engine. Otherwise, the RequestStart
  // call below will trigger it.
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));

  service()->GetUserSettings()->SetSyncRequested(true);
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// Test ConfigureDataTypeManagerReason on First and Nth start.
TEST_F(ProfileSyncServiceTest, ConfigureDataTypeManagerReason) {
  SignIn();

  // First sync.
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForFirstSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(CONFIGURE_REASON_NEW_CLIENT,
            data_type_manager()->last_configure_reason_for_test());

  // Reconfiguration.
  // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and immediately
  // releasing it again (via the temporary unique_ptr going away).
  service()->GetSetupInProgressHandle();
  EXPECT_EQ(CONFIGURE_REASON_RECONFIGURATION,
            data_type_manager()->last_configure_reason_for_test());
  ShutdownAndDeleteService();

  // Nth sync.
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE,
            data_type_manager()->last_configure_reason_for_test());

  // Reconfiguration.
  // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and immediately
  // releasing it again (via the temporary unique_ptr going away).
  service()->GetSetupInProgressHandle();
  EXPECT_EQ(CONFIGURE_REASON_RECONFIGURATION,
            data_type_manager()->last_configure_reason_for_test());
  ShutdownAndDeleteService();
}

// Regression test for crbug.com/1043642, can be removed once
// ProfileSyncService usages after shutdown are addressed.
TEST_F(ProfileSyncServiceTest, ShouldProvideDisableReasonsAfterShutdown) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForFirstSync();
  service()->Shutdown();
  EXPECT_FALSE(service()->GetDisableReasons().Empty());
}

#if defined(OS_ANDROID)
TEST_F(ProfileSyncServiceTest, DecoupleFromMasterSyncIfInitializedSignedOut) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      switches::kDecoupleSyncFromAndroidMasterSync);

  SyncPrefs sync_prefs(prefs());
  CreateService(ProfileSyncService::MANUAL_START);
  ASSERT_FALSE(sync_prefs.GetDecoupledFromAndroidMasterSync());

  service()->Initialize();
  EXPECT_TRUE(sync_prefs.GetDecoupledFromAndroidMasterSync());
}

TEST_F(ProfileSyncServiceTest, DecoupleFromMasterSyncIfSignsOut) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      switches::kDecoupleSyncFromAndroidMasterSync);

  SyncPrefs sync_prefs(prefs());
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  InitializeForNthSync();
  ASSERT_FALSE(sync_prefs.GetDecoupledFromAndroidMasterSync());

  // Sign-out.
  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  // Wait for ProfileSyncService to be notified.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(sync_prefs.GetDecoupledFromAndroidMasterSync());
}
#endif  // defined(OS_ANDROID)

TEST_F(ProfileSyncServiceTestWithSyncInvalidationsServiceCreated,
       ShouldSendDataTypesToSyncInvalidationsService) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  EXPECT_CALL(*sync_invalidations_service(), SetInterestedDataTypes);
  InitializeForFirstSync();
}

MATCHER(ContainsSessions, "") {
  return arg.Has(SESSIONS);
}

TEST_F(ProfileSyncServiceTestWithSyncInvalidationsServiceCreated,
       ShouldEnableAndDisableInvalidationsForSessions) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START, nullptr,
                {{SESSIONS, false}, {TYPED_URLS, false}});
  InitializeForNthSync();

  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(ContainsSessions(), _));
  service()->SetInvalidationsForSessionsEnabled(true);
  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(Not(ContainsSessions()), _));
  service()->SetInvalidationsForSessionsEnabled(false);
}

TEST_F(ProfileSyncServiceTestWithSyncInvalidationsServiceCreated,
       ShouldActivateSyncInvalidationsServiceWhenSyncIsInitialized) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  EXPECT_CALL(*sync_invalidations_service(), SetActive(true)).Times(0);
  EXPECT_CALL(*sync_invalidations_service(), SetActive(true));
  InitializeForFirstSync();
}

TEST_F(ProfileSyncServiceTestWithSyncInvalidationsServiceCreated,
       ShouldActivateSyncInvalidationsServiceOnSignIn) {
  CreateService(ProfileSyncService::MANUAL_START);
  EXPECT_CALL(*sync_invalidations_service(), SetActive(false));
  InitializeForFirstSync();
  EXPECT_CALL(*sync_invalidations_service(), SetActive(true));
  SignIn();
}

// CrOS does not support signout.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfileSyncServiceTestWithSyncInvalidationsServiceCreated,
       ShouldDectivateSyncInvalidationsServiceOnSignOut) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);
  EXPECT_CALL(*sync_invalidations_service(), SetActive(true));
  InitializeForFirstSync();

  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();
  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);

  // Sign out.
  EXPECT_CALL(*sync_invalidations_service(), SetActive(false));
  account_mutator->ClearPrimaryAccount(
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
}
#endif

}  // namespace
}  // namespace syncer
