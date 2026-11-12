// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/alerse_color_mixer.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_utils.h"

namespace {

// Arc's grey on a dark frame; on a light one, its inverse, alpha kept.
ui::ColorTransform Ink(SkColor dark) {
  return ui::ColorTransform(base::BindRepeating(
      [](SkColor dark, SkColor input, const ui::ColorMixer& mixer) {
        if (color_utils::IsDark(mixer.GetResultColor(ui::kColorFrameActive))) {
          return dark;
        }
        return SkColorSetARGB(SkColorGetA(dark), 255 - SkColorGetR(dark),
                              255 - SkColorGetG(dark),
                              255 - SkColorGetB(dark));
      },
      dark));
}

// `dark` on a dark frame, `light` on a light one.
ui::ColorTransform Paired(SkColor dark, SkColor light) {
  return ui::ColorTransform(base::BindRepeating(
      [](SkColor dark, SkColor light, SkColor input,
         const ui::ColorMixer& mixer) {
        return color_utils::IsDark(mixer.GetResultColor(ui::kColorFrameActive))
                   ? dark
                   : light;
      },
      dark, light));
}

}  // namespace

void AddAlerseColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  if (key.contrast_mode == ui::ColorProviderKey::ContrastMode::kHigh) {
    mixer[kColorAlerseSidebarItem] = {ui::kColorSysOnSurface};
    mixer[kColorAlerseSidebarFolder] = {ui::kColorSysOnSurface};
    mixer[kColorAlerseSidebarSubtle] = {ui::kColorSysOnSurfaceSubtle};
    mixer[kColorAlerseSidebarNewTab] = {ui::kColorSysOnSurfaceSubtle};
    mixer[kColorAlerseSidebarPlus] = {ui::kColorSysOnSurfaceSubtle};
    mixer[kColorAlerseSidebarDivider] = {ui::kColorSysDivider};
    mixer[kColorAlerseSidebarGlyph] = {ui::kColorSysOnSurface};
    mixer[kColorAlerseSidebarGlyphFill] = {ui::kColorSysSurface};
    mixer[kColorAlerseSidebarHover] = {ui::kColorSysStateHover};
    mixer[kColorAlerseSidebarActive] = {ui::kColorSysStateFocusHighlight};
    mixer[kColorAlerseSidebarPopup] = {ui::kColorSysSurface};
    mixer[kColorAlerseMiniPlayerCard] = {ui::kColorSysStateHover};
    mixer[kColorAlerseMiniPlayerCardHover] = {
        ui::kColorSysStateFocusHighlight};
    mixer[kColorAlerseSpaceIcon] = {ui::kColorSysOnSurfaceSubtle};
    mixer[kColorAlerseSpaceIconHovered] = {ui::kColorSysOnSurface};
    mixer[kColorAlerseSpaceIconActive] = {ui::kColorSysOnSurface};
    mixer[kColorAlerseArchiveSlot] = {ui::kColorSysSurface};
    mixer[kColorAlerseSidebarPlaceholderHovered] = {kColorOmniboxTextDimmed};
    mixer[kColorAlerseSidebarPlaceholderInactive] = {kColorOmniboxTextDimmed};
    return;
  }
  // Arc for macOS sets tabs as bright as folders, and its New Tab row's "+"
  // brighter than the text.
  mixer[kColorAlerseSidebarItem] = Ink(SkColorSetRGB(0xDA, 0xDA, 0xDA));
  mixer[kColorAlerseSidebarFolder] = Ink(SkColorSetRGB(0xDA, 0xDA, 0xDA));
  mixer[kColorAlerseSidebarNewTab] = Ink(SkColorSetRGB(0x7F, 0x7F, 0x7F));
  mixer[kColorAlerseSidebarPlus] = Ink(SkColorSetRGB(0x8B, 0x8B, 0x8B));
  mixer[kColorAlerseSidebarSubtle] = Ink(SkColorSetRGB(0x84, 0x84, 0x84));
  mixer[kColorAlerseSidebarDivider] = Ink(SkColorSetRGB(0x3E, 0x3E, 0x3E));
  // Arc for macOS' folder outline.
  mixer[kColorAlerseSidebarGlyph] = Ink(SkColorSetRGB(0xF0, 0xF0, 0xF0));
  mixer[kColorAlerseSidebarGlyphFill] = Ink(SkColorSetRGB(0x18, 0x18, 0x18));
  // Arc for macOS' highlights are brighter: #2D2D2D and #444543 on its
  // #161616 frame.
  mixer[kColorAlerseSidebarHover] =
      Ink(SkColorSetARGB(0x19, 0xFF, 0xFF, 0xFF));
  mixer[kColorAlerseSidebarActive] =
      Ink(SkColorSetARGB(0x32, 0xFF, 0xFF, 0xFF));
  // Arc for macOS' folder popup is near the frame's black.
  mixer[kColorAlerseSidebarPopup] = Ink(SkColorSetRGB(0x1A, 0x19, 0x19));
  mixer[kColorAlerseMiniPlayerCard] =
      Ink(SkColorSetARGB(0x1E, 0xFF, 0xFF, 0xFF));
  mixer[kColorAlerseMiniPlayerCardHover] =
      Ink(SkColorSetARGB(0x2A, 0xFF, 0xFF, 0xFF));
  // Arc for macOS dims the other Spaces far more; its archive and "+" take
  // the hovered grey (alerse_space_switcher.cc).
  mixer[kColorAlerseSpaceIcon] = Ink(SkColorSetRGB(0x3F, 0x3F, 0x3F));
  mixer[kColorAlerseSpaceIconHovered] = Ink(SkColorSetRGB(0xA0, 0xA0, 0xA0));
  mixer[kColorAlerseSpaceIconActive] = Ink(SkColorSetRGB(0xF0, 0xF0, 0xF1));
  mixer[kColorAlerseArchiveSlot] = Ink(SkColorSetRGB(0x1E, 0x1F, 0x20));
  // Arc for macOS' command bar placeholder (omnibox_view_views.cc).
  mixer[kColorOmniboxForegroundDisabled] = Ink(SkColorSetRGB(0x97, 0x97, 0x97));
  // The sidebar field's "Search..." (#6F6F6F) under the pointer. In an
  // inactive window it follows the lighter frame, set with that frame
  // (chrome_color_mixers.cc).
  mixer[kColorAlerseSidebarPlaceholderHovered] =
      Ink(SkColorSetRGB(0x75, 0x75, 0x76));
  mixer[kColorAlerseSidebarPlaceholderInactive] = {kColorOmniboxTextDimmed};
  // The find bar is Arc's pill (alerse_find_bar.h): a dark blue plate under
  // a dark frame, a near white one under a light frame.
  mixer[kColorFindBarBackground] = Paired(SkColorSetRGB(0x15, 0x1C, 0x30),
                                          SkColorSetRGB(0xF4, 0xF5, 0xF9));
  mixer[kColorFindBarForeground] = Paired(SkColorSetRGB(0xE3, 0xE5, 0xEA),
                                          SkColorSetRGB(0x1D, 0x1F, 0x26));
  mixer[kColorFindBarMatchCount] = Paired(SkColorSetRGB(0x95, 0x99, 0xA2),
                                          SkColorSetRGB(0x6E, 0x72, 0x7C));
  mixer[kColorFindBarButtonIcon] = {kColorFindBarForeground};
  mixer[kColorFindBarButtonIconHovered] = {kColorFindBarForeground};
  mixer[kColorFindBarButtonIconDisabled] = {kColorFindBarMatchCount};
}
