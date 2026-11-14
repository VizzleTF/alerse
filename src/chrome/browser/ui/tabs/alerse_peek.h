// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_PEEK_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_PEEK_H_

// Peek and the Little window, Arc's two ways to look at a page without a tab
// (alerse_peek.cc in views/tabs/vertical draws them):
//
// - Peek: a link opens in a panel over the page; Open as Tab keeps it as a
//   tab of the window, Esc or a click beside it closes it. Shift+click opens
//   a link in Peek, and so does a click in a pinned tab on a link to another
//   site.
// - Little window: a small window of its own with an address field and Open
//   in Alerse, which moves its page to a tab of the Space. New Little Window
//   (Ctrl+Alt+N, Option-Command-N) opens one, and so does a link from another
//   app that no link route sends to a Space.
//
// Each can be turned off on the Link routing page and on the welcome page.

#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/alerse_link_routes.h"
#include "chrome/browser/ui/tabs/alerse_spaces.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace content {
class NavigationThrottleRegistry;
}

namespace alerse {

inline bool LocalStateFlag(const char* name) {
  PrefService* local_state = g_browser_process->local_state();
  return local_state && local_state->GetBoolean(name);
}

inline bool PeekEnabled() {
  return LocalStateFlag(kPeekPref);
}

inline bool LittleWindowEnabled() {
  return LocalStateFlag(kLittleWindowPref);
}

// Opens `url` in Peek over `browser`'s page.
void ShowPeek(BrowserWindowInterface* browser,
              const GURL& url,
              const content::Referrer& referrer);

// Opens a Little window of `profile` on `url`, or on an empty page with its
// address field focused when `url` is empty.
void ShowLittleWindow(Profile* profile, const GURL& url);

// Adds the throttle that sends a pinned tab's links to other sites to Peek.
void MaybeAddPeekThrottle(content::NavigationThrottleRegistry& registry);

// A tab of a pinned item or a favorite, or one pinned in the tab strip.
inline bool IsPinnedTab(BrowserWindowInterface* browser,
                        content::WebContents* contents) {
  if (!GetPinnedTabUuid(contents).empty()) {
    return true;
  }
  TabStripModel* tabs = browser->GetTabStripModel();
  const int index = tabs->GetIndexOfWebContents(contents);
  return index != TabStripModel::kNoTab && tabs->IsTabPinned(index);
}

// Whether `url` is another site than the page of `contents`.
inline bool IsOtherSite(content::WebContents* contents, const GURL& url) {
  const GURL& from = contents->GetLastCommittedURL();
  return !from.SchemeIsHTTPOrHTTPS() ||
         !net::registry_controlled_domains::SameDomainOrHost(
             url, from,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// Navigate()'s hook: true if Peek took the link. Shift+click asks for a new
// window, and a pinned tab's link for a new tab in front (a target="_blank"
// link, as most sites make theirs, often through a redirect of the site's
// own): with Peek on, either opens in Peek instead. Ctrl+click, a tab in the
// background, stays a tab.
inline bool OpenLinkInPeek(const NavigateParams& params) {
  if (!PeekEnabled() || !params.browser || !params.source_contents ||
      !ui::PageTransitionCoreTypeIs(params.transition,
                                    ui::PAGE_TRANSITION_LINK) ||
      params.browser->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
      !params.url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  const bool shift_click =
      params.disposition == WindowOpenDisposition::NEW_WINDOW;
  const bool pinned_link =
      params.disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB &&
      IsPinnedTab(params.browser, params.source_contents);
  if (!shift_click && !pinned_link) {
    return false;
  }
  ShowPeek(params.browser, params.url, params.referrer);
  return true;
}

// Navigate()'s hook after RouteLinkToSpace(): true if a Little window took a
// link from another app. A link a route names stays a tab of its Space, and
// a window with no tab yet (the browser starting for the link) takes it.
inline bool OpenExternalLinkInLittleWindow(const NavigateParams& params) {
  if (!LittleWindowEnabled() || !params.browser || params.source_contents ||
      params.opener ||
      !ui::PageTransitionCoreTypeIs(params.transition,
                                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL) ||
      (params.disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
       params.disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) ||
      params.browser->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
      params.browser->GetTabStripModel()->empty() ||
      !params.url.SchemeIsHTTPOrHTTPS() ||
      FindLinkRoute(GetLinkRoutes(), params.url)) {
    return false;
  }
  ShowLittleWindow(params.browser->GetProfile(), params.url);
  return true;
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_PEEK_H_
