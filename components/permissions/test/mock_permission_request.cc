// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/mock_permission_request.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/permissions/request_type.h"

#if defined(OS_ANDROID)
#include "components/resources/android/theme_resources.h"
#else
#include "components/vector_icons/vector_icons.h"
#endif

namespace permissions {

MockPermissionRequest::MockPermissionRequest()
    : MockPermissionRequest("test",
                            "button",
                            "button",
                            GURL("http://www.google.com"),
                            RequestType::kNotifications,
                            PermissionRequestGestureType::UNKNOWN,
                            ContentSettingsType::NOTIFICATIONS) {}

MockPermissionRequest::MockPermissionRequest(const std::string& text)
    : MockPermissionRequest(text,
                            "button",
                            "button",
                            GURL("http://www.google.com"),
                            RequestType::kNotifications,
                            PermissionRequestGestureType::UNKNOWN,
                            ContentSettingsType::NOTIFICATIONS) {}

MockPermissionRequest::MockPermissionRequest(
    const std::string& text,
    RequestType request_type,
    PermissionRequestGestureType gesture_type)
    : MockPermissionRequest(text,
                            "button",
                            "button",
                            GURL("http://www.google.com"),
                            request_type,
                            gesture_type,
                            ContentSettingsType::NOTIFICATIONS) {}

MockPermissionRequest::MockPermissionRequest(const std::string& text,
                                             RequestType request_type,
                                             const GURL& url)
    : MockPermissionRequest(text,
                            "button",
                            "button",
                            url,
                            request_type,
                            PermissionRequestGestureType::UNKNOWN,
                            ContentSettingsType::NOTIFICATIONS) {}

MockPermissionRequest::MockPermissionRequest(const std::string& text,
                                             const std::string& accept_label,
                                             const std::string& deny_label)
    : MockPermissionRequest(text,
                            accept_label,
                            deny_label,
                            GURL("http://www.google.com"),
                            RequestType::kNotifications,
                            PermissionRequestGestureType::UNKNOWN,
                            ContentSettingsType::NOTIFICATIONS) {}

MockPermissionRequest::MockPermissionRequest(
    const std::string& text,
    ContentSettingsType content_settings_type)
    : MockPermissionRequest(
          text,
          "button",
          "button",
          GURL("http://www.google.com"),
          permissions::ContentSettingsTypeToRequestType(content_settings_type),
          PermissionRequestGestureType::UNKNOWN,
          content_settings_type) {}

MockPermissionRequest::~MockPermissionRequest() = default;

RequestType MockPermissionRequest::GetRequestType() const {
  return request_type_;
}

#if defined(OS_ANDROID)
base::string16 MockPermissionRequest::GetMessageText() const {
  return text_;
}
#endif

base::string16 MockPermissionRequest::GetMessageTextFragment() const {
  return text_;
}

GURL MockPermissionRequest::GetOrigin() const {
  return origin_;
}

void MockPermissionRequest::PermissionGranted(bool is_one_time) {
  granted_ = true;
}

void MockPermissionRequest::PermissionDenied() {
  granted_ = false;
}

void MockPermissionRequest::Cancelled() {
  granted_ = false;
  cancelled_ = true;
}

void MockPermissionRequest::RequestFinished() {
  finished_ = true;
}

PermissionRequestGestureType MockPermissionRequest::GetGestureType() const {
  return gesture_type_;
}

ContentSettingsType MockPermissionRequest::GetContentSettingsType() const {
  return content_settings_type_;
}

bool MockPermissionRequest::granted() {
  return granted_;
}

bool MockPermissionRequest::cancelled() {
  return cancelled_;
}

bool MockPermissionRequest::finished() {
  return finished_;
}

MockPermissionRequest::MockPermissionRequest(
    const std::string& text,
    const std::string& accept_label,
    const std::string& deny_label,
    const GURL& origin,
    RequestType request_type,
    PermissionRequestGestureType gesture_type,
    ContentSettingsType content_settings_type)
    : granted_(false),
      cancelled_(false),
      finished_(false),
      request_type_(request_type),
      gesture_type_(gesture_type),
      content_settings_type_(content_settings_type) {
  text_ = base::UTF8ToUTF16(text);
  accept_label_ = base::UTF8ToUTF16(accept_label);
  deny_label_ = base::UTF8ToUTF16(deny_label);
  origin_ = origin.GetOrigin();
}

}  // namespace permissions
