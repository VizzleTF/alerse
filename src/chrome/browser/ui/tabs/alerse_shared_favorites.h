// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_SHARED_FAVORITES_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_SHARED_FAVORITES_H_

// The same favorites in every Space, as in Arc, with a switch in a
// favorite's menu. Each Space is a profile with a favorites folder of its
// own; while the switch is on, a change to one Space's favorites goes to
// every other Space. The shared set is kept in local state, so a Space that
// opens later takes it too. Favorites match by address, so a favorite whose
// tab is open in another Space keeps its node and its tab.
//
// Sync carries each Space's favorites on that Space's own chain, so a change
// that sync brings is not copied to the other Spaces: their chains bring it
// too. Copying it made a second node of the same address in each Space, which
// went back up and came down as another, without end.

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace alerse {

// Local state: the switch (on by default, as in Arc) and the shared set, a
// list of {url, title, meta: {key: value}}.
inline constexpr char kSharedFavoritesPref[] = "alerse.shared_favorites";
inline constexpr char kSharedFavoritesSetPref[] =
    "alerse.shared_favorites_set";

inline void RegisterSharedFavoritesLocalState(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kSharedFavoritesPref, true);
  registry->RegisterListPref(kSharedFavoritesSetPref);
}

class FavoritesMirror {
 public:
  static FavoritesMirror& Get() {
    static base::NoDestructor<FavoritesMirror> mirror;
    return *mirror;
  }

  static bool IsOn() {
    PrefService* local_state =
        g_browser_process ? g_browser_process->local_state() : nullptr;
    return local_state && local_state->FindPreference(kSharedFavoritesPref) &&
           local_state->GetBoolean(kSharedFavoritesPref);
  }

  // A Space's bookmarks, once loaded. Its favorites take the shared set, or,
  // when none is kept yet, add theirs to it.
  void Watch(bookmarks::BookmarkModel* model) {
    if (!model || !model->loaded() || watchers_.contains(model)) {
      return;
    }
    watchers_[model] = std::make_unique<Watcher>(this, model);
    if (!IsOn()) {
      return;
    }
    if (!HasSet()) {
      Join(model);
    } else {
      Apply(model, LoadSet());
    }
  }

  // The switch. Turning it on joins every open Space's favorites, the one
  // `first` belongs to leading: the set is every Space's read before any of
  // them changes.
  void SetOn(bool on, bookmarks::BookmarkModel* first) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(kSharedFavoritesPref, on);
    if (!on) {
      return;
    }
    std::vector<Favorite> set;
    if (first) {
      AddMissing(set, Read(first));
    }
    for (const auto& [model, watcher] : watchers_) {
      if (model != first) {
        AddMissing(set, Read(model));
      }
    }
    StoreSet(set);
    for (const auto& [model, watcher] : watchers_) {
      Apply(model, set);
    }
  }

  // A Space about to be deleted: its bookmarks are emptied first, which must
  // not empty every other Space's favorites.
  void Forget(bookmarks::BookmarkModel* model) { watchers_.erase(model); }

 private:
  friend class base::NoDestructor<FavoritesMirror>;

  struct Favorite {
    GURL url;
    std::u16string title;
    bookmarks::BookmarkNode::MetaInfoMap meta;

    bool operator==(const Favorite&) const = default;
  };

  class Watcher : public bookmarks::BaseBookmarkModelObserver {
   public:
    Watcher(FavoritesMirror* mirror, bookmarks::BookmarkModel* model)
        : mirror_(mirror), model_(model) {
      observation_.Observe(model);
    }
    void BookmarkModelChanged() override { mirror_->Changed(model_); }
    void ExtensiveBookmarkChangesEnded() override {
      mirror_->Synced(model_);
    }
    void BookmarkModelBeingDeleted() override {
      observation_.Reset();
      mirror_->watchers_.erase(model_);  // Deletes this.
    }

   private:
    const raw_ptr<FavoritesMirror> mirror_;
    const raw_ptr<bookmarks::BookmarkModel> model_;
    base::ScopedObservation<bookmarks::BookmarkModel,
                            bookmarks::BaseBookmarkModelObserver>
        observation_{this};
  };

  FavoritesMirror() = default;

  static const bookmarks::BookmarkNode* FindFolder(
      bookmarks::BookmarkModel* model) {
    for (const auto& child : model->other_node()->children()) {
      std::string value;
      if (child->is_folder() &&
          child->GetMetaInfo("arcomium.favorites", &value) && value == "1") {
        return child.get();
      }
    }
    return nullptr;
  }

  static std::vector<Favorite> Read(bookmarks::BookmarkModel* model) {
    std::vector<Favorite> favorites;
    const bookmarks::BookmarkNode* folder = FindFolder(model);
    if (!folder) {
      return favorites;
    }
    for (const auto& child : folder->children()) {
      if (!child->is_url()) {
        continue;
      }
      Favorite favorite{child->url(), child->GetTitle(), {}};
      if (const bookmarks::BookmarkNode::MetaInfoMap* meta =
              child->GetMetaInfoMap()) {
        favorite.meta = *meta;
      }
      favorites.push_back(std::move(favorite));
    }
    return favorites;
  }

  static bool HasSet() {
    return g_browser_process->local_state()->HasPrefPath(
        kSharedFavoritesSetPref);
  }

  static std::vector<Favorite> LoadSet() {
    std::vector<Favorite> favorites;
    for (const base::Value& value :
         g_browser_process->local_state()->GetList(kSharedFavoritesSetPref)) {
      const base::DictValue* dict = value.GetIfDict();
      const std::string* url = dict ? dict->FindString("url") : nullptr;
      if (!url) {
        continue;
      }
      Favorite favorite{GURL(*url), {}, {}};
      if (const std::string* title = dict->FindString("title")) {
        favorite.title = base::UTF8ToUTF16(*title);
      }
      if (const base::DictValue* meta = dict->FindDict("meta")) {
        for (const auto [key, item] : *meta) {
          if (item.is_string()) {
            favorite.meta[key] = item.GetString();
          }
        }
      }
      if (favorite.url.is_valid()) {
        favorites.push_back(std::move(favorite));
      }
    }
    return favorites;
  }

  static void StoreSet(const std::vector<Favorite>& favorites) {
    base::ListValue list;
    for (const Favorite& favorite : favorites) {
      base::DictValue meta;
      for (const auto& [key, value] : favorite.meta) {
        meta.Set(key, value);
      }
      list.Append(base::DictValue()
                      .Set("url", favorite.url.spec())
                      .Set("title", base::UTF16ToUTF8(favorite.title))
                      .Set("meta", std::move(meta)));
    }
    g_browser_process->local_state()->SetList(kSharedFavoritesSetPref,
                                              std::move(list));
  }

  static void AddMissing(std::vector<Favorite>& set,
                         std::vector<Favorite> favorites) {
    for (Favorite& favorite : favorites) {
      if (!std::ranges::any_of(set, [&](const Favorite& f) {
            return f.url == favorite.url;
          })) {
        set.push_back(std::move(favorite));
      }
    }
  }

  // Adds `model`'s favorites the shared set lacks, then gives every Space
  // the set.
  void Join(bookmarks::BookmarkModel* model) {
    std::vector<Favorite> set = HasSet() ? LoadSet() : std::vector<Favorite>();
    AddMissing(set, Read(model));
    StoreSet(set);
    for (const auto& [other, watcher] : watchers_) {
      Apply(other, set);
    }
  }

  void Changed(bookmarks::BookmarkModel* model) {
    // Sync's changes come grouped, as do imports, which leave favorites be.
    if (applying_ || !IsOn() || !model->loaded() ||
        model->IsDoingExtensiveChanges()) {
      return;
    }
    std::vector<Favorite> favorites = Read(model);
    if (HasSet() && favorites == LoadSet()) {
      return;
    }
    StoreSet(favorites);
    for (const auto& [other, watcher] : watchers_) {
      if (other != model) {
        Apply(other, favorites);
      }
    }
  }

  // Sync changed `model`: the set follows it, so a Space that opens later
  // does not undo the change, but the other open Spaces wait for their own
  // chains.
  void Synced(bookmarks::BookmarkModel* model) {
    if (applying_ || !IsOn() || !model->loaded()) {
      return;
    }
    std::vector<Favorite> favorites;
    AddMissing(favorites, Read(model));
    StoreSet(favorites);
  }

  // Makes `model`'s favorites `set`: a favorite of the same address keeps
  // its node (and so its tab), takes the set's place, title and meta info;
  // the others are added or removed.
  void Apply(bookmarks::BookmarkModel* model,
             const std::vector<Favorite>& set) {
    if (!model->loaded() || Read(model) == set) {
      return;
    }
    base::AutoReset<bool> applying(&applying_, true);
    const bookmarks::BookmarkNode* folder = FindFolder(model);
    if (!folder) {
      bookmarks::BookmarkNode::MetaInfoMap meta = {
          {"arcomium.favorites", "1"}};
      folder = model->AddFolder(model->other_node(),
                                model->other_node()->children().size(),
                                u"Favorites", &meta);
    }
    std::vector<const bookmarks::BookmarkNode*> used;
    for (size_t i = 0; i < set.size(); ++i) {
      const bookmarks::BookmarkNode* node = nullptr;
      for (const auto& child : folder->children()) {
        if (child->is_url() && child->url() == set[i].url &&
            !std::ranges::contains(used, child.get())) {
          node = child.get();
          break;
        }
      }
      if (node) {
        const size_t index = *folder->GetIndexOf(node);
        if (index != i) {
          model->Move(node, folder, i > index ? i + 1 : i);
        }
        if (node->GetTitle() != set[i].title) {
          model->SetTitle(node, set[i].title,
                          bookmarks::metrics::BookmarkEditSource::kOther);
        }
      } else {
        node = model->AddNewURL(folder, std::min(i, folder->children().size()),
                                set[i].title, set[i].url);
      }
      if (!node->GetMetaInfoMap() || *node->GetMetaInfoMap() != set[i].meta) {
        model->SetNodeMetaInfoMap(node, set[i].meta);
      }
      used.push_back(node);
    }
    for (size_t i = folder->children().size(); i-- > 0;) {
      const bookmarks::BookmarkNode* child = folder->children()[i].get();
      if (child->is_url() && !std::ranges::contains(used, child)) {
        model->Remove(child, bookmarks::metrics::BookmarkEditSource::kOther,
                      FROM_HERE);
      }
    }
  }

  std::map<bookmarks::BookmarkModel*, std::unique_ptr<Watcher>> watchers_;
  bool applying_ = false;
};

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_SHARED_FAVORITES_H_
