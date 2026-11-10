// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_ALERSE_SITE_CONTROLS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_ALERSE_SITE_CONTROLS_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/context_menu_controller.h"

class BrowserWindowInterface;
class ExtensionsToolbarDesktop;
class ToolbarActionViewModel;

namespace views {
class MenuModelAdapter;
class MenuRunner;
class View;
class Widget;
}  // namespace views

// Alerse: Arc's site controls, which the extensions button opens. Actions
// on the page, every extension (a click runs it, a right click pins it), the
// site's permissions and its connection.
namespace alerse_site_controls {

views::Widget* Show(views::View* anchor,
                    BrowserWindowInterface* browser,
                    ExtensionsToolbarDesktop* extensions);
bool IsShowing();
void Hide();

// An extension's context menu (Pin, Options, Remove...) for a plain button,
// as the panel's tiles and the sidebar's field and tiles are:
// ExtensionContextMenuController runs it from a menu button only (it casts
// the source's button controller, which crashed on a tile). `on_closed` runs
// once the menu closes.
class ExtensionMenu : public views::ContextMenuController {
 public:
  ExtensionMenu(ToolbarActionViewModel* action,
                base::RepeatingClosure on_closed);
  ExtensionMenu(const ExtensionMenu&) = delete;
  ExtensionMenu& operator=(const ExtensionMenu&) = delete;
  ~ExtensionMenu() override;

 private:
  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  const raw_ptr<ToolbarActionViewModel> action_;
  const base::RepeatingClosure on_closed_;
  std::unique_ptr<views::MenuModelAdapter> adapter_;
  std::unique_ptr<views::MenuRunner> runner_;
};

std::unique_ptr<ExtensionMenu> CreateExtensionMenu(
    ToolbarActionViewModel* action,
    base::RepeatingClosure on_closed);

}  // namespace alerse_site_controls

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_ALERSE_SITE_CONTROLS_H_
