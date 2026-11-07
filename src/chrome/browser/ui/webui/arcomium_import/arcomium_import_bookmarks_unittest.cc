// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/arcomium_import/arcomium_import_bookmarks.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arcomium_import {
namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

// This profile's marks of Arc and of a Chrome profile.
ImportMark ArcMark() {
  return ImportMark("key-a", kArcSourceId);
}
ImportMark ChromeMark() {
  return ImportMark("key-a", "chrome/Default");
}
// Another device's: what it imported comes here by sync.
ImportMark OtherDeviceArcMark() {
  return ImportMark("key-b", kArcSourceId);
}
ImportMark OtherDeviceChromeMark() {
  return ImportMark("key-b", "chrome/Default");
}

Node Link(const std::string& url) {
  Node node;
  node.title = u"link";
  node.url = GURL(url);
  return node;
}

Node Folder(std::u16string title, std::vector<Node> children) {
  Node node;
  node.title = std::move(title);
  node.children = std::move(children);
  return node;
}

ArcSpace Space(const std::string& id,
               std::u16string title,
               std::vector<Node> pinned,
               const std::string& icon = std::string()) {
  ArcSpace space;
  space.id = id;
  space.title = std::move(title);
  space.icon = icon;
  space.pinned = std::move(pinned);
  return space;
}

std::vector<Node> Links(const std::string& url) {
  std::vector<Node> nodes;
  nodes.push_back(Link(url));
  return nodes;
}

// Arc's Home with one link, and "Work" with a link and a folder "Docs".
std::vector<ArcSpace> ArcSpaces(const std::string& work_icon = "book") {
  std::vector<ArcSpace> spaces;
  spaces.push_back(Space("home-id", u"Home", Links("https://home.example/")));
  std::vector<Node> docs;
  docs.push_back(Link("https://docs.example/a"));
  docs.push_back(Link("https://docs.example/b"));
  std::vector<Node> work;
  work.push_back(Link("https://work.example/"));
  work.push_back(Folder(u"Docs", std::move(docs)));
  spaces.push_back(Space("work-id", u"Work", std::move(work), work_icon));
  return spaces;
}

std::string Meta(const BookmarkNode* node, const std::string& key) {
  std::string value;
  node->GetMetaInfo(key, &value);
  return value;
}

const BookmarkNode* Child(const BookmarkNode* folder,
                          const std::u16string& title) {
  for (const auto& child : folder->children()) {
    if (child->GetTitle() == title) {
      return child.get();
    }
  }
  return nullptr;
}

int CountUrls(const BookmarkNode* node) {
  int count = node->is_url();
  for (const auto& child : node->children()) {
    count += CountUrls(child.get());
  }
  return count;
}

bool HasUrl(const BookmarkNode* node, const std::string& url) {
  if (node->is_url()) {
    return node->url() == GURL(url);
  }
  for (const auto& child : node->children()) {
    if (HasUrl(child.get(), url)) {
      return true;
    }
  }
  return false;
}

class ArcomiumImportBookmarksTest : public testing::Test {
 protected:
  const BookmarkNode* other() { return model_->other_node(); }

  const BookmarkNode* AddUserURL(const BookmarkNode* parent,
                                 const std::string& url) {
    return model_->AddURL(parent, parent->children().size(), u"mine",
                          GURL(url));
  }

  // A folder or, with `url`, a link that `mark` is on.
  const BookmarkNode* AddMarked(const ImportMark& mark,
                                const BookmarkNode* parent,
                                const std::u16string& title,
                                const std::string& url = std::string()) {
    const base::Uuid uuid = base::Uuid::GenerateRandomV4();
    const BookmarkNode::MetaInfoMap meta = {{kImportedMetaKey, mark.Of(uuid)}};
    return url.empty()
               ? model_->AddFolder(parent, parent->children().size(), title,
                                   &meta, std::nullopt, uuid)
               : model_->AddURL(parent, parent->children().size(), title,
                                GURL(url), &meta, std::nullopt, uuid);
  }

  // As copy and paste in the bookmark manager does.
  const BookmarkNode* Copy(const BookmarkNode* node,
                           const BookmarkNode* parent,
                           size_t index) {
    bookmarks::BookmarkNodeData data(node);
    bookmarks::CloneBookmarkNode(model_.get(), data.elements, parent, index,
                                 /*reset_node_times=*/false);
    return parent->children()[index].get();
  }

  const ImportMark arc_ = ArcMark();
  const ImportMark chrome_ = ChromeMark();
  const ImportMark other_device_arc_ = OtherDeviceArcMark();
  const ImportMark other_device_chrome_ = OtherDeviceChromeMark();
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BookmarkModel> model_ =
      bookmarks::TestBookmarkClient::CreateModel();
};

TEST_F(ArcomiumImportBookmarksTest, MarksDependOnKeySourceAndNode) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  EXPECT_EQ(arc_.Of(uuid), ImportMark("key-a", kArcSourceId).Of(uuid));
  EXPECT_EQ(arc_.Of(uuid).size(), 32u);
  EXPECT_NE(arc_.Of(uuid), chrome_.Of(uuid));
  EXPECT_NE(arc_.Of(uuid), other_device_arc_.Of(uuid));
  EXPECT_NE(arc_.Of(uuid), arc_.Of(base::Uuid::GenerateRandomV4()));
  EXPECT_NE(arc_.OfArcSpace("a", uuid), arc_.OfArcSpace("b", uuid));
  EXPECT_EQ(ImportMark::NewKey().size(), 64u);
  EXPECT_NE(ImportMark::NewKey(), ImportMark::NewKey());
}

TEST_F(ArcomiumImportBookmarksTest, RemovingAnImportedNodeKeepsUsersNodes) {
  const BookmarkNode* dest = model_->AddFolder(other(), 0, u"dest");
  const BookmarkNode* imported = AddMarked(arc_, other(), u"Docs");
  AddMarked(arc_, imported, u"a", "https://a.example/");
  const BookmarkNode* mine = AddUserURL(imported, "https://mine.example/");
  const BookmarkNode* sub = AddMarked(arc_, imported, u"Sub");
  const BookmarkNode* user_folder = model_->AddFolder(sub, 0, u"Mine");
  // Inside the user's folder, an imported link goes with it.
  AddMarked(arc_, user_folder, u"b", "https://b.example/");
  const BookmarkNode* mine2 = AddUserURL(sub, "https://mine2.example/");
  const int64_t mine_id = mine->id();

  RemoveKeepingUnmarked(model_.get(), arc_, imported, dest);

  ASSERT_EQ(other()->children().size(), 1u);
  ASSERT_EQ(dest->children().size(), 3u);
  EXPECT_EQ(dest->children()[0].get(), mine);
  EXPECT_EQ(dest->children()[0]->id(), mine_id);
  EXPECT_EQ(dest->children()[1].get(), user_folder);
  EXPECT_EQ(user_folder->children().size(), 1u);
  EXPECT_EQ(dest->children()[2].get(), mine2);
}

TEST_F(ArcomiumImportBookmarksTest, ImportsArcSpaces) {
  ArcResult result = ImportArcSpaces(model_.get(), arc_, ArcSpaces());
  EXPECT_EQ(result.links, 4);
  EXPECT_EQ(result.folders, 1);
  ASSERT_EQ(other()->children().size(), 2u);
  const BookmarkNode* home = other()->children()[0].get();
  const BookmarkNode* work = other()->children()[1].get();
  EXPECT_EQ(Meta(home, kSpaceMetaKey), kHomeSpace);
  EXPECT_EQ(Meta(work, kSpaceMetaKey), "arc:work-id");
  EXPECT_TRUE(arc_.IsOn(work));
  EXPECT_TRUE(arc_.IsOn(Child(work, u"Docs")->children()[0].get()));
  EXPECT_EQ(arc_.ArcSpaceOf(home), "home-id");
  EXPECT_EQ(arc_.ArcSpaceOf(work), "work-id");
  EXPECT_EQ(other_device_arc_.ArcSpaceOf(work), "");
  EXPECT_EQ(Meta(work, kSpaceIconMetaKey), "book");
  ASSERT_EQ(result.unadopted.size(), 1u);
  EXPECT_EQ(result.unadopted[0].first, work);
}

TEST_F(ArcomiumImportBookmarksTest, ImportingArcAgainKeepsUsersNodes) {
  ImportArcSpaces(model_.get(), arc_, ArcSpaces());
  const BookmarkNode* work = other()->children()[1].get();
  // Adopted by a Space; the user puts items in the Space and in "Docs".
  model_->SetNodeMetaInfo(work, kSpaceMetaKey, "space-token");
  AddUserURL(work, "https://in-space.example/");
  const BookmarkNode* in_docs =
      AddUserURL(Child(work, u"Docs"), "https://in-docs.example/");
  const int64_t in_docs_id = in_docs->id();

  ArcResult result = ImportArcSpaces(model_.get(), arc_, ArcSpaces());

  EXPECT_TRUE(result.unadopted.empty());
  ASSERT_EQ(other()->children().size(), 2u);
  ASSERT_EQ(other()->children()[1].get(), work);
  // Arc's 3 links, the user's 2, nothing twice.
  EXPECT_EQ(CountUrls(work), 5);
  EXPECT_EQ(CountUrls(Child(work, u"Docs")), 2);
  const BookmarkNode* moved = work->children().back().get();
  EXPECT_EQ(moved->id(), in_docs_id);
  EXPECT_EQ(moved->url(), GURL("https://in-docs.example/"));
}

TEST_F(ArcomiumImportBookmarksTest, ImportingArcAgainKeepsTheSpaceFolder) {
  ImportArcSpaces(model_.get(), arc_, ArcSpaces());
  const BookmarkNode* home = other()->children()[0].get();
  const BookmarkNode* work = other()->children()[1].get();
  const int64_t home_id = home->id();
  const int64_t work_id = work->id();
  model_->SetNodeMetaInfo(work, kSpaceMetaKey, "space-token");
  model_->SetTitle(work, u"Job", bookmarks::metrics::BookmarkEditSource::kUser);
  model_->SetNodeMetaInfo(work, "arcomium.space_color", "5");
  model_->SetNodeMetaInfo(work, "arcomium.group", "group-uuid");
  // The folder the user put before the Space's.
  model_->Move(work, other(), 0);

  ImportArcSpaces(model_.get(), arc_, ArcSpaces("sunny"));

  ASSERT_EQ(other()->children().size(), 2u);
  EXPECT_EQ(other()->children()[0]->id(), work_id);
  EXPECT_EQ(other()->children()[1]->id(), home_id);
  EXPECT_EQ(work->GetTitle(), u"Job");
  EXPECT_EQ(Meta(work, kSpaceMetaKey), "space-token");
  EXPECT_EQ(Meta(work, "arcomium.space_color"), "5");
  EXPECT_EQ(Meta(work, "arcomium.group"), "group-uuid");
  // The user did not pick an icon: Arc's new one shows.
  EXPECT_EQ(Meta(work, kSpaceIconMetaKey), "sunny");

  model_->SetNodeMetaInfo(work, kSpaceIconMetaKey, "star");
  ImportArcSpaces(model_.get(), arc_, ArcSpaces("book"));
  EXPECT_EQ(Meta(work, kSpaceIconMetaKey), "star");
  EXPECT_EQ(Meta(work, kArcIconMetaKey), "book");
  EXPECT_EQ(CountUrls(work), 3);
}

TEST_F(ArcomiumImportBookmarksTest, ImportsIntoTheUsersHomeAndSpaceByName) {
  const BookmarkNode::MetaInfoMap home_meta = {{kSpaceMetaKey, kHomeSpace}};
  const BookmarkNode* home = model_->AddFolder(other(), 0, u"home", &home_meta);
  const BookmarkNode* mine = AddUserURL(home, "https://mine.example/");
  const BookmarkNode::MetaInfoMap work_meta = {{kSpaceMetaKey, "user-space"}};
  const BookmarkNode* work = model_->AddFolder(other(), 1, u"Work", &work_meta);

  for (int i = 0; i < 2; ++i) {
    ArcResult result = ImportArcSpaces(model_.get(), arc_, ArcSpaces());
    // No new Space and no new folder; importing again doubles nothing.
    EXPECT_TRUE(result.unadopted.empty());
    ASSERT_EQ(other()->children().size(), 2u);
    EXPECT_EQ(CountUrls(home), 2);
    EXPECT_EQ(home->children()[0].get(), mine);
    EXPECT_EQ(CountUrls(work), 3);
    EXPECT_EQ(Meta(work, kSpaceMetaKey), "user-space");
    EXPECT_FALSE(arc_.IsOn(work));
    EXPECT_EQ(arc_.ArcSpaceOf(work), "work-id");
    // The user's own Home keeps its icon.
    EXPECT_EQ(Meta(home, kSpaceIconMetaKey), "");
  }
}

TEST_F(ArcomiumImportBookmarksTest, MatchesByArcIdBeforeName) {
  // Arc's Space "job-id" was imported as "Work" and adopted.
  const BookmarkNode* adopted = AddMarked(arc_, other(), u"Work");
  model_->SetNodeMetaInfo(adopted, kSpaceMetaKey, "space-token");
  model_->SetNodeMetaInfo(adopted, kArcSpaceMetaKey, "job-id");
  model_->SetNodeMetaInfo(adopted, kArcSpaceMarkMetaKey,
                          arc_.OfArcSpace("job-id", adopted->uuid()));

  // Arc renamed it "Job" and has a new "Work" listed first.
  std::vector<ArcSpace> spaces;
  spaces.push_back(Space("home-id", u"Home", {}));
  spaces.push_back(Space("work-id", u"Work", Links("https://work.example/")));
  spaces.push_back(Space("job-id", u"Job", Links("https://job.example/")));
  ArcResult result = ImportArcSpaces(model_.get(), arc_, spaces);

  ASSERT_EQ(adopted->children().size(), 1u);
  EXPECT_EQ(adopted->children()[0]->url(), GURL("https://job.example/"));
  ASSERT_EQ(result.unadopted.size(), 1u);
  EXPECT_EQ(Meta(result.unadopted[0].first, kArcSpaceMetaKey), "work-id");
}

TEST_F(ArcomiumImportBookmarksTest, KeepsFoldersOfSpacesArcNoLongerHas) {
  ImportArcSpaces(model_.get(), arc_, ArcSpaces());
  const BookmarkNode* work = other()->children()[1].get();
  model_->SetNodeMetaInfo(work, kSpaceMetaKey, "space-token");
  AddUserURL(Child(work, u"Docs"), "https://in-docs.example/");

  std::vector<ArcSpace> spaces = ArcSpaces();
  spaces.pop_back();
  ImportArcSpaces(model_.get(), arc_, spaces);

  ASSERT_EQ(other()->children().size(), 2u);
  ASSERT_EQ(work->children().size(), 1u);
  EXPECT_EQ(work->children()[0]->url(), GURL("https://in-docs.example/"));
  EXPECT_EQ(Meta(work, kImportedMetaKey), "");
  EXPECT_EQ(Meta(work, kArcSpaceMetaKey), "");
}

TEST_F(ArcomiumImportBookmarksTest, FollowsSpacesArcReordered) {
  ImportArcSpaces(model_.get(), arc_, ArcSpaces());
  const BookmarkNode* home = other()->children()[0].get();
  const BookmarkNode* work = other()->children()[1].get();
  model_->SetNodeMetaInfo(work, kSpaceMetaKey, "space-token");

  // "Work" now comes first in Arc.
  std::vector<ArcSpace> spaces = ArcSpaces();
  std::swap(spaces[0], spaces[1]);
  for (int i = 0; i < 2; ++i) {
    ArcResult result = ImportArcSpaces(model_.get(), arc_, spaces);
    EXPECT_TRUE(result.unadopted.empty());
    ASSERT_EQ(other()->children().size(), 2u);
    EXPECT_EQ(CountUrls(home), 1);
    EXPECT_TRUE(HasUrl(home, "https://home.example/"));
    EXPECT_TRUE(HasUrl(work, "https://work.example/"));
    EXPECT_FALSE(HasUrl(home, "https://work.example/"));
    EXPECT_EQ(arc_.ArcSpaceOf(home), "home-id");
    EXPECT_EQ(arc_.ArcSpaceOf(work), "work-id");
  }
}

TEST_F(ArcomiumImportBookmarksTest, HomeTakesTheFirstSpaceWhenItsOwnIsGone) {
  ImportArcSpaces(model_.get(), arc_, ArcSpaces());
  const BookmarkNode* home = other()->children()[0].get();
  const BookmarkNode* work = other()->children()[1].get();
  model_->SetNodeMetaInfo(work, kSpaceMetaKey, "space-token");

  std::vector<ArcSpace> spaces;
  spaces.push_back(Space("new-id", u"New", Links("https://new.example/")));
  spaces.push_back(Space("work-id", u"Work", Links("https://work.example/")));
  ArcResult result = ImportArcSpaces(model_.get(), arc_, spaces);

  EXPECT_TRUE(result.unadopted.empty());
  ASSERT_EQ(other()->children().size(), 2u);
  EXPECT_EQ(CountUrls(home), 1);
  EXPECT_TRUE(HasUrl(home, "https://new.example/"));
  EXPECT_EQ(arc_.ArcSpaceOf(home), "new-id");
  EXPECT_EQ(CountUrls(work), 1);
}

TEST_F(ArcomiumImportBookmarksTest, UnbindsTheUsersFolderOfAGoneSpace) {
  const BookmarkNode::MetaInfoMap work_meta = {{kSpaceMetaKey, "user-space"}};
  const BookmarkNode* work = model_->AddFolder(other(), 0, u"Work", &work_meta);
  AddUserURL(work, "https://mine.example/");
  ImportArcSpaces(model_.get(), arc_, ArcSpaces());
  ASSERT_EQ(CountUrls(work), 4);

  // Arc's "Work" is deleted.
  std::vector<ArcSpace> spaces = ArcSpaces();
  spaces.pop_back();
  ImportArcSpaces(model_.get(), arc_, spaces);
  ASSERT_EQ(work->parent(), other());
  EXPECT_EQ(CountUrls(work), 1);
  EXPECT_TRUE(HasUrl(work, "https://mine.example/"));
  EXPECT_EQ(Meta(work, kArcSpaceMetaKey), "");
  EXPECT_EQ(Meta(work, kSpaceMetaKey), "user-space");

  // A new "Work" in Arc finds it by name again.
  spaces = ArcSpaces();
  spaces[1].id = "new-work-id";
  ArcResult result = ImportArcSpaces(model_.get(), arc_, spaces);
  EXPECT_TRUE(result.unadopted.empty());
  EXPECT_EQ(CountUrls(work), 4);
  EXPECT_EQ(arc_.ArcSpaceOf(work), "new-work-id");
}

TEST_F(ArcomiumImportBookmarksTest, LeavesWhatAnotherDeviceImported) {
  // Device B's import, synced here: its Home and a Space's folder.
  ImportArcSpaces(model_.get(), other_device_arc_, ArcSpaces());
  const BookmarkNode* synced_home = other()->children()[0].get();
  const BookmarkNode* synced_work = other()->children()[1].get();
  model_->SetNodeMetaInfo(synced_work, kSpaceMetaKey, "space-token");

  // This device's Arc has other Spaces.
  std::vector<ArcSpace> spaces;
  spaces.push_back(Space("h2", u"Home", Links("https://h2.example/")));
  spaces.push_back(Space("x2", u"Other", Links("https://x2.example/")));
  for (int i = 0; i < 2; ++i) {
    ImportArcSpaces(model_.get(), arc_, spaces);
  }

  // B's items are all there, once; this device's are added next to them.
  EXPECT_EQ(CountUrls(synced_home), 2);
  EXPECT_TRUE(HasUrl(synced_home, "https://home.example/"));
  EXPECT_TRUE(HasUrl(synced_home, "https://h2.example/"));
  EXPECT_EQ(CountUrls(synced_work), 3);
  EXPECT_TRUE(other_device_arc_.IsOn(synced_work));
  EXPECT_EQ(Meta(synced_work, kArcSpaceMetaKey), "work-id");
  ASSERT_EQ(other()->children().size(), 3u);
}

TEST_F(ArcomiumImportBookmarksTest, LeavesCopiesOfImportedNodes) {
  ImportArcSpaces(model_.get(), arc_, ArcSpaces());
  const BookmarkNode* home = other()->children()[0].get();
  const BookmarkNode* work = other()->children()[1].get();
  model_->SetNodeMetaInfo(work, kSpaceMetaKey, "space-token");
  const int64_t work_id = work->id();
  // A backup of the Space's folder before it, a copy of "Docs" at the top
  // and one in Home.
  const BookmarkNode* backup = Copy(work, other(), 0);
  const BookmarkNode* docs_copy = Copy(Child(work, u"Docs"), other(), 0);
  const BookmarkNode* in_home = Copy(Child(work, u"Docs"), home, 0);
  ASSERT_EQ(Meta(backup, kArcSpaceMetaKey), "work-id");
  ASSERT_FALSE(arc_.IsOn(backup));

  for (int i = 0; i < 2; ++i) {
    ArcResult result = ImportArcSpaces(model_.get(), arc_, ArcSpaces());
    EXPECT_TRUE(result.unadopted.empty());
    ASSERT_EQ(other()->children().size(), 4u);
    EXPECT_EQ(other()->children()[0].get(), docs_copy);
    EXPECT_EQ(other()->children()[1].get(), backup);
    EXPECT_EQ(work->id(), work_id);
    EXPECT_EQ(CountUrls(work), 3);
    EXPECT_EQ(CountUrls(backup), 3);
    EXPECT_EQ(CountUrls(docs_copy), 2);
    EXPECT_EQ(in_home->parent(), home);
    EXPECT_EQ(CountUrls(home), 3);
  }
}

TEST_F(ArcomiumImportBookmarksTest, AnArcSpaceWithoutAnIdTakesNoFolder) {
  const BookmarkNode::MetaInfoMap work_meta = {{kSpaceMetaKey, "user-space"}};
  const BookmarkNode* work = model_->AddFolder(other(), 0, u"Work", &work_meta);
  std::vector<ArcSpace> spaces;
  spaces.push_back(Space("home-id", u"Home", {}));
  spaces.push_back(Space("", u"Reading", Links("https://read.example/")));
  ImportArcSpaces(model_.get(), arc_, spaces);
  EXPECT_TRUE(work->children().empty());
  EXPECT_EQ(Meta(work, kArcSpaceMetaKey), "");
  EXPECT_EQ(Meta(work, kSpaceIconMetaKey), "");
}

TEST_F(ArcomiumImportBookmarksTest, ImportingABrowserAgainKeepsUsersNodes) {
  const BookmarkNode::MetaInfoMap meta = {{kImportMetaKey, "chrome/Default"}};
  const BookmarkNode* folder =
      model_->AddFolder(other(), 0, u"Google Chrome", &meta);
  auto nodes = [] {
    std::vector<Node> nodes;
    nodes.push_back(Link("https://a.example/"));
    nodes.push_back(Folder(u"Tools", Links("https://b.example/")));
    return nodes;
  };
  ReplaceImportedBookmarks(model_.get(), chrome_, folder, nodes());
  EXPECT_TRUE(chrome_.IsOn(folder->children()[1].get()));
  EXPECT_FALSE(arc_.IsOn(folder->children()[1].get()));

  const BookmarkNode* mine = AddUserURL(folder, "https://mine.example/");
  const BookmarkNode* in_tools =
      AddUserURL(Child(folder, u"Tools"), "https://in-tools.example/");
  const ImportMark edge_mark("key-a", "edge/Default");
  const BookmarkNode* edge = AddMarked(edge_mark, folder, u"Edge");
  model_->SetNodeMetaInfo(edge, kImportMetaKey, "edge/Default");
  AddMarked(edge_mark, edge, u"e", "https://e.example/");
  // Copies the user made of an imported link, and one of another device's.
  const BookmarkNode* copy = Copy(folder->children()[0].get(), folder, 0);
  const BookmarkNode* synced =
      AddMarked(other_device_chrome_, folder, u"b", "https://synced.example/");

  ReplaceImportedBookmarks(model_.get(), chrome_, folder, nodes());

  // Chrome's 2 links once, the user's 2, the copy, the synced link, Edge's
  // folder as it was.
  EXPECT_EQ(CountUrls(folder), 7);
  EXPECT_EQ(CountUrls(Child(folder, u"Tools")), 1);
  EXPECT_EQ(mine->parent(), folder);
  EXPECT_EQ(in_tools->parent(), folder);
  EXPECT_EQ(copy->parent(), folder);
  EXPECT_EQ(synced->parent(), folder);
  EXPECT_EQ(edge->parent(), folder);
  EXPECT_EQ(edge->children().size(), 1u);
}

TEST_F(ArcomiumImportBookmarksTest, FindsOnlyTheRecordedImportFolder) {
  const BookmarkNode::MetaInfoMap meta = {{kImportMetaKey, "chrome/Default"}};
  const BookmarkNode* synced = model_->AddFolder(other(), 0, u"Chrome", &meta);
  const BookmarkNode* own = model_->AddFolder(other(), 1, u"Chrome", &meta);
  const std::string uuid = own->uuid().AsLowercaseString();
  EXPECT_EQ(FindImportFolder(model_.get(), "chrome/Default", uuid), own);
  EXPECT_EQ(FindImportFolder(model_.get(), "edge/Default", uuid), nullptr);
  EXPECT_NE(FindImportFolder(model_.get(), "chrome/Default", uuid), synced);
  EXPECT_EQ(FindImportFolder(model_.get(), "chrome/Default", "x"), nullptr);
  // A copy of the folder is not it.
  const BookmarkNode* copy = Copy(own, other(), 0);
  EXPECT_EQ(FindImportFolder(model_.get(), "chrome/Default", uuid), own);
  EXPECT_NE(copy->uuid().AsLowercaseString(), uuid);
}

}  // namespace
}  // namespace arcomium_import
