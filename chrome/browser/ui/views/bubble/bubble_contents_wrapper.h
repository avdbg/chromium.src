// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_CONTENTS_WRAPPER_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_CONTENTS_WRAPPER_H_

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

// BubbleContentsWrapper wraps a WebContents that hosts a bubble WebUI (ie a
// WebUI with WebUIController subclassing MojoBubbleWebUIController). This class
// notifies the Host when it should be shown or hidden via ShowUI() and
// CloseUI() in addition to passing through resize events so the Host can adjust
// bounds accordingly.
class BubbleContentsWrapper : public content::WebContentsDelegate,
                              public content::WebContentsObserver,
                              public ui::MojoBubbleWebUIController::Embedder {
 public:
  class Host {
   public:
    virtual void CloseUI() = 0;
    virtual void ShowUI() = 0;
    virtual void ResizeDueToAutoResize(content::WebContents* source,
                                       const gfx::Size& new_size) {}
    virtual bool HandleKeyboardEvent(
        content::WebContents* source,
        const content::NativeWebKeyboardEvent& event);
  };

  BubbleContentsWrapper(content::BrowserContext* browser_context,
                        int task_manager_string_id,
                        bool enable_extension_apis,
                        bool webui_resizes_host);
  ~BubbleContentsWrapper() override;

  // content::WebContentsDelegate:
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;

  // content::WebContentsObserver:
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void RenderProcessGone(base::TerminationStatus status) override;

  // MojoBubbleWebUIController::Embedder:
  void CloseUI() override;
  void ShowUI() override;

  // Reloads the WebContents hosting the WebUI.
  virtual void ReloadWebContents() = 0;

  base::WeakPtr<BubbleContentsWrapper::Host> GetHost();
  void SetHost(base::WeakPtr<BubbleContentsWrapper::Host> host);

  content::WebContents* web_contents() { return web_contents_.get(); }

  void SetWebContentsForTesting(
      std::unique_ptr<content::WebContents> web_contents);

 private:
  // If true will allow the wrapped WebContents to automatically resize its
  // RenderWidgetHostView and send back updates to `Host` for the new size.
  const bool webui_resizes_host_;
  base::WeakPtr<BubbleContentsWrapper::Host> host_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// BubbleContentsWrapperT is designed to be paired with the WebUIController
// subclass used by the hosted WebUI. This type information allows compile time
// checking that the WebUIController subclasses MojoBubbleWebUIController as
// expected.
template <typename T>
class BubbleContentsWrapperT : public BubbleContentsWrapper {
 public:
  BubbleContentsWrapperT(const GURL& webui_url,
                         content::BrowserContext* browser_context,
                         int task_manager_string_id,
                         bool enable_extension_apis = false,
                         bool webui_resizes_host = true)
      : BubbleContentsWrapper(browser_context,
                              task_manager_string_id,
                              enable_extension_apis,
                              webui_resizes_host),
        webui_url_(webui_url) {}

  void ReloadWebContents() override {
    web_contents()->GetController().LoadURL(webui_url_, content::Referrer(),
                                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                            std::string());
    // Depends on the WebUIController object being constructed synchronously
    // when the navigation is started in LoadInitialURL().
    GetWebUIController()->set_embedder(weak_ptr_factory_.GetWeakPtr());
  }

  T* GetWebUIController() {
    return web_contents()->GetWebUI()->GetController()->template GetAs<T>();
  }

 private:
  const GURL webui_url_;
  base::WeakPtrFactory<BubbleContentsWrapper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_CONTENTS_WRAPPER_H_
