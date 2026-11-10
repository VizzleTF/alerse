// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class AlerseImportUI;

inline constexpr char kChromeUIAlerseImportHost[] = "alerse-import";
inline constexpr char kChromeUIAlerseImportURL[] =
    "chrome://alerse-import/";

class AlerseImportUIConfig
    : public content::DefaultWebUIConfig<AlerseImportUI> {
 public:
  AlerseImportUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           kChromeUIAlerseImportHost) {}

  // content::WebUIConfig: only in a regular profile. Incognito and Guest
  // would import into the bookmarks of the profile they come from.
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// chrome://alerse-import: imports Arc's Spaces and the bookmarks of other
// Chromium-based browsers found on this computer.
class AlerseImportUI : public content::WebUIController {
 public:
  explicit AlerseImportUI(content::WebUI* web_ui);
  AlerseImportUI(const AlerseImportUI&) = delete;
  AlerseImportUI& operator=(const AlerseImportUI&) = delete;
  ~AlerseImportUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_UI_H_
