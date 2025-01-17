// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/display_capture/display_capture_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

DisplayCapturePermissionContext::DisplayCapturePermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::DISPLAY_CAPTURE,
          blink::mojom::FeaturePolicyFeature::kDisplayCapture) {}

ContentSetting DisplayCapturePermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return CONTENT_SETTING_ASK;
}

void DisplayCapturePermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  NotifyPermissionSet(id, requesting_origin, embedding_origin,
                      std::move(callback), /*persist=*/false,
                      CONTENT_SETTING_DEFAULT, /*is_one_time=*/false);
}

void DisplayCapturePermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting,
    bool is_one_time) {
  NOTREACHED();
}

bool DisplayCapturePermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
