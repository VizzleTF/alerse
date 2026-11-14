// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_LINK_ROUTES_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_LINK_ROUTES_H_

// Alerse link routing. A link route sends links that other apps open to a
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
#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/alerse_import/alerse_import_data.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace alerse {

// Local State pref, shared by every Space: a list of {"pattern", "space",
// "name"}. The profile pref "arcomium.link_routes" of 0.1 named storage
// partitions and is not read.
inline constexpr char kLinkRoutesPref[] = "alerse.link_routes";

struct LinkRoute {
  std::string pattern;
  // The Space: its profile's directory name ("Profile 1"); empty for Home.
  std::string space;
  // The Space's name when the route was made, shown once the Space is gone.
  std::string name;
};

// How a route names the Space whose profile lives in `path`.
inline std::string LinkRouteSpaceOf(const base::FilePath& path) {
  const std::string name = path.BaseName().AsUTF8Unsafe();
  return name == "Default" ? std::string() : name;
}

// Whether Peek and the Little window are on (alerse_peek.h): Local State
// prefs shared by every Space, set on the Link routing page; both on, as in
// Arc.
inline constexpr char kPeekPref[] = "alerse.peek";
inline constexpr char kLittleWindowPref[] = "alerse.little_window";

// The pages' names of the two switches: "peek" and "littleWindow".
inline const char* LinkOptionPref(std::string_view name) {
  if (name == "peek") {
    return kPeekPref;
  }
  if (name == "littleWindow") {
    return kLittleWindowPref;
  }
  return nullptr;
}

// {peek, littleWindow}, for the pages.
inline base::DictValue LinkOptions() {
  PrefService* local_state = g_browser_process->local_state();
  return base::DictValue()
      .Set("peek", local_state && local_state->GetBoolean(kPeekPref))
      .Set("littleWindow",
           local_state && local_state->GetBoolean(kLittleWindowPref));
}

// Sets a switch the pages name; false for a name that is none.
inline bool SetLinkOption(std::string_view name, bool on) {
  const char* pref = LinkOptionPref(name);
  PrefService* local_state = g_browser_process->local_state();
  if (!pref || !local_state) {
    return false;
  }
  local_state->SetBoolean(pref, on);
  return true;
}

inline void RegisterLinkRoutesLocalState(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kLinkRoutesPref);
  registry->RegisterBooleanPref(kPeekPref, true);
  registry->RegisterBooleanPref(kLittleWindowPref, true);
}

inline void RegisterLinkRoutesPrefs(user_prefs::PrefRegistrySyncable* registry) {
  // chrome://alerse-import's prefs, local to this profile, registered here:
  // this is the call browser_prefs.cc makes for Alerse's profile prefs.
  registry->RegisterDictionaryPref(alerse_import::kImportFoldersPref);
  registry->RegisterStringPref(alerse_import::kImportKeyPref, std::string());
}

inline std::vector<LinkRoute> GetLinkRoutes() {
  std::vector<LinkRoute> routes;
  for (const base::Value& value :
       g_browser_process->local_state()->GetList(kLinkRoutesPref)) {
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

inline void SetLinkRoutes(const std::vector<LinkRoute>& routes) {
  base::ListValue list;
  for (const LinkRoute& route : routes) {
    list.Append(base::DictValue()
                    .Set("pattern", route.pattern)
                    .Set("space", route.space)
                    .Set("name", route.name));
  }
  g_browser_process->local_state()->SetList(kLinkRoutesPref, std::move(list));
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

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_LINK_ROUTES_H_
