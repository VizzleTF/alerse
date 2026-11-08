// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ARCOMIUM_SPACE_COMMAND_HOOK_H_
#define CHROME_BROWSER_UI_TABS_ARCOMIUM_SPACE_COMMAND_HOOK_H_

class BrowserWindowInterface;

namespace arcomium {

// Runs a Space command (IDC_ARCOMIUM_NEW_SPACE, _NEXT_SPACE, _PREVIOUS_SPACE,
// _RENAME_SPACE, _EDIT_SPACE_THEME) in `browser`'s Space bar, or a sidebar
// command (IDC_ARCOMIUM_PIN_TAB, _NEW_FOLDER, _CLEAR_TODAY) in its sidebar;
// false when there is none. Set by the Space bar (ArcomiumSpaceSwitcher), which lives in the views
// layer, so the menu bar's commands can reach it.
using SpaceCommandFunction = bool (*)(BrowserWindowInterface* browser,
                                      int command_id);

inline SpaceCommandFunction& SpaceCommandHook() {
  static SpaceCommandFunction hook = nullptr;
  return hook;
}

inline bool RunSpaceCommand(BrowserWindowInterface* browser, int command_id) {
  return SpaceCommandHook() && SpaceCommandHook()(browser, command_id);
}

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_TABS_ARCOMIUM_SPACE_COMMAND_HOOK_H_
