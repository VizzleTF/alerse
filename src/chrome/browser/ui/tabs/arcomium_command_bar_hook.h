// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ARCOMIUM_COMMAND_BAR_HOOK_H_
#define CHROME_BROWSER_UI_TABS_ARCOMIUM_COMMAND_BAR_HOOK_H_

class BrowserWindowInterface;

namespace arcomium {

// Opens the command bar of `browser`'s Arc sidebar for a new tab; false when
// no sidebar shows there. Set by the sidebar (ArcomiumSidebarView), which
// lives in the views layer, so the browser's New Tab command can reach it.
using OpenNewTabCommandBarFunction = bool (*)(BrowserWindowInterface* browser);

inline OpenNewTabCommandBarFunction& NewTabCommandBarHook() {
  static OpenNewTabCommandBarFunction hook = nullptr;
  return hook;
}

inline bool OpenNewTabCommandBar(BrowserWindowInterface* browser) {
  return NewTabCommandBarHook() && NewTabCommandBarHook()(browser);
}

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_TABS_ARCOMIUM_COMMAND_BAR_HOOK_H_
