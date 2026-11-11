// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_SPACE_REGISTRY_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_SPACE_REGISTRY_H_

// The list of Spaces (docs/adr/0001-space-profiles.md). A Space is a
// Chromium profile, so the list is the profiles of ProfileAttributesStorage.
// A Space's name, icon and color belong to its own profile (its home folder,
// which syncs); the entry keeps a copy of them, so that the Space bar of a
// window of another Space can draw the Space without loading its profile.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"

namespace alerse {

// Keys of a Space's copy in its profile's entry.
inline constexpr char kSpaceEntryNameKey[] = "alerse_space_name";
inline constexpr char kSpaceEntryIconKey[] = "alerse_space_icon";
inline constexpr char kSpaceEntryColorKey[] = "alerse_space_color";
// The Space of Arc ("arc:<id>") an import made this Space of, so that
// importing again fills the same Space.
inline constexpr char kSpaceEntryArcKey[] = "alerse_arc_space";

struct SpaceInfo {
  // The profile's directory, which names the Space on this device.
  base::FilePath path;
  // Home is the Space of the first profile; it cannot be deleted.
  bool home = false;
  std::u16string name;
  // As the Space's home folder records them: "arcomium.space_icon" and
  // "arcomium.space_color"; empty when not set.
  std::string icon;
  std::string color;
};

inline bool IsHomeSpacePath(const base::FilePath& path) {
  return path.BaseName().AsUTF8Unsafe() == chrome::kInitialProfile;
}

inline base::RepeatingClosureList& SpacesChangedCallbacks() {
  static base::NoDestructor<base::RepeatingClosureList> callbacks;
  return *callbacks;
}

inline SpaceInfo SpaceInfoOfEntry(const ProfileAttributesEntry* entry) {
  SpaceInfo info;
  info.path = entry->GetPath();
  info.home = IsHomeSpacePath(info.path);
  info.name = base::UTF8ToUTF16(entry->GetAlerseValue(kSpaceEntryNameKey));
  if (info.name.empty()) {
    // A Space nothing was named in yet: Chromium's name for a profile it
    // made, which New Space sets to "Space N".
    info.name = info.home ? u"home" : entry->GetLocalProfileName();
  }
  info.icon = entry->GetAlerseValue(kSpaceEntryIconKey);
  info.color = entry->GetAlerseValue(kSpaceEntryColorKey);
  return info;
}

// The Spaces in the Space bar's order: home, then the others as they were
// made (Chromium numbers their directories).
inline std::vector<SpaceInfo> ListSpaces() {
  std::vector<SpaceInfo> spaces;
  ProfileManager* manager = g_browser_process->profile_manager();
  if (!manager) {
    return spaces;
  }
  for (const ProfileAttributesEntry* entry :
       manager->GetProfileAttributesStorage().GetAllProfilesAttributes()) {
    if (!entry->IsOmitted()) {
      spaces.push_back(SpaceInfoOfEntry(entry));
    }
  }
  auto order = [](const SpaceInfo& space) {
    // "Profile 12" sorts after "Profile 2".
    const std::string base = space.path.BaseName().AsUTF8Unsafe();
    int number = 0;
    const size_t digits = base.find_last_not_of("0123456789");
    if (digits != std::string::npos) {
      base::StringToInt(base.substr(digits + 1), &number);
    }
    return std::make_tuple(!space.home, number, base);
  };
  std::ranges::sort(spaces, {}, order);
  return spaces;
}

// The Space of `profile` (of its original profile, for an incognito one).
inline SpaceInfo GetSpaceOfProfile(Profile* profile) {
  const base::FilePath path = profile->GetOriginalProfile()->GetPath();
  ProfileManager* manager = g_browser_process->profile_manager();
  if (manager) {
    if (const ProfileAttributesEntry* entry =
            manager->GetProfileAttributesStorage()
                .GetProfileAttributesWithPath(path)) {
      return SpaceInfoOfEntry(entry);
    }
  }
  SpaceInfo info;
  info.path = path;
  info.home = IsHomeSpacePath(path);
  info.name = u"home";
  return info;
}

// Records what the Space of `path` shows, as its own window reads it from
// its home folder, and tells every Space bar when it changed.
inline void UpdateSpaceCopy(const base::FilePath& path,
                            const std::u16string& name,
                            const std::string& icon,
                            const std::string& color) {
  ProfileManager* manager = g_browser_process->profile_manager();
  ProfileAttributesEntry* entry =
      manager ? manager->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(path)
              : nullptr;
  if (!entry) {
    return;
  }
  const std::string name_utf8 = base::UTF16ToUTF8(name);
  if (entry->GetAlerseValue(kSpaceEntryNameKey) == name_utf8 &&
      entry->GetAlerseValue(kSpaceEntryIconKey) == icon &&
      entry->GetAlerseValue(kSpaceEntryColorKey) == color) {
    return;
  }
  entry->SetAlerseValue(kSpaceEntryNameKey, name_utf8);
  if (!name.empty()) {
    // Chromium shows the profile's name in the window's title.
    entry->SetLocalProfileName(name, /*is_default_name=*/false);
  }
  entry->SetAlerseValue(kSpaceEntryIconKey, icon);
  entry->SetAlerseValue(kSpaceEntryColorKey, color);
  SpacesChangedCallbacks().Notify();
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_SPACE_REGISTRY_H_
