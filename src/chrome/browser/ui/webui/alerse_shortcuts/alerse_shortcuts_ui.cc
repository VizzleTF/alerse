// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_shortcuts/alerse_shortcuts_ui.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_table.h"
#include "chrome/browser/ui/tabs/alerse_shortcuts.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#endif

namespace {

#include "chrome/browser/ui/webui/alerse_common/alerse_page_style.inc"
#include "chrome/browser/ui/webui/alerse_shortcuts/alerse_shortcuts_page.inc"

std::string_view FindFile(const std::string& path) {
  if (path.empty() || path == "index.html") {
    return kAlerseShortcutsHtml;
  }
  if (path == "base.css") {
    return kAlersePageBaseCss;
  }
  if (path == "alerse_shortcuts.css") {
    return kAlerseShortcutsCss;
  }
  if (path == "alerse_shortcuts.js") {
    return kAlerseShortcutsJs;
  }
  return {};
}

// macOS takes app shortcuts from System Settings; the keys here are the
// accelerator table's, which Windows and Linux use.
constexpr bool kEditable = !BUILDFLAG(IS_MAC);

// Modifiers a shortcut may carry; others are dropped.
constexpr int kModifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                           ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN;

#if BUILDFLAG(IS_MAC)
// The menu bar's title where it differs from the page's name: App Shortcuts
// in System Settings match a menu item by its exact title. 0 for a command
// the menu bar does not have, which App Shortcuts cannot reach.
int MenuBarNameId(const alerse::ShortcutCommand& command) {
  switch (command.command_id) {
    case IDC_RESTORE_TAB:
      return IDS_ALERSE_MENU_RESTORE_TAB;
    // The title while the sidebar shows; hidden, it reads Show Sidebar.
    case IDC_TOGGLE_VERTICAL_TABS_COLLAPSE:
      return IDS_ALERSE_MENU_HIDE_SIDEBAR;
    // macOS titles Chromium's Preferences… item Settings….
    case IDC_OPTIONS:
      return IDS_ALERSE_MENU_BAR_SETTINGS;
    case IDC_PRINT:
      return IDS_ALERSE_MENU_BAR_PRINT;
    case IDC_ALERSE_BOOST_SITE:
      return 0;
    default:
      return command.name_id;
  }
}
#endif

base::DictValue ShortcutsState() {
#if !BUILDFLAG(IS_MAC)
  const std::vector<AcceleratorMapping> list = GetAcceleratorList();
#endif
  PrefService* local_state = g_browser_process->local_state();
  const base::DictValue& custom = local_state->GetDict(alerse::kShortcutsPref);
  base::ListValue commands;
  for (const alerse::ShortcutCommand& command : alerse::kShortcutCommands) {
    base::ListValue keys;
#if BUILDFLAG(IS_MAC)
    const int name_id = MenuBarNameId(command);
    if (!name_id) {
      continue;
    }
    // The menu bar's key first; App Shortcuts in System Settings override
    // it. Then a key outside the menu, which
    // GetDefaultMacAcceleratorForCommandId() prefers.
    ui::Accelerator menu_key;
    const bool has_menu_key =
        GetMacMenuAcceleratorForCommandId(command.command_id, &menu_key);
    if (has_menu_key) {
      keys.Append(menu_key.GetShortcutText());
    }
    if (ui::Accelerator accelerator;
        GetDefaultMacAcceleratorForCommandId(command.command_id,
                                             &accelerator) &&
        (!has_menu_key || accelerator != menu_key)) {
      keys.Append(accelerator.GetShortcutText());
    }
#else
    const int name_id = command.name_id;
    for (const AcceleratorMapping& mapping : list) {
      // the AltGr copies of Alt keys would read as the bare key
      if (mapping.command_id == command.command_id &&
          !(mapping.modifiers & ui::EF_ALTGR_DOWN)) {
        keys.Append(ui::Accelerator(mapping.keycode, mapping.modifiers)
                        .GetShortcutText());
      }
    }
#endif
    commands.Append(
        base::DictValue()
            .Set("id", command.command_id)
            .Set("name", l10n_util::GetStringUTF16(name_id))
            .Set("keys", std::move(keys))
            .Set("custom", custom.contains(
                               base::NumberToString(command.command_id))));
  }
  return base::DictValue()
      .Set("editable", kEditable)
      .Set("commands", std::move(commands));
}

}  // namespace

AlerseShortcutsUI::AlerseShortcutsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), kChromeUIAlerseShortcutsHost);
  source->SetRequestFilter(
      base::BindRepeating(
          [](const std::string& path) { return !FindFile(path).empty(); }),
      base::BindRepeating(
          [](const std::string& path,
             content::WebUIDataSource::GotDataCallback callback) {
            std::move(callback).Run(
                base::MakeRefCounted<base::RefCountedString>(
                    std::string(FindFile(path))));
          }));
  web_ui->AddMessageHandler(std::make_unique<AlerseShortcutsHandler>());
}

AlerseShortcutsUI::~AlerseShortcutsUI() = default;

AlerseShortcutsHandler::AlerseShortcutsHandler() = default;

AlerseShortcutsHandler::~AlerseShortcutsHandler() = default;

void AlerseShortcutsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getState", base::BindRepeating(&AlerseShortcutsHandler::HandleGetState,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setShortcut",
      base::BindRepeating(&AlerseShortcutsHandler::HandleSetShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetShortcut",
      base::BindRepeating(&AlerseShortcutsHandler::HandleResetShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openSystemShortcuts",
      base::BindRepeating(&AlerseShortcutsHandler::HandleOpenSystemShortcuts,
                          base::Unretained(this)));
}

void AlerseShortcutsHandler::HandleGetState(const base::ListValue& args) {
  AllowJavascript();
  if (!args.empty()) {
    ResolveJavascriptCallback(args[0], ShortcutsState());
  }
}

// [callback, id, keyCode, modifiers].
void AlerseShortcutsHandler::HandleSetShortcut(const base::ListValue& args) {
  AllowJavascript();
  if (args.size() != 4 || !args[1].is_int() || !args[2].is_int() ||
      !args[3].is_int() || !kEditable ||
      !alerse::IsShortcutCommand(args[1].GetInt()) || args[2].GetInt() < 0 ||
      args[2].GetInt() > 0xFF) {
    if (!args.empty()) {
      RejectJavascriptCallback(args[0], base::Value("bad shortcut"));
    }
    return;
  }
  {
    ScopedDictPrefUpdate update(g_browser_process->local_state(),
                                alerse::kShortcutsPref);
    // A key given to this command leaves another command the user set.
    const int key = args[2].GetInt();
    const int mods = args[3].GetInt() & kModifiers;
    if (key) {
      for (auto [id, value] : *update) {
        if (value.is_dict() && value.GetDict().FindInt("key") == key &&
            value.GetDict().FindInt("mods") == mods) {
          value.GetDict().Set("key", 0);
        }
      }
    }
    update->Set(base::NumberToString(args[1].GetInt()),
                base::DictValue().Set("key", key).Set("mods", mods));
  }
  alerse::ShortcutsChangedCallbacks().Notify();
  ResolveJavascriptCallback(args[0], ShortcutsState());
}

// [callback, id]: id 0 resets every command.
void AlerseShortcutsHandler::HandleResetShortcut(const base::ListValue& args) {
  AllowJavascript();
  if (args.size() != 2 || !args[1].is_int()) {
    if (!args.empty()) {
      RejectJavascriptCallback(args[0], base::Value("bad shortcut"));
    }
    return;
  }
  {
    ScopedDictPrefUpdate update(g_browser_process->local_state(),
                                alerse::kShortcutsPref);
    if (args[1].GetInt() == 0) {
      update->clear();
    } else {
      update->Remove(base::NumberToString(args[1].GetInt()));
    }
  }
  alerse::ShortcutsChangedCallbacks().Notify();
  ResolveJavascriptCallback(args[0], ShortcutsState());
}

void AlerseShortcutsHandler::HandleOpenSystemShortcuts(
    const base::ListValue& args) {
#if BUILDFLAG(IS_MAC)
  platform_util::OpenExternal(
      GURL("x-apple.systempreferences:com.apple.Keyboard-Settings.extension"));
#endif
}
