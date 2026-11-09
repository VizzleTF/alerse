// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_GLYPHS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_GLYPHS_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class PointF;
}  // namespace gfx

namespace arcomium {

// Arc's solid icon set for Spaces, folders and pinned items, drawn here in
// the same spirit (none of Arc's artwork). Names follow the ones Arc stores
// in customInfo.iconType.icon, so imported icons show as picked ones.

// Whether `name` is one of the glyphs drawn here.
bool IsArcGlyph(std::string_view name);

// Draws the glyph `name` in a 16 pt box centred on `center`; false when
// there is no such glyph.
bool PaintArcGlyph(gfx::Canvas* canvas,
                   const gfx::PointF& center,
                   std::string_view name,
                   SkColor color);

// The icons Arc's pickers offer: the Space and pinned item picker's 56 and
// the folder picker's 24, in Arc's order.
const std::vector<std::string>& GetSpacePickerGlyphs();
const std::vector<std::string>& GetFolderPickerGlyphs();

// A picked emoji is stored as "emoji:" and its UTF-8 text.
inline constexpr char kEmojiIconPrefix[] = "emoji:";
bool IsEmojiIcon(std::string_view name);
// The emoji the pickers' Emoji pages offer: Unicode's set up to Emoji 15.0
// without skin tones, in the macOS palette's order and eight categories
// (smileys and people, nature, food, activities, travel, objects, symbols,
// flags).
const std::vector<std::string>& GetPickerEmoji();
// The lower-case English name of GetPickerEmoji()[index], for search.
std::string_view GetPickerEmojiName(size_t index);
inline constexpr int kPickerEmojiCategories = 8;
// The index of the first emoji of `category`; the count past the last.
size_t GetPickerEmojiCategoryStart(int category);
// Draws an emoji icon `name` in a 16 pt box centred on `center`. Emoji keep
// their own colors, so a dimmed Space passes a lower `alpha` instead of an
// ink: an ink's brightness says nothing about dimming on a light theme.
void PaintEmojiIcon(gfx::Canvas* canvas,
                    const gfx::PointF& center,
                    std::string_view name,
                    uint8_t alpha = 255,
                    int font_size = 13);

// The name the picker shows as current for `name`, which may be one of the
// older names (Arcomium's first icons).
std::string CanonicalGlyphName(std::string_view name);

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_GLYPHS_H_
