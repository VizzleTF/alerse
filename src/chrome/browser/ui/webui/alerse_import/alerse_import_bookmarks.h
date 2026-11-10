// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_BOOKMARKS_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_BOOKMARKS_H_

// How chrome://alerse-import puts what it read into the bookmark model.
// Importing a source again replaces only what its last import into this
// profile added: each added node is marked (see ImportMark), and what the
// user put among, or inside, the marked nodes stays. Runs on the UI thread.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "chrome/browser/ui/webui/alerse_import/alerse_import_data.h"

namespace base {
class Uuid;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace alerse_import {

// The mark of what one source's imports into this profile added. Each node an
// import adds has meta info kImportedMetaKey: an HMAC, under this profile's
// key (kImportKeyPref, which does not sync), of the source's id and the
// node's uuid. Meta info is copied with a node, and synced: a copy the user
// made (copy and paste, a drag with Ctrl) has another uuid, and what another
// device imported was marked with that device's key, so neither is marked
// here and importing again leaves both to the user. A folder that takes one
// of Arc's Spaces is bound to it the same way (kArcSpaceMarkMetaKey).
class ImportMark {
 public:
  ImportMark(std::string_view key, std::string_view source_id);
  ImportMark(const ImportMark&);
  ImportMark& operator=(const ImportMark&);
  ~ImportMark();

  // A new key for a profile, in hex.
  static std::string NewKey();

  // kImportedMetaKey of a node with `uuid` that an import adds.
  std::string Of(const base::Uuid& uuid) const;
  bool IsOn(const bookmarks::BookmarkNode* node) const;

  // kArcSpaceMarkMetaKey of a folder with `uuid` that takes Arc's Space
  // `arc_space`.
  std::string OfArcSpace(std::string_view arc_space,
                         const base::Uuid& uuid) const;
  // The Arc Space `folder` takes in this profile; empty when none.
  std::string ArcSpaceOf(const bookmarks::BookmarkNode* folder) const;

 private:
  std::string Sign(std::string_view kind,
                   std::string_view subject,
                   const base::Uuid& uuid) const;

  std::string key_;
  std::string source_id_;
};

// Adds `nodes` to `parent`, the first of them at `index`, each marked.
void AddNodes(bookmarks::BookmarkModel* model,
              const ImportMark& mark,
              const bookmarks::BookmarkNode* parent,
              size_t index,
              const std::vector<Node>& nodes);

// Removes `node`, a node `mark` is on. What is in it without the mark, the
// user's, moves to the end of `dest` first, in order: each largest unmarked
// subtree as a whole, keeping its ids. `dest` must not be inside `node`.
void RemoveKeepingUnmarked(bookmarks::BookmarkModel* model,
                           const ImportMark& mark,
                           const bookmarks::BookmarkNode* node,
                           const bookmarks::BookmarkNode* dest);

// Removes the children of `folder` that `mark` is on as
// RemoveKeepingUnmarked() does, keeping what is unmarked in `folder`.
// Returns where the first of them was, for what replaces them.
size_t RemoveMarkedChildren(bookmarks::BookmarkModel* model,
                            const ImportMark& mark,
                            const bookmarks::BookmarkNode* folder);

// What ImportArcSpaces() did.
struct ArcResult {
  ArcResult();
  ArcResult(ArcResult&&);
  ArcResult& operator=(ArcResult&&);
  ~ArcResult();

  int links = 0;
  int folders = 0;
  int icons = 0;
  // The folders of the Arc Spaces that no Space has, with their titles: each
  // needs a Space.
  std::vector<std::pair<const bookmarks::BookmarkNode*, std::u16string>>
      unadopted;
};

// Imports Arc's Spaces into the Spaces' folders, replacing an earlier
// import into this profile; `mark` is Arc's. Each Space goes to the folder
// bound to it (by Arc's id), whatever Arc's order is now. Else Arc's first
// Space goes to Home's folder unless another Space is bound to it, and each
// other one to a Space's folder of the same name bound to none, else to a
// new folder for a Space to adopt. A folder keeps its place, id, title, meta
// info and what the user put in it; only its imported items are replaced. A
// folder bound to a Space Arc no longer has loses its imported items and the
// binding. What tools/arc_import.py imported here before marks existed
// (marked "1") is taken as an earlier import into this profile, its Spaces'
// folders bound as that import left them.
ArcResult ImportArcSpaces(bookmarks::BookmarkModel* model,
                          const ImportMark& mark,
                          const std::vector<ArcSpace>& spaces);

// The folder of the bookmarks of `source_id` that has uuid `uuid`, if it is
// still there.
const bookmarks::BookmarkNode* FindImportFolder(
    bookmarks::BookmarkModel* model,
    const std::string& source_id,
    const std::string& uuid);

// Puts `nodes`, the bookmarks of the source of `mark`, in `folder`, the
// folder of that source, in place of what an earlier import of it put there.
// What else is in the folder stays.
void ReplaceImportedBookmarks(bookmarks::BookmarkModel* model,
                              const ImportMark& mark,
                              const bookmarks::BookmarkNode* folder,
                              const std::vector<Node>& nodes);

}  // namespace alerse_import

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_BOOKMARKS_H_
