// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_WEBKIT_HOST_MAC_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_WEBKIT_HOST_MAC_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// A WebKit view over a tab's page, below the page's bar of `top_inset`
// points: it plays protected video with Apple's FairPlay. Each Space keeps
// its own WebKit cookies and storage; an incognito tab keeps none.
class AlerseWebKitHost {
 public:
  struct State {
    std::u16string title;
    GURL url;
    bool can_go_back = false;
    bool loading = false;
  };
  using Changed = base::RepeatingCallback<void(const State&)>;

  AlerseWebKitHost(content::WebContents* contents,
                   const GURL& url,
                   int top_inset,
                   Changed changed);
  AlerseWebKitHost(const AlerseWebKitHost&) = delete;
  AlerseWebKitHost& operator=(const AlerseWebKitHost&) = delete;
  ~AlerseWebKitHost();

  void GoBack();
  void Reload();

 private:
  struct Views;
  std::unique_ptr<Views> views_;
};

// Opens `url` in Safari, whichever browser is the default.
void AlerseOpenInSafari(const GURL& url);

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_WEBKIT_HOST_MAC_H_
