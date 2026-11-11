// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_LINK_ROUTER_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_LINK_ROUTER_H_

// Sends a link another app opens to the Space its route names
// (alerse_link_routes.h). A Space is a profile with its own window, so the
// link opens in that window, which takes the place of the one shown
// (alerse_window_stack.h).

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/alerse_link_routes.h"
#include "chrome/browser/ui/tabs/alerse_window_stack.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/common/chrome_constants.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace alerse {

// True if it took the navigation `params`: the link opens in another Space.
// Only a link opened from outside the browser is routed: one opened inside
// has a source tab and stays in its Space. Call it before Navigate() makes
// the active tab the source of a navigation that has none.
inline bool RouteLinkToSpace(const NavigateParams& params) {
  if (!params.browser || params.source_contents || params.opener ||
      !ui::PageTransitionCoreTypeIs(params.transition,
                                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL) ||
      (params.tabstrip_add_types & AddTabTypes::ADD_PINNED) ||
      (params.disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
       params.disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) ||
      params.browser->GetProfile()->IsOffTheRecord()) {
    return false;
  }
  const std::optional<LinkRoute> route =
      FindLinkRoute(GetLinkRoutes(), params.url);
  if (!route ||
      route->space == LinkRouteSpaceOf(params.browser->GetProfile()->GetPath())) {
    return false;
  }
  ProfileManager* manager = g_browser_process->profile_manager();
  const base::FilePath path =
      route->space.empty()
          ? manager->user_data_dir().AppendASCII(chrome::kInitialProfile)
          : manager->user_data_dir().Append(
                base::FilePath::FromUTF8Unsafe(route->space));
  // A route to a Space that was deleted: the link opens here.
  if (!manager->GetProfileAttributesStorage().GetProfileAttributesWithPath(
          path)) {
    return false;
  }
  // Posted: the code that hands the link over shows its window once the
  // link is open, which would bring back the member the stack just hid.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](int stack, base::FilePath path, GURL url) {
            if (BrowserWindowInterface* shown =
                    WindowStacks::Get().Shown(stack)) {
              OpenUrlInSpace(shown, path, url);
            }
          },
          WindowStacks::Get().StackOf(params.browser), path, params.url));
  return true;
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_LINK_ROUTER_H_
