// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_BOOST_ALERSE_BOOSTS_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_BOOST_ALERSE_BOOSTS_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;
class PrefService;

namespace content {
class NavigationThrottleRegistry;
}

namespace alerse {

// The site a boost belongs to: the registrable domain (www.youtube.com and
// m.youtube.com share youtube.com's), else the host; empty for a page that
// is not a site.
std::string BoostSiteOf(const GURL& url);

// The style sheet a boost makes; empty while it or every boost is off.
std::string BoostCss(const base::DictValue& boost);

// The boost of `site`, or none.
const base::DictValue* FindBoost(PrefService* prefs, const std::string& site);
// Replaces the boost of `site`; an empty `boost` removes it.
void SetBoost(PrefService* prefs, const std::string& site,
              base::DictValue boost);

// Gives a tab its boost helper on its first navigation; adds no throttle.
void MaybeAttachBoosts(content::NavigationThrottleRegistry& registry);

// Applies the boost of a tab's site to each of its pages, again whenever the
// boosts change, and runs Zap: picking an element of the page to hide.
class BoostTabHelper : public content::WebContentsObserver,
                       public content::WebContentsUserData<BoostTabHelper> {
 public:
  ~BoostTabHelper() override;

  // The element picker, until a click or Esc. `done` gets the selector of
  // the element clicked, empty for Esc or a page left meanwhile.
  void StartZap(base::OnceCallback<void(const std::string&)> done);
  void CancelZap();
  bool zapping() const { return !zap_done_.is_null(); }

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DOMContentLoaded(content::RenderFrameHost* frame) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

 private:
  explicit BoostTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<BoostTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  void Apply();
  void PollZap();
  void FinishZap(const std::string& selector);

  PrefChangeRegistrar prefs_;
  base::OnceCallback<void(const std::string&)> zap_done_;
  base::RepeatingTimer zap_poll_;
  base::WeakPtrFactory<BoostTabHelper> weak_factory_{this};
};

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_BOOST_ALERSE_BOOSTS_H_
