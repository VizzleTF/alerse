// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/arcomium_links/arcomium_links_ui.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/arcomium_link_routes.h"
#include "chrome/browser/ui/tabs/arcomium_spaces.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

#include "chrome/browser/ui/webui/arcomium_common/arcomium_page_style.inc"
#include "chrome/browser/ui/webui/arcomium_links/arcomium_links_page.inc"

std::string_view FindFile(const std::string& path) {
  if (path.empty() || path == "index.html") {
    return kArcomiumLinksHtml;
  }
  if (path == "base.css") {
    return kArcomiumPageBaseCss;
  }
  if (path == "arcomium_links.css") {
    return kArcomiumLinksCss;
  }
  if (path == "arcomium_links.js") {
    return kArcomiumLinksJs;
  }
  return {};
}

struct Space {
  std::string id;
  std::string name;
};

// The Spaces a link can go to: the default one, those open in the profile's
// windows, and those that routes name but have no tabs now.
std::vector<Space> ListSpaces(Profile* profile,
                              const std::vector<arcomium::LinkRoute>& routes) {
  std::vector<Space> spaces = {{std::string(), "Home"}};
  auto known = [&spaces](const std::string& id) {
    return std::ranges::any_of(spaces,
                               [&id](const Space& s) { return s.id == id; });
  };
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (browser->GetProfile() != profile) {
      continue;
    }
    const TabStripModel* model = browser->GetTabStripModel();
    if (!model || !model->group_model()) {
      continue;
    }
    for (const tab_groups::TabGroupId& group :
         model->group_model()->ListTabGroups()) {
      std::optional<std::string> id = arcomium::GetSpaceOfGroup(model, group);
      if (!id || known(*id)) {
        continue;
      }
      const std::u16string& title =
          model->group_model()->GetTabGroup(group)->visual_data()->title();
      spaces.push_back({*id, title.empty() ? "Space" : base::UTF16ToUTF8(title)});
    }
  }
  for (const arcomium::LinkRoute& route : routes) {
    if (!known(route.space)) {
      spaces.push_back({route.space, route.name.empty() ? "Space" : route.name});
    }
  }
  return spaces;
}

}  // namespace

ArcomiumLinksUI::ArcomiumLinksUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, kChromeUIArcomiumLinksHost);
  source->SetRequestFilter(
      base::BindRepeating(
          [](const std::string& path) { return !FindFile(path).empty(); }),
      base::BindRepeating(
          [](const std::string& path,
             content::WebUIDataSource::GotDataCallback callback) {
            std::move(callback).Run(
                base::MakeRefCounted<base::RefCountedString>(
                    std::string(FindFile(path))));
          }));
  web_ui->AddMessageHandler(std::make_unique<ArcomiumLinksHandler>(profile));
}

ArcomiumLinksUI::~ArcomiumLinksUI() = default;

ArcomiumLinksHandler::ArcomiumLinksHandler(Profile* profile)
    : profile_(profile) {}

ArcomiumLinksHandler::~ArcomiumLinksHandler() = default;

void ArcomiumLinksHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getState", base::BindRepeating(&ArcomiumLinksHandler::HandleGetState,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addRoute", base::BindRepeating(&ArcomiumLinksHandler::HandleAddRoute,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeRoute",
      base::BindRepeating(&ArcomiumLinksHandler::HandleRemoveRoute,
                          base::Unretained(this)));
}

base::DictValue ArcomiumLinksHandler::BuildState() const {
  const std::vector<arcomium::LinkRoute> routes =
      arcomium::GetLinkRoutes(*profile_->GetPrefs());
  const std::vector<Space> spaces = ListSpaces(profile_, routes);

  base::ListValue space_list;
  for (const Space& space : spaces) {
    space_list.Append(
        base::DictValue().Set("id", space.id).Set("name", space.name));
  }
  base::ListValue route_list;
  for (const arcomium::LinkRoute& route : routes) {
    // Show the Space's current name; the route keeps the one it was made with.
    std::string name = route.name;
    for (const Space& space : spaces) {
      if (space.id == route.space) {
        name = space.name;
      }
    }
    route_list.Append(base::DictValue()
                          .Set("pattern", route.pattern)
                          .Set("space", route.space)
                          .Set("name", name));
  }
  return base::DictValue()
      .Set("spaces", std::move(space_list))
      .Set("routes", std::move(route_list));
}

void ArcomiumLinksHandler::HandleGetState(const base::ListValue& args) {
  AllowJavascript();
  ResolveJavascriptCallback(args[0], BuildState());
}

void ArcomiumLinksHandler::HandleAddRoute(const base::ListValue& args) {
  AllowJavascript();
  const std::string pattern = arcomium::NormalizeLinkPattern(args[1].GetString());
  const std::string& space = args[2].GetString();
  if (pattern.empty()) {
    RejectJavascriptCallback(
        args[0], base::Value("Enter a site, such as github.com or "
                             "github.com/your-team."));
    return;
  }
  std::vector<arcomium::LinkRoute> routes =
      arcomium::GetLinkRoutes(*profile_->GetPrefs());
  std::string name = "Space";
  for (const Space& s : ListSpaces(profile_, routes)) {
    if (s.id == space) {
      name = s.name;
    }
  }
  // A site has one route: a new one for it replaces the old.
  std::erase_if(routes, [&pattern](const arcomium::LinkRoute& route) {
    return route.pattern == pattern;
  });
  routes.push_back({pattern, space, name});
  std::ranges::sort(routes, {}, &arcomium::LinkRoute::pattern);
  arcomium::SetLinkRoutes(profile_->GetPrefs(), routes);
  ResolveJavascriptCallback(args[0], BuildState());
}

void ArcomiumLinksHandler::HandleRemoveRoute(const base::ListValue& args) {
  AllowJavascript();
  const std::string& pattern = args[1].GetString();
  std::vector<arcomium::LinkRoute> routes =
      arcomium::GetLinkRoutes(*profile_->GetPrefs());
  std::erase_if(routes, [&pattern](const arcomium::LinkRoute& route) {
    return route.pattern == pattern;
  });
  arcomium::SetLinkRoutes(profile_->GetPrefs(), routes);
  ResolveJavascriptCallback(args[0], BuildState());
}
