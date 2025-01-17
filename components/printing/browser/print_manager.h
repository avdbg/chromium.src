// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_

#include <map>
#include <memory>

#include "build/build_config.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_ANDROID)
#include "base/callback.h"
#endif

#if BUILDFLAG(ENABLE_TAGGED_PDF)
#include "ui/accessibility/ax_tree_update_forward.h"
#endif

namespace printing {

class PrintManager : public content::WebContentsObserver,
                     public mojom::PrintManagerHost {
 public:
  PrintManager(const PrintManager&) = delete;
  PrintManager& operator=(const PrintManager&) = delete;
  ~PrintManager() override;

#if defined(OS_ANDROID)
  // TODO(timvolodine): consider introducing PrintManagerAndroid (crbug/500960)
  using PdfWritingDoneCallback =
      base::RepeatingCallback<void(int /* page count */)>;

  virtual void PdfWritingDone(int page_count) = 0;
#endif

  // printing::mojom::PrintManagerHost:
  void DidGetPrintedPagesCount(int32_t cookie, uint32_t number_pages) override;
  void DidGetDocumentCookie(int32_t cookie) override;
  void DidPrintDocument(mojom::DidPrintDocumentParamsPtr params,
                        DidPrintDocumentCallback callback) override;
#if BUILDFLAG(ENABLE_TAGGED_PDF)
  void SetAccessibilityTree(
      int32_t cookie,
      const ui::AXTreeUpdate& accessibility_tree) override;
#endif
  void UpdatePrintSettings(int32_t cookie,
                           base::Value job_settings,
                           UpdatePrintSettingsCallback callback) override;
  void DidShowPrintDialog() override;
  void ShowInvalidPrinterSettingsError() override;
  void PrintingFailed(int32_t cookie) override;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void SetupScriptedPrintPreview(
      SetupScriptedPrintPreviewCallback callback) override;
  void ShowScriptedPrintPreview(bool source_is_modifiable) override;
  void RequestPrintPreview(mojom::RequestPrintPreviewParamsPtr params) override;
  void CheckForCancel(int32_t preview_ui_id,
                      int32_t request_id,
                      CheckForCancelCallback callback) override;
#endif

 protected:
  explicit PrintManager(content::WebContents* contents);

  // Helper method to determine if PrintRenderFrame associated remote interface
  // is still connected.
  bool IsPrintRenderFrameConnected(content::RenderFrameHost* rfh);

  // Helper method to fetch the PrintRenderFrame associated remote interface
  // pointer.
  const mojo::AssociatedRemote<printing::mojom::PrintRenderFrame>&
  GetPrintRenderFrame(content::RenderFrameHost* rfh);

  // Terminates or cancels the print job if one was pending.
  void PrintingRenderFrameDeleted();

  // content::WebContentsObserver
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  uint32_t number_pages_ = 0;  // Number of pages to print in the print job.
  int cookie_ = 0;        // The current document cookie.
  // Holds WebContents associated mojo receivers.
  content::WebContentsFrameReceiverSet<printing::mojom::PrintManagerHost>
      print_manager_host_receivers_;

#if defined(OS_ANDROID)
  // Callback to execute when done writing pdf.
  PdfWritingDoneCallback pdf_writing_done_callback_;
#endif

 private:
  // Stores a PrintRenderFrame associated remote with the RenderFrameHost used
  // to bind it. The PrintRenderFrame is used to transmit mojo interface method
  // calls to the associated receiver.
  std::map<content::RenderFrameHost*,
           mojo::AssociatedRemote<printing::mojom::PrintRenderFrame>>
      print_render_frames_;
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_
