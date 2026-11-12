// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SPACE_COLOR_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SPACE_COLOR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace alerse {

// Meta info of a Space's bookmark folder: its theme color, so it syncs.
inline constexpr char kSpaceColorMetaKey[] = "arcomium.space_color";

// The panel color of Arc's theme color picker.
inline constexpr SkColor kSpaceColorPickerBackground =
    SkColorSetRGB(0x1F, 0x1F, 0x1F);

// A Space's theme color, as Arc's picker sets it: a point on its pad (hue by
// angle, saturation by distance from the grey centre), how strongly it tints
// the window, how dark or light it is (0.5 as is), and light or dark mode.
struct SpaceColor {
  double hue = 0;
  double saturation = 0;
  double intensity = 0.5;
  double brightness = 0.5;
  bool light = false;

  bool operator==(const SpaceColor&) const = default;
};

std::optional<SpaceColor> ParseSpaceColor(const std::string& value);
std::string SerializeSpaceColor(const SpaceColor& color);

// The window frame (and so the sidebar) color for `color`.
SkColor GetSpaceFrameColor(const SpaceColor& color);

// Tints the browser window holding `widget` with `color`; none restores the
// default look.
void ApplySpaceColor(views::Widget* widget,
                     const std::optional<SpaceColor>& color);

// The sidebar's colors are drawn for a dark frame; on a light frame (a light
// theme) they are inverted. In an inactive window on macOS opaque ones follow
// the lighter frame (alerse::InkOverFrame).
SkColor SidebarInk(const views::View* view, SkColor dark_color);
// The sidebar's color `id` (chrome/browser/ui/color/alerse_color_mixer.cc).
SkColor SidebarInk(const views::View* view, ui::ColorId id);

// Arc's theme color picker: the pad, the light/dark switch, the intensity
// slider and preset swatches. `callback` runs on every change; `commit` is
// false while dragging.
using SpaceColorCallback =
    base::RepeatingCallback<void(const SpaceColor& color, bool commit)>;
std::unique_ptr<views::View> CreateSpaceColorPicker(
    const SpaceColor& initial,
    SpaceColorCallback callback);

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SPACE_COLOR_H_
