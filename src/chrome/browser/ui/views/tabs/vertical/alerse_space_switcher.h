// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SPACE_SWITCHER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SPACE_SWITCHER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_attributes_storage_observer.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tabs/alerse_menu_bar_hook.h"
#include "chrome/browser/ui/tabs/alerse_space_command_hook.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/alerse_frame_color.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/tabs/public/tab_alert.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "third_party/skia/include/core/SkColor.h"

class BrowserWindowInterface;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class MenuRunner;
}

namespace gfx {
class Canvas;
class PointF;
}  // namespace gfx

namespace alerse {
class SpaceColorTransition;

// The icon a Space's folder records, "sunny" for Home and "dot" for others
// without one; `folder` may be null.
std::string GetSpaceIconName(const bookmarks::BookmarkNode* folder,
                             const std::string& space);
// `icon` if a Space can show it, else the sun for home and the dot for the
// other Spaces.
std::string ValidSpaceIcon(const std::string& icon, bool home);
// Draws the Space icon `name` centered on `center`; an emoji icon ignores
// `color` and is drawn at `emoji_alpha`.
void PaintSpaceIcon(gfx::Canvas* canvas,
                    const gfx::PointF& center,
                    const std::string& name,
                    SkColor color,
                    uint8_t emoji_alpha = 255);
// The names of the icons a Space can have, in the picker's order.
std::vector<std::string> GetSpaceIconNames();
}  // namespace alerse

// A Space as the Space bar lists it.
struct AlerseSpaceRef {
  // kDefaultSpace for the window's own Space, "profile:<directory>" for
  // another one.
  std::string space;
  std::u16string title;
};

// Alerse: the Space bar, the row of Spaces at the bottom of the sidebar. A
// Space is a Chromium profile (alerse_space_registry.h) and the window shows
// one: an icon shows that Space's window in this one's place
// (alerse_window_stack.h). The "+" button creates a Space.
class AlerseSpaceSwitcher : public views::View,
                              public bookmarks::BaseBookmarkModelObserver,
                              public ProfileAttributesStorageObserver,
                              public TabStripModelObserver,
                              public views::ContextMenuController,
                              public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(AlerseSpaceSwitcher, views::View)

 public:
  explicit AlerseSpaceSwitcher(BrowserWindowInterface* browser);
  AlerseSpaceSwitcher(const AlerseSpaceSwitcher&) = delete;
  AlerseSpaceSwitcher& operator=(const AlerseSpaceSwitcher&) = delete;
  ~AlerseSpaceSwitcher() override;

  // Creates a Space named `name` ("Space N" if empty) and shows it in
  // `browser`'s place. `created` runs with its profile before it shows.
  using SpaceCreatedCallback = base::OnceCallback<void(Profile*)>;
  // `show`: the window switches to the new Space once it has loaded.
  static void CreateSpace(BrowserWindowInterface* browser,
                          const std::u16string& name = std::u16string(),
                          SpaceCreatedCallback created = {},
                          bool show = true);
  // Runs IDC_ALERSE_NEW_SPACE, _NEXT_SPACE or _PREVIOUS_SPACE in `browser`'s
  // Space bar (alerse_space_command_hook.h); false without one.
  static bool RunSpaceCommand(BrowserWindowInterface* browser, int command_id);
  // Whether such a command can run in `browser` now, and which way a toggle
  // goes (alerse_space_command_hook.h).
  static alerse::SpaceCommandState GetSpaceCommandState(
      BrowserWindowInterface* browser,
      int command_id);
  // macOS' menu bar lists: the Space bar's Spaces and the window's
  // extensions (alerse_menu_bar_hook.h).
  static std::vector<alerse::MenuBarEntry> GetMenuBarList(
      BrowserWindowInterface* browser,
      alerse::MenuBarList list);
  static void RunMenuBarEntry(BrowserWindowInterface* browser,
                              alerse::MenuBarList list,
                              size_t index);
  // The shown Space's name, the window's title as in Arc for macOS.
  static std::u16string GetShownSpaceTitle(BrowserWindowInterface* browser);
  // Opens the active Space's menu of `browser`'s Space bar at `point`.
  static void ShowActiveSpaceMenuIn(BrowserWindowInterface* browser,
                                    const gfx::Point& point,
                                    ui::mojom::MenuSourceType source_type);
  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    alerse::RepaintTree(this);
  }

  // TabStripModelObserver: a dialog of a tab opened or closed, its sound
  // started or stopped, it crashed: what the other Spaces' bars of this
  // window mark.
  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // bookmarks::BaseBookmarkModelObserver: the Space's folder records its
  // name, icon and color.
  void BookmarkModelChanged() override;
  void BookmarkMetaInfoChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkModelBeingDeleted() override;

  // ProfileAttributesStorageObserver: a Space is a profile.
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;

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

  // Switches to the next or the previous Space in the row, as the trackpad's
  // horizontal swipe asks. False at either end.
  bool RotateSpace(bool forward);
  // Whether there is a Space to go to that way.
  bool CanRotateSpace(bool forward) const;
  // The Space next to the active one that way, if any.
  std::optional<AlerseSpaceRef> GetNeighborSpace(bool forward) const;

 private:
  // Rebuilds asynchronously: a change may come from a click on one of the
  // buttons being rebuilt.
  void ScheduleRebuild();
  void Rebuild();
  // What each icon in the row does, in order, and which one is active.
  std::vector<base::RepeatingClosure> space_actions_;
  std::vector<AlerseSpaceRef> space_refs_;
  int active_space_ = -1;
  // Each icon's name, the icon it shows and its theme's light shade (Arc
  // tints a themed Space's glyph with it), for macOS' Spaces menu.
  struct MenuBarSpace {
    std::u16string title;
    std::string icon;
    std::optional<SkColor> tint;
  };
  std::vector<MenuBarSpace> menu_bar_spaces_;
  // Shows the Space whose profile lives in `path` in this window's place
  // (alerse_window_stack.h).
  void ShowSpaceOfPath(const base::FilePath& path);
  // The profile directory of the Space an icon names: kDefaultSpace is this
  // window's own, "profile:<directory>" another one.
  base::FilePath PathOfSpace(const std::string& space) const;
  // What a Space hidden behind this window has to tell, the most pressing
  // first. `kind` 3: a tab waits for the user (a dialog, a permission
  // request, a crashed page). 2: a tab records or shares (`alert` says
  // what). 1: a tab plays sound. 0: nothing.
  struct SpaceNotice {
    int kind = 0;
    std::optional<tabs::TabAlert> alert;
    bool operator==(const SpaceNotice&) const = default;
  };
  static SpaceNotice NoticeOfTabs(const TabStripModel* tabs);
  SpaceNotice NoticeOfSpace(const std::string& space) const;
  // Tells every Space bar when this window's own notice changed.
  void TellIfOwnNoticeChanged();
  // One watch per tab of this window.
  class PermissionWatch;
  void SyncPermissionWatches();
  // The folder of this window's Space (alerse_space_folder.h), if it has one.
  const bookmarks::BookmarkNode* SpaceFolder() const;
  // The plus button's menu: a new Space, folder or tab.
  void ShowAddMenu(const ui::Event& event);
  void ShowSpaceMenu(const std::string& space,
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
  // Shows the Space of the icon menu.
  void ActivateMenuSpace();
  // Opens the sidebar's theme color picker for the Space of the icon menu.
  void EditMenuSpaceColor();
  // Asks, then deletes the Space of the icon menu: its profile.
  void ConfirmDeleteMenuSpace();
  void DeleteSpace(const std::string& space,
                   chrome::MessageBoxResult result);
  // Tints the window with the active Space's theme color.
  void ApplyActiveSpaceColor();
  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<views::BoxLayout> layout_ = nullptr;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      bookmark_observation_{this};
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorageObserver>
      profiles_observation_{this};
  // Another window changed what its Space shows (alerse_space_registry.h).
  base::CallbackListSubscription spaces_subscription_;
  // What this window's tabs last had to tell the other Spaces' bars.
  SpaceNotice own_notice_;
  std::map<content::WebContents*, std::unique_ptr<PermissionWatch>>
      permission_watches_;
  // The Space folder being renamed, and its Space.
  int64_t renaming_folder_ = 0;
  std::string renaming_space_;
  // The Space whose icon menu is open.
  std::string menu_space_;
  gfx::Point menu_point_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  // The plus button at the row's end.
  raw_ptr<views::View> add_button_ = nullptr;
  // Blends the window's color into the shown Space's.
  std::unique_ptr<alerse::SpaceColorTransition> color_transition_;
  // Never invalidated: the delete dialog and the popover hold these.
  base::WeakPtrFactory<AlerseSpaceSwitcher> weak_factory_{this};
  // Only for the pending rebuild, which ScheduleRebuild() replaces.
  base::WeakPtrFactory<AlerseSpaceSwitcher> rebuild_weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SPACE_SWITCHER_H_
