// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class AlerseSyncUI;

inline constexpr char kChromeUIAlerseSyncHost[] = "alerse-sync";
inline constexpr char kChromeUIAlerseSyncURL[] = "chrome://alerse-sync/";

class AlerseSyncUIConfig
    : public content::DefaultWebUIConfig<AlerseSyncUI> {
 public:
  AlerseSyncUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           kChromeUIAlerseSyncHost) {}
};

// chrome://alerse-sync: sets up sync with an Alerse sync server.
class AlerseSyncUI : public content::WebUIController {
 public:
  explicit AlerseSyncUI(content::WebUI* web_ui);
  AlerseSyncUI(const AlerseSyncUI&) = delete;
  AlerseSyncUI& operator=(const AlerseSyncUI&) = delete;
  ~AlerseSyncUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_UI_H_
