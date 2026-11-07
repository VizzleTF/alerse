// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ARCOMIUM_LINK_ROUTES_H_
#define CHROME_BROWSER_UI_TABS_ARCOMIUM_LINK_ROUTES_H_

// Arcomium link routing. A link route sends links that other apps open to a
// Space: "github.com -> Work" opens every GitHub link from a chat or mail app
// in the Work Space, with its logins. A pattern is a site, optionally with a
// path: "github.com" matches github.com and its subdomains, "github.com/acme"
// only paths under /acme. The longest matching pattern wins. Links opened
// inside the browser stay in the Space they were opened from.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_util.h"
#include "chrome/browser/ui/tabs/arcomium_spaces.h"
#include "chrome/browser/ui/webui/arcomium_import/arcomium_import_data.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace arcomium {

// Profile pref: a list of {"pattern", "space", "name"}.
inline constexpr char kLinkRoutesPref[] = "arcomium.link_routes";

struct LinkRoute {
  std::string pattern;
  // The Space's id; empty for the default Space, the tabs outside Spaces.
  std::string space;
  // The Space's name when the route was made, to bring the Space back if it
  // has no tabs left.
  std::string name;
};

inline void RegisterLinkRoutesPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kLinkRoutesPref);
  // chrome://arcomium-import's prefs, local to this profile, registered here:
  // this is the call browser_prefs.cc makes for Arcomium's profile prefs.
  registry->RegisterDictionaryPref(arcomium_import::kImportFoldersPref);
  registry->RegisterStringPref(arcomium_import::kImportKeyPref, std::string());
}

inline std::vector<LinkRoute> GetLinkRoutes(const PrefService& prefs) {
  std::vector<LinkRoute> routes;
  for (const base::Value& value : prefs.GetList(kLinkRoutesPref)) {
    const base::DictValue* dict = value.GetIfDict();
    if (!dict) {
      continue;
    }
    const std::string* pattern = dict->FindString("pattern");
    const std::string* space = dict->FindString("space");
    const std::string* name = dict->FindString("name");
    if (pattern && space && !pattern->empty()) {
      routes.push_back({*pattern, *space, name ? *name : std::string()});
    }
  }
  return routes;
}

inline void SetLinkRoutes(PrefService* prefs,
                          const std::vector<LinkRoute>& routes) {
  base::ListValue list;
  for (const LinkRoute& route : routes) {
    list.Append(base::DictValue()
                    .Set("pattern", route.pattern)
                    .Set("space", route.space)
                    .Set("name", route.name));
  }
  prefs->SetList(kLinkRoutesPref, std::move(list));
}

// Reads a pattern as people type it ("https://www.GitHub.com/acme/") into
// its stored form ("github.com/acme"). Returns "" if it names no site.
inline std::string NormalizeLinkPattern(std::string_view typed) {
  std::string text(base::TrimWhitespaceASCII(typed, base::TRIM_ALL));
  if (text.find("://") == std::string::npos) {
    text = "https://" + text;
  }
  const GURL url(text);
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || url.host().empty()) {
    return std::string();
  }
  // A site has a dot in its name, unless it is an address or localhost:
  // "github" alone is a typo.
  if (!url.HostIsIPAddress() && url.host() != "localhost" &&
      url.host().find('.') == std::string::npos) {
    return std::string();
  }
  std::string host = base::ToLowerASCII(url.host());
  if (host.starts_with("www.")) {
    host = host.substr(4);
  }
  std::string path(url.path());
  while (path.ends_with('/')) {
    path.pop_back();
  }
  return host + path;
}

inline bool LinkPatternMatches(std::string_view pattern, const GURL& url) {
  const size_t slash = pattern.find('/');
  const std::string_view host = pattern.substr(0, slash);
  const std::string_view path =
      slash == std::string_view::npos ? std::string_view() : pattern.substr(slash);
  std::string url_host = base::ToLowerASCII(url.host());
  if (url_host.starts_with("www.")) {
    url_host = url_host.substr(4);
  }
  if (url_host != host && !url_host.ends_with("." + std::string(host))) {
    return false;
  }
  if (path.empty()) {
    return true;
  }
  // A path matches at segment boundaries: /acme matches /acme/x, not /acmes.
  const std::string url_path(url.path());
  return url_path.starts_with(path) &&
         (url_path.size() == path.size() || url_path[path.size()] == '/');
}

// The group of `space` in `model`, or nullopt: a Space without tabs has
// none.
inline std::optional<tab_groups::TabGroupId> FindGroupOfSpace(
    const TabStripModel* model,
    const std::string& space) {
  if (!model->group_model()) {
    return std::nullopt;
  }
  for (const tab_groups::TabGroupId& group :
       model->group_model()->ListTabGroups()) {
    if (GetSpaceOfGroup(model, group) == space) {
      return group;
    }
  }
  return std::nullopt;
}

// The route for a link opened from another app, if one matches.
inline std::optional<LinkRoute> FindLinkRoute(
    const std::vector<LinkRoute>& routes,
    const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }
  const LinkRoute* best = nullptr;
  for (const LinkRoute& route : routes) {
    if (LinkPatternMatches(route.pattern, url) &&
        (!best || route.pattern.size() > best->pattern.size())) {
      best = &route;
    }
  }
  return best ? std::optional<LinkRoute>(*best) : std::nullopt;
}

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_TABS_ARCOMIUM_LINK_ROUTES_H_
