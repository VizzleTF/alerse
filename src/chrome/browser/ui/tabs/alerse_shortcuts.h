// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_SHORTCUTS_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_SHORTCUTS_H_

// The user's own keyboard shortcuts, set on chrome://alerse-shortcuts. On
// Windows and Linux they replace the keys of the accelerator table
// (accelerator_table.cc applies them); macOS keeps its own App Shortcuts in
// System Settings, which the page opens.

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace alerse {

// Local state: {"<command id>": {"key": <ui::KeyboardCode>, "mods": <flags>}};
// key 0 takes the command's key away.
inline constexpr char kShortcutsPref[] = "alerse.shortcuts";

struct ShortcutCommand {
  int command_id;
  int name_id;
};

// The commands the page lists, in its order.
inline constexpr ShortcutCommand kShortcutCommands[] = {
    {IDC_NEW_TAB, IDS_ALERSE_MENU_NEW_TAB},
    {IDC_NEW_WINDOW, IDS_ALERSE_MENU_NEW_WINDOW},
    {IDC_NEW_INCOGNITO_WINDOW, IDS_ALERSE_MENU_NEW_INCOGNITO},
    {IDC_ALERSE_NEW_LITTLE_WINDOW, IDS_ALERSE_NEW_LITTLE_WINDOW},
    {IDC_RESTORE_TAB, IDS_ALERSE_MENU_REOPEN_TAB},
    {IDC_ALERSE_PIN_TAB, IDS_ALERSE_MENU_PIN_TAB},
    {IDC_COPY_URL, IDS_ALERSE_MENU_COPY_URL},
    {IDC_ALERSE_COPY_URL_MARKDOWN, IDS_ALERSE_COPY_URL_MARKDOWN},
    {IDC_NEW_SPLIT_TAB, IDS_ALERSE_ADD_SPLIT_VIEW},
    {IDC_TOGGLE_VERTICAL_TABS_COLLAPSE, IDS_ALERSE_TOGGLE_SIDEBAR},
    {IDC_ALERSE_NEW_SPACE, IDS_ALERSE_MENU_NEW_SPACE},
    {IDC_ALERSE_NEXT_SPACE, IDS_ALERSE_MENU_NEXT_SPACE},
    {IDC_ALERSE_PREVIOUS_SPACE, IDS_ALERSE_MENU_PREVIOUS_SPACE},
    {IDC_ALERSE_NEW_FOLDER, IDS_ALERSE_MENU_NEW_FOLDER},
    {IDC_ALERSE_CLEAR_TODAY, IDS_ALERSE_MENU_CLEAR_TODAY},
    {IDC_SELECT_NEXT_TAB, IDS_ALERSE_MENU_NEXT_TAB},
    {IDC_SELECT_PREVIOUS_TAB, IDS_ALERSE_MENU_PREVIOUS_TAB},
    {IDC_BACK, IDS_ALERSE_MENU_GO_BACK},
    {IDC_FORWARD, IDS_ALERSE_MENU_GO_FORWARD},
    {IDC_ALERSE_BOOST_SITE, IDS_ALERSE_BOOST_SITE},
    {IDC_SHOW_HISTORY, IDS_ALERSE_MENU_VIEW_HISTORY},
    {IDC_SHOW_DOWNLOADS, IDS_ALERSE_MENU_DOWNLOADS},
    {IDC_OPTIONS, IDS_ALERSE_MENU_SETTINGS},
    {IDC_PRINT, IDS_ALERSE_MENU_PRINT},
    {IDC_SAVE_PAGE, IDS_ALERSE_MENU_SAVE_PAGE},
    {IDC_DEV_TOOLS, IDS_ALERSE_MENU_DEV_TOOLS},
};

inline bool IsShortcutCommand(int command_id) {
  return std::ranges::any_of(kShortcutCommands, [&](const ShortcutCommand& c) {
    return c.command_id == command_id;
  });
}

// Run when the user changes a shortcut: browser windows load their keys
// again.
inline base::RepeatingClosureList& ShortcutsChangedCallbacks() {
  static base::NoDestructor<base::RepeatingClosureList> callbacks;
  return *callbacks;
}

inline void RegisterShortcutsLocalState(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kShortcutsPref);
}

// Applies the user's shortcuts to `list` (AcceleratorMapping's: keycode,
// modifiers, command_id, action_id): a command the user set loses its own
// keys, and a key the user gave a command leaves the command that had it.
template <typename Mapping>
void ApplyShortcuts(std::vector<Mapping>& list) {
  PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : nullptr;
  if (!local_state || !local_state->FindPreference(kShortcutsPref)) {
    return;
  }
  for (const auto [id_text, value] : local_state->GetDict(kShortcutsPref)) {
    int command_id = 0;
    if (!base::StringToInt(id_text, &command_id) ||
        !IsShortcutCommand(command_id) || !value.is_dict()) {
      continue;
    }
    Mapping mapping{};
    if (auto it = std::ranges::find(list, command_id, &Mapping::command_id);
        it != list.end()) {
      mapping = *it;
    }
    std::erase_if(list, [&](const Mapping& m) {
      return m.command_id == command_id;
    });
    const int key = value.GetDict().FindInt("key").value_or(0);
    if (key <= 0 || key > 0xFF) {
      continue;
    }
    const int mods = value.GetDict().FindInt("mods").value_or(0);
    std::erase_if(list, [&](const Mapping& m) {
      return m.keycode == key && m.modifiers == mods;
    });
    mapping.keycode = static_cast<ui::KeyboardCode>(key);
    mapping.modifiers = mods;
    mapping.command_id = command_id;
    list.push_back(std::move(mapping));
  }
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_SHORTCUTS_H_
