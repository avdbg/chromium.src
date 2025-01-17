// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/fmpi_get_volume_root_function.h"

#include "base/files/file.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chromeos/components/file_manager/url_constants.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/storage_partition.h"

namespace extensions {

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetVolumeRootFunction::Run() {
  using extensions::api::file_manager_private_internal::GetVolumeRoot::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& volume_id = params->options.volume_id;
  if (volume_id.empty()) {
    return RespondNow(Error("Volume ID must be provided."));
  }

  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(
          Profile::FromBrowserContext(browser_context()));
  DCHECK(volume_manager);
  base::WeakPtr<file_manager::Volume> volume =
      volume_manager->FindVolumeById(volume_id);
  if (!volume.get()) {
    return RespondNow(Error("Volume with ID '*' not found", volume_id));
  }

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  DCHECK(policy);
  const auto process_id = source_process_id();
  // Read-only permisisons.
  policy->GrantReadFile(process_id, volume->mount_path());
  if (params->options.writable.get() && *params->options.writable.get()) {
    // Additional write permissions.
    policy->GrantCreateReadWriteFile(process_id, volume->mount_path());
    policy->GrantCopyInto(process_id, volume->mount_path());
  }

  // Convert volume's mount path to a virtual path.
  scoped_refptr<storage::FileSystemContext> file_system_context =
      render_frame_host()->GetStoragePartition()->GetFileSystemContext();
  DCHECK(file_system_context.get());
  storage::ExternalFileSystemBackend* const backend =
      file_system_context->external_backend();
  DCHECK(backend);
  file_manager::util::FileDefinition fd;
  if (!backend->GetVirtualPath(volume->mount_path(), &fd.virtual_path)) {
    return RespondNow(Error("Volume with ID '*' not found", volume_id));
  }

  // Grant the caller right rights to crack URLs based on the virtual path.
  const std::string origin_id = source_url().GetOrigin().host();
  backend->GrantFileAccessToExtension(origin_id, fd.virtual_path);

  // Convert volume's mount path to an EntryDefinition.
  file_manager::util::ConvertFileDefinitionToEntryDefinition(
      file_system_context,
      url::Origin::Create(
          extensions::Extension::GetBaseURLFromExtensionId(origin_id)),
      fd,
      base::BindOnce(
          &FileManagerPrivateInternalGetVolumeRootFunction::OnRequestDone,
          this));

  return RespondLater();
}

void FileManagerPrivateInternalGetVolumeRootFunction::OnRequestDone(
    const file_manager::util::EntryDefinition& entry_definition) {
  if (entry_definition.error != base::File::FILE_OK) {
    Respond(Error("Failed to resolve volume's root directory: *",
                  base::NumberToString(entry_definition.error)));
  } else {
    Respond(OneArgument(base::Value::FromUniquePtrValue(
        ConvertEntryDefinitionToValue(entry_definition))));
  }
}

}  // namespace extensions
