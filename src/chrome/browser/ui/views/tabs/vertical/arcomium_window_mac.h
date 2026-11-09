// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_WINDOW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_WINDOW_MAC_H_

namespace views {
class Widget;
}  // namespace views

namespace arcomium {

// Names `popover`'s window as one of Arc's "..." and "+" popovers: while it
// has the keyboard, the browser window's controls under it are ringed in
// grey, as Arc for macOS shows them (browser_native_widget_window_mac.mm).
void MarkPopoverWindow(views::Widget* popover);

// Drops the macOS shadow of `widget`'s window: its dark outline and the
// light edge it gives a dark window, which Arc's popups do not have.
void RemoveWindowShadow(views::Widget* widget);

// Blurs what is behind `popover`'s window wherever the window paints, as
// Arc's vibrant pickers show; the panel's half-translucent paint goes over it.
void BlurBehindPopover(views::Widget* popover);

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_WINDOW_MAC_H_
