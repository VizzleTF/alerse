// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_import/alerse_import_handler.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/token.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/alerse_spaces.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/alerse_import/alerse_import_bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

using alerse_import::ArcSpace;
using alerse_import::ImportMark;
using alerse_import::Node;
using alerse_import::Source;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

constexpr size_t kMaxIdLength = 512;
constexpr tab_groups::TabGroupColorId kSpaceColors[] = {
    tab_groups::TabGroupColorId::kBlue,   tab_groups::TabGroupColorId::kPurple,
    tab_groups::TabGroupColorId::kGreen,  tab_groups::TabGroupColorId::kPink,
    tab_groups::TabGroupColorId::kOrange, tab_groups::TabGroupColorId::kCyan,
    tab_groups::TabGroupColorId::kYellow, tab_groups::TabGroupColorId::kRed,
};

std::string GetMeta(const BookmarkNode* node, const std::string& key) {
  std::string value;
  node->GetMetaInfo(key, &value);
  return value;
}

// The first folder of Space `space` among the Spaces' folders, as the
// sidebar finds it.
const BookmarkNode* FindSpaceFolder(BookmarkModel* model,
                                    const std::string& space) {
  for (const auto& child : model->other_node()->children()) {
    if (child->is_folder() &&
        GetMeta(child.get(), alerse_import::kSpaceMetaKey) == space) {
      return child.get();
    }
  }
  return nullptr;
}

// The mark of what imports of `source_id` into this profile add.
ImportMark GetImportMark(PrefService* prefs, std::string_view source_id) {
  std::string key = prefs->GetString(alerse_import::kImportKeyPref);
  if (key.empty()) {
    key = ImportMark::NewKey();
    prefs->SetString(alerse_import::kImportKeyPref, key);
  }
  return ImportMark(key, source_id);
}

// Opens Space `space` named `title` with its blank tab at the end of
// `browser`'s tab strip, without switching to it, as
// AlerseSpaceSwitcher::OpenSpace() does in the foreground.
bool OpenSpaceInBackground(BrowserWindowInterface* browser,
                           const std::string& space,
                           const std::u16string& title) {
  Profile* profile = browser->GetProfile();
  TabStripModel* model = browser->GetTabStripModel();
  if (profile->IsOffTheRecord() || !model || !model->SupportsTabGroups()) {
    return false;
  }
  const GURL url("about:blank#arcomium-space");
  content::WebContents::CreateParams params(
      profile, alerse::CreateSpaceSiteInstance(profile, url, space));
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(params);
  content::WebContents* raw = contents.get();
  contents->GetController().LoadURL(url, content::Referrer(),
                                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                    std::string());
  TabStripModel::MarkAlerseEmptySpace(raw);
  const size_t groups = model->group_model()->ListTabGroups().size();
  model->AddWebContents(std::move(contents), model->count(),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                        AddTabTypes::ADD_FORCE_INDEX);
  const int index = model->GetIndexOfWebContents(raw);
  if (index == TabStripModel::kNoTab) {
    return false;
  }
  const tab_groups::TabGroupId group = model->AddToNewGroup({index});
  model->ChangeTabGroupVisuals(
      group, tab_groups::TabGroupVisualData(
                 title, kSpaceColors[groups % std::size(kSpaceColors)]));
  return true;
}

// Imports Arc's Spaces into `model` (see ImportArcSpaces()) and opens a
// Space in `browser` for each one no Space has yet. Returns the result for
// the page.
base::DictValue ApplyArc(BookmarkModel* model,
                         Profile* profile,
                         BrowserWindowInterface* browser,
                         const std::vector<ArcSpace>& spaces) {
  alerse_import::ArcResult result = alerse_import::ImportArcSpaces(
      model,
      GetImportMark(profile->GetPrefs(), alerse_import::kArcSourceId),
      spaces);

  // A Space for each imported one that has none: a fresh Space id that the
  // folder names, then the Space's tab group.
  int created = 0;
  for (const auto& [folder, title] : result.unadopted) {
    if (!browser) {
      break;
    }
    const std::string space =
        base::ToLowerASCII(base::Token::CreateRandom().ToString());
    const std::string arc_space =
        GetMeta(folder, alerse_import::kSpaceMetaKey);
    model->SetNodeMetaInfo(folder, alerse_import::kSpaceMetaKey, space);
    if (OpenSpaceInBackground(browser, space, title)) {
      ++created;
    } else {
      // Left for the Space bar's "+" to adopt.
      model->SetNodeMetaInfo(folder, alerse_import::kSpaceMetaKey, arc_space);
    }
  }

  return base::DictValue()
      .Set("spaces", static_cast<int>(spaces.size()))
      .Set("links", result.links)
      .Set("folders", result.folders)
      .Set("icons", result.icons)
      .Set("createdSpaces", created)
      .Set("pendingSpaces",
           static_cast<int>(result.unadopted.size()) - created);
}

// The Space folder bookmarks imported from a tab of `browser` go to: the
// Space of `tab`, or Home.
const BookmarkNode* GetTargetFolder(BookmarkModel* model,
                                    Profile* profile,
                                    tabs::TabInterface* tab) {
  std::string space = alerse_import::kHomeSpace;
  std::u16string title = u"home";
  BrowserWindowInterface* browser =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  if (browser && tab->GetGroup()) {
    TabStripModel* tabs = browser->GetTabStripModel();
    if (std::optional<std::string> group_space =
            alerse::GetSpaceOfGroupOrFolder(profile, tabs,
                                              *tab->GetGroup())) {
      space = *group_space;
      title = tabs->group_model()
                  ->GetTabGroup(*tab->GetGroup())
                  ->visual_data()
                  ->title();
    }
  }
  if (const BookmarkNode* folder = FindSpaceFolder(model, space)) {
    return folder;
  }
  // As the sidebar does: adopt a folder imported from Arc for a Space of the
  // same name, else make the Space's folder.
  if (space != alerse_import::kHomeSpace) {
    for (const auto& child : model->other_node()->children()) {
      if (child->is_folder() && child->GetTitle() == title &&
          GetMeta(child.get(), alerse_import::kSpaceMetaKey)
              .starts_with("arc:")) {
        model->SetNodeMetaInfo(child.get(), alerse_import::kSpaceMetaKey,
                               space);
        return child.get();
      }
    }
  }
  const BookmarkNode::MetaInfoMap meta = {
      {alerse_import::kSpaceMetaKey, space}};
  return model->AddFolder(
      model->other_node(), model->other_node()->children().size(),
      title.empty() ? std::u16string(u"home") : title, &meta);
}

// Imports `root` as the folder of `source`, replacing what an earlier import
// of it into this profile put there; the user's items in the folder stay.
base::DictValue ApplyBookmarks(BookmarkModel* model,
                               Profile* profile,
                               tabs::TabInterface* tab,
                               const Source& source,
                               const Node& root) {
  std::u16string title = base::UTF8ToUTF16(source.browser);
  if (!source.profile.empty()) {
    title += u" — " + base::UTF8ToUTF16(source.profile);
  }
  const int links = alerse_import::CountLinks(root.children);
  const int folders = alerse_import::CountFolders(root.children);

  // Only the folder this profile made: one synced from another device names
  // the same source but holds that device's import.
  PrefService* prefs = profile->GetPrefs();
  const std::string* uuid =
      prefs->GetDict(alerse_import::kImportFoldersPref).FindString(source.id);
  const BookmarkNode* folder =
      uuid ? alerse_import::FindImportFolder(model, source.id, *uuid)
           : nullptr;
  if (folder) {
    model->SetTitle(folder, title,
                    bookmarks::metrics::BookmarkEditSource::kOther);
  } else {
    const BookmarkNode* parent = GetTargetFolder(model, profile, tab);
    const BookmarkNode::MetaInfoMap meta = {
        {alerse_import::kImportMetaKey, source.id}};
    folder = model->AddFolder(parent, parent->children().size(), title, &meta);
    ScopedDictPrefUpdate(prefs, alerse_import::kImportFoldersPref)
        ->Set(source.id, folder->uuid().AsLowercaseString());
  }
  alerse_import::ReplaceImportedBookmarks(
      model, GetImportMark(prefs, source.id), folder, root.children);

  return base::DictValue()
      .Set("links", links)
      .Set("folders", folders)
      .Set("folder", title);
}

}  // namespace

AlerseImportHandler::AlerseImportHandler(Profile* profile)
    : profile_(profile) {}

AlerseImportHandler::~AlerseImportHandler() = default;

void AlerseImportHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSources",
      base::BindRepeating(&AlerseImportHandler::HandleGetSources,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importSource",
      base::BindRepeating(&AlerseImportHandler::HandleImportSource,
                          base::Unretained(this)));
}

void AlerseImportHandler::OnJavascriptDisallowed() {
  // The page went away or reloaded: drop the answers it waits for.
  weak_factory_.InvalidateWeakPtrs();
  importing_ = false;
}

void AlerseImportHandler::HandleGetSources(const base::ListValue& args) {
  if (args.size() != 1 || !args[0].is_string()) {
    return;
  }
  AllowJavascript();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&alerse_import::FindSources, profile_->GetPath()),
      base::BindOnce(&AlerseImportHandler::OnSourcesFound,
                     weak_factory_.GetWeakPtr(), args[0].GetString()));
}

void AlerseImportHandler::OnSourcesFound(const std::string& callback_id,
                                           std::vector<Source> sources) {
  sources_ = std::move(sources);
  base::ListValue list;
  for (const Source& source : sources_) {
    list.Append(
        base::DictValue()
            .Set("id", source.id)
            .Set("kind", source.kind == alerse_import::SourceKind::kArc
                             ? "arc"
                             : "chromium")
            .Set("browser", source.browser)
            .Set("profile", source.profile)
            .Set("links", source.links)
            .Set("spaces", source.spaces));
  }
  ResolveJavascriptCallback(base::Value(callback_id), list);
}

void AlerseImportHandler::HandleImportSource(const base::ListValue& args) {
  if (args.size() != 2 || !args[0].is_string()) {
    return;
  }
  AllowJavascript();
  const std::string& callback_id = args[0].GetString();
  if (!args[1].is_string() || args[1].GetString().size() > kMaxIdLength) {
    Reject(callback_id, "unknown");
    return;
  }
  const std::string& id = args[1].GetString();
  auto it = std::ranges::find(sources_, id, &Source::id);
  if (it == sources_.end()) {
    Reject(callback_id, "unknown");
    return;
  }
  if (importing_) {
    Reject(callback_id, "busy");
    return;
  }
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile_);
  if (!model || !model->loaded()) {
    Reject(callback_id, "notReady");
    return;
  }
  importing_ = true;
  const base::TaskTraits traits = {
      base::MayBlock(), base::TaskPriority::USER_VISIBLE,
      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};
  if (it->kind == alerse_import::SourceKind::kArc) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, traits,
        base::BindOnce(&alerse_import::ReadArc, it->path,
                       /*with_icons=*/true),
        base::BindOnce(&AlerseImportHandler::OnArcRead,
                       weak_factory_.GetWeakPtr(), callback_id));
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, traits,
        base::BindOnce(
            &alerse_import::ReadChromiumBookmarks, it->path,
            l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OTHER_FOLDER_NAME),
            l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_MOBILE_FOLDER_NAME)),
        base::BindOnce(&AlerseImportHandler::OnBookmarksRead,
                       weak_factory_.GetWeakPtr(), callback_id, *it));
  }
}

void AlerseImportHandler::OnArcRead(
    const std::string& callback_id,
    std::optional<std::vector<ArcSpace>> spaces) {
  importing_ = false;
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile_);
  // No Space at all is a sidebar Arc is rewriting, not one to replace the
  // earlier import with.
  if (!spaces || spaces->empty()) {
    Reject(callback_id, "readFailed");
    return;
  }
  if (!model || !model->loaded()) {
    Reject(callback_id, "notReady");
    return;
  }
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_ui()->GetWebContents());
  BrowserWindowInterface* browser =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  ResolveJavascriptCallback(base::Value(callback_id),
                            ApplyArc(model, profile_, browser, *spaces));
}

void AlerseImportHandler::OnBookmarksRead(const std::string& callback_id,
                                            const Source& source,
                                            std::optional<Node> root) {
  importing_ = false;
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile_);
  if (!root) {
    Reject(callback_id, "readFailed");
    return;
  }
  if (!model || !model->loaded()) {
    Reject(callback_id, "notReady");
    return;
  }
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_ui()->GetWebContents());
  ResolveJavascriptCallback(
      base::Value(callback_id),
      ApplyBookmarks(model, profile_, tab, source, *root));
}

void AlerseImportHandler::Reject(const std::string& callback_id,
                                   const std::string& error) {
  RejectJavascriptCallback(base::Value(callback_id), base::Value(error));
}
