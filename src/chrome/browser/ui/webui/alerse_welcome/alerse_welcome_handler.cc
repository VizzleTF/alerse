// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_welcome/alerse_welcome_handler.h"

#include <array>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/tabs/alerse_space_registry.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_ui.h"

namespace {

// The engines the page offers, by their prepopulated id
// (third_party/search_engines_data/resources/definitions): private ones
// first, Google and Bing last, so that Google is a choice and not a default.
struct Engine {
  int prepopulate_id;
  const char* name;
  const char* note;
};
constexpr auto kEngines = std::to_array<Engine>({
    {92, "DuckDuckGo", "No tracking, no profile of you"},
    {109, "Brave Search", "Its own index, no profiling"},
    {113, "Startpage", "Google's results without Google's tracking"},
    {101, "Ecosia", "Plants trees with its ad revenue"},
    {94, "Qwant", "European, no tracking"},
    {1, "Google", "Your searches go to Google"},
    {3, "Bing", "Your searches go to Microsoft"},
});

// Every Space's profile that is loaded: each Space has its own search engine
// and theme, and the first start sets them all.
std::vector<Profile*> SpaceProfiles(Profile* fallback) {
  std::vector<Profile*> profiles;
  for (const alerse::SpaceInfo& space : alerse::ListSpaces()) {
    if (Profile* profile =
            g_browser_process->profile_manager()->GetProfileByPath(
                space.path)) {
      profiles.push_back(profile);
    }
  }
  if (profiles.empty()) {
    profiles.push_back(fallback);
  }
  return profiles;
}

// The profile's engine with `prepopulate_id`, if its list has one: the list
// is the region's, so an offered engine may be missing.
TemplateURL* FindEngine(TemplateURLService* urls, int prepopulate_id) {
  if (!urls) {
    return nullptr;
  }
  for (TemplateURL* url : urls->GetTemplateURLs()) {
    if (url->prepopulate_id() == prepopulate_id) {
      return url;
    }
  }
  return nullptr;
}

}  // namespace

AlerseWelcomeHandler::AlerseWelcomeHandler(Profile* profile)
    : profile_(profile) {}

AlerseWelcomeHandler::~AlerseWelcomeHandler() = default;

void AlerseWelcomeHandler::RegisterMessages() {
  auto add = [this](const char* name,
                    void (AlerseWelcomeHandler::*method)(
                        const base::ListValue&)) {
    web_ui()->RegisterMessageCallback(
        name, base::BindRepeating(method, base::Unretained(this)));
  };
  add("getWelcomeState", &AlerseWelcomeHandler::HandleGetState);
  add("setSearchEngine", &AlerseWelcomeHandler::HandleSetSearchEngine);
  add("setTheme", &AlerseWelcomeHandler::HandleSetTheme);
  add("setDefaultBrowser", &AlerseWelcomeHandler::HandleSetDefaultBrowser);
}

void AlerseWelcomeHandler::HandleGetState(const base::ListValue& args) {
  AllowJavascript();
  TemplateURLService* urls =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* current =
      urls ? urls->GetDefaultSearchProvider() : nullptr;
  base::ListValue engines;
  for (const Engine& engine : kEngines) {
    if (!FindEngine(urls, engine.prepopulate_id)) {
      continue;
    }
    engines.Append(
        base::DictValue()
            .Set("id", engine.prepopulate_id)
            .Set("name", engine.name)
            .Set("note", engine.note)
            .Set("current",
                 current && current->prepopulate_id() == engine.prepopulate_id));
  }
  const char* theme = "system";
  if (ThemeService* themes = ThemeServiceFactory::GetForProfile(profile_)) {
    switch (themes->GetBrowserColorScheme()) {
      case ThemeService::BrowserColorScheme::kLight:
        theme = "light";
        break;
      case ThemeService::BrowserColorScheme::kDark:
        theme = "dark";
        break;
      case ThemeService::BrowserColorScheme::kSystem:
        break;
    }
  }
  const std::string callback_id = args[0].GetString();
  base::DictValue state;
  state.Set("engines", std::move(engines));
  state.Set("theme", theme);
  // Whether Alerse is the default browser comes from the system, in a
  // moment; the page asks for it with setDefaultBrowser(false).
  ResolveJavascriptCallback(base::Value(callback_id), std::move(state));
}

void AlerseWelcomeHandler::HandleSetSearchEngine(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  const int id = args[1].is_int() ? args[1].GetInt() : 0;
  bool known = false;
  for (const Engine& engine : kEngines) {
    known |= engine.prepopulate_id == id;
  }
  if (!known) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value("unknown engine"));
    return;
  }
  for (Profile* profile : SpaceProfiles(profile_)) {
    TemplateURLService* urls =
        TemplateURLServiceFactory::GetForProfile(profile);
    if (TemplateURL* url = FindEngine(urls, id)) {
      urls->SetUserSelectedDefaultSearchProvider(url);
    }
  }
  ResolveJavascriptCallback(base::Value(callback_id), base::Value());
}

void AlerseWelcomeHandler::HandleSetTheme(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  const std::string mode = args[1].is_string() ? args[1].GetString() : "";
  ThemeService::BrowserColorScheme scheme =
      ThemeService::BrowserColorScheme::kSystem;
  if (mode == "light") {
    scheme = ThemeService::BrowserColorScheme::kLight;
  } else if (mode == "dark") {
    scheme = ThemeService::BrowserColorScheme::kDark;
  }
  for (Profile* profile : SpaceProfiles(profile_)) {
    if (ThemeService* themes = ThemeServiceFactory::GetForProfile(profile)) {
      themes->SetBrowserColorScheme(scheme);
    }
  }
  ResolveJavascriptCallback(base::Value(callback_id), base::Value());
}

// [callback, set]: with `set`, asks the system to make Alerse the default
// browser (Windows opens its settings); either way answers whether it is.
void AlerseWelcomeHandler::HandleSetDefaultBrowser(
    const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  const bool set = args.size() > 1 && args[1].is_bool() && args[1].GetBool();
  auto worker = base::MakeRefCounted<shell_integration::DefaultBrowserWorker>();
  auto done = base::BindOnce(
      [](base::WeakPtr<AlerseWelcomeHandler> self, std::string callback_id,
         shell_integration::DefaultWebClientState state) {
        if (!self) {
          return;
        }
        const char* answer = "unknown";
        if (state == shell_integration::IS_DEFAULT) {
          answer = "yes";
        } else if (state == shell_integration::NOT_DEFAULT ||
                   state == shell_integration::OTHER_MODE_IS_DEFAULT) {
          answer = "no";
        }
        self->ResolveJavascriptCallback(base::Value(callback_id),
                                        base::Value(answer));
      },
      weak_factory_.GetWeakPtr(), callback_id);
  if (set) {
    worker->StartSetAsDefault(std::move(done));
  } else {
    worker->StartCheckIsDefault(std::move(done));
  }
}
