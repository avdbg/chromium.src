// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/clipboard_ash.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard.h"

namespace crosapi {

ClipboardAsh::ClipboardAsh() = default;
ClipboardAsh::~ClipboardAsh() = default;

void ClipboardAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Clipboard> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void ClipboardAsh::GetCopyPasteText(GetCopyPasteTextCallback callback) {
  base::string16 text;

  // There is no source that appropriately represents Lacros. Use kDefault for
  // now.
  const ui::DataTransferEndpoint endpoint(ui::EndpointType::kDefault);
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &endpoint, &text);

  std::move(callback).Run(base::UTF16ToUTF8(text));
}

}  // namespace crosapi
