// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_COMMAND_BAR_HOOK_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_COMMAND_BAR_HOOK_H_

class BrowserWindowInterface;
class GURL;

namespace alerse {

// Opens the command bar of `browser`'s Arc sidebar for a new tab; false when
// no sidebar shows there. Set by the sidebar (AlerseSidebarView), which
// lives in the views layer, so the browser's New Tab command can reach it.
using OpenNewTabCommandBarFunction = bool (*)(BrowserWindowInterface* browser);

inline OpenNewTabCommandBarFunction& NewTabCommandBarHook() {
  static OpenNewTabCommandBarFunction hook = nullptr;
  return hook;
}

inline bool OpenNewTabCommandBar(BrowserWindowInterface* browser) {
  return NewTabCommandBarHook() && NewTabCommandBarHook()(browser);
}

// Opens the command bar for the shown tab, as Arc does for the pane Add
// Split View adds; false when no sidebar shows there.
inline OpenNewTabCommandBarFunction& CommandBarHook() {
  static OpenNewTabCommandBarFunction hook = nullptr;
  return hook;
}

inline bool OpenCommandBar(BrowserWindowInterface* browser) {
  return CommandBarHook() && CommandBarHook()(browser);
}

// Shows the sidebar's pinned item for `url` in `browser`'s shown Space, as a
// click on it does: its tab if open, else a new tab bound to it. False when
// the Space has no such item. Picking a sidebar item in the command bar uses
// it, so the item is not opened a second time in an unbound tab.
using OpenPinnedItemFunction = bool (*)(BrowserWindowInterface* browser,
                                        const GURL& url);

inline OpenPinnedItemFunction& PinnedItemHook() {
  static OpenPinnedItemFunction hook = nullptr;
  return hook;
}

inline bool OpenPinnedItem(BrowserWindowInterface* browser, const GURL& url) {
  return PinnedItemHook() && PinnedItemHook()(browser, url);
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_COMMAND_BAR_HOOK_H_
