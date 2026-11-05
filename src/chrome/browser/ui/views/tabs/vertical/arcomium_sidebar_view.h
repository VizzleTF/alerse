// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_SIDEBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_SIDEBAR_VIEW_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_space_color.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_space_switcher.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

class ArcomiumAddressBar;
class BrowserWindowInterface;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace ui {
class DropTargetEvent;
class LayerTreeOwner;
class OSExchangeData;
}  // namespace ui

namespace views {
class ScrollView;
class BubbleDialogDelegate;
class MenuRunner;
}  // namespace views

// Arcomium: the Arc sidebar of the active Space, drawn as one list:
//
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
class ArcomiumSidebarView : public views::View,
                            public bookmarks::BaseBookmarkModelObserver,
                            public TabStripModelObserver,
                            public views::ContextMenuController,
                            public views::DragController,
                            public ui::SimpleMenuModel::Delegate,
                            public gfx::AnimationDelegate {
  METADATA_HEADER(ArcomiumSidebarView, views::View)

 public:
  explicit ArcomiumSidebarView(BrowserWindowInterface* browser);
  // views::View: rows recolor for a light or dark frame. A right click in the
  // empty space below the rows offers Arc's "Edit Theme Color"; a left click
  // there does nothing.
  void OnThemeChanged() override;

  // Arc's theme color picker for the active Space, at `screen_point`.
  void EditSpaceColorAt(const gfx::Point& screen_point);

  ArcomiumSidebarView(const ArcomiumSidebarView&) = delete;
  ArcomiumSidebarView& operator=(const ArcomiumSidebarView&) = delete;
  ~ArcomiumSidebarView() override;

  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;

  // gfx::AnimationDelegate:
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
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override;

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

  // Arcomium: a horizontal swipe is about to switch Spaces, forward or back;
  // the next Space's rows slide in. CancelSpaceSlide() if it did not switch.
  void PrepareSpaceSlide(bool forward, int distance = 48);
  void CancelSpaceSlide();
  // While the fingers move: the rows follow them by `dx`, the rows of the
  // `neighbor` Space butted against their side; with no Space that way, they
  // give only a little.
  void TrackSpaceSwipe(float dx,
                       bool forward,
                       const std::optional<ArcomiumSpaceRef>& neighbor);
  // The fingers lifted. Committed, both finish the move, then `switch_space`
  // runs; otherwise they spring back.
  void EndSpaceSwipe(bool commit, bool forward, base::OnceClosure switch_space);

 private:
  class Row;

  // Space id of the focused tab group, or "default" outside of Spaces.
  std::string GetActiveSpace() const;
  std::u16string GetActiveSpaceTitle() const;
  // The bookmark folder of the active Space; created on demand.
  const bookmarks::BookmarkNode* GetSpaceFolder(bool create);
  // The pinned folder of `space`, adopting an imported one of the same title.
  const bookmarks::BookmarkNode* GetSpaceFolderFor(const std::string& space,
                                                   const std::u16string& title,
                                                   bool create);

  // A Space the context menu can move an item to.
  struct SpaceTarget {
    // Unset for home, the tabs outside of groups.
    std::optional<tab_groups::TabGroupId> group;
    std::string space;
    std::u16string title;
  };
  std::vector<SpaceTarget> GetOtherSpaces() const;
  // Moves the context menu's item: a pinned item to the target's pinned
  // folder, a tab into the target, opened anew in the target's storage.
  void MoveToSpace(const SpaceTarget& target);
  void MoveTabToSpace(tabs::TabInterface* tab, const SpaceTarget& target);
  void DuplicateItem();
  void CopyLink();
  const bookmarks::BookmarkNode* GetNode(int64_t id) const;
  bool IsInActiveSpace(const tabs::TabInterface* tab) const;
  // The open tab bound to a pinned item, if any.
  tabs::TabInterface* GetBoundTab(int64_t id) const;
  bool IsBound(const tabs::TabInterface* tab) const;
  // Binds open tabs showing a pinned item's URL to it (after a restart).
  void BindTabsByUrl(const bookmarks::BookmarkNode* folder);
  bool HasBoundDescendant(const bookmarks::BookmarkNode* folder) const;

  ui::ImageModel GetBookmarkIcon(const bookmarks::BookmarkNode* node);
  bool HasOwnIcon(const bookmarks::BookmarkNode* node);
  ui::ImageModel GetTabIcon(tabs::TabInterface* tab) const;

  // Rebuilds asynchronously: a change may come from a click on a row.
  void ScheduleRebuild();
  void Rebuild();
  // Slides the rows of a Space that a swipe just switched to in from the side
  // the swipe went to.
  void RunPendingSlide();
  // Shows the rows again if a committed swipe switched nothing.
  void RestoreSlide();
  // The rows the sidebar shows for `space`, without switching to it.
  std::unique_ptr<views::View> BuildSpacePreview(const ArcomiumSpaceRef& space);
  // Rebuild()'s rows of the active Space, or of preview_space_.
  void BuildRows();
  void HideSpacePreview();
  std::optional<tab_groups::TabGroupId> GetActiveGroup() const;
  void AddPinnedRows(const bookmarks::BookmarkNode* folder, int depth);
  void AddBoundTabRows(const bookmarks::BookmarkNode* folder, int depth);
  void AddBookmarkRow(const bookmarks::BookmarkNode* node, int depth);
  void AddTabRow(tabs::TabInterface* tab);
  Row* AddRow(std::unique_ptr<Row> row,
              int64_t node = 0,
              tabs::TabInterface::Handle tab = tabs::TabInterface::Handle());

  void OnBookmarkPressed(int64_t id);
  void OnFolderPressed(int64_t id);
  // Folder open and close animation: the rows that come or go grow or shrink.
  void StartFolderCollapse(const bookmarks::BookmarkNode* folder);
  void FinishFolderAnimation();
  // Shows the folder's rows `progress` of the way, on layers only: the rows
  // are clipped and the ones below shifted, with no layout or repaint.
  void ApplyFolderAnimation(double progress);
  void ResetFolderAnimation();
  void OnTabPressed(tabs::TabInterface::Handle handle);
  void OnTabClosePressed(tabs::TabInterface::Handle handle);
  void OpenBookmark(const bookmarks::BookmarkNode* node);
  void PinTab(tabs::TabInterface* tab, const bookmarks::BookmarkNode* folder);
  void AddFolder(const bookmarks::BookmarkNode* parent);

  // Folder hover popup listing the folder's links. Unlike a menu it leaves
  // the mouse to the sidebar, so hovering another folder switches it.
  void OnFolderHovered(int64_t id, views::View* row, bool hovered);
  void ShowFolderPopup(int64_t id, views::View* row);
  // Fills the popup with `folder`'s items, subfolders opened in place.
  void FillFolderPopup(views::View* contents,
                       const bookmarks::BookmarkNode* folder);
  void AddFolderPopupRows(views::View* container,
                          const bookmarks::BookmarkNode* folder,
                          int depth);
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
  void OnSpaceColorPicked(const arcomium::SpaceColor& color, bool commit);

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<bookmarks::BookmarkModel> model_;
  raw_ptr<views::View> rows_ = nullptr;
  raw_ptr<views::ScrollView> scroll_ = nullptr;
  int pending_slide_ = 0;
  // The neighbor Space's rows beside these during a swipe.
  raw_ptr<views::View> space_preview_ = nullptr;
  std::optional<ArcomiumSpaceRef> preview_space_;
  int space_preview_side_ = 0;
  bool drop_preview_after_rebuild_ = false;
  int pending_slide_distance_ = 48;
  base::WeakPtrFactory<ArcomiumSidebarView> slide_weak_factory_{this};
  std::string pending_slide_from_;
  base::TimeTicks pending_slide_until_;
  // Arcomium: its address bar, which opens Arc's command bar.
  raw_ptr<ArcomiumAddressBar> address_bar_ = nullptr;

  // What each row stands for, for the context menu.
  struct RowTarget {
    int64_t node = 0;
    tabs::TabInterface::Handle tab;
  };
  base::flat_map<views::View*, RowTarget> row_targets_;

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
  };
  std::optional<DropTarget> GetDropTarget(const gfx::Point& point);
  void ShowDropIndicator(const std::optional<DropTarget>& target);
  void PerformDrop(DropTarget target,
                   const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);
  // Moves `tab` so it lands before the tab now at `slot`.
  void MoveTabToSlot(tabs::TabInterface* tab, int slot);
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
  // The folder whose rows are shrinking; it closes when they are gone.
  int64_t collapsing_folder_ = 0;
  // The folder being opened, and the height its rows took while closed.
  int64_t expanding_folder_ = 0;
  int expand_from_ = 0;

  // The row being dragged; rows are not rebuilt meanwhile.
  std::optional<RowTarget> drag_source_;
  bool rebuild_after_drag_ = false;
  raw_ptr<views::View> drop_indicator_ = nullptr;
  // Running position used to give rows Arc's fractional pitch.
  float row_pitch_offset_ = 0;
  // Pinned item id -> the tab opened from it.
  std::map<int64_t, tabs::TabInterface::Handle> bound_;
  // Decoded icons imported from Arc ("arcomium.icon" meta info).
  base::flat_map<int64_t, ui::ImageModel> imported_icons_;

  // Context menu target, and where it opened (screen coordinates).
  int64_t menu_node_ = 0;
  gfx::Point menu_point_;
  tabs::TabInterface::Handle menu_tab_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> move_menu_model_;
  // The Spaces behind the "Move to" submenu's commands.
  std::vector<SpaceTarget> menu_spaces_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The folder popup, its folder and the timers that show and close it.
  std::unique_ptr<views::BubbleDialogDelegate> folder_popup_delegate_;
  std::unique_ptr<views::Widget> folder_popup_;
  int64_t folder_popup_id_ = 0;
  // Subfolders opened inside the popup.
  std::set<int64_t> folder_popup_open_;
  base::OneShotTimer hover_timer_;
  base::RepeatingTimer folder_popup_timer_;
  int folder_popup_outside_ticks_ = 0;

  // The theme color picker.
  std::unique_ptr<views::BubbleDialogDelegate> color_picker_delegate_;
  std::unique_ptr<views::Widget> color_picker_;

  base::WeakPtrFactory<ArcomiumSidebarView> weak_factory_{this};
  // Not invalidated by rebuilds, unlike `weak_factory_`.
  base::WeakPtrFactory<ArcomiumSidebarView> drop_weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_SIDEBAR_VIEW_H_
