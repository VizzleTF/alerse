// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_PROTECTED_VIDEO_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_PROTECTED_VIDEO_H_

// Sites whose video is protected (DRM) open in chrome://alerse-webkit.
// Their players need Widevine, which Google licenses only to browsers it
// signs (docs/legal.md). On macOS that page plays them in WebKit, which has
// Apple's FairPlay; elsewhere it offers another browser.

#include <string>

#include "url/gurl.h"

namespace content {
class NavigationThrottleRegistry;
class WebContents;
}  // namespace content

namespace alerse {

inline constexpr char kChromeUIAlerseWebKitHost[] = "alerse-webkit";

// True for a streaming service that plays only protected video.
bool IsProtectedVideoSite(const GURL& url);

// chrome://alerse-webkit/?u=<url>, and the site it names (empty if none).
GURL WebKitPageUrl(const GURL& site);
GURL SiteOfWebKitPage(const GURL& page);

// Lets `site`'s host open in Chromium in this tab: the user chose it.
void AllowInChromium(content::WebContents* contents, const GURL& site);

// Adds the throttle that sends a protected video site to the page.
void MaybeAddProtectedVideoThrottle(
    content::NavigationThrottleRegistry& registry);

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_PROTECTED_VIDEO_H_
