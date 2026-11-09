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
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tabs/arcomium_menu_bar_hook.h"
#include "chrome/browser/ui/tabs/arcomium_space_command_hook.h"
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
#include "third_party/skia/include/core/SkColor.h"

class BrowserWindowInterface;

namespace views {
class MenuRunner;
}
class TabStripModel;

namespace gfx {
class Canvas;
class PointF;
}  // namespace gfx

namespace arcomium {
class SpaceColorTransition;

// The icon a Space's folder records, "sunny" for Home and "dot" for others
// without one; `folder` may be null.
std::string GetSpaceIconName(const bookmarks::BookmarkNode* folder,
                             const std::string& space);
// Draws the Space icon `name` centered on `center`; an emoji icon ignores
// `color` and is drawn at `emoji_alpha`.
void PaintSpaceIcon(gfx::Canvas* canvas,
                    const gfx::PointF& center,
                    const std::string& name,
                    SkColor color,
                    uint8_t emoji_alpha = 255);
// The names of the icons a Space can have, in the picker's order.
std::vector<std::string> GetSpaceIconNames();
}  // namespace arcomium

// Arcomium: row of Spaces at the bottom of the vertical tab strip. Every Space
// is a tab group; its dot switches the strip into focus mode for that group.
// The "+" button creates a Space with its own storage partition. Spaces sync
// as saved tab groups: one from another device, or closed here, shows too and
// opens with its tabs.
// A Space as the switcher's row lists it: open, closed or synced.
struct ArcomiumSpaceRef {
  // The Space's id; kDefaultSpace for home, empty for a synced Space.
  std::string space;
  std::u16string title;
  // Its tab group here, if it is open.
  std::optional<tab_groups::TabGroupId> group;
};

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
  // Runs IDC_ARCOMIUM_NEW_SPACE, _NEXT_SPACE or _PREVIOUS_SPACE in `browser`'s
  // Space bar (arcomium_space_command_hook.h); false without one.
  static bool RunSpaceCommand(BrowserWindowInterface* browser, int command_id);
  // Whether such a command can run in `browser` now, and which way a toggle
  // goes (arcomium_space_command_hook.h).
  static arcomium::SpaceCommandState GetSpaceCommandState(
      BrowserWindowInterface* browser,
      int command_id);
  // macOS' menu bar lists: the Space bar's Spaces and the window's
  // extensions (arcomium_menu_bar_hook.h).
  static std::vector<arcomium::MenuBarEntry> GetMenuBarList(
      BrowserWindowInterface* browser,
      arcomium::MenuBarList list);
  static void RunMenuBarEntry(BrowserWindowInterface* browser,
                              arcomium::MenuBarList list,
                              size_t index);
  // The shown Space's name, the window's title as in Arc for macOS.
  static std::u16string GetShownSpaceTitle(BrowserWindowInterface* browser);
  // Opens the active Space's menu of `browser`'s Space bar at `point`.
  static void ShowActiveSpaceMenuIn(BrowserWindowInterface* browser,
                                    const gfx::Point& point,
                                    ui::mojom::MenuSourceType source_type);
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
  void MenuClosed(ui::SimpleMenuModel* source) override;

  // Arc's Space menu for the Space shown, at `point` (screen coordinates), as
  // the sidebar opens it from its own empty places.
  void ShowActiveSpaceMenu(const gfx::Point& point,
                           ui::mojom::MenuSourceType source_type);
  // The Space's name follows its folder's, as Rename Space edits it.
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;

  // views::View:
  void AddedToWidget() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // Switches to the next or the previous Space in the row, closed ones
  // included, as the trackpad's horizontal swipe asks. False at either end.
  bool RotateSpace(bool forward);
  // Whether there is a Space to go to that way.
  bool CanRotateSpace(bool forward) const;
  // The Space next to the active one that way, if any.
  std::optional<ArcomiumSpaceRef> GetNeighborSpace(bool forward) const;

 private:
  // Rebuilds asynchronously: a change may come from a click on one of the
  // buttons being rebuilt.
  void ScheduleRebuild();
  // ScheduleRebuild() in two seconds, for a step that waits on session
  // restore or on the bookmark model.
  void RetryRebuildLater();
  void Rebuild();
  // What each icon in the row does, in order, and which one is active.
  std::vector<base::RepeatingClosure> space_actions_;
  std::vector<ArcomiumSpaceRef> space_refs_;
  int active_space_ = -1;
  // Each icon's name, the icon it shows and its theme's light shade (Arc
  // tints a themed Space's glyph with it), for macOS' Spaces menu.
  struct MenuBarSpace {
    std::u16string title;
    std::string icon;
    std::optional<SkColor> tint;
  };
  std::vector<MenuBarSpace> menu_bar_spaces_;
  void SwitchTo(const tab_groups::TabGroupId& group);
  // Opens a saved Space that has no tab group here, then switches to it.
  void OpenSavedSpace(const base::Uuid& sync_id);
  // Opens every saved Space without a tab group here, as Arc keeps every
  // Space open, without leaving the Space shown. True if it opened one.
  bool OpenSavedSpacesHere();
  // Folds the tab groups of a Space that has more than one here (one saved
  // group per device, from before the group id derived from the Space) into
  // the group with the derived id, once per profile. True if it merged any.
  bool MergeDuplicateSpaces();
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
  // The plus button's menu: a new Space, folder or tab.
  void ShowAddMenu(const ui::Event& event);
  void ShowSpaceMenu(const std::string& space,
                     const std::string& sync_id,
                     const gfx::Point& point,
                     ui::mojom::MenuSourceType source_type);
  // The bookmark folder of the menu's Space, added if it has none yet.
  const bookmarks::BookmarkNode* GetOrAddMenuSpaceFolder();
  // Edits the name of the menu's Space in its folder.
  void RenameMenuSpace();
  // Change Space Icon...: Arc's picker under the Space's name or button.
  void ShowMenuSpaceIconPicker();
  // Collapse / Expand All Folders: closes every folder of the menu's Space
  // while one is open, else opens them all.
  void ToggleMenuSpaceFolders();
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
  // The Space folder being renamed, and its Space.
  int64_t renaming_folder_ = 0;
  std::string renaming_space_;
  // The Space whose icon menu is open.
  std::string menu_space_;
  std::string menu_sync_id_;
  gfx::Point menu_point_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  // The plus button at the row's end.
  raw_ptr<views::View> add_button_ = nullptr;
  // Whether the tab group sync service has loaded its groups: before, a
  // Space's group looks missing.
  bool sync_initialized_ = false;
  // Saved Spaces OpenSavedSpacesHere() has looked at: open here already or
  // opened by it. None is opened twice.
  std::set<std::string> opened_saved_;
  // What the icons last showed of each saved group (OnTabGroupUpdated()).
  std::map<base::Uuid, std::string> group_signatures_;
  // Blends the window's color into the shown Space's.
  std::unique_ptr<arcomium::SpaceColorTransition> color_transition_;
  // The delayed retry of MergeDuplicateSpaces() and OpenSavedSpacesHere().
  base::OneShotTimer retry_timer_;
  // Never invalidated: the delete dialog and RestoreActiveTab() hold these.
  base::WeakPtrFactory<ArcomiumSpaceSwitcher> weak_factory_{this};
  // Only for the pending rebuild, which ScheduleRebuild() replaces.
  base::WeakPtrFactory<ArcomiumSpaceSwitcher> rebuild_weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_SPACE_SWITCHER_H_
