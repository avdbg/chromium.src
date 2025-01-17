// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/aw_gl_surface.h"

#include <utility>

#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"
#include "ui/gl/gl_bindings.h"

#define EGL_EXTERNAL_SURFACE_ANGLE 0x348F

namespace android_webview {

AwGLSurface::AwGLSurface(bool is_angle) : is_angle_(is_angle) {}

AwGLSurface::~AwGLSurface() {
  Destroy();
}

bool AwGLSurface::Initialize(gl::GLSurfaceFormat format) {
  if (!is_angle_)
    return true;

  Destroy();

  EGLint attribs[] = {EGL_WIDTH,      size_.width(), EGL_HEIGHT,
                      size_.height(), EGL_NONE,      EGL_NONE};
  surface_ = eglCreatePbufferFromClientBuffer(
      GetDisplay(), EGL_EXTERNAL_SURFACE_ANGLE, nullptr, GetConfig(), attribs);
  DCHECK_NE(surface_, EGL_NO_SURFACE);
  return surface_ != EGL_NO_SURFACE;
}

void AwGLSurface::Destroy() {
  if (surface_) {
    eglDestroySurface(GetDisplay(), surface_);
    surface_ = nullptr;
  }
}

bool AwGLSurface::IsOffscreen() {
  return false;
}

unsigned int AwGLSurface::GetBackingFramebufferObject() {
  return ScopedAppGLStateRestore::Current()->framebuffer_binding_ext();
}

gfx::SwapResult AwGLSurface::SwapBuffers(PresentationCallback callback) {
  DCHECK(!pending_presentation_callback_);
  pending_presentation_callback_ = std::move(callback);
  return gfx::SwapResult::SWAP_ACK;
}

gfx::Size AwGLSurface::GetSize() {
  return size_;
}

void* AwGLSurface::GetHandle() {
  return surface_;
}

void* AwGLSurface::GetDisplay() {
  if (!is_angle_)
    return nullptr;
  return gl::GLSurfaceEGL::GetDisplay();
}

gl::GLSurfaceFormat AwGLSurface::GetFormat() {
  return gl::GLSurfaceFormat();
}

bool AwGLSurface::Resize(const gfx::Size& size,
                         float scale_factor,
                         const gfx::ColorSpace& color_space,
                         bool has_alpha) {
  if (size_ == size)
    return true;
  size_ = size;

  return Initialize(gl::GLSurfaceFormat());
}

void AwGLSurface::SetSize(const gfx::Size& size) {
  size_ = size;
}

EGLConfig AwGLSurface::GetConfig() {
  if (!is_angle_)
    return nullptr;
  return gl::GLSurfaceEGL::GetConfig();
}

void AwGLSurface::MaybeDidPresent(const gfx::PresentationFeedback& feedback) {
  if (!pending_presentation_callback_)
    return;
  std::move(pending_presentation_callback_).Run(feedback);
}

}  // namespace android_webview
