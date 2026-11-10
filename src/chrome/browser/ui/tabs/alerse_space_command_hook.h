// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_SPACE_COMMAND_HOOK_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_SPACE_COMMAND_HOOK_H_

class BrowserWindowInterface;

namespace alerse {

// Runs a Space command (IDC_ALERSE_NEW_SPACE, _NEXT_SPACE, _PREVIOUS_SPACE,
// _RENAME_SPACE, _EDIT_SPACE_THEME) in `browser`'s Space bar, or a sidebar
// command (IDC_ALERSE_PIN_TAB, _NEW_FOLDER, _CLEAR_TODAY,
// _TOGGLE_PINNED) in its sidebar; false when there is none. Set by the Space
// bar (AlerseSpaceSwitcher), which lives in the views
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

// Whether such a command can run in `browser` now, and for a toggle which way
// it goes: kOn when the shown tab is pinned (the command unpins it) or the
// pinned tabs are collapsed (it expands them). The command bar names its
// actions by it, as Arc's Pin Tab / Unpin Tab.
enum class SpaceCommandState { kUnavailable, kAvailable, kOn };

using SpaceCommandStateFunction =
    SpaceCommandState (*)(BrowserWindowInterface* browser, int command_id);

inline SpaceCommandStateFunction& SpaceCommandStateHook() {
  static SpaceCommandStateFunction hook = nullptr;
  return hook;
}

inline SpaceCommandState GetSpaceCommandState(BrowserWindowInterface* browser,
                                              int command_id) {
  return SpaceCommandStateHook() ? SpaceCommandStateHook()(browser, command_id)
                                 : SpaceCommandState::kUnavailable;
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_SPACE_COMMAND_HOOK_H_
