// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_SPACE_FOLDER_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_SPACE_FOLDER_H_

// The folder of a Space (docs/adr/0001-space-profiles.md): the folder of its
// profile's Other Bookmarks that holds the Space's pinned items and records
// its name, icon and color. Moving an item to another Space copies it into
// that profile's folder.

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"

namespace alerse {

// Marks the folder; the value is the one home's folder had in 0.1, where
// every other Space's folder named its storage partition here.
inline constexpr char kSpaceFolderMetaKey[] = "arcomium.space";
inline constexpr char kSpaceFolderMetaValue[] = "default";

// The Space's folder in `model`, which has loaded; null if it has none yet.
inline const bookmarks::BookmarkNode* FindSpaceFolder(
    const bookmarks::BookmarkModel* model) {
  for (const auto& folder : model->other_node()->children()) {
    std::string value;
    if (folder->is_folder() &&
        folder->GetMetaInfo(kSpaceFolderMetaKey, &value) &&
        value == kSpaceFolderMetaValue) {
      return folder.get();
    }
  }
  return nullptr;
}

// The same, added with `title` if the Space has none.
inline const bookmarks::BookmarkNode* GetOrAddSpaceFolder(
    bookmarks::BookmarkModel* model,
    const std::u16string& title) {
  if (const bookmarks::BookmarkNode* folder = FindSpaceFolder(model)) {
    return folder;
  }
  bookmarks::BookmarkNode::MetaInfoMap meta;
  meta[kSpaceFolderMetaKey] = kSpaceFolderMetaValue;
  return model->AddFolder(model->other_node(),
                          model->other_node()->children().size(), title, &meta);
}

// Copies `node`, with what it holds and its meta info (icons, open folders),
// to the end of `parent` in `model`, the bookmarks of another profile.
inline void CopyNodeTo(const bookmarks::BookmarkNode* node,
                       bookmarks::BookmarkModel* model,
                       const bookmarks::BookmarkNode* parent) {
  bookmarks::CloneBookmarkNode(model, {bookmarks::BookmarkNodeData::Element(node)},
                               parent, parent->children().size(),
                               /*reset_node_times=*/false);
}

// Runs a callback once a profile's bookmarks have loaded, then deletes
// itself; also if the model goes first.
class SpaceBookmarksWaiter : public bookmarks::BaseBookmarkModelObserver {
 public:
  using Callback =
      base::OnceCallback<void(Profile*, bookmarks::BookmarkModel*)>;

  SpaceBookmarksWaiter(Profile* profile,
                       bookmarks::BookmarkModel* model,
                       Callback done)
      : profile_path_(profile->GetPath()), done_(std::move(done)) {
    observation_.Observe(model);
  }

  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override {
    bookmarks::BookmarkModel* model = observation_.GetSource();
    // By its path: the profile may have been deleted meanwhile.
    Profile* profile =
        g_browser_process->profile_manager()->GetProfileByPath(profile_path_);
    Callback done = std::move(done_);
    delete this;
    if (profile) {
      std::move(done).Run(profile, model);
    }
  }
  void BookmarkModelBeingDeleted() override { delete this; }
  void BookmarkModelChanged() override {}

 private:
  base::FilePath profile_path_;
  Callback done_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      observation_{this};
};

// Runs `done` with `profile` and its bookmarks once they have loaded.
inline void WithBookmarksOf(Profile* profile,
                            SpaceBookmarksWaiter::Callback done) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  if (!model) {
    return;
  }
  if (model->loaded()) {
    std::move(done).Run(profile, model);
    return;
  }
  new SpaceBookmarksWaiter(profile, model, std::move(done));
}

// Runs `done` with the profile of the Space in `path` and its bookmarks,
// loading the profile if it is not; never if the profile cannot load.
inline void WithSpaceBookmarks(const base::FilePath& path,
                               SpaceBookmarksWaiter::Callback done) {
  g_browser_process->profile_manager()->CreateProfileAsync(
      path, base::BindOnce(
                [](SpaceBookmarksWaiter::Callback done, Profile* profile) {
                  if (profile) {
                    WithBookmarksOf(profile, std::move(done));
                  }
                },
                std::move(done)));
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_SPACE_FOLDER_H_
