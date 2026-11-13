// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_import/alerse_import_data.h"

#include <algorithm>
#include <array>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/md5.h"

namespace alerse_import {
namespace {

base::Value Json(std::string_view json) {
  std::optional<base::Value> value =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  CHECK(value);
  return std::move(*value);
}

// A sidebar in Arc's layout: maps as flat [key, value, ...] lists.
constexpr char kSidebar[] = R"({"sidebar": {"containers": [
  {"global": {}},
  {"spaces": [
     "home-id", {"title": "Home", "containerIDs": ["unpinned", "u1", "pinned", "p1"],
                 "customInfo": {"iconType": {"icon": "sunny"}}},
     "work-id", {"title": "Work", "containerIDs": ["pinned", "p2"]},
     "untitled", {"containerIDs": ["pinned", "p3"]},
     "no-pins", {"title": "Empty", "containerIDs": ["unpinned", "u4"]}
   ],
   "items": [
     "p1", {"childrenIds": ["t1", "f1", "missing", "t-empty"]},
     "t1", {"title": "Mail", "data": {"tab": {"savedURL": "https://mail.example/"}}},
     "f1", {"title": "Docs", "childrenIds": ["t2", "t3"],
            "data": {"list": {"customInfo": {"iconType": {"icon": "book"}}}}},
     "t2", {"data": {"tab": {"savedURL": "https://docs.example/a",
                             "savedTitle": "Doc A"}}},
     "t3", {"data": {"tab": {"savedURL": "arc://settings"}}},
     "t-empty", {"title": "Nothing", "data": {"tab": {"savedURL": ""}}},
     "p2", {"childrenIds": ["loop"]},
     "loop", {"title": "Loop", "childrenIds": ["loop"], "data": {"list": {}}},
     "p3", {"childrenIds": ["t1"]}
   ]}
]}})";

TEST(AlerseImportDataTest, ParsesArcSpacesInOrder) {
  std::vector<ArcSpace> spaces = ParseArcSidebar(Json(kSidebar));
  ASSERT_EQ(spaces.size(), 2u);

  EXPECT_EQ(spaces[0].id, "home-id");
  EXPECT_EQ(spaces[0].title, u"Home");
  EXPECT_EQ(spaces[0].icon, "sunny");
  ASSERT_EQ(spaces[0].pinned.size(), 2u);

  const Node& mail = spaces[0].pinned[0];
  EXPECT_EQ(mail.title, u"Mail");
  EXPECT_EQ(mail.url, GURL("https://mail.example/"));
  EXPECT_EQ(mail.arc_item, "t1");

  const Node& docs = spaces[0].pinned[1];
  EXPECT_TRUE(docs.is_folder());
  EXPECT_EQ(docs.title, u"Docs");
  EXPECT_EQ(docs.meta.at(kFolderIconMetaKey), "book");
  ASSERT_EQ(docs.children.size(), 2u);
  EXPECT_EQ(docs.children[0].title, u"Doc A");
  // arc:// pages are Alerse's chrome:// ones; a tab without a title shows
  // its address.
  EXPECT_EQ(docs.children[1].url, GURL("chrome://settings"));
  EXPECT_EQ(docs.children[1].title, u"chrome://settings");

  EXPECT_EQ(CountLinks(spaces[0].pinned), 3);
  EXPECT_EQ(CountFolders(spaces[0].pinned), 1);
}

TEST(AlerseImportDataTest, StopsAtFoldersThatContainThemselves) {
  std::vector<ArcSpace> spaces = ParseArcSidebar(Json(kSidebar));
  ASSERT_EQ(spaces.size(), 2u);
  ASSERT_EQ(spaces[1].pinned.size(), 1u);
  EXPECT_EQ(spaces[1].pinned[0].title, u"Loop");
  EXPECT_TRUE(spaces[1].pinned[0].children.empty());
}

TEST(AlerseImportDataTest, KeepsEmptyFoldersListedTwice) {
  std::vector<ArcSpace> spaces = ParseArcSidebar(Json(R"({"sidebar": {
    "containers": [{"spaces": ["s", {"title": "S",
                                     "containerIDs": ["pinned", "p"]}],
                    "items": ["p", {"childrenIds": ["e", "e"]},
                              "e", {"title": "Empty", "data": {"list": {}}}]}]
  }})"));
  ASSERT_EQ(spaces.size(), 1u);
  EXPECT_EQ(spaces[0].pinned.size(), 2u);
}

TEST(AlerseImportDataTest, BoundsFoldersListedOverAndOver) {
  // Each folder lists the next one twice: 2^40 items unless cut.
  std::string items = R"("p", {"childrenIds": ["f0"]})";
  for (int i = 0; i < 40; ++i) {
    const std::string next = "\"f" + base::NumberToString(i + 1) + "\"";
    items += ", \"f" + base::NumberToString(i) + R"(", {"childrenIds": [)" +
             next + ", " + next + R"(], "data": {"list": {}}})";
  }
  std::vector<ArcSpace> spaces = ParseArcSidebar(Json(
      R"({"sidebar": {"containers": [{"spaces": ["s", {"title": "S",
          "containerIDs": ["pinned", "p"]}], "items": [)" +
      items + "]}]}}"));
  ASSERT_EQ(spaces.size(), 1u);
  EXPECT_LE(CountFolders(spaces[0].pinned), kMaxArcItems);
}

// A container with Space `space` whose pinned folder lists the next folder
// twice, `depth` times over, and the last one a tab.
std::string DoublingContainer(const std::string& space, int depth) {
  std::string items = R"("p", {"childrenIds": ["f0"]})";
  for (int i = 0; i < depth; ++i) {
    const std::string next = "\"f" + base::NumberToString(i + 1) + "\"";
    items += ", \"f" + base::NumberToString(i) + R"(", {"childrenIds": [)" +
             next + ", " + next + R"(], "data": {"list": {}}})";
  }
  items += ", \"f" + base::NumberToString(depth) +
           R"(", {"childrenIds": ["t"], "data": {"list": {}}},
              "t", {"data": {"tab": {"savedURL": "https://a.example/"}}})";
  return R"({"spaces": [")" + space +
         R"(", {"title": "S", "containerIDs": ["pinned", "p"]}], "items": [)" +
         items + "]}";
}

TEST(AlerseImportDataTest, BoundsTheWholeSidebarNotEachContainer) {
  std::string containers;
  for (int i = 0; i < 8; ++i) {
    containers += (i ? ", " : "") + DoublingContainer(
                                        "s" + base::NumberToString(i), 20);
  }
  std::vector<ArcSpace> spaces = ParseArcSidebar(
      Json(R"({"sidebar": {"containers": [)" + containers + "]}}"));
  ASSERT_FALSE(spaces.empty());
  int items = 0;
  for (const ArcSpace& space : spaces) {
    items += CountLinks(space.pinned) + CountFolders(space.pinned);
  }
  EXPECT_GT(items, 0);
  EXPECT_LE(items, kMaxArcItems);
  // The first container spends the budget; the rest are not read.
  EXPECT_EQ(spaces.size(), 1u);
}

TEST(AlerseImportDataTest, KeepsATabOncePerFolder) {
  std::vector<ArcSpace> spaces = ParseArcSidebar(Json(R"({"sidebar": {
    "containers": [{"spaces": ["s", {"title": "S",
                                     "containerIDs": ["pinned", "p"]}],
                    "items": [
      "p", {"childrenIds": ["t", "t", "f", "t"]},
      "f", {"title": "F", "childrenIds": ["t", "t"], "data": {"list": {}}},
      "t", {"data": {"tab": {"savedURL": "https://a.example/"}}}]}]
  }})"));
  ASSERT_EQ(spaces.size(), 1u);
  ASSERT_EQ(spaces[0].pinned.size(), 2u);
  EXPECT_EQ(spaces[0].pinned[0].url, GURL("https://a.example/"));
  // Another folder may list it.
  EXPECT_EQ(spaces[0].pinned[1].children.size(), 1u);
}

// The bytes of text in `nodes`, as kMaxArcBytes counts them.
size_t CountTextBytes(const std::vector<Node>& nodes, size_t& longest_title) {
  size_t bytes = 0;
  for (const Node& node : nodes) {
    bytes += node.title.size() * sizeof(char16_t) + node.url.spec().size() +
             node.arc_item.size() + CountTextBytes(node.children, longest_title);
    longest_title = std::max(longest_title, node.title.size());
  }
  return bytes;
}

TEST(AlerseImportDataTest, BoundsTheTextOfItemsListedOverAndOver) {
  // 2^16 copies of a tab with a 100 KB title and a 100 KB address: 13 GB of
  // text unless cut.
  const std::string long_text(100 * 1024, 'x');
  std::string items = R"("p", {"childrenIds": ["f0"]})";
  for (int i = 0; i < 16; ++i) {
    const std::string next = "\"f" + base::NumberToString(i + 1) + "\"";
    items += ", \"f" + base::NumberToString(i) + R"(", {"childrenIds": [)" +
             next + ", " + next + R"(], "data": {"list": {}}})";
  }
  items += R"(, "f16", {"childrenIds": ["t"], "data": {"list": {}}}, "t", {)" +
           std::string(R"("title": ")") + long_text +
           R"(", "data": {"tab": {"savedURL": "https://a.example/)" +
           long_text + R"("}}})";
  std::vector<ArcSpace> spaces = ParseArcSidebar(Json(
      R"({"sidebar": {"containers": [{"spaces": ["s", {"title": "S",
          "containerIDs": ["pinned", "p"]}], "items": [)" +
      items + "]}]}}"));
  ASSERT_EQ(spaces.size(), 1u);
  EXPECT_GT(CountLinks(spaces[0].pinned), 0);
  size_t longest_title = 0;
  EXPECT_LE(CountTextBytes(spaces[0].pinned, longest_title), kMaxArcBytes);
  EXPECT_LE(longest_title, kMaxTitleBytes);
}

TEST(AlerseImportDataTest, SkipsSpacesWithoutAUsableId) {
  std::vector<ArcSpace> spaces = ParseArcSidebar(Json(R"({"sidebar": {
    "containers": [{"spaces": [
        "", {"title": "No id", "containerIDs": ["pinned", "p"]},
        "a", {"title": "A", "containerIDs": ["pinned", "p"]},
        "a", {"title": "A again", "containerIDs": ["pinned", "p"]}],
                    "items": ["p", {"childrenIds": []}]}]
  }})"));
  ASSERT_EQ(spaces.size(), 1u);
  EXPECT_EQ(spaces[0].id, "a");
  EXPECT_EQ(spaces[0].title, u"A");
}

int CountIconBytes(const std::vector<Node>& nodes, int& icons) {
  int bytes = 0;
  for (const Node& node : nodes) {
    if (auto it = node.meta.find(kIconMetaKey); it != node.meta.end()) {
      bytes += it->second.size();
      ++icons;
    }
    bytes += CountIconBytes(node.children, icons);
  }
  return bytes;
}

TEST(AlerseImportDataTest, BoundsTheIconsOfOneRead) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  // 200 folders list one tab with a 60 KB icon: 16 MB of base64 if each got
  // its copy.
  std::string items = R"("p", {"childrenIds": [)";
  for (int i = 0; i < 200; ++i) {
    items += (i ? ", \"f" : "\"f") + base::NumberToString(i) + "\"";
  }
  items += "]}";
  for (int i = 0; i < 200; ++i) {
    items += ", \"f" + base::NumberToString(i) +
             R"(", {"childrenIds": ["t"], "data": {"list": {}}})";
  }
  items += R"(, "t", {"data": {"tab": {"savedURL": "https://a.example/"}}})";
  ASSERT_TRUE(base::WriteFile(
      dir.GetPath().AppendASCII("StorableSidebar.json"),
      R"({"sidebar": {"containers": [{"spaces": ["s", {"title": "S",
          "containerIDs": ["pinned", "p"]}], "items": [)" +
          items + "]}]}}"));
  const base::FilePath icons =
      dir.GetPath().AppendASCII("SidebarItemsFaviconCache");
  ASSERT_TRUE(base::CreateDirectory(icons));
  std::array<uint8_t, MD5_DIGEST_LENGTH> digest;
  MD5(reinterpret_cast<const uint8_t*>("t"), 1, digest.data());
  std::string png = "\x89PNG";
  png.resize(60 * 1024, 'x');
  ASSERT_TRUE(
      base::WriteFile(icons.AppendASCII(base::HexEncodeLower(digest)), png));

  std::optional<std::vector<ArcSpace>> spaces =
      ReadArc(dir.GetPath(), /*with_icons=*/true);
  ASSERT_TRUE(spaces);
  ASSERT_EQ(spaces->size(), 1u);
  EXPECT_EQ(CountLinks((*spaces)[0].pinned), 200);
  int count = 0;
  const int bytes = CountIconBytes((*spaces)[0].pinned, count);
  EXPECT_GT(count, 0);
  EXPECT_LT(count, 200);
  EXPECT_LE(static_cast<size_t>(bytes), kMaxArcIconBytes);
}

TEST(AlerseImportDataTest, IgnoresWhatIsNotASidebar) {
  EXPECT_TRUE(ParseArcSidebar(Json("[]")).empty());
  EXPECT_TRUE(ParseArcSidebar(Json(R"({"sidebar": {}})")).empty());
  EXPECT_TRUE(
      ParseArcSidebar(Json(R"({"sidebar": {"containers": [{"spaces": 1}]}})"))
          .empty());
}

constexpr char kBookmarks[] = R"({"version": 1, "roots": {
  "bookmark_bar": {"type": "folder", "name": "Bookmarks bar", "children": [
    {"type": "url", "name": "News", "url": "https://news.example/"},
    {"type": "folder", "name": "Tools", "children": [
      {"type": "url", "name": "", "url": "https://tools.example/"},
      {"type": "url", "name": "Broken", "url": "not a url"}
    ]}
  ]},
  "other": {"type": "folder", "name": "Other bookmarks", "children": [
    {"type": "url", "name": "Recipes", "url": "https://food.example/"}
  ]},
  "synced": {"type": "folder", "name": "Mobile bookmarks", "children": []}
}})";

// Writes an extension's version directory under `extensions`.
void WriteExtension(const base::FilePath& extensions,
                    std::string_view id,
                    std::string_view version,
                    std::string_view manifest,
                    bool signed_list) {
  const base::FilePath dir =
      extensions.AppendASCII(id).AppendASCII(version);
  ASSERT_TRUE(base::CreateDirectory(dir.AppendASCII("_metadata")));
  ASSERT_TRUE(base::WriteFile(dir.AppendASCII("manifest.json"), manifest));
  if (signed_list) {
    ASSERT_TRUE(base::WriteFile(
        dir.AppendASCII("_metadata").AppendASCII("verified_contents.json"),
        "[]"));
  }
}

TEST(AlerseImportDataTest, FindsWebStoreExtensionsOfArcProfiles) {
  base::ScopedTempDir arc;
  ASSERT_TRUE(arc.CreateUniqueTempDir());
  const base::FilePath user_data = arc.GetPath().AppendASCII("User Data");
  const base::FilePath first = user_data.AppendASCII("Default")
                                   .AppendASCII("Extensions");
  const base::FilePath second = user_data.AppendASCII("Profile 1")
                                    .AppendASCII("Extensions");
  // Split so that domain substitution leaves the address as it is.
  const std::string store =
      std::string(R"("update_url": "https://clients2.goo)") +
      R"(gle.com/service/update2/crx")";
  // The newest whole version wins; a localized name is looked up.
  WriteExtension(first, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "1.2.0_0",
                 R"({"name": "Old", "key": "K1", )" + store + "}", true);
  WriteExtension(first, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "1.10.0_0",
                 R"({"name": "__MSG_appName__", "default_locale": "en",
                     "key": "K1", )" + store + "}",
                 true);
  ASSERT_TRUE(base::CreateDirectory(first.AppendASCII(
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa").AppendASCII("1.10.0_0")
          .AppendASCII("_locales").AppendASCII("en")));
  ASSERT_TRUE(base::WriteFile(
      first.AppendASCII("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
          .AppendASCII("1.10.0_0").AppendASCII("_locales").AppendASCII("en")
          .AppendASCII("messages.json"),
      R"({"appname": {"message": "Reader"}})"));
  // A version being updated has no manifest yet.
  ASSERT_TRUE(base::CreateDirectory(first.AppendASCII(
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa").AppendASCII("2.0.0_0")));
  // Not the store's: another update address, no signed list, no key, an id
  // that is no extension id.
  WriteExtension(first, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "1.0_0",
                 R"({"name": "Elsewhere", "key": "K2",
                     "update_url": "https://example.com/u"})",
                 true);
  WriteExtension(first, "cccccccccccccccccccccccccccccccc", "1.0_0",
                 R"({"name": "Unsigned", "key": "K3", )" + store + "}", false);
  WriteExtension(first, "dddddddddddddddddddddddddddddddd", "1.0_0",
                 R"({"name": "No key", )" + store + "}", true);
  WriteExtension(first, "Temp", "1.0_0",
                 R"({"name": "Temp", "key": "K4", )" + store + "}", true);
  // The same extension in a second profile is listed once.
  WriteExtension(second, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "1.10.0_0",
                 R"({"name": "Again", "key": "K1", )" + store + "}", true);
  WriteExtension(second, "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", "3_0",
                 "\xEF\xBB\xBF" R"({"name": "Lister", "key": "K5", )" + store +
                     "}",
                 true);

  Source source;
  source.kind = SourceKind::kArc;
  source.path = arc.GetPath();
  const std::vector<SourceExtension> found = FindExtensions(source);
  ASSERT_EQ(found.size(), 2u);
  EXPECT_EQ(found[0].name, "Lister");
  EXPECT_EQ(found[0].id, "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  EXPECT_EQ(found[1].name, "Reader");
  EXPECT_EQ(found[1].public_key, "K1");
  EXPECT_EQ(found[1].dir.BaseName().AsUTF8Unsafe(), "1.10.0_0");
}

TEST(AlerseImportDataTest, ParsesChromiumBookmarks) {
  std::optional<Node> root =
      ParseChromiumBookmarks(Json(kBookmarks), u"Other", u"Mobile");
  ASSERT_TRUE(root);
  // The bar's items, then the other bookmarks in their folder; the empty
  // mobile folder is left out.
  ASSERT_EQ(root->children.size(), 3u);
  EXPECT_EQ(root->children[0].title, u"News");
  EXPECT_EQ(root->children[1].title, u"Tools");
  ASSERT_EQ(root->children[1].children.size(), 1u);
  EXPECT_EQ(root->children[1].children[0].title, u"https://tools.example/");
  EXPECT_EQ(root->children[2].title, u"Other bookmarks");
  EXPECT_EQ(root->children[2].children[0].url, GURL("https://food.example/"));
  EXPECT_EQ(CountLinks(root->children), 3);
  EXPECT_EQ(CountFolders(root->children), 2);
}

TEST(AlerseImportDataTest, NamesUnnamedRootFolders) {
  std::optional<Node> root =
      ParseChromiumBookmarks(Json(R"({"roots": {"synced": {"children": [
          {"type": "url", "url": "https://phone.example/"}]}}})"),
                             u"Other", u"Mobile");
  ASSERT_TRUE(root);
  ASSERT_EQ(root->children.size(), 1u);
  EXPECT_EQ(root->children[0].title, u"Mobile");
}

TEST(AlerseImportDataTest, RejectsWhatIsNotABookmarksFile) {
  EXPECT_FALSE(ParseChromiumBookmarks(Json("{}"), u"", u""));
  EXPECT_FALSE(ParseChromiumBookmarks(Json("[1]"), u"", u""));
}

TEST(AlerseImportDataTest, ReadsProfileNames) {
  std::map<std::string, std::string> names =
      ParseProfileNames(Json(R"({"profile": {"info_cache": {
          "Default": {"name": "Personal"}, "Profile 2": {"name": "Work"},
          "Profile 3": {}}}})"));
  ASSERT_EQ(names.size(), 3u);
  EXPECT_EQ(names["Default"], "Personal");
  EXPECT_EQ(names["Profile 2"], "Work");
  EXPECT_EQ(names["Profile 3"], "");
  EXPECT_TRUE(ParseProfileNames(Json("{}")).empty());
}

TEST(AlerseImportDataTest, TellsAlerseProfiles) {
  EXPECT_TRUE(IsAlersePreferences(R"({"arcomium":{"spaces":{}}})"));
  EXPECT_FALSE(IsAlersePreferences(R"({"browser":{"alerse_x":1}})"));
}

}  // namespace
}  // namespace alerse_import
