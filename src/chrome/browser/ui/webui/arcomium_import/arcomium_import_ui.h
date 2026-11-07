// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ARCOMIUM_IMPORT_ARCOMIUM_IMPORT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ARCOMIUM_IMPORT_ARCOMIUM_IMPORT_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class ArcomiumImportUI;

inline constexpr char kChromeUIArcomiumImportHost[] = "arcomium-import";
inline constexpr char kChromeUIArcomiumImportURL[] =
    "chrome://arcomium-import/";

class ArcomiumImportUIConfig
    : public content::DefaultWebUIConfig<ArcomiumImportUI> {
 public:
  ArcomiumImportUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           kChromeUIArcomiumImportHost) {}

  // content::WebUIConfig: only in a regular profile. Incognito and Guest
  // would import into the bookmarks of the profile they come from.
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// chrome://arcomium-import: imports Arc's Spaces and the bookmarks of other
// Chromium-based browsers found on this computer.
class ArcomiumImportUI : public content::WebUIController {
 public:
  explicit ArcomiumImportUI(content::WebUI* web_ui);
  ArcomiumImportUI(const ArcomiumImportUI&) = delete;
  ArcomiumImportUI& operator=(const ArcomiumImportUI&) = delete;
  ~ArcomiumImportUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ARCOMIUM_IMPORT_ARCOMIUM_IMPORT_UI_H_
