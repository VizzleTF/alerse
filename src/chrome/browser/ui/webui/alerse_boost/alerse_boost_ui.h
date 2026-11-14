// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_BOOST_ALERSE_BOOST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_BOOST_ALERSE_BOOST_UI_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class AlerseBoostUI;
class Profile;

namespace content {
class WebContents;
}

inline constexpr char kChromeUIAlerseBoostHost[] = "alerse-boost";
inline constexpr char kChromeUIAlerseBoostURL[] = "chrome://alerse-boost/";

class AlerseBoostUIConfig
    : public content::DefaultWebUIConfig<AlerseBoostUI> {
 public:
  AlerseBoostUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           kChromeUIAlerseBoostHost) {}
};

// chrome://alerse-boost: with ?tab=<tab handle>, the Boost panel's editor
// for that tab's site; without, the list of every boost and their switch.
class AlerseBoostUI : public content::WebUIController {
 public:
  explicit AlerseBoostUI(content::WebUI* web_ui);
  AlerseBoostUI(const AlerseBoostUI&) = delete;
  AlerseBoostUI& operator=(const AlerseBoostUI&) = delete;
  ~AlerseBoostUI() override;
};

// The page calls:
//
//   getState()           -> {site, hasTab, enabled, boost, boosts: [...]}
//   setBoost(site, dict) -> state      (an empty dict removes the boost)
//   setEnabled(on)       -> state
//   zap()                -> state, once the element is picked or not
//   close()              the panel closes
//   openList()           chrome://alerse-boost/ opens in a tab
class AlerseBoostHandler : public content::WebUIMessageHandler {
 public:
  explicit AlerseBoostHandler(Profile* profile);
  AlerseBoostHandler(const AlerseBoostHandler&) = delete;
  AlerseBoostHandler& operator=(const AlerseBoostHandler&) = delete;
  ~AlerseBoostHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleGetState(const base::ListValue& args);
  void HandleSetBoost(const base::ListValue& args);
  void HandleSetEnabled(const base::ListValue& args);
  void HandleZap(const base::ListValue& args);
  void HandleClose(const base::ListValue& args);
  void HandleOpenList(const base::ListValue& args);
  void OnZapped(std::string callback_id, const std::string& selector);

  // The tab the panel edits, if it is still there.
  content::WebContents* TargetTab();
  base::DictValue BuildState();

  const raw_ptr<Profile> profile_;
  base::WeakPtrFactory<AlerseBoostHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_BOOST_ALERSE_BOOST_UI_H_
