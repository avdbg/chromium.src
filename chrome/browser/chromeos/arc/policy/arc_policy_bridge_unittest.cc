// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/enterprise/arc_data_snapshotd_delegate.h"
#include "chrome/browser/chromeos/arc/enterprise/cert_store/cert_store_service.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/test/test_arc_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/enterprise/arc_data_snapshotd_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/arc/test/fake_policy_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/remote_commands/remote_commands_queue.h"
#include "components/policy/policy_constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::ReturnRef;

namespace arc {

namespace {

constexpr char kFakeONC[] =
    "{\"NetworkConfigurations\":["
    "{\"GUID\":\"{485d6076-dd44-6b6d-69787465725f5040}\","
    "\"Type\":\"WiFi\","
    "\"Name\":\"My WiFi Network\","
    "\"WiFi\":{"
    "\"HexSSID\":\"737369642D6E6F6E65\","  // "ssid-none"
    "\"Security\":\"None\"}"
    "}"
    "],"
    "\"GlobalNetworkConfiguration\":{"
    "\"AllowOnlyPolicyNetworksToAutoconnect\":true,"
    "},"
    "\"Certificates\":["
    "{ \"GUID\":\"{f998f760-272b-6939-4c2beffe428697ac}\","
    "\"PKCS12\":\"abc\","
    "\"Type\":\"Client\"},"
    "{\"Type\":\"Authority\","
    "\"TrustBits\":[\"Web\"],"
    "\"X509\":\"TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ"
    "1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpc"
    "yBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCB"
    "pbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZ"
    "GdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4"
    "=\","
    "\"GUID\":\"{00f79111-51e0-e6e0-76b3b55450d80a1b}\"}"
    "]}";

constexpr char kPolicyCompliantResponse[] = "{ \"policyCompliant\": true }";

constexpr char kFakeCertName[] = "cert_name";
constexpr char kRequiredKeyPairsEmpty[] = "\"requiredKeyPairs\":[]";
constexpr char kRequiredKeyPairsFormat[] =
    "\"requiredKeyPairs\":[{\"alias\":\"%s\"}]";

constexpr char kChoosePrivateKeyRulesFormat[] =
    "\"choosePrivateKeyRules\":["
    "{\"packageNames\":[\"%s\"],"
    "\"privateKeyAlias\":\"%s\"}]";

constexpr char kSupervisedUserPlayStoreModePolicySetting[] =
    "\"playStoreMode\":\"SUPERVISED\"";

constexpr char kPlayStoreManagedRestriction[] =
    "\"managedConfiguration\":{"
    "\"allowed_accounts\":\"%s\"},";

constexpr char kApplicationsPolicy[] =
    "\"applications\":["
    "{"
    "\"disabled\":false,"
    "\"installType\":\"OPTIONAL\","
    "%s"
    "\"packageName\":\"com.android.vending\""
    "},"
    "{"
    "\"disabled\":false,"
    "\"installType\":\"OPTIONAL\","
    "\"packageName\":\"com.a.b\""
    "}]";

constexpr char kTestUserEmail[] = "user@gmail.com";

constexpr char kChromeAppId[] = "chromeappid";
constexpr char kAndroidAppId[] = "android.app.id";

std::string GetSupervisedUserPlayStoreApplicationPolicy(
    bool include_playstore_restriction,
    const std::string& user_email) {
  std::string restriction_used =
      include_playstore_restriction
          ? base::StringPrintf(kPlayStoreManagedRestriction, user_email.c_str())
          : "";
  return base::StringPrintf(kApplicationsPolicy, restriction_used.c_str());
}

void AddKeyPermissionForAppId(base::Value* key_permissions,
                              const std::string& app_id,
                              bool allowed) {
  base::Value cert_key_permission(base::Value::Type::DICTIONARY);
  cert_key_permission.SetKey("allowCorporateKeyUsage", base::Value(allowed));
  key_permissions->SetKey(app_id, std::move(cert_key_permission));
}

MATCHER_P(ValueEquals, expected, "value matches") {
  return *expected == *arg;
}

class MockArcPolicyBridgeObserver : public ArcPolicyBridge::Observer {
 public:
  MockArcPolicyBridgeObserver() = default;
  ~MockArcPolicyBridgeObserver() override = default;

  MOCK_METHOD1(OnPolicySent, void(const std::string&));
  MOCK_METHOD1(OnComplianceReportReceived, void(const base::Value*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockArcPolicyBridgeObserver);
};

// Helper class to define callbacks that verify that they were run.
// Wraps a bool initially set to |false| and verifies that it's been set to
// |true| before destruction.
class CheckedBoolean {
 public:
  CheckedBoolean() {}
  ~CheckedBoolean() { EXPECT_TRUE(value_); }

  void set_value(bool value) { value_ = value; }

 private:
  bool value_ = false;

  DISALLOW_COPY_AND_ASSIGN(CheckedBoolean);
};

void ExpectString(std::unique_ptr<CheckedBoolean> was_run,
                  const std::string& expected,
                  const std::string& received) {
  EXPECT_EQ(expected, received);
  was_run->set_value(true);
}

void ExpectStringWithClosure(base::OnceClosure quit_closure,
                             std::unique_ptr<CheckedBoolean> was_run,
                             const std::string& expected,
                             const std::string& received) {
  EXPECT_EQ(expected, received);
  was_run->set_value(true);
  std::move(quit_closure).Run();
}

arc::ArcPolicyBridge::GetPoliciesCallback PolicyStringCallback(
    const std::string& expected) {
  auto was_run = std::make_unique<CheckedBoolean>();
  return base::BindOnce(&ExpectString, std::move(was_run), expected);
}

arc::ArcPolicyBridge::ReportComplianceCallback PolicyComplianceCallback(
    base::OnceClosure quit_closure,
    const std::string& expected) {
  auto was_run = std::make_unique<CheckedBoolean>();
  return base::BindOnce(&ExpectStringWithClosure, std::move(quit_closure),
                        std::move(was_run), expected);
}

}  // namespace

class ArcPolicyBridgeTestBase {
 public:
  ArcPolicyBridgeTestBase() = default;

  void DoSetUp(bool is_affiliated) {
    bridge_service_ = std::make_unique<ArcBridgeService>();
    EXPECT_CALL(policy_service_,
                GetPolicies(policy::PolicyNamespace(
                    policy::POLICY_DOMAIN_CHROME, std::string())))
        .WillRepeatedly(ReturnRef(policy_map_));
    EXPECT_CALL(policy_service_, AddObserver(policy::POLICY_DOMAIN_CHROME, _))
        .Times(1);
    EXPECT_CALL(policy_service_,
                RemoveObserver(policy::POLICY_DOMAIN_CHROME, _))
        .Times(1);

    // Setting up user profile for ReportCompliance() tests.
    chromeos::FakeChromeUserManager* const fake_user_manager =
        new chromeos::FakeChromeUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager));
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kTestUserEmail, "1111111111"));
    fake_user_manager->AddUserWithAffiliation(account_id, is_affiliated);
    fake_user_manager->LoginUser(account_id);
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(kTestUserEmail);
    ASSERT_TRUE(profile_);

    cert_store_service_ = GetCertStoreService();

    // TODO(hidehiko): Use Singleton instance tied to BrowserContext.
    policy_bridge_ = std::make_unique<ArcPolicyBridge>(
        profile_, bridge_service_.get(), &policy_service_);
    policy_bridge_->OverrideIsManagedForTesting(true);
    policy_bridge_->AddObserver(&observer_);
    instance_guid_ = policy_bridge_->GetInstanceGuidForTesting();

    policy_instance_ = std::make_unique<FakePolicyInstance>();
    bridge_service_->policy()->SetInstance(policy_instance_.get());
    WaitForInstanceReady(bridge_service_->policy());
  }

  void DoTearDown() {
    bridge_service_->policy()->CloseInstance(policy_instance_.get());
    policy_instance_.reset();
    policy_bridge_->RemoveObserver(&observer_);
    testing_profile_manager_.reset();
  }

 protected:
  void GetPoliciesAndVerifyResult(const std::string& expected_policy_json) {
    Mock::VerifyAndClearExpectations(&observer_);
    EXPECT_CALL(observer_, OnPolicySent(expected_policy_json));
    policy_bridge()->GetPolicies(PolicyStringCallback(expected_policy_json));
    EXPECT_EQ(expected_policy_json,
              policy_bridge()->get_arc_policy_for_reporting());
    Mock::VerifyAndClearExpectations(&observer_);
  }

  void ReportComplianceAndVerifyObserverCallback(
      const std::string& compliance_report) {
    Mock::VerifyAndClearExpectations(&observer_);
    std::unique_ptr<base::Value> compliance_report_value =
        base::JSONReader::ReadDeprecated(compliance_report);
    if (compliance_report_value && compliance_report_value->is_dict()) {
      EXPECT_CALL(observer_, OnComplianceReportReceived(
                                 ValueEquals(compliance_report_value.get())));
    } else {
      EXPECT_CALL(observer_, OnComplianceReportReceived(_)).Times(0);
    }
    policy_bridge()->ReportCompliance(
        compliance_report, PolicyComplianceCallback(run_loop().QuitClosure(),
                                                    kPolicyCompliantResponse));
    run_loop().Run();
    Mock::VerifyAndClearExpectations(&observer_);

    if (compliance_report_value) {
      std::unique_ptr<base::Value> saved_compliance_report_value =
          base::JSONReader::ReadDeprecated(
              policy_bridge()->get_arc_policy_compliance_report());
      ASSERT_TRUE(saved_compliance_report_value);
      EXPECT_TRUE(
          compliance_report_value->Equals(saved_compliance_report_value.get()));
    } else {
      EXPECT_TRUE(policy_bridge()->get_arc_policy_compliance_report().empty());
    }
  }

  // Specifies a testing factory for CertStoreService and returns instance.
  // Returns nullptr by default.
  // Override if the test wants to use a real cert store service.
  virtual CertStoreService* GetCertStoreService() {
    return static_cast<CertStoreService*>(
        CertStoreService::GetFactory()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(
                [](content::BrowserContext* profile)
                    -> std::unique_ptr<KeyedService> { return nullptr; })));
  }

  ArcPolicyBridge* policy_bridge() { return policy_bridge_.get(); }
  const std::string& instance_guid() { return instance_guid_; }
  FakePolicyInstance* policy_instance() { return policy_instance_.get(); }
  policy::PolicyMap& policy_map() { return policy_map_; }
  base::RunLoop& run_loop() { return run_loop_; }
  TestingProfile* profile() { return profile_; }
  ArcBridgeService* bridge_service() { return bridge_service_.get(); }
  CertStoreService* cert_store_service() { return cert_store_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  base::RunLoop run_loop_;
  TestingProfile* profile_;
  std::unique_ptr<ArcBridgeService> bridge_service_;
  CertStoreService* cert_store_service_;  // Not owned.

  std::unique_ptr<ArcPolicyBridge> policy_bridge_;
  std::string instance_guid_;
  MockArcPolicyBridgeObserver observer_;
  // Always keep policy_instance_ below bridge_service_, so that
  // policy_instance_ is destructed first. It needs to remove itself as
  // observer.
  std::unique_ptr<FakePolicyInstance> policy_instance_;
  policy::PolicyMap policy_map_;
  policy::MockPolicyService policy_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcPolicyBridgeTestBase);
};

class ArcPolicyBridgeTest : public ArcPolicyBridgeTestBase,
                            public testing::Test {
 public:
  void SetUp() override { DoSetUp(true /* affiliated */); }

  void TearDown() override { DoTearDown(); }
};

class ArcPolicyBridgeAffiliatedTest : public ArcPolicyBridgeTestBase,
                                      public testing::TestWithParam<bool> {
 public:
  ArcPolicyBridgeAffiliatedTest() : is_affiliated_(GetParam()) {}
  void SetUp() override { DoSetUp(is_affiliated_); }

  void TearDown() override { DoTearDown(); }

 protected:
  void GetPoliciesAndVerifyResultWithAffiliation(
      const std::string& expected_policy_json_affiliated,
      const std::string& expected_policy_json_not_affiliated) {
    if (is_affiliated_)
      GetPoliciesAndVerifyResult(expected_policy_json_affiliated);
    else
      GetPoliciesAndVerifyResult(expected_policy_json_not_affiliated);
  }
  const bool is_affiliated_;
};

// Tests required key pair policy.
class ArcPolicyBridgeCertStoreTest : public ArcPolicyBridgeTest {
 protected:
  CertStoreService* GetCertStoreService() override {
    return static_cast<CertStoreService*>(
        CertStoreService::GetFactory()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating([](content::BrowserContext* profile)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<CertStoreService>(nullptr);
            })));
  }
};

TEST_F(ArcPolicyBridgeTest, UnmanagedTest) {
  policy_bridge()->OverrideIsManagedForTesting(false);
  GetPoliciesAndVerifyResult("");
}

TEST_F(ArcPolicyBridgeTest, EmptyPolicyTest) {
  // No policy is set, result should be empty except for the instance GUID.
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"}");
}

TEST_F(ArcPolicyBridgeTest, DISABLED_ArcPolicyTest) {
  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value("{\"applications\":"
                  "[{\"packageName\":\"com.google.android.apps.youtube.kids\","
                  "\"installType\":\"REQUIRED\","
                  "\"lockTaskAllowed\":false,"
                  "\"permissionGrants\":[]"
                  "}],"
                  "\"defaultPermissionPolicy\":\"GRANT\""
                  "}"),
      nullptr);
  GetPoliciesAndVerifyResult(
      "\"apkCacheEnabled\":true,"
      "{\"applications\":"
      "[{\"installType\":\"REQUIRED\","
      "\"lockTaskAllowed\":false,"
      "\"packageName\":\"com.google.android.apps.youtube.kids\","
      "\"permissionGrants\":[]"
      "}],"
      "\"defaultPermissionPolicy\":\"GRANT\","
      "\"guid\":\"" +
      instance_guid() + "\"}");
}

TEST_F(ArcPolicyBridgeTest, HompageLocationTest) {
  // This policy will not be passed on, result should be empty except for the
  // instance GUID.
  policy_map().Set(policy::key::kHomepageLocation,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value("http://chromium.org"), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"}");
}

TEST_F(ArcPolicyBridgeTest, DisableScreenshotsTest) {
  policy_map().Set(policy::key::kDisableScreenshots,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() +
                             "\",\"screenCaptureDisabled\":true}");
}

TEST_F(ArcPolicyBridgeTest, DisablePrintingTest) {
  policy_map().Set(policy::key::kPrintingEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\",\"printingDisabled\":true}");
}

TEST_F(ArcPolicyBridgeTest, VideoCaptureAllowedTest) {
  policy_map().Set(policy::key::kVideoCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"cameraDisabled\":true,\"guid\":\"" +
      instance_guid() + "\"}");
}

TEST_F(ArcPolicyBridgeTest, AudioCaptureAllowedTest) {
  policy_map().Set(policy::key::kAudioCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() +
                             "\",\"unmuteMicrophoneDisabled\":true}");
}

TEST_F(ArcPolicyBridgeTest, DefaultGeolocationSettingTest) {
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() +
                             "\",\"shareLocationDisabled\":false}");
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(2), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() +
                             "\",\"shareLocationDisabled\":true}");
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(3), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() +
                             "\",\"shareLocationDisabled\":false}");
}

TEST_F(ArcPolicyBridgeTest, ExternalStorageDisabledTest) {
  policy_map().Set(policy::key::kExternalStorageDisabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() +
                             "\",\"mountPhysicalMediaDisabled\":true}");
}

TEST_F(ArcPolicyBridgeTest, WallpaperImageSetTest) {
  base::DictionaryValue dict;
  dict.SetString("url", "https://example.com/wallpaper.jpg");
  dict.SetString("hash", "somehash");
  policy_map().Set(policy::key::kWallpaperImage, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   dict.Clone(), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() +
                             "\",\"setWallpaperDisabled\":true}");
}

TEST_F(ArcPolicyBridgeTest, WallpaperImageSet_NotCompletePolicyTest) {
  base::DictionaryValue dict;
  dict.SetString("url", "https://example.com/wallpaper.jpg");
  // "hash" attribute is missing, so the policy shouldn't be set
  policy_map().Set(policy::key::kWallpaperImage, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   dict.Clone(), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"}");
}

TEST_F(ArcPolicyBridgeTest, CaCertificateTest) {
  // Enable CA certificates sync.
  policy_map().Set(policy::key::kArcCertificatesSyncMode,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(ArcCertsSyncMode::COPY_CA_CERTS), nullptr);
  policy_map().Set(policy::key::kOpenNetworkConfiguration,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(kFakeONC), nullptr);
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,"
      "\"caCerts\":"
      "[{\"X509\":\"TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24"
      "sIGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGl"
      "jaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGV"
      "saWdodCBpbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Y"
      "ga25vd2xlZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCB"
      "wbGVhc3VyZS4=\"}"
      "],"
      "\"credentialsConfigDisabled\":true,"
      "\"guid\":\"" +
      instance_guid() + "\"}");

  // Disable CA certificates sync.
  policy_map().Set(policy::key::kArcCertificatesSyncMode,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(ArcCertsSyncMode::SYNC_DISABLED), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"}");
}

TEST_F(ArcPolicyBridgeTest, DeveloperToolsPolicyAllowedTest) {
  profile()->GetTestingPrefService()->SetManagedPref(
      ::prefs::kDevToolsAvailability,
      std::make_unique<base::Value>(static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kAllowed)));
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"debuggingFeaturesDisabled\":false,\"guid\":"
      "\"" +
      instance_guid() + "\"}");
}

TEST_F(ArcPolicyBridgeTest,
       DeveloperToolsPolicyDisallowedForForceInstalledExtensionsTest) {
  profile()->GetTestingPrefService()->SetManagedPref(
      ::prefs::kDevToolsAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(policy::DeveloperToolsPolicyHandler::Availability::
                               kDisallowedForForceInstalledExtensions)));
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"debuggingFeaturesDisabled\":false,\"guid\":"
      "\"" +
      instance_guid() + "\"}");
}

TEST_F(ArcPolicyBridgeTest, DeveloperToolsPolicyDisallowedTest) {
  profile()->GetTestingPrefService()->SetManagedPref(
      ::prefs::kDevToolsAvailability,
      std::make_unique<base::Value>(static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed)));
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"debuggingFeaturesDisabled\":true,\"guid\":"
      "\"" +
      instance_guid() + "\"}");
}

TEST_F(ArcPolicyBridgeTest, MultiplePoliciesTest) {
  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value("{\"applications\":"
                  "[{\"packageName\":\"com.google.android.apps.youtube.kids\","
                  "\"installType\":\"REQUIRED\","
                  "\"lockTaskAllowed\":false,"
                  "\"permissionGrants\":[]"
                  "}],"
                  "\"defaultPermissionPolicy\":\"GRANT\"}"),
      nullptr);
  policy_map().Set(policy::key::kHomepageLocation,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value("http://chromium.org"), nullptr);
  policy_map().Set(policy::key::kVideoCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  GetPoliciesAndVerifyResult(
      "{\"apkCacheEnabled\":true,\"applications\":"
      "[{\"installType\":\"REQUIRED\","
      "\"lockTaskAllowed\":false,"
      "\"packageName\":\"com.google.android.apps.youtube.kids\","
      "\"permissionGrants\":[]"
      "}],"
      "\"cameraDisabled\":true,"
      "\"defaultPermissionPolicy\":\"GRANT\","
      "\"guid\":\"" +
      instance_guid() + "\"}");
}

TEST_F(ArcPolicyBridgeTest, EmptyReportComplianceTest) {
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  ReportComplianceAndVerifyObserverCallback("{}");
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

TEST_F(ArcPolicyBridgeTest, ParsableReportComplianceTest) {
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  ReportComplianceAndVerifyObserverCallback("{\"nonComplianceDetails\" : []}");
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

TEST_F(ArcPolicyBridgeTest, NonParsableReportComplianceTest) {
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  ReportComplianceAndVerifyObserverCallback("\"nonComplianceDetails\" : [}");
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

TEST_F(ArcPolicyBridgeTest, ReportComplianceTest_WithNonCompliantDetails) {
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  ReportComplianceAndVerifyObserverCallback(
      "{\"nonComplianceDetails\" : "
      "[{\"fieldPath\":\"\",\"nonComplianceReason\":0,\"packageName\":\"\","
      "\"settingName\":\"someSetting\",\"cachedSize\":-1},"
      "{\"cachedSize\":-1,\"fieldPath\":\"\",\"nonComplianceReason\":6,"
      "\"packageName\":\"\",\"settingName\":\"guid\"}]}");
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

// This and the following test send the policies through a mojo connection
// between a PolicyInstance and the PolicyBridge.
TEST_F(ArcPolicyBridgeTest, PolicyInstanceUnmanagedTest) {
  policy_bridge()->OverrideIsManagedForTesting(false);
  GetPoliciesAndVerifyResult("");
}

TEST_F(ArcPolicyBridgeTest, PolicyInstanceManagedTest) {
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() + "\"}");
}

TEST_F(ArcPolicyBridgeTest, VpnConfigAllowedTest) {
  policy_map().Set(policy::key::kVpnConfigAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  GetPoliciesAndVerifyResult("{\"apkCacheEnabled\":true,\"guid\":\"" +
                             instance_guid() +
                             "\",\"vpnConfigDisabled\":true}");
}

TEST_F(ArcPolicyBridgeTest, ManualChildUserPoliciesSet) {
  // Mark profile as supervised user.
  profile()->SetSupervisedUserId(::supervised_users::kChildAccountSUID);
  EXPECT_TRUE(profile()->IsChild());

  policy_map().Set(policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value("{}"), /* external_data_fetcher */ nullptr);

  // Applications policy is not present so only playStoreMode policy is set.
  GetPoliciesAndVerifyResult(
      base::StrCat({"{\"apkCacheEnabled\":true,\"guid\":\"", instance_guid(),
                    "\",", kSupervisedUserPlayStoreModePolicySetting, "}"}));

  // ARC policy with applications policy:
  // The  managedConfiguration for Play Store should be set in this case.
  // PlayStoreMode policy should also be set in this case.
  const std::string arc_policy =
      base::StrCat({"{",
                    GetSupervisedUserPlayStoreApplicationPolicy(
                        /* include_playstore_restriction */ false, ""),
                    "}"});

  policy_map().Set(policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(arc_policy),
                   /* external_data_fetcher */ nullptr);
  const std::string expected_policy_result = base::StrCat(
      {"{\"apkCacheEnabled\":true,",
       GetSupervisedUserPlayStoreApplicationPolicy(
           /* include_playstore_restriction */ true, kTestUserEmail),
       ",\"guid\":\"", instance_guid(), "\",",
       kSupervisedUserPlayStoreModePolicySetting, "}"});
  GetPoliciesAndVerifyResult(expected_policy_result);
}

// Test that required and force-installed apps get disabled during ARC data
// snapshot update.
TEST_F(ArcPolicyBridgeTest, DisableAppsInSnapshot) {
  constexpr char kDisabledApplicationsPolicyFormat[] =
      "\"applications\":["
      "{"
      "\"disabled\":%s,"
      "\"installType\":\"REQUIRED\","
      "\"packageName\":\"com.android.vending\""
      "},"
      "{"
      "\"disabled\":%s,"
      "\"installType\":\"FORCE_INSTALLED\","
      "\"packageName\":\"com.force.installed\""
      "},"
      "{"
      "\"disabled\":%s,"
      "\"installType\":\"OPTIONAL\","
      "\"packageName\":\"com.optional\""
      "}],"
      "\"defaultPermissionPolicy\":\"GRANT\"";

  constexpr char kFalse[] = "false";
  constexpr char kTrue[] = "true";

  chromeos::DBusThreadManager::Initialize();

  auto upstart_client = std::make_unique<chromeos::FakeUpstartClient>();
  arc::prefs::RegisterLocalStatePrefs(
      profile()->GetTestingPrefService()->registry());
  auto arc_session_manager =
      CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
          base::BindRepeating(FakeArcSession::Create)));

  auto manager = std::make_unique<arc::data_snapshotd::ArcDataSnapshotdManager>(
      profile()->GetTestingPrefService(),
      std::make_unique<arc::data_snapshotd::ArcDataSnapshotdDelegate>(),
      base::DoNothing());
  EXPECT_TRUE(arc::data_snapshotd::ArcDataSnapshotdManager::Get());
  manager->set_state_for_testing(
      arc::data_snapshotd::ArcDataSnapshotdManager::State::kMgsLaunched);
  EXPECT_TRUE(manager->IsSnapshotInProgress());
  policy_map().Set(policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(base::StrCat(
                       {"{",
                        base::StringPrintf(kDisabledApplicationsPolicyFormat,
                                           kFalse, kFalse, kFalse),
                        "}"})),
                   nullptr);
  GetPoliciesAndVerifyResult(base::StrCat(
      {"{\"apkCacheEnabled\":true,",
       base::StringPrintf(kDisabledApplicationsPolicyFormat, kTrue, kTrue,
                          kFalse),
       ",\"guid\":\"", instance_guid(), "\",\"resetAndroidIdEnabled\":true}"}));

  manager.reset();
  upstart_client.reset();
  arc_session_manager.reset();
  chromeos::DBusThreadManager::Shutdown();
}

TEST_P(ArcPolicyBridgeAffiliatedTest, ApkCacheEnabledTest) {
  const std::string expected_apk_cache_enabled_result(
      "{\"apkCacheEnabled\":true,\"guid\":\"" + instance_guid() + "\"}");
  const std::string expected_apk_cache_disabled_result(
      "{\"apkCacheEnabled\":false,\"guid\":\"" + instance_guid() + "\"}");

  const std::string arc_apk_cache_enabled_policy("{\"apkCacheEnabled\":true}");
  policy_map().Set(policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(arc_apk_cache_enabled_policy), nullptr);
  GetPoliciesAndVerifyResultWithAffiliation(
      /* expected_policy_json_affiliated */ expected_apk_cache_enabled_result,
      /* expected_policy_json_not_affiliated */
      expected_apk_cache_disabled_result);

  const std::string arc_apk_cache_disabled_policy(
      "{\"apkCacheEnabled\":false}");
  policy_map().Set(policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(arc_apk_cache_disabled_policy), nullptr);
  GetPoliciesAndVerifyResultWithAffiliation(
      /* expected_policy_json_affiliated */ expected_apk_cache_enabled_result,
      /* expected_policy_json_not_affiliated */
      expected_apk_cache_disabled_result);

  const std::string arc_apk_cache_no_policy("{}");
  policy_map().Set(policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(arc_apk_cache_no_policy), nullptr);
  GetPoliciesAndVerifyResultWithAffiliation(
      /* expected_policy_json_affiliated */ expected_apk_cache_enabled_result,
      /* expected_policy_json_not_affiliated */
      expected_apk_cache_disabled_result);
}

// Boolean parameter means if user is affiliated on the device. Affiliated
// users belong to the domain that owns the device.
// Affiliated user should always have enabled APK cache; not affiliated user
// should always have it disabled.
INSTANTIATE_TEST_SUITE_P(ArcPolicyBridgeAffiliatedTestInstance,
                         ArcPolicyBridgeAffiliatedTest,
                         testing::Bool());

// Tests that if cert store service is non-null, the required key pair policy is
// set to the required certificate list.
TEST_F(ArcPolicyBridgeCertStoreTest, RequiredKeyPairsBasicTest) {
  // One certificate is required to be installed.
  cert_store_service()->set_required_cert_names_for_testing({kFakeCertName});
  GetPoliciesAndVerifyResult(base::StrCat(
      {"{\"apkCacheEnabled\":true,\"guid\":\"", instance_guid(), "\",",
       base::StringPrintf(kRequiredKeyPairsFormat, kFakeCertName), "}"}));

  // An empty list is required to be installed.
  cert_store_service()->set_required_cert_names_for_testing({});
  GetPoliciesAndVerifyResult(
      base::StrCat({"{\"apkCacheEnabled\":true,\"guid\":\"", instance_guid(),
                    "\",", kRequiredKeyPairsEmpty, "}"}));
}

// Tests that if cert store service is non-null, corporate usage key exists and
// available to ARC app, ChoosePrivateKeyRules policy is propagated correctly.
TEST_F(ArcPolicyBridgeCertStoreTest, KeyPermissionsBasicTest) {
  EXPECT_TRUE(cert_store_service());

  // One certificate is required to be installed.
  cert_store_service()->set_required_cert_names_for_testing({kFakeCertName});

  base::Value key_permissions(base::Value::Type::DICTIONARY);
  AddKeyPermissionForAppId(&key_permissions, kAndroidAppId, true /* allowed */);
  AddKeyPermissionForAppId(&key_permissions, kChromeAppId, true /* allowed */);

  policy_map().Set(policy::key::kKeyPermissions, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   std::move(key_permissions),
                   /* external_data_fetcher */ nullptr);
  GetPoliciesAndVerifyResult(base::StrCat(
      {"{\"apkCacheEnabled\":true,",
       base::StringPrintf(kChoosePrivateKeyRulesFormat, kAndroidAppId,
                          kFakeCertName),
       ",\"guid\":\"", instance_guid(),
       "\",\"privateKeySelectionEnabled\":true,",
       base::StringPrintf(kRequiredKeyPairsFormat, kFakeCertName), "}"}));
}

// Tests that if cert store service is non-null, corporate usage key exists and
// not to any ARC apps, ChoosePrivateKeyRules policy is not set.
TEST_F(ArcPolicyBridgeCertStoreTest, KeyPermissionsEmptyTest) {
  base::Value key_permissions(base::Value::Type::DICTIONARY);
  AddKeyPermissionForAppId(&key_permissions, kAndroidAppId,
                           false /* allowed */);
  AddKeyPermissionForAppId(&key_permissions, kChromeAppId, true /* allowed */);

  // One certificate is required to be installed.
  cert_store_service()->set_required_cert_names_for_testing({kFakeCertName});

  policy_map().Set(policy::key::kKeyPermissions, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   std::move(key_permissions),
                   /* external_data_fetcher */ nullptr);
  GetPoliciesAndVerifyResult(base::StrCat(
      {"{\"apkCacheEnabled\":true,\"guid\":\"", instance_guid(), "\",",
       base::StringPrintf(kRequiredKeyPairsFormat, kFakeCertName), "}"}));
}

// Tests that if cert store service is non-null, corporate usage keys do not
// exist, but in theory are available to ARC apps, ChoosePrivateKeyRules policy
// is not set.
TEST_F(ArcPolicyBridgeCertStoreTest, KeyPermissionsNoCertsTest) {
  base::Value key_permissions(base::Value::Type::DICTIONARY);
  AddKeyPermissionForAppId(&key_permissions, kAndroidAppId, true /* allowed */);
  AddKeyPermissionForAppId(&key_permissions, kChromeAppId, true /* allowed */);

  cert_store_service()->set_required_cert_names_for_testing({});

  policy_map().Set(policy::key::kKeyPermissions, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   std::move(key_permissions),
                   /* external_data_fetcher */ nullptr);
  GetPoliciesAndVerifyResult(
      base::StrCat({"{\"apkCacheEnabled\":true,\"guid\":\"", instance_guid(),
                    "\",", kRequiredKeyPairsEmpty, "}"}));
}

}  // namespace arc
