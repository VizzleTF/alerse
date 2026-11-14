// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Boosts, Arc's restyling of a site for the user alone: the prefs and the
// panel's entry point. The rest is in chrome/browser/ui/webui/alerse_boost.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_BOOST_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_BOOST_H_

class BrowserWindowInterface;

namespace alerse {

// A profile's boosts: {site: {enabled, hue, dark, font, size, zaps, css}},
// a site being a registrable domain (youtube.com) or a host.
inline constexpr char kBoostsPref[] = "alerse.boosts";
// The switch for every boost of the profile.
inline constexpr char kBoostsEnabledPref[] = "alerse.boosts_enabled";

// Opens the Boost panel for the window's shown tab, over the page's right
// side (alerse_boost_panel.cc). Does nothing for a page that is not a site.
void ShowBoostPanel(BrowserWindowInterface* browser);

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_BOOST_H_
