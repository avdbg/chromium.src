// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PREFERENCES_RENDERER_PREFERENCES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PREFERENCES_RENDERER_PREFERENCES_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/gfx/font_render_params.h"

namespace blink {

// User preferences passed between the browser and renderer processes.
// See //third_party/blink/public/mojom/renderer_preferences.mojom for a
// description of what each field is about.
struct BLINK_COMMON_EXPORT RendererPreferences {
  bool can_accept_load_drops{true};
  bool should_antialias_text{true};
  gfx::FontRenderParams::Hinting hinting{gfx::FontRenderParams::HINTING_MEDIUM};
  bool use_autohinter{false};
  bool use_bitmaps{false};
  gfx::FontRenderParams::SubpixelRendering subpixel_rendering{
      gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE};
  bool use_subpixel_positioning{false};
  uint32_t focus_ring_color{0xFFE59700};
  uint32_t active_selection_bg_color{0xFF1E90FF};
  uint32_t active_selection_fg_color{0xFFFFFFFF};
  uint32_t inactive_selection_bg_color{0xFFC8C8C8};
  uint32_t inactive_selection_fg_color{0xFF323232};
  bool browser_handles_all_top_level_requests{false};
  base::Optional<base::TimeDelta> caret_blink_interval;
  bool use_custom_colors{true};
  bool enable_referrers{true};
  bool allow_cross_origin_auth_prompt{false};
  bool enable_do_not_track{false};
  bool enable_encrypted_media{true};
  std::string webrtc_ip_handling_policy;
  uint16_t webrtc_udp_min_port{0};
  uint16_t webrtc_udp_max_port{0};
  std::vector<std::string> webrtc_local_ips_allowed_urls;
  bool webrtc_allow_legacy_tls_protocols{false};
  UserAgentOverride user_agent_override;
  std::string accept_languages;
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  std::string system_font_family_name;
#endif
#if defined(OS_WIN)
  base::string16 caption_font_family_name;
  int32_t caption_font_height{0};
  base::string16 small_caption_font_family_name;
  int32_t small_caption_font_height{0};
  base::string16 menu_font_family_name;
  int32_t menu_font_height{0};
  base::string16 status_font_family_name;
  int32_t status_font_height{0};
  base::string16 message_font_family_name;
  int32_t message_font_height{0};
  int32_t vertical_scroll_bar_width_in_dips{0};
  int32_t horizontal_scroll_bar_height_in_dips{0};
  int32_t arrow_bitmap_height_vertical_scroll_bar_in_dips{0};
  int32_t arrow_bitmap_width_horizontal_scroll_bar_in_dips{0};
#endif
#if defined(USE_X11) || defined(USE_OZONE)
  bool selection_clipboard_buffer_available{false};
#endif
  bool plugin_fullscreen_allowed{true};
  bool caret_browsing_enabled{false};
  std::string nw_inject_js_doc_start;
  std::string nw_inject_js_doc_end;

  RendererPreferences();
  RendererPreferences(const RendererPreferences& other);
  RendererPreferences(RendererPreferences&& other);
  ~RendererPreferences();
  RendererPreferences& operator=(const RendererPreferences& other);
  RendererPreferences& operator=(RendererPreferences&& other);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PREFERENCES_RENDERER_PREFERENCES_H_
