// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_metrics_app_state_agent.h"

#import "base/test/task_environment.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/metrics/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class FakeProfileSessionDurationsService
    : public IOSProfileSessionDurationsService {
 public:
  FakeProfileSessionDurationsService()
      : IOSProfileSessionDurationsService(nullptr, nullptr) {}
  ~FakeProfileSessionDurationsService() override = default;

  static std::unique_ptr<KeyedService> Create(
      web::BrowserState* browser_state) {
    return std::make_unique<FakeProfileSessionDurationsService>();
  }

  void OnSessionStarted(base::TimeTicks session_start) override {
    ++session_started_count_;
  }
  void OnSessionEnded(base::TimeDelta session_length) override {
    ++session_ended_count_;
  }

  // IOSProfileSessionDurationsService:
  int session_started_count() const { return session_started_count_; }
  int session_ended_count() const { return session_ended_count_; }

 private:
  int session_started_count_ = 0;
  int session_ended_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeProfileSessionDurationsService);
};
}  // namespace

// A fake that allows overriding connectedScenes.
@interface FakeAppState : AppState
@property(nonatomic, strong) NSArray<SceneState*>* connectedScenes;
@property(nonatomic, assign) BOOL isInSafeMode;
@end

@implementation FakeAppState
@end

class AppMetricsAppStateAgentTest : public PlatformTest {
 protected:
  AppMetricsAppStateAgentTest() {
    agent_ = [[AppMetricsAppStateAgent alloc] init];

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        IOSProfileSessionDurationsServiceFactory::GetInstance(),
        base::BindRepeating(&FakeProfileSessionDurationsService::Create));
    browser_state_ = test_cbs_builder.Build();

    app_state_ = [[FakeAppState alloc] initWithBrowserLauncher:nil
                                            startupInformation:nil
                                           applicationDelegate:nil];
  }

  void SetUp() override {
    PlatformTest::SetUp();
    app_state_.mainBrowserState = browser_state_.get();
    [agent_ setAppState:app_state_];
  }

  FakeProfileSessionDurationsService* getProfileSessionDurationsService() {
    return static_cast<FakeProfileSessionDurationsService*>(
        IOSProfileSessionDurationsServiceFactory::GetForBrowserState(
            browser_state_.get()));
  }

  AppMetricsAppStateAgent* agent_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeAppState* app_state_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AppMetricsAppStateAgentTest, CountSessionDuration) {
  SceneState* scene = [[SceneState alloc] initWithAppState:app_state_];
  app_state_.connectedScenes = @[ scene ];
  [agent_ appState:app_state_ sceneConnected:scene];

  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Going to background at app start doesn't log anything.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Going foreground starts the session.
  scene.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Going to background stops the session.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_ended_count());
}

TEST_F(AppMetricsAppStateAgentTest, CountSessionDurationMultiwindow) {
  SceneState* sceneA = [[SceneState alloc] initWithAppState:app_state_];
  SceneState* sceneB = [[SceneState alloc] initWithAppState:app_state_];
  app_state_.connectedScenes = @[ sceneA, sceneB ];
  [agent_ appState:app_state_ sceneConnected:sceneA];
  [agent_ appState:app_state_ sceneConnected:sceneB];

  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // One scene is enough to start a session.
  sceneA.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Two scenes at the same time, still the session goes on.
  sceneB.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Only scene B in foreground, session still going.
  sceneA.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // No sessions in foreground, session is over.
  sceneB.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_ended_count());
}

TEST_F(AppMetricsAppStateAgentTest, CountSessionDurationSafeMode) {
  SceneState* scene = [[SceneState alloc] initWithAppState:app_state_];
  app_state_.connectedScenes = @[ scene ];
  app_state_.isInSafeMode = YES;
  [agent_ appState:app_state_ sceneConnected:scene];

  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Going to background at app start doesn't log anything.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Going foreground doesn't start the session while in safe mode.
  scene.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Session starts when safe mode completes.
  app_state_.isInSafeMode = NO;
  [agent_ appStateDidExitSafeMode:app_state_];
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Going to background stops the session.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_ended_count());
}
