// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_SPACE_SWITCHER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_SPACE_SWITCHER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

namespace views {
class MenuRunner;
}
class TabStripModel;

// Arcomium: row of Spaces at the bottom of the vertical tab strip. Every Space
// is a tab group; its dot switches the strip into focus mode for that group.
// The "+" button creates a Space with its own storage partition. Spaces sync
// as saved tab groups: one from another device, or closed here, shows too and
// opens with its tabs.
class ArcomiumSpaceSwitcher : public views::View,
                              public TabStripModelObserver,
                              public tab_groups::TabGroupSyncService::Observer,
                              public bookmarks::BaseBookmarkModelObserver,
                              public views::ContextMenuController,
                              public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(ArcomiumSpaceSwitcher, views::View)

 public:
  explicit ArcomiumSpaceSwitcher(BrowserWindowInterface* browser);
  ArcomiumSpaceSwitcher(const ArcomiumSpaceSwitcher&) = delete;
  ArcomiumSpaceSwitcher& operator=(const ArcomiumSpaceSwitcher&) = delete;
  ~ArcomiumSpaceSwitcher() override;

  // Creates a Space with a blank tab in a fresh storage partition and switches
  // to it.
  static void CreateSpace(BrowserWindowInterface* browser);
  // Opens the Space `space` named `title` with a blank tab in its storage
  // partition and switches to it.
  static void OpenSpace(BrowserWindowInterface* browser,
                        const std::string& space,
                        const std::u16string& title);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override;

  // tab_groups::TabGroupSyncService::Observer:
  using TabStripModelObserver::OnTabGroupAdded;
  using tab_groups::TabGroupSyncService::Observer::OnTabGroupRemoved;
  void OnInitialized() override;
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override;
  void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group,
                         tab_groups::TriggerSource source) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         tab_groups::TriggerSource source) override;

  // bookmarks::BaseBookmarkModelObserver: a saved Space shows once the bookmark
  // folder that names its storage partition has synced.
  void BookmarkModelChanged() override;
  void BookmarkMetaInfoChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkModelBeingDeleted() override;

  // views::ContextMenuController: a Space's icon offers the icons to pick.
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // views::View:
  void AddedToWidget() override;

 private:
  // Rebuilds asynchronously: a change may come from a click on one of the
  // buttons being rebuilt.
  void ScheduleRebuild();
  void Rebuild();
  void SwitchTo(const tab_groups::TabGroupId& group);
  // Opens a saved Space that has no tab group here, then switches to it.
  void OpenSavedSpace(const base::Uuid& sync_id);
  // Records in each Space's bookmark folder the saved tab group it syncs as,
  // so other devices open its tabs in the Space's storage partition.
  void LinkSpaceFolders();
  // Ids of the saved tab groups that a Space's bookmark folder links to.
  std::set<std::string> GetLinkedSavedGroups() const;
  // The bookmark folder of the Space `space`, or of the saved Space `sync_id`.
  const bookmarks::BookmarkNode* FindSpaceFolder(
      const std::string& space,
      const std::string& sync_id) const;
  // The tab group of the Space `space` in this window, if it is open.
  std::optional<tab_groups::TabGroupId> FindGroupOfSpace(
      const std::string& space) const;
  // Switches to the Space of the icon menu, opening it if it is closed.
  void ActivateMenuSpace();
  // Opens the sidebar's theme color picker for the Space of the icon menu.
  void EditMenuSpaceColor();
  // Asks, then deletes the Space of the icon menu: its tabs, its saved tab
  // group and its folder of pinned tabs.
  void ConfirmDeleteMenuSpace();
  void DeleteSpace(const std::string& space,
                   const std::string& sync_id,
                   chrome::MessageBoxResult result);
  // Tints the window with the active Space's theme color.
  void ApplyActiveSpaceColor();
  // The icon a Space's folder records (its "arcomium.space_icon").
  std::string GetSpaceIcon(const std::string& space,
                           const std::string& sync_id) const;

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<views::BoxLayout> layout_ = nullptr;
  raw_ptr<TabStripModel> model_;
  raw_ptr<tab_groups::TabGroupSyncService> sync_service_ = nullptr;
  base::ScopedObservation<tab_groups::TabGroupSyncService,
                          tab_groups::TabGroupSyncService::Observer>
      sync_observation_{this};
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      bookmark_observation_{this};
  // Makes the tab a Space was last on active again, once switched to it.
  void RestoreActiveTab(std::optional<tab_groups::TabGroupId> space);

  // The tab each Space was last on, by its tab group; home has none.
  std::map<std::optional<tab_groups::TabGroupId>, tabs::TabHandle>
      last_active_tabs_;
  // The Space whose icon menu is open.
  std::string menu_space_;
  std::string menu_sync_id_;
  gfx::Point menu_point_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> icon_menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  base::WeakPtrFactory<ArcomiumSpaceSwitcher> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_SPACE_SWITCHER_H_
