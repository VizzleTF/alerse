// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_boost/alerse_boost_ui.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/alerse_boost.h"
#include "chrome/browser/ui/webui/alerse_boost/alerse_boosts.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/views/widget/widget.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

#include "chrome/browser/ui/webui/alerse_common/alerse_page_style.inc"
#include "chrome/browser/ui/webui/alerse_boost/alerse_boost_page.inc"

std::string_view FindFile(const std::string& path) {
  if (path.empty() || path == "index.html" || path.starts_with("?")) {
    return kAlerseBoostHtml;
  }
  if (path == "base.css") {
    return kAlersePageBaseCss;
  }
  if (path == "alerse_boost.css") {
    return kAlerseBoostCss;
  }
  if (path == "alerse_boost.js") {
    return kAlerseBoostJs;
  }
  return {};
}

// The panel's window: the bubble the page is shown in.
views::Widget* PanelOf(content::WebUI* web_ui) {
  return views::Widget::GetTopLevelWidgetForNativeView(
      web_ui->GetWebContents()->GetNativeView());
}

}  // namespace

AlerseBoostUI::AlerseBoostUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, kChromeUIAlerseBoostHost);
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
  web_ui->AddMessageHandler(std::make_unique<AlerseBoostHandler>(profile));
}

AlerseBoostUI::~AlerseBoostUI() = default;

AlerseBoostHandler::AlerseBoostHandler(Profile* profile) : profile_(profile) {}

AlerseBoostHandler::~AlerseBoostHandler() {
  if (content::WebContents* tab = TargetTab()) {
    if (auto* helper = alerse::BoostTabHelper::FromWebContents(tab)) {
      helper->CancelZap();
    }
  }
}

void AlerseBoostHandler::RegisterMessages() {
  auto add = [this](const char* name,
                    void (AlerseBoostHandler::*method)(const base::ListValue&)) {
    web_ui()->RegisterMessageCallback(
        name, base::BindRepeating(method, base::Unretained(this)));
  };
  add("getState", &AlerseBoostHandler::HandleGetState);
  add("setBoost", &AlerseBoostHandler::HandleSetBoost);
  add("setEnabled", &AlerseBoostHandler::HandleSetEnabled);
  add("zap", &AlerseBoostHandler::HandleZap);
  add("close", &AlerseBoostHandler::HandleClose);
  add("openList", &AlerseBoostHandler::HandleOpenList);
}

content::WebContents* AlerseBoostHandler::TargetTab() {
  std::string value;
  int id = 0;
  if (!net::GetValueForKeyInQuery(web_ui()->GetWebContents()->GetVisibleURL(),
                                  "tab", &value) ||
      !base::StringToInt(value, &id)) {
    return nullptr;
  }
  tabs::TabInterface* tab = tabs::TabHandle(id).Get();
  return tab ? tab->GetContents() : nullptr;
}

base::DictValue AlerseBoostHandler::BuildState() {
  PrefService* prefs = profile_->GetPrefs();
  content::WebContents* tab = TargetTab();
  const std::string site =
      tab ? alerse::BoostSiteOf(tab->GetLastCommittedURL()) : std::string();
  const base::DictValue* boost = alerse::FindBoost(prefs, site);
  base::ListValue boosts;
  for (const auto [name, value] : prefs->GetDict(alerse::kBoostsPref)) {
    if (const base::DictValue* dict = value.GetIfDict()) {
      boosts.Append(base::DictValue().Set("site", name).Set("boost",
                                                            dict->Clone()));
    }
  }
  return base::DictValue()
      .Set("site", site)
      .Set("hasTab", tab != nullptr)
      .Set("enabled", prefs->GetBoolean(alerse::kBoostsEnabledPref))
      .Set("boost", boost ? boost->Clone() : base::DictValue())
      .Set("boosts", std::move(boosts));
}

void AlerseBoostHandler::HandleGetState(const base::ListValue& args) {
  AllowJavascript();
  ResolveJavascriptCallback(args[0], BuildState());
}

// [callback, site, boost]
void AlerseBoostHandler::HandleSetBoost(const base::ListValue& args) {
  AllowJavascript();
  if (args.size() != 3 || !args[1].is_string() || !args[2].is_dict()) {
    RejectJavascriptCallback(args[0], base::Value("bad arguments"));
    return;
  }
  alerse::SetBoost(profile_->GetPrefs(), args[1].GetString(),
                   args[2].GetDict().Clone());
  ResolveJavascriptCallback(args[0], BuildState());
}

// [callback, on]
void AlerseBoostHandler::HandleSetEnabled(const base::ListValue& args) {
  AllowJavascript();
  if (args.size() != 2 || !args[1].is_bool()) {
    RejectJavascriptCallback(args[0], base::Value("bad arguments"));
    return;
  }
  profile_->GetPrefs()->SetBoolean(alerse::kBoostsEnabledPref,
                                   args[1].GetBool());
  ResolveJavascriptCallback(args[0], BuildState());
}

// [callback]: the panel steps aside while the page is picked from.
void AlerseBoostHandler::HandleZap(const base::ListValue& args) {
  AllowJavascript();
  content::WebContents* tab = TargetTab();
  if (!tab || alerse::BoostSiteOf(tab->GetLastCommittedURL()).empty()) {
    RejectJavascriptCallback(args[0], base::Value("no page"));
    return;
  }
  if (views::Widget* panel = PanelOf(web_ui())) {
    panel->Hide();
  }
  alerse::BoostTabHelper::CreateForWebContents(tab);
  alerse::BoostTabHelper::FromWebContents(tab)->StartZap(
      base::BindOnce(&AlerseBoostHandler::OnZapped, weak_factory_.GetWeakPtr(),
                     args[0].GetString()));
}

void AlerseBoostHandler::OnZapped(std::string callback_id,
                                  const std::string& selector) {
  if (views::Widget* panel = PanelOf(web_ui())) {
    panel->Show();
  }
  ResolveJavascriptCallback(base::Value(callback_id), BuildState());
}

void AlerseBoostHandler::HandleClose(const base::ListValue& args) {
  if (!TargetTab()) {
    return;  // The list in a tab: nothing to close.
  }
  if (views::Widget* panel = PanelOf(web_ui())) {
    panel->Close();
  }
}

void AlerseBoostHandler::HandleOpenList(const base::ListValue& args) {
  content::WebContents* tab = TargetTab();
  if (!tab) {
    return;
  }
  tab->OpenURL(content::OpenURLParams(GURL(kChromeUIAlerseBoostURL),
                                     content::Referrer(),
                                     WindowOpenDisposition::SINGLETON_TAB,
                                     ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                     /*is_renderer_initiated=*/false),
               /*navigation_handle_callback=*/{});
  HandleClose(args);
}
