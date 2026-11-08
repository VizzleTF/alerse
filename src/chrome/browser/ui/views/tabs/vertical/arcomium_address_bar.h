// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_ADDRESS_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_ADDRESS_BAR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_observer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/gfx/font_list.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;
class LocationBarView;

namespace views {
class EventMonitor;
class ImageButton;
class Widget;
}

// Arcomium: the address bar at the top of the Arc sidebar, as in Arc. Where
// the sidebar replaces the toolbar (macOS) it holds the toolbar's own location
// bar while the sidebar shows and gives it back while the sidebar is hidden,
// so the address stays reachable; beside a toolbar it stays empty.
// Hovering it shows a button that copies the page's address. Editing the
// address, here or in the toolbar beside the sidebar, happens in Arc's command
// bar: a panel centred on the window.
class ArcomiumAddressBar : public views::View,
                           public views::ViewObserver,
                           public views::FocusChangeListener,
                           public views::WidgetObserver {
  METADATA_HEADER(ArcomiumAddressBar, views::View)

 public:
  explicit ArcomiumAddressBar(BrowserWindowInterface* browser);
  ArcomiumAddressBar(const ArcomiumAddressBar&) = delete;
  ArcomiumAddressBar& operator=(const ArcomiumAddressBar&) = delete;
  ~ArcomiumAddressBar() override;

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

  // views::ViewObserver: a placeholder goes with the toolbar when the window
  // closes; the location bar then stays where it is and goes with it.
  void OnViewIsDeleting(views::View* observed_view) override;

  // views::FocusChangeListener: the floating field opens when the address gets
  // focus and closes when something else does.
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::WidgetObserver: the floating field closes with the window; the
  // location bar must not go with it.
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  class ClickWatcher;

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
  void UpdateCopyButton();
  void CopyAddress();
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
  bool hovered_ = false;
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
  // What the field showed when the command bar took the location bar.
  std::u16string field_text_;
  bool field_placeholder_ = false;
  gfx::FontList field_font_;
  base::WeakPtrFactory<ArcomiumAddressBar> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ARCOMIUM_ADDRESS_BAR_H_
