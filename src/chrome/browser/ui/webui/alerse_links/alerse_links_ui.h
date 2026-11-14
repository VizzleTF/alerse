// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_LINKS_ALERSE_LINKS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_LINKS_ALERSE_LINKS_UI_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class AlerseLinksUI;
class Profile;

inline constexpr char kChromeUIAlerseLinksHost[] = "alerse-links";
inline constexpr char kChromeUIAlerseLinksURL[] = "chrome://alerse-links/";

class AlerseLinksUIConfig
    : public content::DefaultWebUIConfig<AlerseLinksUI> {
 public:
  AlerseLinksUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           kChromeUIAlerseLinksHost) {}
};

// chrome://alerse-links: the link routes that send links from other apps
// to Spaces (see alerse_link_routes.h).
class AlerseLinksUI : public content::WebUIController {
 public:
  explicit AlerseLinksUI(content::WebUI* web_ui);
  AlerseLinksUI(const AlerseLinksUI&) = delete;
  AlerseLinksUI& operator=(const AlerseLinksUI&) = delete;
  ~AlerseLinksUI() override;
};

// The page calls:
//
//   getState()                    -> {spaces: [{id, name}], routes: [...]}
//   addRoute(pattern, spaceId)    -> state
//   removeRoute(pattern)          -> state
class AlerseLinksHandler : public content::WebUIMessageHandler {
 public:
  explicit AlerseLinksHandler(Profile* profile);
  AlerseLinksHandler(const AlerseLinksHandler&) = delete;
  AlerseLinksHandler& operator=(const AlerseLinksHandler&) = delete;
  ~AlerseLinksHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleGetState(const base::ListValue& args);
  void HandleAddRoute(const base::ListValue& args);
  void HandleRemoveRoute(const base::ListValue& args);
  void HandleSetLinkOption(const base::ListValue& args);

  base::DictValue BuildState() const;

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_LINKS_ALERSE_LINKS_UI_H_
