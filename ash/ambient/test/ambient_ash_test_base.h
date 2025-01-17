// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_
#define ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/test_ambient_client.h"
#include "ash/ambient/ui/ambient_background_image_view.h"
#include "ash/test/ash_test_base.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AmbientAccessTokenController;
class AmbientContainerView;
class AmbientPhotoController;
class FakeAmbientBackendControllerImpl;
class MediaStringView;

// The base class to test the Ambient Mode in Ash.
class AmbientAshTestBase : public AshTestBase {
 public:
  AmbientAshTestBase();
  ~AmbientAshTestBase() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

  // Enables/disables ambient mode for the currently active user session.
  void SetAmbientModeEnabled(bool enabled);

  // Creates ambient screen in its own widget.
  void ShowAmbientScreen();

  // Hides ambient screen. Can only be called after |ShowAmbientScreen| has been
  // called.
  void HideAmbientScreen();

  // Closes ambient screen. Can only be called after |ShowAmbientScreen| has
  // been called.
  void CloseAmbientScreen();

  // Simulates user locks/unlocks screen which will result in ambient widget
  // shown/closed.
  void LockScreen();
  void UnlockScreen();
  // Whether lockscreen is shown.
  bool IsLocked();

  // Simulates the system starting to suspend with Reason |reason|.
  // Wait until the event has been processed.
  void SimulateSystemSuspendAndWait(
      power_manager::SuspendImminent::Reason reason);

  // Return all media string view text containers. There is one per display.
  std::vector<views::View*> GetMediaStringViewTextContainers();
  // Return the media string view text container for the ambient mode container
  // on the default display.
  views::View* GetMediaStringViewTextContainer();

  // Return all media string view text labels. There is one per display.
  std::vector<views::Label*> GetMediaStringViewTextLabels();
  // Return the media string view text label for the ambient mode container on
  // the default display.
  views::Label* GetMediaStringViewTextLabel();

  // Simulates the system starting to resume.
  // Wait until the event has been processed.
  void SimulateSystemResumeAndWait();

  // Simulates a screen idle state event.
  // Wait until the event has been processed.
  void SetScreenIdleStateAndWait(bool is_screen_dimmed, bool is_off);

  void SimulateMediaMetadataChanged(media_session::MediaMetadata metadata);

  void SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState state);

  // Set the size of the next image that will be loaded.
  void SetDecodedPhotoSize(int width, int height);

  // Advance the task environment timer to expire the lock screen inactivity
  // timer.
  void FastForwardToLockScreenTimeout();

  // Advance the task environment timer to load the next photo.
  void FastForwardToNextImage();

  // Advance the task environment timer a tiny amount. This is intended to
  // trigger any pending async operations.
  void FastForwardTiny();

  // Advance the task environment timer to load the weather info.
  void FastForwardToRefreshWeather();

  // Advance the task environment timer to ambient mode lock screen delay.
  void FastForwardToBackgroundLockScreenTimeout();
  void FastForwardHalfLockScreenDelay();

  void SetPowerStateCharging();
  void SetPowerStateDischarging();
  void SetPowerStateFull();

  // Returns the number of active wake locks of type |type|.
  int GetNumOfActiveWakeLocks(device::mojom::WakeLockType type);

  // Simulate to issue an |access_token|.
  // If |with_error| is true, will return an empty access token.
  void IssueAccessToken(const std::string& access_token, bool with_error);

  bool IsAccessTokenRequestPending();

  base::TimeDelta GetRefreshTokenDelay();

  // Returns the ambient image view for each display.
  std::vector<AmbientBackgroundImageView*> GetAmbientBackgroundImageViews();
  // Returns the AmbientBackgroundImageView for the default display.
  AmbientBackgroundImageView* GetAmbientBackgroundImageView();

  // Returns the media string views for displaying ongoing media info.
  std::vector<MediaStringView*> GetMediaStringViews();
  // Returns the media string view for the default display.
  MediaStringView* GetMediaStringView();

  const std::map<int, PhotoCacheEntry>& GetCachedFiles();
  const std::map<int, PhotoCacheEntry>& GetBackupCachedFiles();

  AmbientController* ambient_controller();

  AmbientPhotoController* photo_controller();

  AmbientPhotoCache* photo_cache();

  // Returns the top-level views which contains all the ambient components.
  std::vector<AmbientContainerView*> GetContainerViews();
  // Returns the top level ambient container view for the primary root window.
  AmbientContainerView* GetContainerView();

  AmbientAccessTokenController* token_controller();

  FakeAmbientBackendControllerImpl* backend_controller();

  void FetchTopics();

  void FetchImage();

  void FetchBackupImages();

  void SetDownloadPhotoData(std::string data);

  void ClearDownloadPhotoData();

  void SetBackupDownloadPhotoData(std::string data);

  void ClearBackupDownloadPhotoData();

  void SetDecodePhotoImage(const gfx::ImageSkia& image);

 private:
  std::unique_ptr<views::Widget> widget_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_
