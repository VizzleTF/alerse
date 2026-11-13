// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_webkit/alerse_protected_video.h"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/referrer.h"
#include "net/base/url_util.h"
#include "ui/base/page_transition_types.h"

namespace alerse {

namespace {

// Streaming services that play only protected video in a browser, by host
// and its subdomains. Spotify and Tidal are their web players' hosts: their
// other pages are ordinary.
constexpr std::string_view kProtectedVideoHosts[] = {
    "netflix.com",        "disneyplus.com",    "primevideo.com",
    "max.com",            "hbomax.com",        "hulu.com",
    "peacocktv.com",      "paramountplus.com", "tv.apple.com",
    "crunchyroll.com",    "open.spotify.com",  "listen.tidal.com",
    "hd.kinopoisk.ru",    "ivi.ru",            "okko.tv",
    "start.ru",           "wink.ru",           "premier.one",
};

// The hosts the user opened in Chromium in a tab: the page's "Open in
// Chromium" keeps a host there until the tab closes.
class AllowedHosts : public content::WebContentsUserData<AllowedHosts> {
 public:
  ~AllowedHosts() override = default;
  std::set<std::string, std::less<>>& hosts() { return hosts_; }

 private:
  explicit AllowedHosts(content::WebContents* contents)
      : content::WebContentsUserData<AllowedHosts>(*contents) {}
  friend class content::WebContentsUserData<AllowedHosts>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  std::set<std::string, std::less<>> hosts_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AllowedHosts);

bool IsAllowed(content::WebContents* contents, const GURL& url) {
  AllowedHosts* allowed = AllowedHosts::FromWebContents(contents);
  return allowed && allowed->hosts().contains(url.host());
}

class ProtectedVideoThrottle : public content::NavigationThrottle {
 public:
  using content::NavigationThrottle::NavigationThrottle;

  ThrottleCheckResult WillStartRequest() override { return Check(); }
  ThrottleCheckResult WillRedirectRequest() override { return Check(); }
  const char* GetNameForLogging() override {
    return "AlerseProtectedVideoThrottle";
  }

 private:
  ThrottleCheckResult Check() {
    const GURL& url = navigation_handle()->GetURL();
    content::WebContents* contents = navigation_handle()->GetWebContents();
    if (!IsProtectedVideoSite(url) || IsAllowed(contents, url)) {
      return PROCEED;
    }
    // Posted: a navigation cannot start another while it is being decided.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<content::WebContents> contents,
                          GURL page) {
                         if (contents) {
                           contents->GetController().LoadURL(
                               page, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());
                         }
                       },
                       contents->GetWeakPtr(), WebKitPageUrl(url)));
    return CANCEL_AND_IGNORE;
  }
};

}  // namespace

bool IsProtectedVideoSite(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  const std::string_view host = url.host();
  for (std::string_view site : kProtectedVideoHosts) {
    if (host == site ||
        (host.size() > site.size() && base::EndsWith(host, site) &&
         host[host.size() - site.size() - 1] == '.')) {
      return true;
    }
  }
  return false;
}

GURL WebKitPageUrl(const GURL& site) {
  return GURL(std::string("chrome://") + kChromeUIAlerseWebKitHost + "/?u=" +
              base::EscapeQueryParamValue(site.spec(), /*use_plus=*/true));
}

GURL SiteOfWebKitPage(const GURL& page) {
  std::string value;
  if (!net::GetValueForKeyInQuery(page, "u", &value)) {
    return GURL();
  }
  GURL site(value);
  return site.SchemeIsHTTPOrHTTPS() ? site : GURL();
}

void AllowInChromium(content::WebContents* contents, const GURL& site) {
  AllowedHosts::CreateForWebContents(contents);
  AllowedHosts::FromWebContents(contents)->hosts().insert(std::string(site.host()));
}

void MaybeAddProtectedVideoThrottle(
    content::NavigationThrottleRegistry& registry) {
  if (registry.GetNavigationHandle().IsInPrimaryMainFrame()) {
    registry.AddThrottle(std::make_unique<ProtectedVideoThrottle>(registry));
  }
}

}  // namespace alerse
