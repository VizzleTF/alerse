// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/arcomium_color_mixer.h"

#include "base/functional/bind.h"
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

}  // namespace

void AddArcomiumColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  if (key.contrast_mode == ui::ColorProviderKey::ContrastMode::kHigh) {
    mixer[kColorArcomiumSidebarItem] = {ui::kColorSysOnSurface};
    mixer[kColorArcomiumSidebarFolder] = {ui::kColorSysOnSurface};
    mixer[kColorArcomiumSidebarSubtle] = {ui::kColorSysOnSurfaceSubtle};
    mixer[kColorArcomiumSidebarNewTab] = {ui::kColorSysOnSurfaceSubtle};
    mixer[kColorArcomiumSidebarPlus] = {ui::kColorSysOnSurfaceSubtle};
    mixer[kColorArcomiumSidebarDivider] = {ui::kColorSysDivider};
    mixer[kColorArcomiumSidebarGlyph] = {ui::kColorSysOnSurface};
    mixer[kColorArcomiumSidebarGlyphFill] = {ui::kColorSysSurface};
    mixer[kColorArcomiumSidebarHover] = {ui::kColorSysStateHover};
    mixer[kColorArcomiumSidebarActive] = {ui::kColorSysStateFocusHighlight};
    mixer[kColorArcomiumSidebarPopup] = {ui::kColorSysSurface};
    mixer[kColorArcomiumMiniPlayerCard] = {ui::kColorSysStateHover};
    mixer[kColorArcomiumMiniPlayerCardHover] = {
        ui::kColorSysStateFocusHighlight};
    mixer[kColorArcomiumSpaceIcon] = {ui::kColorSysOnSurfaceSubtle};
    mixer[kColorArcomiumSpaceIconHovered] = {ui::kColorSysOnSurface};
    mixer[kColorArcomiumSpaceIconActive] = {ui::kColorSysOnSurface};
    mixer[kColorArcomiumArchiveSlot] = {ui::kColorSysSurface};
    return;
  }
  mixer[kColorArcomiumSidebarItem] = Ink(SkColorSetRGB(0xCF, 0xCF, 0xCF));
  mixer[kColorArcomiumSidebarFolder] = Ink(SkColorSetRGB(0xDB, 0xDB, 0xDB));
  mixer[kColorArcomiumSidebarSubtle] = Ink(SkColorSetRGB(0x84, 0x84, 0x84));
  mixer[kColorArcomiumSidebarNewTab] = Ink(SkColorSetRGB(0x87, 0x87, 0x87));
  mixer[kColorArcomiumSidebarPlus] = Ink(SkColorSetRGB(0x77, 0x77, 0x77));
  mixer[kColorArcomiumSidebarDivider] = Ink(SkColorSetRGB(0x3E, 0x3E, 0x3E));
  mixer[kColorArcomiumSidebarGlyph] = Ink(SkColorSetRGB(0xEB, 0xEB, 0xEB));
  mixer[kColorArcomiumSidebarGlyphFill] = Ink(SkColorSetRGB(0x18, 0x18, 0x18));
  mixer[kColorArcomiumSidebarHover] =
      Ink(SkColorSetARGB(0x10, 0xFF, 0xFF, 0xFF));
  mixer[kColorArcomiumSidebarActive] =
      Ink(SkColorSetARGB(0x1E, 0xFF, 0xFF, 0xFF));
  mixer[kColorArcomiumSidebarPopup] = Ink(SkColorSetRGB(0x2B, 0x2B, 0x2B));
  mixer[kColorArcomiumMiniPlayerCard] =
      Ink(SkColorSetARGB(0x1E, 0xFF, 0xFF, 0xFF));
  mixer[kColorArcomiumMiniPlayerCardHover] =
      Ink(SkColorSetARGB(0x2A, 0xFF, 0xFF, 0xFF));
  mixer[kColorArcomiumSpaceIcon] = Ink(SkColorSetRGB(0x7C, 0x7C, 0x7C));
  mixer[kColorArcomiumSpaceIconHovered] = Ink(SkColorSetRGB(0xC4, 0xC4, 0xC4));
  mixer[kColorArcomiumSpaceIconActive] = Ink(SkColorSetRGB(0xF2, 0xF2, 0xF2));
  mixer[kColorArcomiumArchiveSlot] = Ink(SkColorSetRGB(0x1E, 0x1F, 0x20));
}
