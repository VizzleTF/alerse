// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_WEBKIT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_WEBKIT_UI_H_

#include "chrome/browser/ui/webui/alerse_webkit/alerse_protected_video.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class AlerseWebKitUI;

class AlerseWebKitUIConfig : public content::DefaultWebUIConfig<AlerseWebKitUI> {
 public:
  AlerseWebKitUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           alerse::kChromeUIAlerseWebKitHost) {}
};

// chrome://alerse-webkit/?u=<site>: a streaming site with protected video.
// On macOS it plays in WebKit under a bar; elsewhere the page says why it
// cannot play and offers another browser (alerse_protected_video.h).
class AlerseWebKitUI : public content::WebUIController {
 public:
  explicit AlerseWebKitUI(content::WebUI* web_ui);
  AlerseWebKitUI(const AlerseWebKitUI&) = delete;
  AlerseWebKitUI& operator=(const AlerseWebKitUI&) = delete;
  ~AlerseWebKitUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_WEBKIT_UI_H_
