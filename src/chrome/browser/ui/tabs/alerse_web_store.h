// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_WEB_STORE_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_WEB_STORE_H_

// No Google unless chosen. The Chrome Web Store, Google's, is off until the
// user turns it on: in the welcome page, the Extensions menu, or by adding an
// extension from the store. While it is off, Alerse checks nothing with the
// store's update server, so store extensions do not update and synced ones do
// not install. An install from before the switch keeps the store on, as its
// extensions update from it.
//
// The search engine picked in the welcome page goes to every Space, also to
// one made later; without a pick the default is DuckDuckGo
// (defaults/no-google-default.patch).

#include <map>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/common/extension_urls.h"

namespace alerse {

// Local state: the store switch; the engine picked in the welcome page (its
// prepopulated id, 0 for none); and, per Space folder, the pick it was given.
inline constexpr char kWebStorePref[] = "alerse.web_store";
inline constexpr char kSearchChoicePref[] = "alerse.search_engine";
inline constexpr char kSearchChoiceAppliedPref[] =
    "alerse.search_engine_applied";

inline void RegisterWebStoreLocalState(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kWebStorePref, false);
  registry->RegisterIntegerPref(kSearchChoicePref, 0);
  registry->RegisterDictionaryPref(kSearchChoiceAppliedPref);
}

inline bool IsWebStoreOn() {
  return extension_urls::IsAlerseWebStoreOn();
}

inline void SetWebStoreOn(bool on) {
  extension_urls::SetAlerseWebStoreOn(on);
}

// At start, before the extension updater runs. `turned_on` runs when the
// switch goes on, to check for updates at once: synced extensions install and
// the others catch up.
inline void InitWebStore(PrefService* local_state,
                         bool first_run,
                         base::RepeatingClosure turned_on) {
  if (!local_state->HasPrefPath(kWebStorePref)) {
    // an install from before 0.1.20 already has Spaces; --no-first-run
    // alone does not make a new user data dir an old install
    local_state->SetBoolean(
        kWebStorePref,
        !first_run && local_state->HasPrefPath(prefs::kProfileAttributes));
  }
  extension_urls::SetAlerseWebStoreOn(local_state->GetBoolean(kWebStorePref));
  extension_urls::SetAlerseWebStoreObserver(base::BindRepeating(
      [](base::RepeatingClosure turned_on, bool on) {
        if (!g_browser_process) {
          return;
        }
        g_browser_process->local_state()->SetBoolean(kWebStorePref, on);
        if (on) {
          turned_on.Run();
        }
      },
      std::move(turned_on)));
}

// The engine of `urls` with `prepopulate_id`, if its list has it.
inline TemplateURL* FindPrepopulatedEngine(TemplateURLService* urls,
                                           int prepopulate_id) {
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

// Gives `profile`'s Space the engine picked in the welcome page, once: a
// change made later in that Space's settings stays.
inline void ApplySearchChoice(Profile* profile) {
  PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : nullptr;
  if (!profile || profile->IsOffTheRecord() || !local_state) {
    return;
  }
  const int id = local_state->GetInteger(kSearchChoicePref);
  const std::string key = profile->GetBaseName().AsUTF8Unsafe();
  if (!id ||
      local_state->GetDict(kSearchChoiceAppliedPref).FindInt(key) == id) {
    return;
  }
  TemplateURLService* urls = TemplateURLServiceFactory::GetForProfile(profile);
  if (!urls) {
    return;
  }
  if (!urls->loaded()) {
    // Once loaded; the subscription goes with the next call for the profile.
    static base::NoDestructor<std::map<Profile*, base::CallbackListSubscription>>
        waiting;
    (*waiting)[profile] = urls->RegisterOnLoadedCallback(base::BindOnce(
        [](base::WeakPtr<Profile> profile) {
          if (profile) {
            ApplySearchChoice(profile.get());
          }
        },
        profile->GetWeakPtr()));
    urls->Load();
    return;
  }
  if (TemplateURL* url = FindPrepopulatedEngine(urls, id)) {
    urls->SetUserSelectedDefaultSearchProvider(url);
  }
  ScopedDictPrefUpdate(local_state, kSearchChoiceAppliedPref)->Set(key, id);
}

// The welcome page's pick, for every Space open now and every one to come.
inline void SetSearchChoice(int prepopulate_id) {
  g_browser_process->local_state()->SetInteger(kSearchChoicePref,
                                               prepopulate_id);
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    ApplySearchChoice(profile);
  }
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_WEB_STORE_H_
