// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ARCOMIUM_IMPORT_ARCOMIUM_IMPORT_DATA_H_
#define CHROME_BROWSER_UI_WEBUI_ARCOMIUM_IMPORT_ARCOMIUM_IMPORT_DATA_H_

// What chrome://arcomium-import reads from other browsers on this computer:
// Arc's sidebar (its Spaces and pinned items) and the bookmarks of
// Chromium-based browsers. Parsing is pure; the functions that touch the disk
// block and run on the thread pool. Nothing here writes to another browser's
// files.

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/values.h"
#include "url/gurl.h"

namespace arcomium_import {

// Files larger than this are not read.
inline constexpr size_t kMaxFileSize = 64 * 1024 * 1024;
// Arc's cached favicons larger than this are skipped.
inline constexpr size_t kMaxIconSize = 64 * 1024;
// Arc's items name their children by id, so one folder can be listed in many
// places: a sidebar that does so over and over would grow without end. What
// the sidebar yields past this many items, over all its containers, is left
// out.
inline constexpr int kMaxArcItems = 100000;
// The same for the text the items copy out of the sidebar, in bytes: titles
// (twice, as UTF-16), addresses, item ids and icon names. An item listed in
// many places is copied into each.
inline constexpr size_t kMaxArcBytes = 32 * 1024 * 1024;
// Longer titles are cut to this many bytes of UTF-8.
inline constexpr size_t kMaxTitleBytes = 4096;
// Arc's ids are UUIDs: a Space with a longer id is skipped, and an item with
// one gets no favicon.
inline constexpr size_t kMaxArcIdBytes = 256;
// Arc's icon names ("sunny", "book", ...): longer ones are dropped.
inline constexpr size_t kMaxIconNameBytes = 64;
// The favicons one read of Arc attaches, at most, in bytes of base64: one
// icon listed in many places is copied into each.
inline constexpr size_t kMaxArcIconBytes = 8 * 1024 * 1024;

// Meta info of each bookmark and folder an import added: see ImportMark.
inline constexpr char kImportedMetaKey[] = "arcomium.imported";
// Meta info of the folder a browser's bookmarks went to: "<browser>/<profile>".
inline constexpr char kImportMetaKey[] = "arcomium.import";
inline constexpr char kSpaceMetaKey[] = "arcomium.space";
inline constexpr char kSpaceIconMetaKey[] = "arcomium.space_icon";
// The icon Arc gave a Space folder's Space when it was last imported: the
// Space's icon follows Arc's only while the user has not picked another.
inline constexpr char kArcIconMetaKey[] = "arcomium.arc_icon";
inline constexpr char kFolderIconMetaKey[] = "arcomium.folder_icon";
inline constexpr char kIconMetaKey[] = "arcomium.icon";
// A pinned item the user named: Arc's items with a title of their own.
inline constexpr char kRenamedMetaKey[] = "arcomium.renamed";
// The id of the Arc Space a folder was imported from, kept after a Space
// adopts the folder so that importing again finds that Space. It counts only
// with kArcSpaceMarkMetaKey: see ImportMark.
inline constexpr char kArcSpaceMetaKey[] = "arcomium.arc_space";
inline constexpr char kArcSpaceMarkMetaKey[] = "arcomium.arc_space_mark";
inline constexpr char kHomeSpace[] = "default";
inline constexpr char kArcSourceId[] = "arc";

// Profile prefs, not synced. kImportFoldersPref: {source id: uuid of the
// folder an import of that browser into this profile made}. Bookmarks' meta
// info syncs, so a folder another device imported names the same source; only
// the one recorded here is this profile's to import into again.
// kImportKeyPref: the key of this profile's ImportMarks, in hex.
inline constexpr char kImportFoldersPref[] = "arcomium.import_folders";
inline constexpr char kImportKeyPref[] = "arcomium.import_key";

// A bookmark to add: a folder when `url` is empty.
struct Node {
  Node();
  Node(Node&&);
  Node& operator=(Node&&);
  ~Node();

  bool is_folder() const { return url.is_empty(); }

  std::u16string title;
  GURL url;
  // As bookmarks::BookmarkNode::MetaInfoMap.
  base::flat_map<std::string, std::string> meta;
  std::vector<Node> children;
  // Arc's id of the item, which names its cached favicon.
  std::string arc_item;
};

// A Space of Arc with its pinned items.
struct ArcSpace {
  ArcSpace();
  ArcSpace(ArcSpace&&);
  ArcSpace& operator=(ArcSpace&&);
  ~ArcSpace();

  std::string id;
  std::u16string title;
  // Arc's icon name ("sunny", "book", ...), if any.
  std::string icon;
  std::vector<Node> pinned;
};

// Parses Arc's StorableSidebar.json: every Space that has an id, a title and
// a pinned container, in Arc's order, with its pinned tabs and folders, up to
// kMaxArcItems and kMaxArcBytes. Tabs without an address are skipped; arc://
// pages become chrome:// ones.
std::vector<ArcSpace> ParseArcSidebar(const base::Value& sidebar);

// Parses a Chromium "Bookmarks" file into one folder: the bookmark bar's
// items, then the other and the mobile bookmarks in folders named
// `other_title` and `mobile_title` when the file does not name them. Returns
// nullopt when the file is not a bookmarks file.
std::optional<Node> ParseChromiumBookmarks(const base::Value& bookmarks,
                                           const std::u16string& other_title,
                                           const std::u16string& mobile_title);

// Display names of a user data dir's profiles, by directory name, from its
// "Local State" (profile.info_cache).
std::map<std::string, std::string> ParseProfileNames(
    const base::Value& local_state);

// Whether a profile's Preferences are those of an Arcomium profile.
bool IsArcomiumPreferences(std::string_view preferences);

int CountLinks(const std::vector<Node>& nodes);
int CountFolders(const std::vector<Node>& nodes);

enum class SourceKind { kArc, kChromium };

// Something to import from, as FindSources() finds it.
struct Source {
  Source();
  Source(const Source&);
  Source& operator=(const Source&);
  ~Source();

  // kArcSourceId, or "<browser key>/<profile directory>".
  std::string id;
  SourceKind kind = SourceKind::kChromium;
  std::string browser;
  // The profile's name; empty when the browser has one profile here.
  std::string profile;
  // Arc's data directory, or the profile's directory.
  base::FilePath path;
  int links = 0;
  int spaces = 0;
};

// Finds Arc and the profiles with bookmarks of the Chromium-based browsers
// installed for this user, skipping `own_profile` and Arcomium's profiles.
// Blocks.
std::vector<Source> FindSources(const base::FilePath& own_profile);

// Reads Arc's sidebar in `dir`, with the favicons Arc cached for its items
// (meta info "arcomium.icon") up to kMaxArcIconBytes. Blocks.
std::optional<std::vector<ArcSpace>> ReadArc(const base::FilePath& dir,
                                             bool with_icons);

// Reads the bookmarks of the profile in `dir`. Blocks.
std::optional<Node> ReadChromiumBookmarks(const base::FilePath& dir,
                                          const std::u16string& other_title,
                                          const std::u16string& mobile_title);

}  // namespace arcomium_import

#endif  // CHROME_BROWSER_UI_WEBUI_ARCOMIUM_IMPORT_ARCOMIUM_IMPORT_DATA_H_
