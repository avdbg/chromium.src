// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_handler_registration_utils_win.h"

#include "base/files/file_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/windows_version.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class WebAppHandlerRegistrationUtilsWinTest : public testing::Test {
 protected:
  WebAppHandlerRegistrationUtilsWinTest() = default;

  void SetUp() override {
    // Set up fake windows registry
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    profile_ =
        testing_profile_manager_->CreateTestingProfile(chrome::kInitialProfile);
  }
  void TearDown() override {
    profile_ = nullptr;
    testing_profile_manager_->DeleteAllTestingProfiles();
  }

  Profile* profile() const { return profile_; }
  ProfileManager* profile_manager() const {
    return testing_profile_manager_->profile_manager();
  }
  TestingProfileManager* testing_profile_manager() const {
    return testing_profile_manager_.get();
  }
  const AppId& app_id() const { return app_id_; }
  const std::wstring& app_name() const { return app_name_; }

  // Adds a launcher file and OS registry entries for the given app parameters.
  void RegisterApp(const AppId& app_id,
                   const std::wstring& app_name,
                   const std::wstring& app_name_extension,
                   const base::FilePath& profile_path) {
    base::Optional<base::FilePath> launcher_path = CreateAppLauncherFile(
        app_name, app_name_extension,
        GetOsIntegrationResourcesDirectoryForApp(profile_path, app_id, GURL()));
    ASSERT_TRUE(launcher_path.has_value());

    base::CommandLine launcher_command =
        GetAppLauncherCommand(app_id, launcher_path.value(), profile_path);
    std::wstring prog_id = GetProgIdForApp(profile_path, app_id);
    std::wstring user_visible_app_name(app_name);
    user_visible_app_name.append(app_name_extension);

    ASSERT_TRUE(ShellUtil::AddApplicationClass(
        prog_id, launcher_command, user_visible_app_name, std::wstring(),
        base::FilePath()));
  }

  // Tests that an app with |app_id| is registered with the expected name /
  // extension.
  void TestRegisteredApp(const AppId& app_id,
                         const std::wstring& expected_app_name,
                         const std::wstring& expected_app_name_extension,
                         const base::FilePath& profile_path) {
    // Ensure that the OS registry contains the expected app name.
    std::wstring expected_user_visible_app_name(app_name());
    expected_user_visible_app_name.append(expected_app_name_extension);
    std::wstring app_progid = GetProgIdForApp(profile_path, app_id);
    ShellUtil::FileAssociationsAndAppName registered_app =
        ShellUtil::GetFileAssociationsAndAppName(app_progid);
    EXPECT_EQ(expected_user_visible_app_name, registered_app.app_name);

    // Ensure that the launcher file contains the expected app name.
    // On Windows 7 the extension is omitted.
    base::FilePath expected_launcher_filename =
        base::win::GetVersion() > base::win::Version::WIN7
            ? base::FilePath(expected_user_visible_app_name.append(L".exe"))
            : base::FilePath(expected_user_visible_app_name);
    base::FilePath registered_launcher_path =
        ShellUtil::GetApplicationPathForProgId(app_progid);
    ASSERT_TRUE(base::PathExists(registered_launcher_path));
    EXPECT_EQ(expected_launcher_filename, registered_launcher_path.BaseName());
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
  base::ScopedTempDir temp_version_dir_;
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  TestingProfile* profile_ = nullptr;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  const AppId app_id_ = "app_id";
  const std::wstring app_name_ = L"app_name";
};

TEST_F(WebAppHandlerRegistrationUtilsWinTest,
       GetAppNameExtensionForNextInstall) {
  // If no installations are present in any profile, the next app name extension
  // should be an empty string.
  std::wstring app_name_extension =
      GetAppNameExtensionForNextInstall(app_id(), profile()->GetPath());
  EXPECT_EQ(app_name_extension, std::wstring());

  // After registering an app, the next app name should include a
  // profile-specific extension.
  RegisterApp(app_id(), app_name(), app_name_extension, profile()->GetPath());
  Profile* profile2 =
      testing_profile_manager()->CreateTestingProfile("Profile 2");
  ProfileAttributesStorage& storage =
      profile_manager()->GetProfileAttributesStorage();
  ASSERT_EQ(2u, storage.GetNumberOfProfiles());

  app_name_extension =
      GetAppNameExtensionForNextInstall(app_id(), profile2->GetPath());
  EXPECT_EQ(app_name_extension, L" (Profile 2)");
}

// Test various attributes of ProgIds returned by GetAppIdForApp.
TEST_F(WebAppHandlerRegistrationUtilsWinTest, GetProgIdForApp) {
  // Create a long app_id and verify that the prog id is less
  // than 39 characters, and only contains alphanumeric characters and
  // non leading '.'s See
  // https://docs.microsoft.com/en-us/windows/win32/com/-progid--key.
  AppId app_id1("app_id12345678901234567890123456789012345678901234");
  constexpr unsigned int kMaxProgIdLen = 39;
  std::wstring prog_id1 = GetProgIdForApp(profile()->GetPath(), app_id1);
  EXPECT_LE(prog_id1.length(), kMaxProgIdLen);
  for (auto itr = prog_id1.begin(); itr != prog_id1.end(); itr++)
    EXPECT_TRUE(std::isalnum(*itr) || (*itr == '.' && itr != prog_id1.begin()));
  AppId app_id2("different_appid");
  // Check that different app ids in the same profile have different
  // prog ids.
  EXPECT_NE(prog_id1, GetProgIdForApp(profile()->GetPath(), app_id2));

  // Create a different profile, and verify that the prog id for the same
  // app_id in a different profile is different.
  TestingProfile profile2;
  EXPECT_NE(prog_id1, GetProgIdForApp(profile2.GetPath(), app_id1));
}

TEST_F(WebAppHandlerRegistrationUtilsWinTest,
       CheckAndUpdateExternalInstallationsAfterRegistration) {
  // Register the same app to profile1 and profile2.
  Profile* profile1 = profile();
  RegisterApp(app_id(), app_name(), std::wstring(), profile1->GetPath());

  Profile* profile2 =
      testing_profile_manager()->CreateTestingProfile("Profile 2");

  std::wstring app_name_extension(
      GetAppNameExtensionForNextInstall(app_id(), profile2->GetPath()));
  RegisterApp(app_id(), app_name(), app_name_extension, profile2->GetPath());

  // Update installations external to profile 2 (i.e. profile1).
  CheckAndUpdateExternalInstallations(profile2->GetPath(), app_id());
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Test that the profile1 installation is updated with a profile-specific
  // name.
  TestRegisteredApp(
      app_id(), app_name(),
      GetAppNameExtensionForNextInstall(app_id(), profile1->GetPath()),
      profile1->GetPath());
}

TEST_F(WebAppHandlerRegistrationUtilsWinTest,
       CheckAndUpdateExternalInstallationsAfterUnregistration) {
  // Create a profile-specific installation for an app without any duplicate
  // external installations. This is the state of a profile-specific app that
  // remains after its external duplicate is unregistered.
  RegisterApp(app_id(), app_name(), L" (Default)", profile()->GetPath());

  Profile* profile2 =
      testing_profile_manager()->CreateTestingProfile("Profile 2");
  CheckAndUpdateExternalInstallations(profile2->GetPath(), app_id());
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Ensure that after updating from profile2 (which has no installation),
  // the single app installation is updated with a non profile-specific name.
  TestRegisteredApp(app_id(), app_name(), std::wstring(), profile()->GetPath());
}

TEST_F(WebAppHandlerRegistrationUtilsWinTest,
       CheckAndUpdateExternalInstallationsWithTwoExternalApps) {
  // Register the same profile-specific apps to profile1 and profile2.
  Profile* profile1 = profile();
  RegisterApp(app_id(), app_name(), L" (Default)", profile1->GetPath());
  TestRegisteredApp(app_id(), app_name(), L" (Default)", profile1->GetPath());

  Profile* profile2 =
      testing_profile_manager()->CreateTestingProfile("Profile 2");
  RegisterApp(app_id(), app_name(), L" (Profile 2)", profile2->GetPath());
  TestRegisteredApp(app_id(), app_name(), L" (Profile 2)", profile2->GetPath());

  Profile* profile3 =
      testing_profile_manager()->CreateTestingProfile("Profile 3");

  // Attempting updates from profile3 when there are already 2 app installations
  // in other profiles shouldn't change the original 2 installations since they
  // already have app-specific names.
  CheckAndUpdateExternalInstallations(profile3->GetPath(), app_id());
  base::ThreadPoolInstance::Get()->FlushForTesting();

  TestRegisteredApp(app_id(), app_name(), L" (Default)", profile1->GetPath());
  TestRegisteredApp(app_id(), app_name(), L" (Profile 2)", profile2->GetPath());
}

TEST_F(WebAppHandlerRegistrationUtilsWinTest, CreateAppLauncherFile) {
  std::wstring app_name_extension = L" extension";
  base::Optional<base::FilePath> launcher_path =
      CreateAppLauncherFile(app_name(), app_name_extension,
                            GetOsIntegrationResourcesDirectoryForApp(
                                profile()->GetPath(), app_id(), GURL()));
  EXPECT_TRUE(launcher_path.has_value());
  EXPECT_TRUE(base::PathExists(launcher_path.value()));

  // On Windows 7 the extension is omitted.
  std::wstring expected_user_visible_app_name(app_name());
  expected_user_visible_app_name.append(app_name_extension);
  base::FilePath expected_launcher_filename =
      base::win::GetVersion() > base::win::Version::WIN7
          ? base::FilePath(expected_user_visible_app_name.append(L".exe"))
          : base::FilePath(expected_user_visible_app_name);
  EXPECT_EQ(launcher_path.value().BaseName(), expected_launcher_filename);
}

// Test that invalid file name characters in app_name are replaced with '_'.
TEST_F(WebAppHandlerRegistrationUtilsWinTest, AppNameWithInvalidChars) {
  // '*' is an invalid char in Windows file names, so it should be replaced
  // with '_'.
  std::wstring app_name = L"app*name";
  // On Windows 7 the extension is omitted.
  base::FilePath expected_launcher_name =
      base::win::GetVersion() > base::win::Version::WIN7
          ? base::FilePath(L"app_name.exe")
          : base::FilePath(L"app_name");
  EXPECT_EQ(GetAppSpecificLauncherFilename(app_name), expected_launcher_name);
}

// Test that an app name that is a reserved filename on Windows has '_'
// prepended to it when used as a filename for its launcher.
TEST_F(WebAppHandlerRegistrationUtilsWinTest, AppNameIsReservedFilename) {
  // "con" is a reserved filename on Windows, so it should have '_' prepended.
  std::wstring app_name = L"con";
  // On Windows 7 the extension is omitted.
  base::FilePath expected_launcher_name =
      base::win::GetVersion() > base::win::Version::WIN7
          ? base::FilePath(L"_con.exe")
          : base::FilePath(L"_con");
  EXPECT_EQ(GetAppSpecificLauncherFilename(app_name), expected_launcher_name);
}

// Test that an app name containing '.' characters has them replaced with '_' on
// Windows 7 when used as a filename for its launcher.
TEST_F(WebAppHandlerRegistrationUtilsWinTest, AppNameContainsDot) {
  std::wstring app_name = L"some.app.name";

  // "some.app.name" should become "some_app_name" on Windows 7 and the
  // extension is also omitted.
  base::FilePath expected_launcher_name =
      base::win::GetVersion() > base::win::Version::WIN7
          ? base::FilePath(L"some.app.name.exe")
          : base::FilePath(L"some_app_name");
  EXPECT_EQ(GetAppSpecificLauncherFilename(app_name), expected_launcher_name);
}

}  // namespace web_app
