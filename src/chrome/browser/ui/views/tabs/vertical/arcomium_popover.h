// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_POPOVER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_POPOVER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "ui/base/accelerators/accelerator.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace arcomium {

// The glyphs of Arc for macOS' popover rows, drawn here.
enum class PopoverIcon {
  kNone,
  kReload,
  kSidebar,
  kNewSpace,
  kNewFolder,
  kNewSplit,
  kNewTab,
};

// One row of the popover, or a separator when `label` is empty.
struct PopoverItem {
  PopoverItem();
  PopoverItem(PopoverIcon icon,
              std::u16string label,
              std::optional<ui::Accelerator> shortcut,
              bool enabled,
              base::OnceClosure action);
  PopoverItem(PopoverItem&&);
  PopoverItem& operator=(PopoverItem&&);
  ~PopoverItem();

  PopoverIcon icon = PopoverIcon::kNone;
  std::u16string label;
  std::optional<ui::Accelerator> shortcut;
  bool enabled = true;
  base::OnceClosure action;
};

// Arc for macOS' popover menu, as its header's "..." and its Space bar's "+"
// open: a black panel with 12 pt corners and an arrow at `anchor`'s middle,
// below it or, with `above`, over it. Rows are 40 pt: a 20 pt glyph, the
// label in 13 pt medium and the shortcut in grey at the right. The row
// nearest the arrow that can be used starts selected. A row runs its action
// once the popover is gone; `on_closed` runs when it closes either way.
void ShowArcPopover(views::View* anchor,
                    std::vector<PopoverItem> items,
                    bool above,
                    int width,
                    base::OnceClosure on_closed);

// Which of Arc's icon pickers: a pinned item's or favorite's Emoji and Icon
// pages (18 glyphs), a folder's 18 glyphs under an "Icon" title, or a
// Space's glyphs and emoji with a trash button.
enum class IconPickerKind {
  kItem,
  kFolder,
  kSpace,
};

// Arc's icon picker under `anchor_bounds` (a row, in screen coordinates),
// its arrow at `arrow_x`, or above it when there is no room below; in
// `anchor`'s window. `picked` gets the glyph's name, "emoji:" and the emoji,
// or "" to go back to no icon of its own (a Space's trash, or an item's
// current glyph picked again). Escape or a click elsewhere picks nothing;
// `on_closed` runs when it closes either way.
void ShowArcIconPicker(views::View* anchor,
                       const gfx::Rect& anchor_bounds,
                       int arrow_x,
                       IconPickerKind kind,
                       const std::string& current,
                       base::OnceCallback<void(const std::string&)> picked,
                       base::OnceClosure on_closed);

// Whether a popover of `anchor` closed just now: the press on the anchor
// that closed it should not open it again.
bool ArcPopoverJustClosed(const views::View* anchor);

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_POPOVER_H_
