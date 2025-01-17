// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_UTIL_H_

#import "ios/chrome/browser/ui/main/scene_state.h"

// Checks whether the Start Surface should be shown for the given scene state.
bool ShouldShowStartSurfaceForSceneState(SceneState* sceneState);

// Sets the session related objects for the Start Surface.
void SetStartSurfaceSessionObjectForSceneState(SceneState* sceneState);

#endif  // IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_UTIL_H_.
