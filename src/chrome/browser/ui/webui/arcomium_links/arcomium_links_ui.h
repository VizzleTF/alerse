// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ARCOMIUM_LINKS_ARCOMIUM_LINKS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ARCOMIUM_LINKS_ARCOMIUM_LINKS_UI_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class ArcomiumLinksUI;
class Profile;

inline constexpr char kChromeUIArcomiumLinksHost[] = "arcomium-links";
inline constexpr char kChromeUIArcomiumLinksURL[] = "chrome://arcomium-links/";

class ArcomiumLinksUIConfig
    : public content::DefaultWebUIConfig<ArcomiumLinksUI> {
 public:
  ArcomiumLinksUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           kChromeUIArcomiumLinksHost) {}
};

// chrome://arcomium-links: the link routes that send links from other apps
// to Spaces (see arcomium_link_routes.h).
class ArcomiumLinksUI : public content::WebUIController {
 public:
  explicit ArcomiumLinksUI(content::WebUI* web_ui);
  ArcomiumLinksUI(const ArcomiumLinksUI&) = delete;
  ArcomiumLinksUI& operator=(const ArcomiumLinksUI&) = delete;
  ~ArcomiumLinksUI() override;
};

// The page calls:
//
//   getState()                    -> {spaces: [{id, name}], routes: [...]}
//   addRoute(pattern, spaceId)    -> state
//   removeRoute(pattern)          -> state
class ArcomiumLinksHandler : public content::WebUIMessageHandler {
 public:
  explicit ArcomiumLinksHandler(Profile* profile);
  ArcomiumLinksHandler(const ArcomiumLinksHandler&) = delete;
  ArcomiumLinksHandler& operator=(const ArcomiumLinksHandler&) = delete;
  ~ArcomiumLinksHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleGetState(const base::ListValue& args);
  void HandleAddRoute(const base::ListValue& args);
  void HandleRemoveRoute(const base::ListValue& args);

  base::DictValue BuildState() const;

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ARCOMIUM_LINKS_ARCOMIUM_LINKS_UI_H_
