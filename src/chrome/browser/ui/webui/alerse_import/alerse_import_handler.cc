// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_import/alerse_import_handler.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/tabs/alerse_space_folder.h"
#include "chrome/browser/ui/tabs/alerse_space_registry.h"
#include "chrome/browser/ui/webui/alerse_import/alerse_import_bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/crx_installer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

using alerse_import::ArcSpace;
using alerse_import::ImportMark;
using alerse_import::Node;
using alerse_import::Source;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

constexpr size_t kMaxIdLength = 512;
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

// The Space that took Arc's Space `arc_space` in an earlier import; empty if
// none did or it was deleted.
base::FilePath FindSpaceOfArcSpace(const std::string& arc_space) {
  if (arc_space.empty()) {
    return base::FilePath();
  }
  for (const ProfileAttributesEntry* entry :
       g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetAllProfilesAttributes()) {
    if (!entry->IsOmitted() &&
        entry->GetAlerseValue(alerse::kSpaceEntryArcKey) == arc_space) {
      return entry->GetPath();
    }
  }
  return base::FilePath();
}

// Moves what the folder `id` of the profile in `from_path` holds into the
// Space folder of `space_profile`, whose bookmarks are `to`. A Space made
// now (`fresh`) takes the folder's icon and color too; one that took this
// Arc Space before keeps its own and loses only what that import brought.
void MoveFolderToSpace(base::FilePath from_path,
                       int64_t id,
                       bool fresh,
                       Profile* space_profile,
                       BookmarkModel* to) {
  Profile* from =
      g_browser_process->profile_manager()->GetProfileByPath(from_path);
  BookmarkModel* model =
      from ? BookmarkModelFactory::GetForBrowserContext(from) : nullptr;
  const BookmarkNode* folder =
      model ? bookmarks::GetBookmarkNodeByID(model, id) : nullptr;
  if (!folder) {
    return;
  }
  const BookmarkNode* space =
      alerse::GetOrAddSpaceFolder(to, folder->GetTitle());
  std::string arc_space;
  folder->GetMetaInfo(alerse_import::kSpaceMetaKey, &arc_space);
  if (fresh) {
    if (ProfileAttributesEntry* entry =
            g_browser_process->profile_manager()
                ->GetProfileAttributesStorage()
                .GetProfileAttributesWithPath(space_profile->GetPath())) {
      entry->SetAlerseValue(alerse::kSpaceEntryArcKey, arc_space);
    }
    if (const BookmarkNode::MetaInfoMap* meta = folder->GetMetaInfoMap()) {
      for (const auto& [key, value] : *meta) {
        // All but the mark of a folder no Space has: Arc's id of the Space,
        // its icon and its color.
        if (key != alerse_import::kSpaceMetaKey) {
          to->SetNodeMetaInfo(space, key, value);
        }
      }
    }
  } else {
    // By the mark's presence: the copies have new uuids, which the mark of
    // the importing profile does not sign.
    for (size_t i = space->children().size(); i-- > 0;) {
      std::string mark;
      if (space->children()[i]->GetMetaInfo(alerse_import::kImportedMetaKey,
                                            &mark)) {
        to->Remove(space->children()[i].get(),
                   bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
      }
    }
  }
  for (const auto& child : folder->children()) {
    alerse::CopyNodeTo(child.get(), to, space);
  }
  model->Remove(folder, bookmarks::metrics::BookmarkEditSource::kOther,
                FROM_HERE);
}

// Gives `folder`, the folder of an imported Arc Space that no folder of
// `profile` took, to a Space: the one that took this Arc Space in an earlier
// import, else a new profile. `folder` then goes from `profile`'s bookmarks.
// No window opens: a new Space shows in the Space bar.
void MakeSpaceOfFolder(Profile* profile,
                       const BookmarkNode* folder,
                       const std::u16string& title) {
  std::string arc_space;
  folder->GetMetaInfo(alerse_import::kSpaceMetaKey, &arc_space);
  if (const base::FilePath taken = FindSpaceOfArcSpace(arc_space);
      !taken.empty()) {
    alerse::WithSpaceBookmarks(
        taken, base::BindOnce(&MoveFolderToSpace, profile->GetPath(),
                              folder->id(), /*fresh=*/false));
    return;
  }
  ProfileManager::CreateMultiProfileAsync(
      title.empty() ? std::u16string(u"Space") : title,
      profiles::GetPlaceholderAvatarIndex(), /*is_hidden=*/false,
      base::BindOnce(
          [](base::FilePath from_path, int64_t id, Profile* created) {
            if (created) {
              alerse::WithBookmarksOf(
                  created, base::BindOnce(&MoveFolderToSpace, from_path, id,
                                          /*fresh=*/true));
            }
          },
          profile->GetPath(), folder->id()));
}

// Imports Arc's Spaces into `model` (see ImportArcSpaces()) and makes a
// Space for each one no Space has yet. Returns the result for the page.
base::DictValue ApplyArc(BookmarkModel* model,
                         Profile* profile,
                         const std::vector<ArcSpace>& spaces) {
  alerse_import::ArcResult result = alerse_import::ImportArcSpaces(
      model,
      GetImportMark(profile->GetPrefs(), alerse_import::kArcSourceId),
      spaces);
  int created = 0;
  for (const auto& [folder, title] : result.unadopted) {
    std::string arc_space;
    folder->GetMetaInfo(alerse_import::kSpaceMetaKey, &arc_space);
    created += FindSpaceOfArcSpace(arc_space).empty();
    MakeSpaceOfFolder(profile, folder, title);
  }
  return base::DictValue()
      .Set("spaces", static_cast<int>(spaces.size()))
      .Set("links", result.links)
      .Set("folders", result.folders)
      .Set("icons", result.icons)
      .Set("createdSpaces", created)
      .Set("pendingSpaces", 0);
}

// The folder bookmarks imported into `profile` go to: its Space's.
const BookmarkNode* GetTargetFolder(BookmarkModel* model, Profile* profile) {
  return alerse::GetOrAddSpaceFolder(
      model, alerse::GetSpaceOfProfile(profile).name);
}

// Imports `root` as the folder of `source`, replacing what an earlier import
// of it into this profile put there; the user's items in the folder stay.
base::DictValue ApplyBookmarks(BookmarkModel* model,
                               Profile* profile,
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
    const BookmarkNode* parent = GetTargetFolder(model, profile);
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
  web_ui()->RegisterMessageCallback(
      "getExtensions",
      base::BindRepeating(&AlerseImportHandler::HandleGetExtensions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importExtensions",
      base::BindRepeating(&AlerseImportHandler::HandleImportExtensions,
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
  ResolveJavascriptCallback(base::Value(callback_id),
                            ApplyArc(model, profile_, *spaces));
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
  ResolveJavascriptCallback(
      base::Value(callback_id),
      ApplyBookmarks(model, profile_, source, *root));
}

const alerse_import::Source* AlerseImportHandler::FindSource(
    const base::ListValue& args) {
  if (args.size() != 2 || !args[1].is_string() ||
      args[1].GetString().size() > kMaxIdLength) {
    return nullptr;
  }
  auto it = std::ranges::find(sources_, args[1].GetString(), &Source::id);
  return it == sources_.end() ? nullptr : &*it;
}

void AlerseImportHandler::HandleGetExtensions(const base::ListValue& args) {
  if (args.empty() || !args[0].is_string()) {
    return;
  }
  AllowJavascript();
  const std::string& callback_id = args[0].GetString();
  const Source* source = FindSource(args);
  if (!source) {
    Reject(callback_id, "unknown");
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&alerse_import::FindExtensions, *source),
      base::BindOnce(&AlerseImportHandler::OnExtensionsFound,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseImportHandler::OnExtensionsFound(
    const std::string& callback_id,
    std::vector<alerse_import::SourceExtension> found) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  base::ListValue list;
  for (const alerse_import::SourceExtension& extension : found) {
    list.Append(
        base::DictValue()
            .Set("id", extension.id)
            .Set("name", extension.name)
            .Set("installed",
                 registry &&
                     registry->GetInstalledExtension(extension.id) != nullptr));
  }
  ResolveJavascriptCallback(base::Value(callback_id), list);
}

void AlerseImportHandler::HandleImportExtensions(const base::ListValue& args) {
  if (args.empty() || !args[0].is_string()) {
    return;
  }
  AllowJavascript();
  const std::string& callback_id = args[0].GetString();
  const Source* source = FindSource(args);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  if (!source || !registry) {
    Reject(callback_id, "unknown");
    return;
  }
  if (importing_) {
    Reject(callback_id, "busy");
    return;
  }
  importing_ = true;
  std::set<std::string> installed;
  for (const auto& extension : registry->GenerateInstalledExtensionsSet()) {
    installed.insert(extension->id());
  }
  // The installer moves the files it installs, so it gets copies: the
  // source's own stay as they are.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [](Source source, std::set<std::string> installed) {
            std::vector<alerse_import::SourceExtension> copies;
            for (alerse_import::SourceExtension& extension :
                 alerse_import::FindExtensions(source)) {
              base::FilePath temp;
              if (installed.contains(extension.id) ||
                  !base::CreateNewTempDirectory(
                      FILE_PATH_LITERAL("alerse_extension"), &temp)) {
                continue;
              }
              const base::FilePath copy =
                  temp.Append(FILE_PATH_LITERAL("extension"));
              if (!base::CopyDirectory(extension.dir, copy,
                                       /*recursive=*/true)) {
                base::DeletePathRecursively(temp);
                continue;
              }
              extension.dir = copy;
              copies.push_back(std::move(extension));
            }
            return copies;
          },
          *source, std::move(installed)),
      base::BindOnce(&AlerseImportHandler::OnExtensionsCopied,
                     weak_factory_.GetWeakPtr(), callback_id));
}

namespace {

// How the installs of one importExtensions() call went.
struct ExtensionTally {
  int left = 0;
  int added = 0;
  int failed = 0;
  base::OnceCallback<void(int added, int failed)> done;
};

}  // namespace

void AlerseImportHandler::OnExtensionsCopied(
    const std::string& callback_id,
    std::vector<alerse_import::SourceExtension> copies) {
  importing_ = false;
  auto tally = std::make_shared<ExtensionTally>();
  tally->left = static_cast<int>(copies.size());
  tally->done = base::BindOnce(
      [](base::WeakPtr<AlerseImportHandler> self, std::string callback_id,
         int added, int failed) {
        if (self) {
          self->ResolveJavascriptCallback(
              base::Value(callback_id),
              base::DictValue().Set("added", added).Set("failed", failed));
        }
      },
      weak_factory_.GetWeakPtr(), callback_id);
  if (copies.empty()) {
    std::move(tally->done).Run(0, 0);
    return;
  }
  for (const alerse_import::SourceExtension& extension : copies) {
    scoped_refptr<extensions::CrxInstaller> installer =
        extensions::CrxInstaller::CreateSilent(profile_);
    // The user installed it from the Web Store in the source and granted
    // its permissions there; it stays the store's, which updates it.
    installer->set_install_source(
        extensions::mojom::ManifestLocation::kInternal);
    installer->set_creation_flags(extensions::Extension::FROM_WEBSTORE);
    installer->set_allow_silent_install(true);
    installer->set_delete_source(true);
    installer->AddInstallerCallback(base::BindOnce(
        [](std::shared_ptr<ExtensionTally> tally, base::FilePath temp,
           const std::optional<extensions::CrxInstallError>& error) {
          (error ? tally->failed : tally->added)++;
          // The copy's directory, which the installer leaves behind.
          base::ThreadPool::PostTask(
              FROM_HERE, {base::MayBlock()},
              base::GetDeletePathRecursivelyCallback(temp));
          if (--tally->left == 0) {
            std::move(tally->done).Run(tally->added, tally->failed);
          }
        },
        tally, extension.dir.DirName()));
    installer->InstallUnpackedCrx(extension.id, extension.public_key,
                                  extension.dir);
  }
}

void AlerseImportHandler::Reject(const std::string& callback_id,
                                   const std::string& error) {
  RejectJavascriptCallback(base::Value(callback_id), base::Value(error));
}
