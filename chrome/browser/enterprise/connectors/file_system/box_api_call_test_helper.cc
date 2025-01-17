// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"

namespace enterprise_connectors {
const char kFileSystemBoxFindFolderUrl[] =
    "https://api.box.com/2.0/search?type=folder&query=ChromeDownloads";
const char kFileSystemBoxCreateFolderUrl[] = "https://api.box.com/2.0/folders";
const char kFileSystemBoxWholeFileUploadUrl[] =
    "https://upload.box.com/api/2.0/files/content";

const char kFileSystemBoxFindFolderResponseBody[] = R"({
    "entries": [
      {
        "id": 12345,
        "etag": 1,
        "type": "folder",
        "sequence_id": 3,
        "name": "ChromeDownloads"
      }
    ]
  })";

const char kFileSystemBoxFindFolderResponseFolderId[] = "12345";
// Should match id in kFileSystemBoxFindFolderResponseBody, as it's used to
// verify extracted folder_id from body above

const char kFileSystemBoxFindFolderResponseEmptyEntriesList[] = R"({
    "entries": [
        ]
  })";

const char kFileSystemBoxCreateFolderResponseBody[] = R"({
    "id": 67890,
    "type": "folder",
    "content_created_at": "2012-12-12T10:53:43-08:00",
    "content_modified_at": "2012-12-12T10:53:43-08:00",
    "created_at": "2012-12-12T10:53:43-08:00",
    "created_by": {
      "id": 11446498,
      "type": "user",
      "login": "ceo@example.com",
      "name": "Aaron Levie"
    },
    "description": "Files downloaded in Chrome",
    "etag": 1,
    "expires_at": "2012-12-12T10:53:43-08:00",
    "folder_upload_email": {
      "access": "open",
      "email": "upload.Contracts.asd7asd@u.box.com"
    },
    "name": "ChromeDownloads",
    "owned_by": {
      "id": 11446498,
      "type": "user",
      "login": "ceo@example.com",
      "name": "Aaron Levie"
    },
    "parent": {
      "id": 0,
      "type": "folder",
      "etag": 1,
      "name": "",
      "sequence_id": 3
    }
  })";

const char kFileSystemBoxCreateFolderResponseFolderId[] = "67890";
// Should match id in kFileSystemBoxCreateFolderResponseBody, as it's used to
// verify extracted folder_id from body above

}  // namespace enterprise_connectors
