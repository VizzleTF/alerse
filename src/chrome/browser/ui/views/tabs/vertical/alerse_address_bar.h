// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_ADDRESS_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_ADDRESS_BAR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "chrome/browser/ui/views/extensions/alerse_site_controls.h"
#include "chrome/browser/ui/views/frame/alerse_frame_color.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_observer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"
#include "ui/gfx/font_list.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;
class LocationBarView;

namespace views {
class EventMonitor;
class ImageButton;
class Widget;
}

// Alerse: the address bar at the top of the Arc sidebar, as in Arc. Where
// the sidebar replaces the toolbar (macOS) it holds the toolbar's own location
// bar while the sidebar shows and gives it back while the sidebar is hidden,
// so the address stays reachable; beside a toolbar it stays empty.
// Hovering it shows a button that copies the page's address. Editing the
// address, here or in the toolbar beside the sidebar, happens in Arc's command
// bar: a panel centred on the window.
class AlerseAddressBar : public views::View,
                           public views::ViewObserver,
                           public views::FocusChangeListener,
                           public views::WidgetObserver,
                           public ExtensionsToolbarViewModel::Observer {
  METADATA_HEADER(AlerseAddressBar, views::View)

 public:
  explicit AlerseAddressBar(BrowserWindowInterface* browser);
  AlerseAddressBar(const AlerseAddressBar&) = delete;
  AlerseAddressBar& operator=(const AlerseAddressBar&) = delete;
  ~AlerseAddressBar() override;

  // Opens Arc's command bar: on the address, or empty to start a new tab.
  // In a window still placing its location bar, once it is in place.
  void OpenCommandBar(bool new_tab);

  // Runs with (open, new_tab) as the command bar opens and closes.
  void SetCommandBarCallback(base::RepeatingCallback<void(bool, bool)> callback) {
    command_bar_callback_ = std::move(callback);
  }

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  // On macOS the held location bar takes no events until edited: a click on
  // the field opens the command bar, a right click its context menu (the
  // sidebar's).
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  // While the location bar is in the command bar, the field it left stays
  // drawn, as Arc's does.
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    alerse::RepaintTree(this);
  }

  // views::ViewObserver: a placeholder goes with the toolbar when the window
  // closes; the location bar then stays where it is and goes with it.
  void OnViewIsDeleting(views::View* observed_view) override;
  void OnViewRemovedFromWidget(views::View* observed_view) override;

  // views::FocusChangeListener: the floating field opens when the address gets
  // focus and closes when something else does.
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::WidgetObserver: the floating field closes with the window; the
  // location bar must not go with it.
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // ExtensionsToolbarViewModel::Observer: Arc's pinned extensions, in the
  // field (one) or as tiles under it (two or more).
  void OnActionsInitialized() override;
  void OnActionAdded(const ToolbarActionsModel::ActionId& action_id) override;
  void OnActionRemoved(const ToolbarActionsModel::ActionId& action_id) override;
  void OnActionUpdated(const ToolbarActionsModel::ActionId& action_id) override;
  void OnPinnedActionsChanged() override;
  void OnActiveWebContentsChanged(bool is_same_document,
                                  content::WebContents* web_contents) override;

 private:
  class ClickWatcher;
  class SidebarHoverWatcher;

  // A click anywhere in the window outside the command bar and its
  // suggestions closes it, as in Arc, even where the click takes no focus
  // (the sidebar's empty space).
  void OnWindowClick();
  void CloseCommandBar();
  // Moves the location bar here or back to the toolbar, whichever matches the
  // sidebar's visibility. Posted: visibility changes arrive mid-layout.
  void ScheduleUpdatePlacement();
  void UpdatePlacement();
  void TakeLocationBar();
  void ReturnLocationBar();
  // Shows the field's hover buttons (site controls, Copy Link and a single
  // pinned extension) while the pointer is anywhere on the sidebar, as Arc
  // for macOS does.
  void UpdateCopyButton();
  void CopyAddress();
  // The copy button's check mark after a copy, or its link again.
  void ShowCopied(bool copied);
  // Stops observing the extensions model, telling it so while it exists.
  void ReleaseExtensions(bool model_exists);
  void ShowSiteControls();
  // Watches the extensions of the window's toolbar once it is there.
  void ObserveExtensions();
  // Rebuilds the pinned extensions' button and tiles.
  void UpdateExtensions();
  void RunExtension(const ToolbarActionsModel::ActionId& id);
  // Asks where the pointer is: over the sidebar or not.
  void PollSidebarHover();
  // Shows or hides the hover buttons, and polls only while an exit may be
  // missed.
  void SetSidebarHovered(bool hovered);
  // The field's rect, and how many rows of extension tiles go under it.
  gfx::Rect GetFieldBounds() const;
  int GetTileRows() const;
  // Move the location bar into the floating field and back. Posted: focus
  // changes arrive while the focus manager is busy.
  void ScheduleUpdateEditing();
  void UpdateEditing();
  void StartEditing(LocationBarView* location_bar);
  void StopEditing();
  // The location bar whose focus opens the command bar: the one held here, or
  // beside the sidebar the toolbar's.
  LocationBarView* GetEditableLocationBar() const;

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<LocationBarView> location_bar_ = nullptr;
  // Stands in the toolbar's flexible slot while the location bar is here, so
  // the toolbar buttons keep their places.
  raw_ptr<views::View> toolbar_placeholder_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      placeholder_observation_{this};
  raw_ptr<views::ImageButton> copy_button_ = nullptr;
  raw_ptr<views::ImageButton> site_button_ = nullptr;
  // The single pinned extension, in the field.
  raw_ptr<views::ImageButton> pinned_button_ = nullptr;
  // Fades the address out before the hover buttons.
  raw_ptr<views::View> text_fade_ = nullptr;
  // Two or more pinned extensions, as tiles under the field.
  std::vector<raw_ptr<views::View>> extension_tiles_;
  // The tiles' and the field button's context menus.
  std::vector<std::unique_ptr<alerse_site_controls::ExtensionMenu>>
      extension_menus_;
  std::vector<std::string> pinned_ids_;
  // The single pinned extension shows a badge or was blocked on this page.
  bool pinned_notice_ = false;
  // Observed by hand, not with a ScopedObservation: the model can be gone
  // before this view learns it, and then it must not be told anything.
  raw_ptr<ExtensionsToolbarViewModel> extensions_model_ = nullptr;
  // The toolbar's extensions container owns the model and can go before this
  // view when the window closes: the model is let go of as the container
  // leaves the window (the model still exists then) or, failing that, as it
  // is deleted (the model is already gone: nothing is removed from it).
  raw_ptr<views::View> extensions_container_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      extensions_container_observation_{this};
  bool extensions_container_gone_ = false;
  base::RepeatingTimer sidebar_hover_poll_;
  // The sidebar's region and the handler that hears the pointer on it.
  views::ViewTracker hover_region_;
  std::unique_ptr<SidebarHoverWatcher> hover_watcher_;
  // The window: its activation and visibility decide the poll.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      window_observation_{this};
  // Puts the link back on the copy button after the check mark of a copy.
  base::OneShotTimer copy_feedback_timer_;
  // The pointer is on the field itself, and anywhere on the sidebar.
  bool hovered_ = false;
  bool sidebar_hovered_ = false;
  // The floating field and whether the location bar is in it now.
  std::unique_ptr<views::Widget> editor_;
  bool editing_ = false;
  // The location bar in the command bar, and the toolbar slot it came from,
  // held by a placeholder until it goes back.
  raw_ptr<LocationBarView> editing_location_bar_ = nullptr;
  raw_ptr<views::View> edit_placeholder_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      edit_placeholder_observation_{this};
  // Set while the location bar changes widgets: the focus it loses then does
  // not close the field.
  bool moving_ = false;
  raw_ptr<views::FocusManager> focus_manager_ = nullptr;
  // Watches the window's clicks while the command bar is open.
  std::unique_ptr<ClickWatcher> click_watcher_;
  std::unique_ptr<views::EventMonitor> click_monitor_;
  base::RepeatingCallback<void(bool, bool)> command_bar_callback_;
  // A new tab's command bar asked for before the location bar was placed.
  bool pending_new_tab_ = false;
  // The next command bar opens compact, anchored to the field (macOS: a
  // click on the field).
  bool compact_command_bar_ = false;
  // What the field showed when the command bar took the location bar.
  std::u16string field_text_;
  bool field_placeholder_ = false;
  gfx::FontList field_font_;
  base::WeakPtrFactory<AlerseAddressBar> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_ADDRESS_BAR_H_
