// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_APP_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_APP_MENU_H_

class BrowserWindowInterface;

namespace views {
class View;
}  // namespace views

namespace arcomium {

// The menu of the sidebar's logo: the browser's everyday commands in a few
// short submenus (Spaces, tabs, extensions, developer tools), and "More" for
// the full app menu. It deletes itself once closed.
void ShowAppMenu(BrowserWindowInterface* browser, views::View* anchor);

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_APP_MENU_H_
