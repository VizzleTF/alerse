// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_APP_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_APP_MENU_H_

#include "base/functional/callback_forward.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"

class BrowserWindowInterface;

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace alerse {

// The menu of the sidebar's logo: the browser's everyday commands in a few
// short submenus (Spaces, tabs, extensions, developer tools), and "More" for
// the full app menu. It deletes itself once closed.
void ShowAppMenu(BrowserWindowInterface* browser, views::View* anchor);

// The popover of the "..." button in Arc for macOS' header: Reload and
// Toggle Sidebar, as in Arc (alerse_popover.h). `on_closed` runs when it
// closes.
void ShowHeaderMenu(BrowserWindowInterface* browser,
                    views::View* anchor,
                    base::RepeatingClosure toggle_sidebar,
                    base::OnceClosure on_closed);

// The active Space's menu at `point`, which Arc for macOS opens on a right
// click on its header's "..." as anywhere else in the sidebar.
void ShowSpaceMenu(BrowserWindowInterface* browser,
                   const gfx::Point& point,
                   ui::mojom::MenuSourceType source_type);

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_APP_MENU_H_
