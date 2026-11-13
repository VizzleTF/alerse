// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_WELCOME_ALERSE_WELCOME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_WELCOME_ALERSE_WELCOME_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class AlerseWelcomeUI;

inline constexpr char kChromeUIAlerseWelcomeHost[] = "alerse-welcome";
inline constexpr char kChromeUIAlerseWelcomeURL[] = "chrome://alerse-welcome/";

class AlerseWelcomeUIConfig
    : public content::DefaultWebUIConfig<AlerseWelcomeUI> {
 public:
  AlerseWelcomeUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           kChromeUIAlerseWelcomeHost) {}

  // content::WebUIConfig: only in a regular profile, as the import page.
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The page the first start opens: where you come from (with the import),
// the search engine, the theme, sync and the default browser.
class AlerseWelcomeUI : public content::WebUIController {
 public:
  explicit AlerseWelcomeUI(content::WebUI* web_ui);
  AlerseWelcomeUI(const AlerseWelcomeUI&) = delete;
  AlerseWelcomeUI& operator=(const AlerseWelcomeUI&) = delete;
  ~AlerseWelcomeUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_WELCOME_ALERSE_WELCOME_UI_H_
