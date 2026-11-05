// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ARCOMIUM_SYNC_ARCOMIUM_SYNC_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ARCOMIUM_SYNC_ARCOMIUM_SYNC_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class ArcomiumSyncUI;

inline constexpr char kChromeUIArcomiumSyncHost[] = "arcomium-sync";
inline constexpr char kChromeUIArcomiumSyncURL[] = "chrome://arcomium-sync/";

class ArcomiumSyncUIConfig
    : public content::DefaultWebUIConfig<ArcomiumSyncUI> {
 public:
  ArcomiumSyncUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           kChromeUIArcomiumSyncHost) {}
};

// chrome://arcomium-sync: sets up sync with an Arcomium sync server.
class ArcomiumSyncUI : public content::WebUIController {
 public:
  explicit ArcomiumSyncUI(content::WebUI* web_ui);
  ArcomiumSyncUI(const ArcomiumSyncUI&) = delete;
  ArcomiumSyncUI& operator=(const ArcomiumSyncUI&) = delete;
  ~ArcomiumSyncUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ARCOMIUM_SYNC_ARCOMIUM_SYNC_UI_H_
