// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_ARCOMIUM_SITE_CONTROLS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_ARCOMIUM_SITE_CONTROLS_H_

class BrowserWindowInterface;
class ExtensionsToolbarDesktop;

namespace views {
class View;
class Widget;
}  // namespace views

// Arcomium: Arc's site controls, which the extensions button opens. Actions
// on the page, every extension (a click runs it, a right click pins it), the
// site's permissions and its connection.
namespace arcomium_site_controls {

views::Widget* Show(views::View* anchor,
                    BrowserWindowInterface* browser,
                    ExtensionsToolbarDesktop* extensions);
bool IsShowing();
void Hide();

}  // namespace arcomium_site_controls

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_ARCOMIUM_SITE_CONTROLS_H_
