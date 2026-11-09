// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_TOOLTIP_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_TOOLTIP_H_

#include <optional>
#include <string>

#include "ui/base/accelerators/accelerator.h"

namespace views {
class View;
}  // namespace views

namespace arcomium {

// Arc for macOS' tooltip: a black pill with the name in light grey and its
// shortcut as keycaps, or a grey second line. It shows a second after the
// pointer settles on `view`, by the pointer, and hides when it leaves or
// clicks. The caller leaves `view` without a native tooltip; setting it again
// replaces the text. Used on macOS, where Arc draws its own.
void SetArcTooltip(views::View* view,
                   const std::u16string& title,
                   const std::u16string& detail = std::u16string(),
                   std::optional<ui::Accelerator> shortcut = std::nullopt);

// Keeps `view`'s Arc tooltip by the pointer even at the window's bottom,
// where it reaches out of the window, as Arc's Library tooltip does.
void SetArcTooltipBelowPointer(views::View* view);

// Changes only the second line of `view`'s Arc tooltip, if it has one.
void SetArcTooltipDetail(views::View* view, const std::u16string& detail);

// Hides any Arc tooltip and keeps new ones away while `suppressed`: a
// popover (the icon picker) opens over the view the pointer rests on.
void SetArcTooltipsSuppressed(bool suppressed);

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_TOOLTIP_H_
