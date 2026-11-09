// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ARCOMIUM_MENU_BAR_HOOK_H_
#define CHROME_BROWSER_UI_TABS_ARCOMIUM_MENU_BAR_HOOK_H_

#include <cstddef>
#include <string>
#include <vector>

#include "ui/gfx/image/image_skia.h"

class BrowserWindowInterface;

namespace arcomium {

// The lists in macOS' menu bar that follow the window: the Spaces menu's
// Spaces and the Extensions menu's extensions, as in Arc for macOS. The menu
// (main_menu_builder.mm) fills them each time it opens; the Space bar
// (ArcomiumSpaceSwitcher), in the views layer, sets these hooks.
enum class MenuBarList { kSpaces, kExtensions };

struct MenuBarEntry {
  std::u16string title;
  gfx::ImageSkia icon;
  // Drawn in black, for the menu to tint as its text (a glyph, not an emoji
  // or an extension's own icon).
  bool icon_is_template = false;
  bool checked = false;
};

using MenuBarListFunction =
    std::vector<MenuBarEntry> (*)(BrowserWindowInterface* browser,
                                  MenuBarList list);
using MenuBarRunFunction = void (*)(BrowserWindowInterface* browser,
                                    MenuBarList list,
                                    size_t index);

inline MenuBarListFunction& MenuBarListHook() {
  static MenuBarListFunction hook = nullptr;
  return hook;
}

inline MenuBarRunFunction& MenuBarRunHook() {
  static MenuBarRunFunction hook = nullptr;
  return hook;
}

inline std::vector<MenuBarEntry> GetMenuBarList(BrowserWindowInterface* browser,
                                                MenuBarList list) {
  return MenuBarListHook() ? MenuBarListHook()(browser, list)
                           : std::vector<MenuBarEntry>();
}

// Runs the entry at `index` of what GetMenuBarList() gave for `list`.
inline void RunMenuBarEntry(BrowserWindowInterface* browser,
                            MenuBarList list,
                            size_t index) {
  if (MenuBarRunHook()) {
    MenuBarRunHook()(browser, list, index);
  }
}

// The name of the Space `browser` shows, which names its window on macOS as
// in Arc; "" without a Space bar (a popup, an incognito window).
using ShownSpaceTitleFunction =
    std::u16string (*)(BrowserWindowInterface* browser);

inline ShownSpaceTitleFunction& ShownSpaceTitleHook() {
  static ShownSpaceTitleFunction hook = nullptr;
  return hook;
}

inline std::u16string GetShownSpaceTitle(BrowserWindowInterface* browser) {
  return ShownSpaceTitleHook() ? ShownSpaceTitleHook()(browser)
                               : std::u16string();
}

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_TABS_ARCOMIUM_MENU_BAR_HOOK_H_
