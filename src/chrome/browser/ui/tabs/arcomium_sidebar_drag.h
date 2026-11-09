// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ARCOMIUM_SIDEBAR_DRAG_H_
#define CHROME_BROWSER_UI_TABS_ARCOMIUM_SIDEBAR_DRAG_H_

class BrowserWindowInterface;

namespace arcomium {

// Whether a row of an Arc sidebar (ArcomiumSidebarView) is being dragged.
// The row carries its page's address so that Chromium's split drop zone on
// the page's edges takes it; the page itself then takes no drop, which would
// load that address in it or paste it.
inline bool& SidebarRowDragging() {
  static bool dragging = false;
  return dragging;
}

// Puts the dragged row's own tab in a split beside `browser`'s shown tab, on
// the start side (`start`) or below it (`stacked`). True when the drop was
// the sidebar's, done or not; false when `browser`'s sidebar drags no row
// (or a favorite), so the drop opens the address in a new pane as Chromium
// does. Set by the sidebar, which lives in the views layer.
using SplitDraggedRowFunction = bool (*)(BrowserWindowInterface* browser,
                                         bool start,
                                         bool stacked);

inline SplitDraggedRowFunction& SplitDraggedRowHook() {
  static SplitDraggedRowFunction hook = nullptr;
  return hook;
}

inline bool SplitDraggedRow(BrowserWindowInterface* browser,
                            bool start,
                            bool stacked) {
  return SidebarRowDragging() && SplitDraggedRowHook() &&
         SplitDraggedRowHook()(browser, start, stacked);
}

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_TABS_ARCOMIUM_SIDEBAR_DRAG_H_
