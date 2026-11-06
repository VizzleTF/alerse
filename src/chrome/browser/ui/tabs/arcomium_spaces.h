// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ARCOMIUM_SPACES_H_
#define CHROME_BROWSER_UI_TABS_ARCOMIUM_SPACES_H_

// Arcomium Spaces. A Space is a tab group shown in focus mode, so the vertical
// tab strip lists only its tabs. Web tabs of a Space live in a dedicated
// storage partition ("arcomium" domain, partition name = Space id), so each
// Space has its own cookies and site data. The Space of a group is the
// partition of its tabs; the session keeps a group -> Space map in the window
// extra data so restored tabs come back in their partition.

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace arcomium {

inline constexpr char kSpacePartitionDomain[] = "arcomium";
// Window extra data: JSON object {tab group id: Space id}.
inline constexpr char kSpacesWindowExtraDataKey[] = "arcomium.spaces";
// Tab extra data handed from session restore to the restored tab factory.
inline constexpr char kSpaceTabExtraDataKey[] = "arcomium.space";

// Arcomium's namespace for name-based ids (RFC 9562, UUIDv5).
inline constexpr std::array<uint8_t, 16> kSpaceGuidNamespace = {
    0x6b, 0x1f, 0x2e, 0x8a, 0x4c, 0x37, 0x4d, 0x0e,
    0x9a, 0x5b, 0xc2, 0x71, 0x3e, 0x90, 0x58, 0xd4};

// The saved tab group of `space`: a UUIDv5 of the Space id. Every device
// derives the same one, so a Space has a single saved group however many
// devices open it or restore it, as a client tag does in Chromium sync.
inline base::Uuid SavedGroupGuidForSpace(const std::string& space) {
  std::string data(kSpaceGuidNamespace.begin(), kSpaceGuidNamespace.end());
  data += space;
  base::SHA1Digest hash = base::SHA1Hash(base::as_byte_span(data));
  hash[6] = (hash[6] & 0x0f) | 0x50;
  hash[8] = (hash[8] & 0x3f) | 0x80;
  return base::Uuid::ParseLowercase(base::StringPrintf(
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
      hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14],
      hash[15]));
}

// Whether `space` names a Space with its own tab group: not home, not a
// folder imported from Arc that no Space has adopted yet.
inline bool IsGroupSpace(const std::string& space) {
  return !space.empty() && space != "default" && !space.starts_with("arc:");
}

// The Space whose saved tab group `sync_id` is, read from the Spaces'
// bookmark folders: the group derived from the Space id, or else the group a
// folder still names in "arcomium.group" from before ids were derived.
inline std::optional<std::string> GetSpaceOfSavedGroup(
    content::BrowserContext* context,
    const base::Uuid& sync_id) {
  const bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(context);
  if (!bookmarks || !bookmarks->loaded()) {
    return std::nullopt;
  }
  const std::string id = sync_id.AsLowercaseString();
  std::optional<std::string> legacy;
  for (const auto& folder : bookmarks->other_node()->children()) {
    std::string space;
    if (!folder->is_folder() ||
        !folder->GetMetaInfo("arcomium.space", &space) ||
        !IsGroupSpace(space)) {
      continue;
    }
    if (SavedGroupGuidForSpace(space) == sync_id) {
      return space;
    }
    std::string group;
    if (!legacy && folder->GetMetaInfo("arcomium.group", &group) &&
        group == id) {
      legacy = space;
    }
  }
  return legacy;
}

// Returns the Space of `contents`, or nullopt for the default partition.
inline std::optional<std::string> GetSpaceOfContents(
    content::WebContents* contents) {
  if (!contents) {
    return std::nullopt;
  }
  content::StoragePartition* partition =
      contents->GetBrowserContext()->GetStoragePartition(
          contents->GetSiteInstance());
  if (!partition ||
      partition->GetConfig().partition_domain() != kSpacePartitionDomain) {
    return std::nullopt;
  }
  return partition->GetConfig().partition_name();
}

inline std::optional<std::string> GetSpaceOfGroup(
    const TabStripModel* model,
    const tab_groups::TabGroupId& group) {
  if (!model->group_model() || !model->group_model()->ContainsTabGroup(group)) {
    return std::nullopt;
  }
  // Any tab in the Space's partition tells: the New Tab page and other
  // WebUI cannot live there.
  const gfx::Range tabs = model->group_model()->GetTabGroup(group)->ListTabs();
  for (size_t i = tabs.start(); i < tabs.end(); ++i) {
    if (std::optional<std::string> space = GetSpaceOfContents(
            model->GetWebContentsAt(static_cast<int>(i)))) {
      return space;
    }
  }
  return std::nullopt;
}

// The Space of `group` as GetSpaceOfGroup() finds it, or else the Space whose
// bookmark folder has the group's title: a Space none of whose tabs is in its
// partition any more, as when only a New Tab page is open in it.
inline std::optional<std::string> GetSpaceOfGroupOrFolder(
    content::BrowserContext* context,
    const TabStripModel* model,
    const tab_groups::TabGroupId& group) {
  if (std::optional<std::string> space = GetSpaceOfGroup(model, group)) {
    return space;
  }
  if (!model->group_model() || !model->group_model()->ContainsTabGroup(group)) {
    return std::nullopt;
  }
  const std::u16string& title =
      model->group_model()->GetTabGroup(group)->visual_data()->title();
  const bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(context);
  if (title.empty() || !bookmarks || !bookmarks->loaded()) {
    return std::nullopt;
  }
  for (const auto& folder : bookmarks->other_node()->children()) {
    std::string space;
    if (folder->is_folder() && folder->GetTitle() == title &&
        folder->GetMetaInfo("arcomium.space", &space) && !space.empty() &&
        space != "default" && !space.starts_with("arc:")) {
      return space;
    }
  }
  return std::nullopt;
}

// SiteInstance that pins a new tab to the partition of `space`. Only web
// pages and about:blank can live there; returns nullptr for anything else.
inline scoped_refptr<content::SiteInstance> CreateSpaceSiteInstance(
    content::BrowserContext* browser_context,
    const GURL& url,
    const std::string& space) {
  if (space.empty() || browser_context->IsOffTheRecord() ||
      (!url.SchemeIsHTTPOrHTTPS() && !url.IsAboutBlank())) {
    return nullptr;
  }
  return content::SiteInstance::CreateForFixedStoragePartition(
      browser_context, url,
      content::StoragePartitionConfig::Create(
          browser_context, kSpacePartitionDomain, space, /*in_memory=*/false));
}

inline std::string BuildSpacesExtraData(const TabStripModel* model) {
  base::DictValue spaces;
  if (model->group_model()) {
    for (const tab_groups::TabGroupId& group :
         model->group_model()->ListTabGroups()) {
      if (std::optional<std::string> space = GetSpaceOfGroup(model, group)) {
        spaces.Set(group.ToString(), *space);
      }
    }
  }
  std::string json;
  base::JSONWriter::Write(spaces, &json);
  return json;
}

// Parses BuildSpacesExtraData() output into {tab group id string: Space id}.
inline base::flat_map<std::string, std::string> ParseSpacesExtraData(
    const std::string& json) {
  base::flat_map<std::string, std::string> spaces;
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!dict) {
    return spaces;
  }
  for (const auto [group, space] : *dict) {
    if (space.is_string()) {
      spaces[group] = space.GetString();
    }
  }
  return spaces;
}

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_TABS_ARCOMIUM_SPACES_H_
