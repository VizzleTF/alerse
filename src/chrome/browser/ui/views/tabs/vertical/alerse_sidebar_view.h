// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SIDEBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SIDEBAR_VIEW_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_space_color.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_space_switcher.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

class AlerseAddressBar;
class BrowserWindowInterface;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class DropTargetEvent;
class LayerTreeOwner;
class OSExchangeData;
}  // namespace ui

namespace views {
class ScrollView;
class Textfield;
class TextfieldController;
class BubbleDialogDelegate;
class MenuRunner;
}  // namespace views

// Alerse: the Arc sidebar of the active Space, drawn as one list:
//
//   favorites     tiles shared by every Space: the bookmarks of a folder
//                 beside the Spaces' (meta info "arcomium.favorites"); one
//                 opens as a pinned tab outside the Spaces, bound to it
//   Space name
//   pinned tree   bookmarks and nested folders of the Space's bookmark folder
//                 (meta info "arcomium.space" = Space id). A pinned item opens
//                 as a tab bound to it; a collapsed folder lists its open tabs
//                 under itself, and hovering it pops up its links.
//   -----------
//   + New Tab
//   tabs          the Space's tabs that are not bound to a pinned item
//
// It replaces the vertical tab list while the strip is expanded. Rows can be
// dragged: within the pinned tree and into folders, from the tabs into the
// pinned tree (pins the tab), from the pinned tree into the tabs (unpins it,
// keeping its tab) and within the tabs.
class AlerseSidebarView : public views::View,
                            public bookmarks::BaseBookmarkModelObserver,
                            public TabStripModelObserver,
                            public views::ContextMenuController,
                            public views::DragController,
                            public ui::SimpleMenuModel::Delegate,
                            public views::AnimationDelegateViews {
  METADATA_HEADER(AlerseSidebarView, views::View)

 public:
  explicit AlerseSidebarView(BrowserWindowInterface* browser);
  // views::View: rows recolor for a light or dark frame. A right click in the
  // empty space below the rows offers Arc's "Edit Theme Color"; a left click
  // there does nothing.
  void OnThemeChanged() override;
  // Rows recolor when the window turns active or not: Arc for macOS lightens
  // an inactive window, and its shades follow (alerse::InkOverFrame).
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // A new folder at the top of the active Space's pinned items, as the plus
  // button's menu asks.
  void NewFolder() { AddFolder(nullptr); }

  // The sidebar's commands, from the macOS menu bar and the command bar:
  // IDC_ALERSE_PIN_TAB (pins the shown tab, or unpins it, as Arc's
  // Cmd+D), IDC_ALERSE_NEW_FOLDER, IDC_ALERSE_CLEAR_TODAY and
  // IDC_ALERSE_TOGGLE_PINNED (macOS: collapses or expands the pinned tabs).
  void RunMenuBarCommand(int command_id);
  // Whether one can run now, and which way a toggle goes.
  alerse::SpaceCommandState GetMenuBarCommandState(int command_id);

  // Arc's theme color picker for the active Space, at `screen_point`.
  void EditSpaceColorAt(const gfx::Point& screen_point);

  // The Space's name in the sidebar (screen coordinates) and where its icon
  // is, for the Space icon picker; false while it is not shown.
  bool GetSpaceTitleAnchor(gfx::Rect* bounds, int* arrow_x) const;
  // Keeps the Space's name lit while its icon picker is open (macOS).
  void SetSpaceTitleHighlighted(bool highlighted);

  // The command bar opened or closed; `new_tab` while it starts a new tab,
  // which Arc for macOS shows on the New Tab row.
  void OnCommandBarChanged(bool open, bool new_tab);

  AlerseSidebarView(const AlerseSidebarView&) = delete;
  AlerseSidebarView& operator=(const AlerseSidebarView&) = delete;
  ~AlerseSidebarView() override;

  // The browser's New Tab command (alerse_command_bar_hook.h): opens the
  // command bar of `browser`'s sidebar, if one is drawn, for a new tab.
  static bool OpenNewTabCommandBar(BrowserWindowInterface* browser);
  // Add Split View (alerse_command_bar_hook.h): opens the command bar of
  // `browser`'s sidebar, if one is drawn, for the pane just added.
  static bool OpenSplitCommandBar(BrowserWindowInterface* browser);
  // alerse::SplitDraggedRowHook: a row dropped on the page's split zone.
  static bool SplitDraggedRow(BrowserWindowInterface* browser,
                              bool start,
                              bool stacked);

  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  // Folders keep their open state in meta info.
  void BookmarkMetaInfoChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkModelBeingDeleted() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // views::DragController:
  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& p) override;
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  // views::View, as the drop target of dragged rows:
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override;

  // This window's Space takes the place of another in the stack: its rows
  // slide in from the side it lies on in the Space bar (`forward`: the
  // trailing side) while a picture of the rows of `source`, the sidebar of
  // the window left, slides out. Prepare, before the window shows, puts both
  // where the slide starts; Play, once it has drawn that, runs the slide.
  void PrepareSpaceEnter(bool forward, AlerseSidebarView* source);
  void PlaySpaceEnter();
  // alerse::SpaceShownHook(): `to` is about to show in the place of `from`.
  static void OnSpaceShown(BrowserWindowInterface* from,
                           BrowserWindowInterface* to);
  // alerse::SpaceDrawnHook(): `to` shows its first frame.
  static void OnSpaceDrawn(BrowserWindowInterface* to);
  // While the fingers move: the rows follow them by `dx`, the rows of the
  // `neighbor` Space butted against their side; with no Space that way, they
  // give only a little.
  void TrackSpaceSwipe(float dx,
                       bool forward,
                       const std::optional<AlerseSpaceRef>& neighbor);
  // The fingers lifted. Committed, both finish the move, then `switch_space`
  // runs; otherwise they spring back.
  void EndSpaceSwipe(bool commit, bool forward, base::OnceClosure switch_space);

 private:
  class Row;

  // The Space the rows are built for: "default", the window's own, or the
  // neighbor a swipe previews.
  std::string GetActiveSpace() const;
  std::u16string GetActiveSpaceTitle() const;
  // The bookmark folder of the active Space; created on demand.
  const bookmarks::BookmarkNode* GetSpaceFolder(bool create);
  // The pinned folder of `space`, adopting an imported one of the same title.
  const bookmarks::BookmarkNode* GetSpaceFolderFor(const std::string& space,
                                                   const std::u16string& title,
                                                   bool create);

  // A Space the context menu can move an item to: another profile.
  struct SpaceTarget {
    base::FilePath path;
    std::u16string title;
  };
  std::vector<SpaceTarget> GetOtherSpaces() const;
  // Moves the context menu's item: a pinned item or folder into the target's
  // folder, a tab into the target's window, opened anew in its storage.
  void MoveToSpace(const SpaceTarget& target);
  void MoveTabToSpace(tabs::TabInterface* tab, const SpaceTarget& target);
  // Removes `node`, copied to another Space, and closes the tabs bound to
  // it and to what it holds.
  void RemoveMovedNode(const bookmarks::BookmarkNode* node);
  void DuplicateItem();
  // The context menu's page: its link and title.
  std::pair<GURL, std::u16string> GetMenuLink() const;
  void CopyLink();
  const bookmarks::BookmarkNode* GetNode(int64_t id) const;
  bool IsInActiveSpace(const tabs::TabInterface* tab) const;
  // The open tab bound to a pinned item, if any.
  tabs::TabInterface* GetBoundTab(int64_t id) const;
  bool IsBound(const tabs::TabInterface* tab) const;
  // Binds tabs to the pinned items their contents name: the binding a
  // restored tab carried through the session.
  void BindTabsByUuid();
  // Records each tab's binding on its contents and in the session.
  void SaveBindings();
  // The Space whose folder holds `node`, or "" outside the Spaces' folders.
  std::string GetSpaceOfNode(const bookmarks::BookmarkNode* node) const;
  // Records on `tab` the uuid of the item it is bound to, "" for none.
  void WritePinnedItem(tabs::TabInterface* tab, const std::string& uuid);
  bool HasBoundDescendant(const bookmarks::BookmarkNode* folder) const;
  // Which tabs under `folder` show their speaker, and muted or not.
  std::string SpeakerState(const bookmarks::BookmarkNode* folder) const;

  ui::ImageModel GetBookmarkIcon(const bookmarks::BookmarkNode* node);
  bool HasOwnIcon(const bookmarks::BookmarkNode* node);
  ui::ImageModel GetTabIcon(tabs::TabInterface* tab) const;
  // The tab's title, or what stands for one while it has none.
  std::u16string GetTabTitle(tabs::TabInterface* tab) const;
  // Whether `tab` shows another page than its pinned item's: not for a
  // redirect of the item's own address nor for another fragment of it.
  bool LeftItsPage(tabs::TabInterface* tab,
                   const bookmarks::BookmarkNode* node) const;

  // Rebuilds asynchronously: a change may come from a click on a row.
  void ScheduleRebuild();
  void Rebuild();
  // Puts the rows of the Space a swipe switched to where its preview was.
  void DropSwipePreview();
  void EndSpaceEnter();
  // Shows the rows again if a committed swipe switched nothing.
  void RestoreSlide();
  // The rows the sidebar shows for `space`, without switching to it.
  std::unique_ptr<views::View> BuildSpacePreview(const AlerseSpaceRef& space);
  // A picture of the rows of `space`, painted off screen from the sidebar
  // of its hidden window in this window's stack; null if the Space has no
  // window there yet.
  std::unique_ptr<views::View> BuildSpaceSnapshot(const AlerseSpaceRef& space);
  // A picture of the rows `other` shows, where it shows them.
  std::unique_ptr<views::View> SnapshotRowsOf(AlerseSidebarView* other);
  // Puts `rows` over the scroll view, clipped to it, on a layer that slides.
  views::View* AddSlideFrame(std::unique_ptr<views::View> rows);
  // Rebuild()'s rows of the active Space, or of preview_space_.
  void BuildRows();
  void HideSpacePreview();
  // Arc's Add Split View: `picked`, shown while `chooser` (the "Choose a
  // tab" pane of a new split) was, takes the chooser's place in the split.
  void FillSplitChooser(base::WeakPtr<content::WebContents> chooser,
                        base::WeakPtr<content::WebContents> picked);
  void AddPinnedRows(const bookmarks::BookmarkNode* folder, int depth);
  void AddBoundTabRows(const bookmarks::BookmarkNode* folder, int depth);
  void AddBookmarkRow(const bookmarks::BookmarkNode* node, int depth);
  void AddTabRow(tabs::TabInterface* tab);
  std::unique_ptr<Row> MakeTabRow(tabs::TabInterface* tab,
                                  ui::ColorId color,
                                  bool active);
  // Arc's one row for a split's two pages, `start` on the start side.
  void AddSplitRow(tabs::TabInterface* start, tabs::TabInterface* end);
  // The speaker of `tab` on its row, if it plays or is muted.
  void AddSpeaker(Row* row, tabs::TabInterface* tab);
  // Moves the keyboard focus from the row `from` to the one below or above.
  void FocusNeighborRow(views::View* from, bool down);
  Row* AddRow(std::unique_ptr<Row> row,
              int64_t node = 0,
              tabs::TabInterface::Handle tab = tabs::TabInterface::Handle());
  // Makes `row` a target for clicks, menus and drags.
  void RegisterRow(Row* row, int64_t node, tabs::TabInterface::Handle tab);

  void OnBookmarkPressed(int64_t id);
  // Loads the saved page of pinned item `id` in its tab again and shows it.
  void ResetPinnedTab(int64_t id);
  void OnFolderPressed(int64_t id);
  // Folder open and close animation: the rows that come or go grow or shrink.
  void StartFolderCollapse(const bookmarks::BookmarkNode* folder);
  // Runs folder_anim_, as long as its distance asks.
  void StartFolderAnimation();
  void FinishFolderAnimation();
  // Shows the folder's rows `progress` of the way, on layers only: the rows
  // are clipped and the ones below shifted, with no layout or repaint.
  void ApplyFolderAnimation(double progress);
  void ResetFolderAnimation();
  void OnTabPressed(tabs::TabInterface::Handle handle);
  void OnTabClosePressed(tabs::TabInterface::Handle handle);
  // The "x" of a pinned item with no tab open: removes the item.
  void OnRemovePinnedPressed(int64_t id);
  // Mutes or unmutes the tab.
  void OnTabMutePressed(tabs::TabInterface::Handle handle);
  void OpenBookmark(const bookmarks::BookmarkNode* node);
  void PinTab(tabs::TabInterface* tab, const bookmarks::BookmarkNode* folder);
  void AddFolder(const bookmarks::BookmarkNode* parent);

  // Folder hover popup listing the folder's links. Unlike a menu it leaves
  // the mouse to the sidebar, so hovering another folder switches it.
  void OnFolderHovered(int64_t id, views::View* row, bool hovered);
  void ShowFolderPopup(int64_t id, views::View* row);
  // Favorites: their folder (created on demand), whether `node` is one, and
  // the tiles above the rows.
  const bookmarks::BookmarkNode* GetFavoritesFolder(bool create);
  bool IsFavorite(const bookmarks::BookmarkNode* node) const;
  void BuildFavorites();
  // Binds pinned tabs outside the Spaces showing a favorite's URL to it.
  void BindFavoritesByUrl(const bookmarks::BookmarkNode* folder);
  // Opens a favorite in its pinned tab, outside the Spaces.
  void OpenFavorite(const bookmarks::BookmarkNode* node);
  // Makes a favorite of `tab` and/or the pinned item `node` at `index` of
  // the favorites (the end if past it); the tab becomes the favorite's.
  void AddFavorite(tabs::TabInterface* tab,
                   const bookmarks::BookmarkNode* node,
                   size_t index);
  // The favorite's tab stays open, as an ordinary tab of home.
  void RemoveFavorite(const bookmarks::BookmarkNode* node);
  // Unpins the browser-pinned tab of a favorite that stops being one.
  void UnpinBoundTab(const bookmarks::BookmarkNode* node);
  // Updates the title and icon of `tab`'s rows in place. False when its rows
  // show more of the tab than that, or it has none: then they are rebuilt.
  bool UpdateTabRows(tabs::TabInterface* tab);
  // `tab` as a pinned tab outside the Spaces (in their default storage), or
  // its replacement opened there.
  tabs::TabInterface* PinAsFavoriteTab(tabs::TabInterface* tab);
  // The links of `folder` and its subfolders, in the sidebar's order.
  std::vector<GURL> GetFolderLinks(const bookmarks::BookmarkNode* folder) const;
  void CopyAllLinks(const bookmarks::BookmarkNode* folder);
#if BUILDFLAG(IS_MAC)
  // Arc's Share Folder...: the system's share picker for its links.
  void ShareFolder(int64_t id);
#endif
  // Makes a new Space of `folder`, its items the Space's pinned ones, and
  // shows it, as Arc's Turn into Space.
  void TurnIntoSpace(const bookmarks::BookmarkNode* folder);
  // Edits the name (and address) of a pinned item or folder.
  void RenameNode(int64_t id);
  // Arc's Change Icon...: its glyph picker under the item's row.
  void ShowIconPicker(const bookmarks::BookmarkNode* node);
  // Sets the meta info `key` of node `id` to the glyph `name`, "" for none.
  void SetNodeIcon(int64_t id, const std::string& key, const std::string& name);
  void DuplicateFolder(const bookmarks::BookmarkNode* folder);
  // Arc's Space menu (the Space bar's), at `screen_point`.
  void ShowSpaceMenu(const gfx::Point& screen_point,
                     ui::mojom::MenuSourceType source_type);
  // Collapses or expands the active Space's pinned tabs under its name.
  void ToggleCollapsedPinned();
  bool IsPinnedCollapsed(const bookmarks::BookmarkNode* folder) const;

  // Fills the popup with `folder`'s items, subfolders opened in place.
  void FillFolderPopup(views::View* contents,
                       const bookmarks::BookmarkNode* folder);
  void AddFolderPopupRows(views::View* container,
                          const bookmarks::BookmarkNode* folder,
                          int depth);
  void AddFolderPopupLink(views::View* container,
                          const bookmarks::BookmarkNode* node,
                          int depth);
  // Arc for macOS' popup: a search field over the rows, which it filters;
  // Up, Down and Return pick a row.
  void BuildFolderPopup(views::View* contents);
  void OnFolderPopupQuery(const std::u16string& query);
  bool OnFolderPopupKey(ui::KeyboardCode key);
  void SelectFolderPopupRow(int index);
  // The links under `folder`, at any depth, that match the query.
  void AddFilteredPopupRows(views::View* container,
                            const bookmarks::BookmarkNode* folder);
  void ToggleFolderPopupFolder(int64_t id);
  void RefreshFolderPopup();
  // Closes the popup once the pointer has left the folder row and the popup.
  void CheckFolderPopupHover();
  void CloseFolderPopup();
  void OnFolderPopupClosed(views::Widget::ClosedReason reason);
  // Arc's theme color picker for the active Space, at the context menu.
  void ShowColorPicker();
  void CloseColorPicker();
  void OnColorPickerClosed(views::Widget::ClosedReason reason);
  void OnSpaceColorPicked(const alerse::SpaceColor& color, bool commit);

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<bookmarks::BookmarkModel> model_;
  raw_ptr<views::View> rows_ = nullptr;
  raw_ptr<views::ScrollView> scroll_ = nullptr;
  // The Space's name (in `title_host_`) and under it the rows that scroll:
  // what slides between Spaces.
  raw_ptr<views::View> list_ = nullptr;
  raw_ptr<views::View> title_host_ = nullptr;
  // The neighbor Space's rows beside these during a swipe.
  raw_ptr<views::View> space_preview_ = nullptr;
  std::optional<AlerseSpaceRef> preview_space_;
  // 1 or -1: the side a swipe's neighbor stands on. 2: the picture of the
  // Space left, sliding out as this one enters.
  int space_preview_side_ = 0;
  // The side a prepared entrance comes from, until it plays; 0: none.
  int enter_side_ = 0;
  bool drop_preview_after_rebuild_ = false;
  base::WeakPtrFactory<AlerseSidebarView> slide_weak_factory_{this};
  // Alerse: its address bar, which opens Arc's command bar.
  raw_ptr<AlerseAddressBar> address_bar_ = nullptr;
  // The favorites' tiles, between the address bar and the rows; they stay
  // while Spaces slide.
  raw_ptr<views::View> favorites_ = nullptr;
  // The command bar is open to start a new tab.
  bool command_bar_new_tab_ = false;

  // What each row stands for, for the context menu.
  struct RowTarget {
    int64_t node = 0;
    tabs::TabInterface::Handle tab;
  };
  base::flat_map<views::View*, RowTarget> row_targets_;
  // Arc's sidebar items for Command-1 to -9: the rows and favorites that are
  // pages, in the order drawn.
  std::vector<RowTarget> GetSidebarItems() const;
  // The row of the shown tab, for Reveal Tab in Sidebar; null when it has
  // none drawn (a favorite's tab, an empty Space).
  views::View* FindActiveTabRow() const;
  // Scrolls the rows to it.
  void RevealActiveTab();

  // Where a dragged row would land.
  struct DropTarget {
    // Into the pinned tree: under `parent` (0: the Space's folder) at `index`.
    int64_t parent = 0;
    size_t index = 0;
    // Into the tabs instead: at this tab strip index.
    std::optional<int> tab_index;
    // Drawn while dragging, in this view's coordinates: a line between rows,
    // or the folder row a drop goes into.
    gfx::Rect indicator;
    bool into_folder = false;
    // Into the favorites instead, at `index`.
    bool favorite = false;
    // Onto this tab's row instead: a split of the two, as in Arc.
    tabs::TabInterface::Handle split_with;
  };
  std::optional<DropTarget> GetDropTarget(const gfx::Point& point);
  void ShowDropIndicator(const std::optional<DropTarget>& target);
  void PerformDrop(DropTarget target,
                   const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);
  // Moves `tab` so it lands before the tab now at `slot`.
  void MoveTabToSlot(tabs::TabInterface* tab, int slot);
  // The dragged row's tab for a split: a tab's own, or a pinned item's,
  // opened if it is not; nullptr for a folder or a favorite.
  tabs::TabInterface* GetDraggedRowTab(bool open);
  // Puts `tab` in a new split beside `with`, on the start side or below it,
  // and shows it; `tab` leaves a split it is in. False when the two cannot
  // share one: the same tab, `with` already in a split, or not of one Space.
  bool SplitTabs(tabs::TabInterface* with,
                 tabs::TabInterface* tab,
                 bool start,
                 bool stacked);
  void EndRowDrag();
  void OnRowDragDone();

  gfx::SlideAnimation folder_animation_{this};
  // The folder's rows: `count` children of `rows_` from `first`, shown from
  // `from` to `to` pixels of their height.
  struct FolderAnimation {
    size_t first = 0;
    size_t count = 0;
    int from = 0;
    int to = 0;
  };
  std::optional<FolderAnimation> folder_anim_;
  // A rebuild asked for while the curtain ran.
  bool rebuild_after_folder_animation_ = false;
  // The folder whose rows are shrinking; it closes when they are gone.
  int64_t collapsing_folder_ = 0;
  // The folder being opened, and the height its rows took while closed.
  int64_t expanding_folder_ = 0;
  int expand_from_ = 0;

  // The row being dragged; rows are not rebuilt meanwhile.
  std::optional<RowTarget> drag_source_;
  bool rebuild_after_drag_ = false;
  // The next rebuild scrolls the rows to the active tab's: it has changed,
  // or the sidebar is new.
  bool reveal_active_tab_ = true;
  raw_ptr<views::View> drop_indicator_ = nullptr;
  // What the indicator shows now: a folder's plate, a split's outline.
  bool drop_indicator_folder_ = false;
  bool drop_indicator_outlined_ = false;
  // Running position used to give rows Arc's fractional pitch.
  float row_pitch_offset_ = 0;
  // Pinned item id -> the tab opened from it.
  std::map<int64_t, tabs::TabInterface::Handle> bound_;
  // Decoded icons imported from Arc ("arcomium.icon" meta info).
  base::flat_map<int64_t, ui::ImageModel> imported_icons_;

  // The item Rename... is editing and its title before: a new one marks it.
  int64_t renaming_node_ = 0;
  std::u16string renaming_title_;
  // The Space name's view, and whether the Space's icon picker is open.
  raw_ptr<views::View> space_title_ = nullptr;
  bool space_title_highlighted_ = false;

  // Context menu target, and where it opened (screen coordinates).
  int64_t menu_node_ = 0;
  gfx::Point menu_point_;
  tabs::TabInterface::Handle menu_tab_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> move_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> pinned_menu_model_;
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<ui::SimpleMenuModel> share_menu_model_;
#endif
  // The Spaces behind the "Move to" submenu's commands.
  std::vector<SpaceTarget> menu_spaces_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The folder popup, its folder and the timers that show and close it.
  std::unique_ptr<views::BubbleDialogDelegate> folder_popup_delegate_;
  std::unique_ptr<views::Widget> folder_popup_;
  int64_t folder_popup_id_ = 0;
  // Subfolders opened inside the popup.
  std::set<int64_t> folder_popup_open_;
  // SpeakerState() of the popup's folder when it was filled.
  std::string folder_popup_speakers_;
  base::OneShotTimer hover_timer_;
  // Coalesces the rebuilds a run of theme changes asks for.
  base::OneShotTimer theme_rebuild_timer_;
  base::CallbackListSubscription paint_as_active_subscription_;
  base::RepeatingTimer folder_popup_timer_;
  int folder_popup_outside_ticks_ = 0;
  class PopupSearchController;
  std::unique_ptr<PopupSearchController> folder_popup_search_;
  raw_ptr<views::Textfield> folder_popup_field_ = nullptr;
  raw_ptr<views::View> folder_popup_rows_ = nullptr;
  std::u16string folder_popup_query_;
  // The popup's rows' items, in order, and the selected one.
  std::vector<int64_t> folder_popup_items_;
  int folder_popup_selected_ = -1;
  // The rows' height unfiltered: Arc's popup keeps it while a query filters.
  int folder_popup_rows_height_ = 0;
  gfx::Point folder_popup_last_point_;

  // The theme color picker.
  std::unique_ptr<views::BubbleDialogDelegate> color_picker_delegate_;
  std::unique_ptr<views::Widget> color_picker_;

  base::WeakPtrFactory<AlerseSidebarView> weak_factory_{this};
  // Not invalidated by rebuilds, unlike `weak_factory_`.
  base::WeakPtrFactory<AlerseSidebarView> drop_weak_factory_{this};
  base::WeakPtrFactory<AlerseSidebarView> split_weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SIDEBAR_VIEW_H_
