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
class Point;
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
// label in 13 pt semibold and the shortcut in grey at the right. The row
// nearest the arrow that can be used starts selected. A row runs its action
// once the popover is gone; `on_closed` runs when it closes either way.
void ShowArcPopover(views::View* anchor,
                    std::vector<PopoverItem> items,
                    bool above,
                    int width,
                    base::OnceClosure on_closed);

// Arc's icon picker: a black panel with a grid of Space glyphs `names` (""
// for none) at `screen_point`, in `anchor`'s window. `picked` gets the name
// clicked; Escape or a click elsewhere picks nothing.
void ShowArcIconPicker(views::View* anchor,
                       const gfx::Point& screen_point,
                       std::vector<std::string> names,
                       const std::string& current,
                       base::OnceCallback<void(const std::string&)> picked);

// Whether a popover of `anchor` closed just now: the press on the anchor
// that closed it should not open it again.
bool ArcPopoverJustClosed(const views::View* anchor);

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_POPOVER_H_
