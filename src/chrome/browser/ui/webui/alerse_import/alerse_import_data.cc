// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_import/alerse_import_data.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <set>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/boringssl/src/include/openssl/md5.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

namespace alerse_import {

namespace {

// Arc's folders nest; a sidebar that loops is cut here.
constexpr int kMaxDepth = 64;

// Where a browser keeps its user data, relative to a base directory.
enum class Base { kLocal, kRoaming };

struct Browser {
  const char* key;
  const char* name;
  Base base;
  const base::FilePath::CharType* dir;
  // The directory is the browser's only profile (Opera), not a user data dir
  // with profiles in it.
  bool single_profile;
};

#if BUILDFLAG(IS_WIN)
constexpr Browser kBrowsers[] = {
    {"chrome", "Google Chrome", Base::kLocal,
     FILE_PATH_LITERAL("Google\\Chrome\\User Data"), false},
    {"edge", "Microsoft Edge", Base::kLocal,
     FILE_PATH_LITERAL("Microsoft\\Edge\\User Data"), false},
    {"brave", "Brave", Base::kLocal,
     FILE_PATH_LITERAL("BraveSoftware\\Brave-Browser\\User Data"), false},
    {"vivaldi", "Vivaldi", Base::kLocal,
     FILE_PATH_LITERAL("Vivaldi\\User Data"), false},
    {"opera", "Opera", Base::kRoaming,
     FILE_PATH_LITERAL("Opera Software\\Opera Stable"), true},
    {"yandex", "Yandex Browser", Base::kLocal,
     FILE_PATH_LITERAL("Yandex\\YandexBrowser\\User Data"), false},
    {"chromium", "Chromium", Base::kLocal,
     FILE_PATH_LITERAL("Chromium\\User Data"), false},
};
#elif BUILDFLAG(IS_MAC)
constexpr Browser kBrowsers[] = {
    {"chrome", "Google Chrome", Base::kLocal, "Google/Chrome", false},
    {"edge", "Microsoft Edge", Base::kLocal, "Microsoft Edge", false},
    {"brave", "Brave", Base::kLocal, "BraveSoftware/Brave-Browser", false},
    {"vivaldi", "Vivaldi", Base::kLocal, "Vivaldi", false},
    {"opera", "Opera", Base::kLocal, "com.operasoftware.Opera", true},
    {"yandex", "Yandex Browser", Base::kLocal, "Yandex/YandexBrowser", false},
    {"chromium", "Chromium", Base::kLocal, "Chromium", false},
};
#else
constexpr Browser kBrowsers[] = {
    {"chrome", "Google Chrome", Base::kLocal, "google-chrome", false},
    {"edge", "Microsoft Edge", Base::kLocal, "microsoft-edge", false},
    {"brave", "Brave", Base::kLocal, "BraveSoftware/Brave-Browser", false},
    {"vivaldi", "Vivaldi", Base::kLocal, "vivaldi", false},
    {"opera", "Opera", Base::kLocal, "opera", true},
    {"yandex", "Yandex Browser", Base::kLocal, "yandex-browser", false},
    {"chromium", "Chromium", Base::kLocal, "chromium", false},
};
#endif

// The directory the browsers' data directories are in. Windows' known
// folders, so not what a LOCALAPPDATA variable of this process says.
base::FilePath GetBaseDir(Base which) {
  base::FilePath dir;
#if BUILDFLAG(IS_WIN)
  base::PathService::Get(which == Base::kRoaming ? base::DIR_ROAMING_APP_DATA
                                                 : base::DIR_LOCAL_APP_DATA,
                         &dir);
#elif BUILDFLAG(IS_MAC)
  base::PathService::Get(base::DIR_APP_DATA, &dir);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  dir = base::nix::GetXDGDirectory(env.get(), "XDG_CONFIG_HOME", ".config");
#endif
  return dir;
}

// Reads a file another process may have open and be writing: shared for
// reading, writing and replacing, so the browser that owns it is not blocked.
std::optional<std::string> ReadSharedFile(const base::FilePath& path,
                                          size_t max_size) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);
  if (!file.IsValid()) {
    return std::nullopt;
  }
  const int64_t length = file.GetLength();
  if (length < 0 || static_cast<uint64_t>(length) > max_size) {
    return std::nullopt;
  }
  std::string data(static_cast<size_t>(length), '\0');
  std::optional<size_t> read = file.Read(0, base::as_writable_byte_span(data));
  if (!read) {
    return std::nullopt;
  }
  data.resize(*read);
  return data;
}

std::optional<base::Value> ReadJsonFile(const base::FilePath& path) {
  std::optional<std::string> data = ReadSharedFile(path, kMaxFileSize);
  if (!data) {
    return std::nullopt;
  }
  return base::JSONReader::Read(
      *data, base::JSON_PARSE_RFC | base::JSON_REPLACE_INVALID_CHARACTERS);
}

// Arc stores maps as flat [key, value, key, value] lists; keys that are not
// strings are skipped. Keeps the list's order.
std::vector<std::pair<std::string, const base::Value*>> Pairs(
    const base::Value* flat) {
  std::vector<std::pair<std::string, const base::Value*>> pairs;
  if (!flat || !flat->is_list()) {
    return pairs;
  }
  const base::ListValue& list = flat->GetList();
  for (size_t i = 0; i + 1 < list.size(); i += 2) {
    if (list[i].is_string()) {
      pairs.emplace_back(list[i].GetString(), &list[i + 1]);
    }
  }
  return pairs;
}

const std::string* FindString(const base::DictValue& dict,
                              std::string_view key) {
  const std::string* value = dict.FindString(key);
  return value && !value->empty() ? value : nullptr;
}

// customInfo.iconType.icon of an Arc Space or folder, if it is a name.
std::string ArcIcon(const base::DictValue& dict) {
  const std::string* icon =
      dict.FindStringByDottedPath("customInfo.iconType.icon");
  return icon && icon->size() <= kMaxIconNameBytes ? *icon : std::string();
}

// `text` as a title: cut to kMaxTitleBytes.
std::u16string Title(std::string_view text) {
  return base::UTF8ToUTF16(base::TruncateUTF8ToByteSize(text, kMaxTitleBytes));
}

// The bytes the strings of `node` take, as kMaxArcBytes counts them.
size_t TextBytes(const Node& node) {
  size_t bytes = node.title.size() * sizeof(char16_t) +
                 node.url.spec().size() + node.arc_item.size();
  for (const auto& [key, value] : node.meta) {
    bytes += key.size() + value.size();
  }
  return bytes;
}

// The items of Arc's sidebar, one container at a time, with the budgets of
// kMaxArcItems and kMaxArcBytes that all of them share.
class ArcTree {
 public:
  ArcTree() = default;
  ArcTree(const ArcTree&) = delete;
  ArcTree& operator=(const ArcTree&) = delete;

  // Reads the items of a container in place of the last one's.
  void SetItems(const base::Value* items) {
    items_.clear();
    for (auto& [id, item] : Pairs(items)) {
      if (item->is_dict()) {
        items_.emplace(id, &item->GetDict());
      }
    }
  }

  bool exhausted() const { return items_left_ <= 0 || bytes_left_ == 0; }

  std::vector<Node> Children(const std::string& id, int depth) {
    std::vector<Node> nodes;
    auto it = items_.find(id);
    if (it == items_.end() || depth > kMaxDepth ||
        !visiting_.insert(id).second) {
      return nodes;
    }
    const base::ListValue* children = it->second->FindList("childrenIds");
    if (!children) {
      visiting_.erase(id);
      return nodes;
    }
    // Arc lists a tab in one place; a folder that lists one twice gets it
    // once.
    std::set<std::string_view> tabs;
    for (const base::Value& child_id : *children) {
      if (exhausted()) {
        break;
      }
      if (!child_id.is_string()) {
        continue;
      }
      auto child = items_.find(child_id.GetString());
      if (child == items_.end()) {
        continue;
      }
      const base::DictValue& item = *child->second;
      const base::DictValue* data = item.FindDict("data");
      if (!data) {
        continue;
      }
      const std::string* title = FindString(item, "title");
      if (const base::DictValue* tab = data->FindDict("tab")) {
        const std::string* saved = FindString(*tab, "savedURL");
        if (!saved || saved->size() > url::kMaxURLChars ||
            !tabs.insert(child_id.GetString()).second) {
          continue;
        }
        std::string spec = *saved;
        if (spec.starts_with("arc://")) {
          spec = "chrome://" + spec.substr(6);
        }
        GURL url(spec);
        if (!url.is_valid()) {
          continue;
        }
        const std::string* saved_title = FindString(*tab, "savedTitle");
        Node node;
        node.title = Title(title         ? *title
                           : saved_title ? *saved_title
                                         : spec);
        node.url = std::move(url);
        if (title) {
          node.meta[kRenamedMetaKey] = "1";
        }
        if (child_id.GetString().size() <= kMaxArcIdBytes) {
          node.arc_item = child_id.GetString();
        }
        if (!Spend(node)) {
          break;
        }
        nodes.push_back(std::move(node));
      } else if (const base::DictValue* list = data->FindDict("list")) {
        if (visiting_.contains(child_id.GetString())) {
          continue;
        }
        Node node;
        node.title = Title(title ? *title : "Folder");
        if (std::string icon = ArcIcon(*list); !icon.empty()) {
          node.meta[kFolderIconMetaKey] = icon;
        }
        if (!Spend(node)) {
          break;
        }
        node.children = Children(child_id.GetString(), depth + 1);
        nodes.push_back(std::move(node));
      }
    }
    visiting_.erase(id);
    return nodes;
  }

 private:
  // Takes `node` out of the budgets; false, and nothing more is read, when it
  // does not fit.
  bool Spend(const Node& node) {
    const size_t bytes = TextBytes(node);
    if (bytes > bytes_left_) {
      bytes_left_ = 0;
      return false;
    }
    bytes_left_ -= bytes;
    --items_left_;
    return true;
  }

  std::map<std::string, const base::DictValue*> items_;
  std::set<std::string> visiting_;
  int items_left_ = kMaxArcItems;
  size_t bytes_left_ = kMaxArcBytes;
};

// More bookmarks than sync lets a profile have; a file with more is not a
// profile's own, and adding them all would hang the browser.
constexpr int kMaxChromiumItems = 200000;

// `items_left` is the budget of kMaxChromiumItems the whole file shares.
void AddChromiumChildren(const base::DictValue& folder,
                         std::vector<Node>& out,
                         int depth,
                         int& items_left) {
  const base::ListValue* children = folder.FindList("children");
  if (!children || depth > kMaxDepth) {
    return;
  }
  for (const base::Value& child : *children) {
    if (items_left <= 0) {
      return;
    }
    if (!child.is_dict()) {
      continue;
    }
    const base::DictValue& dict = child.GetDict();
    const std::string* type = dict.FindString("type");
    const std::string* name = dict.FindString("name");
    if (!type) {
      continue;
    }
    if (*type == "url") {
      const std::string* spec = dict.FindString("url");
      if (!spec || spec->size() > url::kMaxURLChars) {
        continue;
      }
      GURL url(*spec);
      if (!url.is_valid()) {
        continue;
      }
      Node node;
      node.title = Title(name && !name->empty() ? *name : url.spec());
      node.url = std::move(url);
      --items_left;
      out.push_back(std::move(node));
    } else if (*type == "folder") {
      Node node;
      node.title = Title(name ? *name : std::string());
      --items_left;
      AddChromiumChildren(dict, node.children, depth + 1, items_left);
      out.push_back(std::move(node));
    }
  }
}

// Arc names a cached favicon after the MD5 of the item's id, in hex. MD5 is
// what Arc's file names use, not a security measure.
std::string ArcIconFileName(const std::string& item_id) {
  std::array<uint8_t, MD5_DIGEST_LENGTH> digest;
  MD5(base::as_byte_span(item_id).data(), item_id.size(), digest.data());
  return base::HexEncodeLower(digest);
}

// Attaches the favicons Arc cached for its items, reading each file once,
// until kMaxArcIconBytes of them are attached.
class ArcIcons {
 public:
  explicit ArcIcons(const base::FilePath& dir) : dir_(dir) {}
  ArcIcons(const ArcIcons&) = delete;
  ArcIcons& operator=(const ArcIcons&) = delete;

  void Attach(std::vector<Node>& nodes) {
    for (Node& node : nodes) {
      if (!node.is_folder() && !node.arc_item.empty() && budget_ > 0) {
        auto [it, inserted] = icons_.try_emplace(node.arc_item);
        if (inserted) {
          std::optional<std::string> png = ReadSharedFile(
              dir_.AppendASCII(ArcIconFileName(node.arc_item)), kMaxIconSize);
          if (png && png->starts_with("\x89PNG")) {
            it->second = base::Base64Encode(*png);
          }
        }
        if (it->second && it->second->size() > budget_) {
          budget_ = 0;
        } else if (it->second) {
          budget_ -= it->second->size();
          node.meta[kIconMetaKey] = *it->second;
        }
      }
      Attach(node.children);
    }
  }

 private:
  const base::FilePath dir_;
  // By Arc's item id; nullopt when the item has no icon.
  std::map<std::string, std::optional<std::string>> icons_;
  size_t budget_ = kMaxArcIconBytes;
};

#if BUILDFLAG(IS_WIN)
// Arc for Windows is an MSIX package: its data is in the package's
// LocalCache.
base::FilePath FindArcDir() {
  base::FileEnumerator packages(
      GetBaseDir(Base::kLocal).Append(FILE_PATH_LITERAL("Packages")),
      /*recursive=*/false, base::FileEnumerator::DIRECTORIES,
      FILE_PATH_LITERAL("TheBrowserCompany.Arc_*"));
  std::vector<base::FilePath> found;
  for (base::FilePath package = packages.Next(); !package.empty();
       package = packages.Next()) {
    found.push_back(package);
  }
  std::ranges::sort(found);
  for (const base::FilePath& package : found) {
    base::FilePath dir = package.Append(FILE_PATH_LITERAL("LocalCache"))
                             .Append(FILE_PATH_LITERAL("Local"))
                             .Append(FILE_PATH_LITERAL("Arc"));
    if (base::PathExists(
            dir.Append(FILE_PATH_LITERAL("StorableSidebar.json")))) {
      return dir;
    }
  }
  return base::FilePath();
}
#elif BUILDFLAG(IS_MAC)
base::FilePath FindArcDir() {
  base::FilePath dir = GetBaseDir(Base::kLocal).Append("Arc");
  return base::PathExists(dir.Append("StorableSidebar.json"))
             ? dir
             : base::FilePath();
}
#else
// There is no Arc for Linux.
base::FilePath FindArcDir() {
  return base::FilePath();
}
#endif

bool IsSameDir(const base::FilePath& a, const base::FilePath& b) {
  return !a.empty() && !b.empty() &&
         base::FilePath::CompareEqualIgnoreCase(
             a.StripTrailingSeparators().NormalizePathSeparators().value(),
             b.StripTrailingSeparators().NormalizePathSeparators().value());
}

// Whether `dir` is `own` (this Alerse profile) or an Alerse profile.
bool IsAlerseLineProfile(const base::FilePath& dir, const base::FilePath& own) {
  if (IsSameDir(dir, own)) {
    return true;
  }
  std::optional<std::string> preferences = ReadSharedFile(
      dir.Append(FILE_PATH_LITERAL("Preferences")), kMaxFileSize);
  return preferences && IsAlersePreferences(*preferences);
}

int CountBookmarks(const base::FilePath& profile_dir) {
  std::optional<Node> root = ReadChromiumBookmarks(profile_dir, u"", u"");
  return root ? CountLinks(root->children) : -1;
}

// The profiles of `browser` that have bookmarks, as sources.
void FindBrowserSources(const Browser& browser,
                        const base::FilePath& own_profile,
                        std::vector<Source>& sources) {
  const base::FilePath data_dir = GetBaseDir(browser.base).Append(browser.dir);
  // Alerse's own user data dir: an Alerse that kept the profile it had
  // before its own identity runs from Chromium's directory, with all of its
  // profiles there.
  if (IsSameDir(data_dir, own_profile.DirName())) {
    return;
  }
  std::vector<std::pair<base::FilePath, std::string>> profiles;
  if (browser.single_profile) {
    profiles.emplace_back(data_dir, std::string());
  } else {
    std::map<std::string, std::string> names;
    if (std::optional<base::Value> local_state =
            ReadJsonFile(data_dir.Append(FILE_PATH_LITERAL("Local State")))) {
      names = ParseProfileNames(*local_state);
    }
    if (names.empty() &&
        base::PathExists(data_dir.Append(FILE_PATH_LITERAL("Default")))) {
      names["Default"] = std::string();
    }
    for (const auto& [dir, name] : names) {
      // Directory names come from another browser's file: one plain name.
      const base::FilePath relative = base::FilePath::FromUTF8Unsafe(dir);
      if (dir.empty() || relative.BaseName() != relative ||
          relative.ReferencesParent() || dir == "." ||
          dir.find_first_of("/\\:") != std::string::npos) {
        continue;
      }
      profiles.emplace_back(data_dir.Append(relative), name);
    }
  }

  std::vector<Source> found;
  for (const auto& [dir, name] : profiles) {
    if (!base::PathExists(dir.Append(FILE_PATH_LITERAL("Bookmarks"))) ||
        IsAlerseLineProfile(dir, own_profile)) {
      continue;
    }
    const int links = CountBookmarks(dir);
    if (links < 0) {
      continue;
    }
    Source source;
    source.id = std::string(browser.key) + "/" + dir.BaseName().AsUTF8Unsafe();
    source.kind = SourceKind::kChromium;
    source.browser = browser.name;
    source.profile = name.empty() ? dir.BaseName().AsUTF8Unsafe() : name;
    source.path = dir;
    source.links = links;
    found.push_back(std::move(source));
  }
  // One profile needs no name.
  if (found.size() == 1) {
    found[0].profile.clear();
  }
  for (Source& source : found) {
    sources.push_back(std::move(source));
  }
}

}  // namespace

Node::Node() = default;
Node::Node(Node&&) = default;
Node& Node::operator=(Node&&) = default;
Node::~Node() = default;

ArcSpace::ArcSpace() = default;
ArcSpace::ArcSpace(ArcSpace&&) = default;
ArcSpace& ArcSpace::operator=(ArcSpace&&) = default;
ArcSpace::~ArcSpace() = default;

Source::Source() = default;
Source::Source(const Source&) = default;
Source& Source::operator=(const Source&) = default;
Source::~Source() = default;

std::vector<ArcSpace> ParseArcSidebar(const base::Value& sidebar) {
  std::vector<ArcSpace> spaces;
  const base::ListValue* containers =
      sidebar.is_dict()
          ? sidebar.GetDict().FindListByDottedPath("sidebar.containers")
          : nullptr;
  if (!containers) {
    return spaces;
  }
  ArcTree tree;
  std::set<std::string> ids;
  for (const base::Value& container : *containers) {
    if (tree.exhausted()) {
      break;
    }
    if (!container.is_dict()) {
      continue;
    }
    const base::Value* space_list = container.GetDict().Find("spaces");
    if (!space_list) {
      continue;
    }
    tree.SetItems(container.GetDict().Find("items"));
    for (const auto& [space_id, space_value] : Pairs(space_list)) {
      if (tree.exhausted()) {
        break;
      }
      if (!space_value->is_dict()) {
        continue;
      }
      const base::DictValue& space = space_value->GetDict();
      const std::string* title = FindString(space, "title");
      std::string pinned;
      for (const auto& [kind, id] : Pairs(space.Find("containerIDs"))) {
        if (kind == "pinned" && id->is_string()) {
          pinned = id->GetString();
          break;
        }
      }
      // The id names the Space's folder the next time: one that names no
      // Space, or names it twice, would take another Space's.
      if (!title || pinned.empty() || space_id.empty() ||
          space_id.size() > kMaxArcIdBytes || !ids.insert(space_id).second) {
        continue;
      }
      ArcSpace arc_space;
      arc_space.id = space_id;
      arc_space.title = Title(*title);
      arc_space.icon = ArcIcon(space);
      arc_space.pinned = tree.Children(pinned, 0);
      spaces.push_back(std::move(arc_space));
    }
  }
  return spaces;
}

std::optional<Node> ParseChromiumBookmarks(const base::Value& bookmarks,
                                           const std::u16string& other_title,
                                           const std::u16string& mobile_title) {
  const base::DictValue* roots =
      bookmarks.is_dict() ? bookmarks.GetDict().FindDict("roots") : nullptr;
  if (!roots) {
    return std::nullopt;
  }
  Node root;
  int items_left = kMaxChromiumItems;
  if (const base::DictValue* bar = roots->FindDict("bookmark_bar")) {
    AddChromiumChildren(*bar, root.children, 0, items_left);
  }
  const std::pair<const char*, const std::u16string*> folders[] = {
      {"other", &other_title}, {"synced", &mobile_title}};
  for (const auto& [key, fallback] : folders) {
    const base::DictValue* folder = roots->FindDict(key);
    if (!folder) {
      continue;
    }
    Node node;
    const std::string* name = folder->FindString("name");
    node.title = name && !name->empty() ? base::UTF8ToUTF16(*name) : *fallback;
    AddChromiumChildren(*folder, node.children, 0, items_left);
    if (!node.children.empty()) {
      root.children.push_back(std::move(node));
    }
  }
  return root;
}

std::map<std::string, std::string> ParseProfileNames(
    const base::Value& local_state) {
  std::map<std::string, std::string> names;
  const base::DictValue* cache =
      local_state.is_dict()
          ? local_state.GetDict().FindDictByDottedPath("profile.info_cache")
          : nullptr;
  if (!cache) {
    return names;
  }
  for (const auto [dir, info] : *cache) {
    const std::string* name =
        info.is_dict() ? info.GetDict().FindString("name") : nullptr;
    names[dir] = name ? *name : std::string();
  }
  return names;
}

bool IsAlersePreferences(std::string_view preferences) {
  return preferences.find("\"arcomium\":") != std::string_view::npos;
}

int CountLinks(const std::vector<Node>& nodes) {
  int count = 0;
  for (const Node& node : nodes) {
    count += node.is_folder() ? CountLinks(node.children) : 1;
  }
  return count;
}

int CountFolders(const std::vector<Node>& nodes) {
  int count = 0;
  for (const Node& node : nodes) {
    if (node.is_folder()) {
      count += 1 + CountFolders(node.children);
    }
  }
  return count;
}

std::vector<Source> FindSources(const base::FilePath& own_profile) {
  std::vector<Source> sources;
  if (base::FilePath arc = FindArcDir(); !arc.empty()) {
    if (std::optional<std::vector<ArcSpace>> spaces =
            ReadArc(arc, /*with_icons=*/false);
        spaces && !spaces->empty()) {
      Source source;
      source.id = kArcSourceId;
      source.kind = SourceKind::kArc;
      source.browser = "Arc";
      source.path = arc;
      source.spaces = static_cast<int>(spaces->size());
      for (const ArcSpace& space : *spaces) {
        source.links += CountLinks(space.pinned);
      }
      sources.push_back(std::move(source));
    }
  }
  for (const Browser& browser : kBrowsers) {
    FindBrowserSources(browser, own_profile, sources);
  }
  return sources;
}

std::optional<std::vector<ArcSpace>> ReadArc(const base::FilePath& dir,
                                             bool with_icons) {
  std::optional<base::Value> sidebar =
      ReadJsonFile(dir.Append(FILE_PATH_LITERAL("StorableSidebar.json")));
  if (!sidebar) {
    return std::nullopt;
  }
  std::vector<ArcSpace> spaces = ParseArcSidebar(*sidebar);
  if (with_icons) {
    ArcIcons icons(dir.Append(FILE_PATH_LITERAL("SidebarItemsFaviconCache")));
    for (ArcSpace& space : spaces) {
      icons.Attach(space.pinned);
    }
  }
  return spaces;
}

std::optional<Node> ReadChromiumBookmarks(const base::FilePath& dir,
                                          const std::u16string& other_title,
                                          const std::u16string& mobile_title) {
  std::optional<base::Value> bookmarks =
      ReadJsonFile(dir.Append(FILE_PATH_LITERAL("Bookmarks")));
  if (!bookmarks) {
    return std::nullopt;
  }
  return ParseChromiumBookmarks(*bookmarks, other_title, mobile_title);
}

}  // namespace alerse_import
