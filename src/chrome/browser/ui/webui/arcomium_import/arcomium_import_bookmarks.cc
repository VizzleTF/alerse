// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/arcomium_import/arcomium_import_bookmarks.h"

#include <array>
#include <map>
#include <optional>
#include <set>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "crypto/hmac.h"
#include "ui/base/models/tree_node_iterator.h"

namespace arcomium_import {

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

// Bytes of the HMAC a mark keeps, in hex.
constexpr size_t kMarkBytes = 16;

std::string GetMeta(const BookmarkNode* node, const std::string& key) {
  std::string value;
  node->GetMetaInfo(key, &value);
  return value;
}

bool HasMeta(const BookmarkNode* node,
             const std::string& key,
             const std::string& value) {
  std::string found;
  return node->GetMetaInfo(key, &found) && found == value;
}

// As arcomium::IsGroupSpace(): a Space with its own tab group, not Home and
// not one an Arc import left for a Space to adopt.
bool IsGroupSpace(const std::string& space) {
  return !space.empty() && space != kHomeSpace && !space.starts_with("arc:");
}

void SetOrDeleteMeta(BookmarkModel* model,
                     const BookmarkNode* node,
                     const std::string& key,
                     const std::string& value) {
  if (value.empty()) {
    model->DeleteNodeMetaInfo(node, key);
  } else {
    model->SetNodeMetaInfo(node, key, value);
  }
}

// Records `icon`, Arc's icon of the Space of `folder`, and makes it the
// Space's icon unless the user has picked another one since the last import.
void SetArcIcon(BookmarkModel* model,
                const BookmarkNode* folder,
                const std::string& icon) {
  const std::string shown = GetMeta(folder, kSpaceIconMetaKey);
  if (shown.empty() || shown == GetMeta(folder, kArcIconMetaKey)) {
    SetOrDeleteMeta(model, folder, kSpaceIconMetaKey, icon);
  }
  SetOrDeleteMeta(model, folder, kArcIconMetaKey, icon);
}

// Binds `folder` to Arc's Space `arc_space` in this profile.
void Bind(BookmarkModel* model,
          const ImportMark& mark,
          const BookmarkNode* folder,
          const std::string& arc_space) {
  model->SetNodeMetaInfo(folder, kArcSpaceMetaKey, arc_space);
  model->SetNodeMetaInfo(folder, kArcSpaceMarkMetaKey,
                         mark.OfArcSpace(arc_space, folder->uuid()));
}

// Unbinds `folder` from the Arc Space it took; the Space's icon stays.
void Unbind(BookmarkModel* model, const BookmarkNode* folder) {
  model->DeleteNodeMetaInfo(folder, kArcSpaceMetaKey);
  model->DeleteNodeMetaInfo(folder, kArcSpaceMarkMetaKey);
  model->DeleteNodeMetaInfo(folder, kArcIconMetaKey);
}

// kImportedMetaKey of what tools/arc_import.py imported before ImportMark.
constexpr char kLegacyImportedMark[] = "1";

// Takes what tools/arc_import.py imported into this profile before ImportMark
// existed as `mark`'s, Arc's import here. The tool marked each node it added
// "1" and made a folder under Other bookmarks for each Arc Space: each such
// folder and each node in it marked "1" gets `mark`. A folder no Space has
// adopted yet ("arc:<Arc's id>") is bound to its Arc Space. The Space's icon
// is not recorded as Arc's: the user may have picked it since, so it stays.
// "1" tells neither a copy the user made nor what the tool imported on
// another device (synced here) from what it imported here, so such nodes in
// these folders are taken as imported here too.
void AdoptLegacyArcImport(BookmarkModel* model, const ImportMark& mark) {
  for (const auto& child : model->other_node()->children()) {
    const BookmarkNode* folder = child.get();
    const std::string space = GetMeta(folder, kSpaceMetaKey);
    if (!folder->is_folder() || space.empty() ||
        GetMeta(folder, kImportedMetaKey) != kLegacyImportedMark) {
      continue;
    }
    model->SetNodeMetaInfo(folder, kImportedMetaKey, mark.Of(folder->uuid()));
    if (space.starts_with("arc:") && space.size() > 4 &&
        mark.ArcSpaceOf(folder).empty()) {
      Bind(model, mark, folder, space.substr(4));
    }
    ui::TreeNodeIterator<const BookmarkNode> nodes(folder);
    while (nodes.has_next()) {
      const BookmarkNode* node = nodes.Next();
      if (GetMeta(node, kImportedMetaKey) == kLegacyImportedMark) {
        model->SetNodeMetaInfo(node, kImportedMetaKey, mark.Of(node->uuid()));
      }
    }
  }
}

int CountIcons(const std::vector<Node>& nodes) {
  int count = 0;
  for (const Node& node : nodes) {
    count += node.meta.contains(kIconMetaKey) + CountIcons(node.children);
  }
  return count;
}

}  // namespace

ImportMark::ImportMark(std::string_view key, std::string_view source_id)
    : key_(key), source_id_(source_id) {}
ImportMark::ImportMark(const ImportMark&) = default;
ImportMark& ImportMark::operator=(const ImportMark&) = default;
ImportMark::~ImportMark() = default;

// static
std::string ImportMark::NewKey() {
  return base::HexEncodeLower(base::RandBytesAsVector(32));
}

std::string ImportMark::Sign(std::string_view kind,
                             std::string_view subject,
                             const base::Uuid& uuid) const {
  // tools/arc_import.py signs the same way.
  std::string message(kind);
  message += '\n';
  message += subject;
  message += '\n';
  message += uuid.AsLowercaseString();
  const std::array<uint8_t, 32> hmac = crypto::hmac::SignSha256(
      base::as_byte_span(key_), base::as_byte_span(message));
  return base::HexEncodeLower(base::span(hmac).first(kMarkBytes));
}

std::string ImportMark::Of(const base::Uuid& uuid) const {
  return Sign("node", source_id_, uuid);
}

bool ImportMark::IsOn(const BookmarkNode* node) const {
  return HasMeta(node, kImportedMetaKey, Of(node->uuid()));
}

std::string ImportMark::OfArcSpace(std::string_view arc_space,
                                   const base::Uuid& uuid) const {
  return Sign("arc_space", arc_space, uuid);
}

std::string ImportMark::ArcSpaceOf(const BookmarkNode* folder) const {
  const std::string arc_space = GetMeta(folder, kArcSpaceMetaKey);
  return !arc_space.empty() &&
                 HasMeta(folder, kArcSpaceMarkMetaKey,
                         OfArcSpace(arc_space, folder->uuid()))
             ? arc_space
             : std::string();
}

ArcResult::ArcResult() = default;
ArcResult::ArcResult(ArcResult&&) = default;
ArcResult& ArcResult::operator=(ArcResult&&) = default;
ArcResult::~ArcResult() = default;

void AddNodes(BookmarkModel* model,
              const ImportMark& mark,
              const BookmarkNode* parent,
              size_t index,
              const std::vector<Node>& nodes) {
  for (const Node& node : nodes) {
    const base::Uuid uuid = base::Uuid::GenerateRandomV4();
    BookmarkNode::MetaInfoMap meta = node.meta;
    meta[kImportedMetaKey] = mark.Of(uuid);
    if (node.is_folder()) {
      AddNodes(model, mark,
               model->AddFolder(parent, index, node.title, &meta,
                                /*creation_time=*/std::nullopt, uuid),
               0, node.children);
    } else {
      model->AddURL(parent, index, node.title, node.url, &meta,
                    /*creation_time=*/std::nullopt, uuid);
    }
    ++index;
  }
}

void RemoveKeepingUnmarked(BookmarkModel* model,
                           const ImportMark& mark,
                           const BookmarkNode* node,
                           const BookmarkNode* dest) {
  DCHECK(!dest->HasAncestor(node));
  // The largest unmarked subtrees, in order. The tree may be deep: no
  // recursion.
  std::vector<const BookmarkNode*> kept;
  std::vector<const BookmarkNode*> stack = {node};
  while (!stack.empty()) {
    const BookmarkNode* top = stack.back();
    stack.pop_back();
    if (top != node && !mark.IsOn(top)) {
      kept.push_back(top);
      continue;
    }
    for (auto it = top->children().rbegin(); it != top->children().rend();
         ++it) {
      stack.push_back(it->get());
    }
  }
  for (const BookmarkNode* user_node : kept) {
    model->Move(user_node, dest, dest->children().size());
  }
  model->Remove(node, bookmarks::metrics::BookmarkEditSource::kOther,
                FROM_HERE);
}

size_t RemoveMarkedChildren(BookmarkModel* model,
                            const ImportMark& mark,
                            const BookmarkNode* folder) {
  std::vector<const BookmarkNode*> marked;
  size_t first = 0;
  for (size_t i = 0; i < folder->children().size(); ++i) {
    if (mark.IsOn(folder->children()[i].get())) {
      if (marked.empty()) {
        first = i;
      }
      marked.push_back(folder->children()[i].get());
    }
  }
  for (const BookmarkNode* node : marked) {
    RemoveKeepingUnmarked(model, mark, node, folder);
  }
  return marked.empty() ? folder->children().size() : first;
}

ArcResult ImportArcSpaces(BookmarkModel* model,
                          const ImportMark& mark,
                          const std::vector<ArcSpace>& spaces) {
  model->BeginExtensiveChanges();
  AdoptLegacyArcImport(model, mark);

  const BookmarkNode* other = model->other_node();
  std::vector<const BookmarkNode*> folders;
  // The Arc Space each folder is bound to in this profile, if any.
  std::map<const BookmarkNode*, std::string> arc_space_of;
  bool has_home = false;
  for (const auto& child : other->children()) {
    if (child->is_folder()) {
      folders.push_back(child.get());
      arc_space_of[child.get()] = mark.ArcSpaceOf(child.get());
      has_home |= GetMeta(child.get(), kSpaceMetaKey) == kHomeSpace;
    }
  }

  // The folder already there that each of Arc's Spaces goes to; one Space's
  // folder takes one Arc Space.
  std::vector<const BookmarkNode*> targets(spaces.size(), nullptr);
  std::set<const BookmarkNode*> taken;
  std::set<std::string> taken_spaces;
  auto find = [&](auto matches) -> const BookmarkNode* {
    for (const BookmarkNode* folder : folders) {
      if (!taken.contains(folder) &&
          !taken_spaces.contains(GetMeta(folder, kSpaceMetaKey)) &&
          matches(folder)) {
        return folder;
      }
    }
    return nullptr;
  };
  auto take = [&](size_t i, const BookmarkNode* folder) {
    targets[i] = folder;
    taken.insert(folder);
    if (std::string space = GetMeta(folder, kSpaceMetaKey); !space.empty()) {
      taken_spaces.insert(std::move(space));
    }
  };
  // Every Space bound to a folder here, by Arc's id, wherever Arc lists it
  // now: Home's folder too, and before any by name.
  for (size_t i = 0; i < spaces.size(); ++i) {
    if (spaces[i].id.empty()) {
      continue;
    }
    if (const BookmarkNode* folder = find([&](const BookmarkNode* folder) {
          return arc_space_of[folder] == spaces[i].id;
        })) {
      take(i, folder);
    }
  }
  // Arc's first Space goes to Home unless another Space has Home: the user's
  // own Home folder, else the one an earlier import made.
  if (!spaces.empty() && !targets[0]) {
    for (bool imported : {false, true}) {
      if (const BookmarkNode* home = find([&](const BookmarkNode* folder) {
            return mark.IsOn(folder) == imported &&
                   GetMeta(folder, kSpaceMetaKey) == kHomeSpace;
          })) {
        take(0, home);
        break;
      }
    }
  }
  // With no Home folder at all, the first Space makes it.
  const bool new_home = !spaces.empty() && !targets[0] && !has_home;
  // Then by name, among the Spaces' folders bound to no Arc Space, here or
  // on another device.
  for (size_t i = new_home ? 1 : 0; i < spaces.size(); ++i) {
    if (targets[i]) {
      continue;
    }
    if (const BookmarkNode* folder = find([&](const BookmarkNode* folder) {
          return folder->GetTitle() == spaces[i].title &&
                 IsGroupSpace(GetMeta(folder, kSpaceMetaKey)) &&
                 GetMeta(folder, kArcSpaceMetaKey).empty();
        })) {
      take(i, folder);
    }
  }

  ArcResult result;
  // The folder of each Space now.
  std::map<std::string, const BookmarkNode*> folder_by_space;
  for (size_t i = 0; i < spaces.size(); ++i) {
    const ArcSpace& space = spaces[i];
    result.links += CountLinks(space.pinned);
    result.folders += CountFolders(space.pinned);
    result.icons += CountIcons(space.pinned);
    const BookmarkNode* folder = targets[i];
    if (folder) {
      const size_t index = RemoveMarkedChildren(model, mark, folder);
      AddNodes(model, mark, folder, index, space.pinned);
    } else {
      const base::Uuid uuid = base::Uuid::GenerateRandomV4();
      const BookmarkNode::MetaInfoMap meta = {
          {kImportedMetaKey, mark.Of(uuid)},
          {kSpaceMetaKey,
           i == 0 && new_home ? std::string(kHomeSpace) : "arc:" + space.id},
      };
      folder = model->AddFolder(other, other->children().size(), space.title,
                                &meta, /*creation_time=*/std::nullopt, uuid);
      AddNodes(model, mark, folder, 0, space.pinned);
    }
    if (!space.id.empty()) {
      Bind(model, mark, folder, space.id);
    }
    const std::string target = GetMeta(folder, kSpaceMetaKey);
    // The icon of the user's own Home is theirs alone.
    if (target != kHomeSpace || mark.IsOn(folder)) {
      SetArcIcon(model, folder, space.icon);
    }
    if (!target.empty()) {
      folder_by_space.emplace(target, folder);
    }
    if (target.starts_with("arc:")) {
      result.unadopted.emplace_back(folder, folder->GetTitle());
    }
  }

  // The rest of what earlier imports here brought goes; what the user put in
  // it stays: in the folder of the same Space, or in its own folder when Arc
  // no longer has that Space. A folder of the user's that a Space Arc no
  // longer has was bound to stays, unbound.
  for (const BookmarkNode* folder : folders) {
    const bool imported = mark.IsOn(folder);
    if (taken.contains(folder) || (!imported && arc_space_of[folder].empty())) {
      continue;
    }
    const std::string space = GetMeta(folder, kSpaceMetaKey);
    if (auto it = folder_by_space.find(space);
        imported && it != folder_by_space.end()) {
      RemoveKeepingUnmarked(model, mark, folder, it->second);
      continue;
    }
    RemoveMarkedChildren(model, mark, folder);
    // A folder no Space adopted that is now empty would only name the next
    // Space the "+" button makes: it goes.
    if (imported && folder->children().empty() && !IsGroupSpace(space)) {
      model->Remove(folder, bookmarks::metrics::BookmarkEditSource::kOther,
                    FROM_HERE);
      continue;
    }
    if (imported) {
      model->DeleteNodeMetaInfo(folder, kImportedMetaKey);
    }
    Unbind(model, folder);
  }
  model->EndExtensiveChanges();
  return result;
}

const BookmarkNode* FindImportFolder(BookmarkModel* model,
                                     const std::string& source_id,
                                     const std::string& uuid) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->is_folder() && node->uuid().AsLowercaseString() == uuid &&
        HasMeta(node, kImportMetaKey, source_id)) {
      return node;
    }
  }
  return nullptr;
}

void ReplaceImportedBookmarks(BookmarkModel* model,
                              const ImportMark& mark,
                              const BookmarkNode* folder,
                              const std::vector<Node>& nodes) {
  model->BeginExtensiveChanges();
  const size_t index = RemoveMarkedChildren(model, mark, folder);
  AddNodes(model, mark, folder, index, nodes);
  model->EndExtensiveChanges();
}

}  // namespace arcomium_import
