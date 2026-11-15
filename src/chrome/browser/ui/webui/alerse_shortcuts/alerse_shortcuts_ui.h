// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_SHORTCUTS_ALERSE_SHORTCUTS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_SHORTCUTS_ALERSE_SHORTCUTS_UI_H_

#include "base/values.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class AlerseShortcutsUI;

inline constexpr char kChromeUIAlerseShortcutsHost[] = "alerse-shortcuts";
inline constexpr char kChromeUIAlerseShortcutsURL[] =
    "chrome://alerse-shortcuts/";

class AlerseShortcutsUIConfig
    : public content::DefaultWebUIConfig<AlerseShortcutsUI> {
 public:
  AlerseShortcutsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           kChromeUIAlerseShortcutsHost) {}
};

// chrome://alerse-shortcuts: the user's own keyboard shortcuts
// (alerse_shortcuts.h).
class AlerseShortcutsUI : public content::WebUIController {
 public:
  explicit AlerseShortcutsUI(content::WebUI* web_ui);
  AlerseShortcutsUI(const AlerseShortcutsUI&) = delete;
  AlerseShortcutsUI& operator=(const AlerseShortcutsUI&) = delete;
  ~AlerseShortcutsUI() override;
};

// The page calls:
//
//   getState()                        -> {editable, commands: [{id, name,
//                                          keys, custom}]}
//   setShortcut(id, keyCode, mods)    -> state; keyCode 0 takes the key away
//   resetShortcut(id)                 -> state; id 0 resets every command
//   openSystemShortcuts()             (macOS: System Settings' App Shortcuts)
class AlerseShortcutsHandler : public content::WebUIMessageHandler {
 public:
  AlerseShortcutsHandler();
  AlerseShortcutsHandler(const AlerseShortcutsHandler&) = delete;
  AlerseShortcutsHandler& operator=(const AlerseShortcutsHandler&) = delete;
  ~AlerseShortcutsHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleGetState(const base::ListValue& args);
  void HandleSetShortcut(const base::ListValue& args);
  void HandleResetShortcut(const base::ListValue& args);
  void HandleOpenSystemShortcuts(const base::ListValue& args);
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_SHORTCUTS_ALERSE_SHORTCUTS_UI_H_
