// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_SPACE_PAGES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_SPACE_PAGES_HANDLER_H_

// The Space picker of History and Download history. Each Space is a profile
// with a history and downloads of its own (docs/adr/0001-space-profiles.md),
// so the page lists the other Spaces and a click shows the same page in that
// Space, in the place of the window's shown Space.

#include <algorithm>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/alerse_space_registry.h"
#include "chrome/browser/ui/tabs/alerse_window_stack.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace alerse {

class SpacePagesHandler : public content::WebUIMessageHandler {
 public:
  explicit SpacePagesHandler(GURL page) : page_(std::move(page)) {}

  // Adds "alerseSpaces", the Spaces other than `profile`'s as a JSON list of
  // {name, label, path}; `label` is the page's title in that Space.
  static void AddSpaces(content::WebUIDataSource* source,
                        Profile* profile,
                        int title_id) {
    base::ListValue spaces;
    for (const SpaceInfo& space : ListSpaces()) {
      if (space.path == profile->GetOriginalProfile()->GetPath()) {
        continue;
      }
      spaces.Append(
          base::DictValue()
              .Set("name", space.name)
              .Set("label", l10n_util::GetStringFUTF16(
                                IDS_ALERSE_IN_SPACE,
                                l10n_util::GetStringUTF16(title_id),
                                space.name))
              .Set("path", space.path.AsUTF8Unsafe()));
    }
    source->AddString("alerseSpaces", base::WriteJson(spaces).value_or("[]"));
  }

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "alerseShowSpacePage",
        base::BindRepeating(&SpacePagesHandler::OnShowSpacePage,
                            base::Unretained(this)));
  }

 private:
  void OnShowSpacePage(const base::ListValue& args) {
    if (args.size() != 1 || !args[0].is_string()) {
      return;
    }
    // Only a Space of the list: the path comes from the page.
    const base::FilePath path =
        base::FilePath::FromUTF8Unsafe(args[0].GetString());
    if (std::ranges::none_of(ListSpaces(), [&](const SpaceInfo& space) {
          return space.path == path;
        })) {
      return;
    }
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_ui()->GetWebContents());
    BrowserWindowInterface* browser =
        tab ? tab->GetBrowserWindowInterface() : nullptr;
    if (!browser || browser->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
        browser->GetProfile()->IsOffTheRecord()) {
      return;
    }
    ShowSpaceInStack(browser, path,
                     base::BindOnce(
                         [](GURL page, BrowserWindowInterface* member) {
                           if (!member) {
                             return;
                           }
                           // The Space's own page if it has one open.
                           NavigateParams params(
                               member, page, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
                           params.disposition =
                               WindowOpenDisposition::SINGLETON_TAB;
                           Navigate(&params);
                         },
                         page_));
  }

  const GURL page_;
};

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_SPACE_PAGES_HANDLER_H_
