// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_SPACE_LIST_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_SPACE_LIST_H_

// The list of Spaces that the user's devices share
// (docs/adr/0001-space-profiles.md, Sync). Each Space syncs as a chain of
// its own, named by the Space's id, so a device learns which Spaces exist
// from home's chain: home's Space folder carries the list as bookmark meta
// info, which syncs with home's bookmarks. A device that reads an id it has
// no profile for makes one, which then joins that Space's chain.

#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/tabs/alerse_space_folder.h"
#include "chrome/browser/ui/tabs/alerse_space_registry.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/alerse_sync_account.h"

namespace alerse {

// Meta info of home's Space folder: JSON object {Space id: {"name", "icon",
// "color"}}, or {"deleted": true} for a Space some device deleted. The name,
// icon and color are what the Space had when it was listed: they label it
// until its own chain brings the current ones.
inline constexpr char kSpaceListMetaKey[] = "arcomium.spaces";

// The Spaces whose profiles are being made from the list.
inline std::set<std::string>& SpacesBeingMade() {
  static base::NoDestructor<std::set<std::string>> spaces;
  return *spaces;
}

// Records the id of the Space of `profile` in its registry entry, where the
// list finds it without loading the profile.
inline void RecordSpaceId(Profile* profile) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  const std::string id =
      syncer::AlerseSyncAccount::GetSpaceId(*profile->GetPrefs());
  if (entry && !id.empty() && entry->GetAlerseValue(kSpaceEntryIdKey) != id) {
    entry->SetAlerseValue(kSpaceEntryIdKey, id);
    // Home's window lists it.
    SpacesChangedCallbacks().Notify();
  }
}

// Tells the list that the Space in `path` is being deleted, so that its
// other devices' copies of the list do not bring it back here.
inline void RecordSpaceDeleted(const base::FilePath& path) {
  const ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(path);
  const std::string id =
      entry ? entry->GetAlerseValue(kSpaceEntryIdKey) : std::string();
  if (!id.empty()) {
    syncer::AlerseSyncAccount::AddDeletedSpace(g_browser_process->local_state(),
                                               id);
  }
}

// Makes a profile for the Space `id` of the list.
inline void MakeSpaceOfList(const std::string& id,
                            const std::string& name,
                            const std::string& icon,
                            const std::string& color) {
  ProfileManager* manager = g_browser_process->profile_manager();
  const base::FilePath path = manager->GenerateNextProfileDirectoryPath();
  SpacesBeingMade().insert(id);
  syncer::AlerseSyncAccount::SetSpaceIdOfNewProfile(
      g_browser_process->local_state(), path.BaseName().AsUTF8Unsafe(), id);
  manager->CreateProfileAsync(
      path, base::BindOnce(
                [](std::string id, std::string name, std::string icon,
                   std::string color, Profile* profile) {
                  SpacesBeingMade().erase(id);
                  if (!profile) {
                    return;
                  }
                  RecordSpaceId(profile);
                  UpdateSpaceCopy(profile->GetPath(), base::UTF8ToUTF16(name),
                                  icon, color);
                },
                id, name, icon, color));
}

// Brings the list in `model`, home's loaded bookmarks, and this device's
// Spaces in line: lists the Spaces the list lacks, marks the ones deleted
// here, and makes the ones this device lacks.
inline void UpdateSpaceList(bookmarks::BookmarkModel* model) {
  const bookmarks::BookmarkNode* folder = FindSpaceFolder(model);
  std::string json;
  if (folder) {
    folder->GetMetaInfo(kSpaceListMetaKey, &json);
  }
  base::DictValue list =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC)
          .value_or(base::DictValue());
  bool changed = false;
  for (const std::string& id : syncer::AlerseSyncAccount::TakeDeletedSpaces(
           g_browser_process->local_state())) {
    list.Set(id, base::DictValue().Set("deleted", true));
    changed = true;
  }

  std::set<std::string> here;
  for (const ProfileAttributesEntry* entry :
       g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetAllProfilesAttributes()) {
    const std::string id = entry->GetAlerseValue(kSpaceEntryIdKey);
    if (entry->IsOmitted() || IsHomeSpacePath(entry->GetPath()) ||
        id.empty()) {
      continue;
    }
    here.insert(id);
    if (!list.FindDict(id)) {
      const SpaceInfo space = SpaceInfoOfEntry(entry);
      list.Set(id, base::DictValue()
                       .Set("name", base::UTF16ToUTF8(space.name))
                       .Set("icon", space.icon)
                       .Set("color", space.color));
      changed = true;
    }
  }
  for (const auto [id, value] : list) {
    const base::DictValue* space = value.GetIfDict();
    if (!space || space->FindBool("deleted").value_or(false) ||
        here.contains(id) || SpacesBeingMade().contains(id)) {
      continue;
    }
    auto text = [&](const char* key) {
      const std::string* found = space->FindString(key);
      return found ? *found : std::string();
    };
    MakeSpaceOfList(id, text("name"), text("icon"), text("color"));
  }
  if (changed) {
    if (!folder) {
      folder = GetOrAddSpaceFolder(model, u"home");
    }
    model->SetNodeMetaInfo(folder, kSpaceListMetaKey,
                           base::WriteJson(list).value_or(std::string()));
  }
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_SPACE_LIST_H_
