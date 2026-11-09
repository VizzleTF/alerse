// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/arcomium_address_bar.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/arcomium_split.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/arcomium_frame_color.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "cc/paint/paint_flags.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"
#include "ui/gfx/color_utils.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace {

// Room around the address field inside the sidebar.
#if BUILDFLAG(IS_MAC)
// Arc for macOS' field: 148x36 pt, 8 pt from the sidebar's edges.
constexpr int kHorizontalInset = 8;
constexpr int kFieldHeight = 36;
#else
constexpr int kHorizontalInset = 4;
#endif
constexpr int kVerticalInset = 4;
constexpr int kCopyIconSize = 16;
constexpr int kCopyButtonSize = 24;
constexpr int kCopyButtonEndInset = 6;
#if BUILDFLAG(IS_MAC)
// Arc for macOS' command bar: 765 pt wide and centred on the window, rims
// and all (the panel 764 pt and its dark outer ring), its 327 pt panel (five
// rows) 5 pt above the middle; the address row is its top 63 pt; 12 pt
// corners and a grey rim.
constexpr int kCommandBarWidth = 764;
constexpr int kCommandBarHeight = 327;
constexpr int kCommandBarOffset = -5;
constexpr int kCommandBarInputHeight = 63;
constexpr int kCommandBarRadius = 12;
constexpr SkAlpha kCommandBarRim = 0x3B;
// Arc for macOS' compact command bar, opened by clicking the field: 348 pt
// wide from the field's left edge, its top a point above the field's, a
// 45 pt input row.
constexpr int kCompactCommandBarWidth = 348;
constexpr int kCompactCommandBarInputHeight = 45;
#else
// Arc's command bar: 722 px wide and centred on the window, its 318 px panel
// 30 px below the middle; the address row is its top 52 px.
constexpr int kCommandBarWidth = 722;
constexpr int kCommandBarHeight = 318;
constexpr int kCommandBarOffset = 30;
constexpr int kCommandBarInputHeight = 52;
constexpr int kCommandBarRadius = 9;
constexpr SkAlpha kCommandBarRim = 0x56;
#endif
constexpr int kCommandBarMargin = 16;

// The top of Arc's command bar panel, behind the address row.
class ArcomiumCommandBarPanel : public views::View {
  METADATA_HEADER(ArcomiumCommandBarPanel, views::View)

 public:
  // Arc for macOS' compact command bar has no line under its input.
  void set_draws_divider(bool draws_divider) {
    if (draws_divider_ != draws_divider) {
      draws_divider_ = draws_divider;
      SchedulePaint();
    }
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    const SkColor background =
        GetColorProvider()->GetColor(kColorOmniboxResultsBackground);
    views::Painter::CreateRoundRectWith1PxBorderPainter(
        background,
        color_utils::BlendTowardMaxContrast(background, kCommandBarRim),
        kCommandBarRadius)
        ->Paint(canvas, size());
#if BUILDFLAG(IS_MAC)
    // Arc for macOS' line under the input: one device pixel, #313030 on
    // #191919.
    if (!draws_divider_) {
      return;
    }
    gfx::ScopedCanvas scoped(canvas);
    const float scale = canvas->UndoDeviceScaleFactor();
    const int bottom = static_cast<int>(std::round(height() * scale));
    canvas->FillRect(
        gfx::Rect(static_cast<int>(std::round(scale)), bottom - 1,
                  static_cast<int>(std::round((width() - 2) * scale)), 1),
        color_utils::BlendTowardMaxContrast(background, 0x1B));
#endif
  }

 private:
  bool draws_divider_ = true;
};

BEGIN_METADATA(ArcomiumCommandBarPanel)
END_METADATA

}  // namespace

class ArcomiumAddressBar::ClickWatcher : public ui::EventObserver {
 public:
  explicit ClickWatcher(ArcomiumAddressBar* bar) : bar_(bar) {}

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override { bar_->OnWindowClick(); }

 private:
  raw_ptr<ArcomiumAddressBar> bar_;
};

ArcomiumAddressBar::ArcomiumAddressBar(BrowserWindowInterface* browser)
    : browser_(browser) {
  SetNotifyEnterExitOnChild(true);
  auto copy = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&ArcomiumAddressBar::CopyAddress,
                          base::Unretained(this)),
      vector_icons::kContentCopyIcon, kCopyIconSize);
  copy->SetTooltipText(l10n_util::GetStringUTF16(IDS_ARCOMIUM_COPY_LINK));
  copy->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ARCOMIUM_COPY_LINK));
  copy->SetVisible(false);
  copy_button_ = AddChildView(std::move(copy));
}

ArcomiumAddressBar::~ArcomiumAddressBar() {
  ReturnLocationBar();
  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
  }
  if (editor_) {
    editor_->RemoveObserver(this);
  }
}

gfx::Size ArcomiumAddressBar::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
#if BUILDFLAG(IS_MAC)
  const int height = kFieldHeight;
#else
  const int height = location_bar_ ? location_bar_->GetPreferredSize().height()
                                   : 0;
#endif
  return gfx::Size(0, location_bar_ ? height + 2 * kVerticalInset : 0);
}

void ArcomiumAddressBar::Layout(PassKey) {
  if (!location_bar_) {
    return;
  }
  gfx::Rect field(size());
  field.Inset(gfx::Insets::VH(kVerticalInset, kHorizontalInset));
  if (!editing_) {
    location_bar_->SetBoundsRect(field);
  }
  copy_button_->SetBounds(
      field.right() - kCopyButtonEndInset - kCopyButtonSize,
      field.y() + (field.height() - kCopyButtonSize) / 2, kCopyButtonSize,
      kCopyButtonSize);
}

void ArcomiumAddressBar::VisibilityChanged(views::View* starting_from,
                                           bool is_visible) {
  ScheduleUpdatePlacement();
}

void ArcomiumAddressBar::AddedToWidget() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_) {
    focus_manager_->AddFocusChangeListener(this);
  }
  ScheduleUpdatePlacement();
}

void ArcomiumAddressBar::RemovedFromWidget() {
  ReturnLocationBar();
  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = nullptr;
  }
}

void ArcomiumAddressBar::OnMouseEntered(const ui::MouseEvent& event) {
  hovered_ = true;
  UpdateCopyButton();
}

void ArcomiumAddressBar::OnMouseExited(const ui::MouseEvent& event) {
  hovered_ = false;
  UpdateCopyButton();
}

bool ArcomiumAddressBar::OnMousePressed(const ui::MouseEvent& event) {
#if BUILDFLAG(IS_MAC)
  return location_bar_ && !editing_ && event.IsOnlyLeftMouseButton();
#else
  return false;
#endif
}

void ArcomiumAddressBar::OnMouseReleased(const ui::MouseEvent& event) {
#if BUILDFLAG(IS_MAC)
  if (location_bar_ && !editing_ && HitTestPoint(event.location())) {
    compact_command_bar_ = true;
    OpenCommandBar(/*new_tab=*/false);
  }
#endif
}

void ArcomiumAddressBar::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
#if BUILDFLAG(IS_MAC)
  if (!editing_ || !location_bar_ || editing_location_bar_ != location_bar_) {
    return;
  }
  // As the sidebar's location bar paints itself (location_bar_view.cc).
  const ui::ColorProvider* colors = GetColorProvider();
  gfx::Rect field(size());
  field.Inset(gfx::Insets::VH(kVerticalInset, kHorizontalInset));
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(
      color_utils::BlendTowardMaxContrast(arcomium::FrameColor(this), 0x1B));
  canvas->DrawRoundRect(gfx::RectF(field), 12, flags);
  canvas->DrawImageInt(
      gfx::CreateVectorIcon(vector_icons::kArcomiumSearchIcon, 16,
                            colors->GetColor(kColorArcomiumSidebarNewTab)),
      field.x() + 10, field.y() + (field.height() - 16) / 2);
  canvas->DrawStringRect(
      field_text_, field_font_,
      colors->GetColor(field_placeholder_ ? kColorOmniboxTextDimmed
                                          : kColorOmniboxText),
      gfx::Rect(field.x() + 34, field.y(), std::max(0, field.width() - 42),
                field.height()));
#endif
}

void ArcomiumAddressBar::OnViewIsDeleting(views::View* observed_view) {
  if (observed_view == edit_placeholder_) {
    edit_placeholder_observation_.Reset();
    edit_placeholder_ = nullptr;
    return;
  }
  placeholder_observation_.Reset();
  toolbar_placeholder_ = nullptr;
}

void ArcomiumAddressBar::OpenCommandBar(bool new_tab) {
  LocationBarView* location_bar = GetEditableLocationBar();
  if (!location_bar) {
    // A new window's sidebar takes the location bar just after it shows.
    if (new_tab && GetWidget() && IsDrawn() && !pending_new_tab_) {
      pending_new_tab_ = true;
      ScheduleUpdatePlacement();
      return;
    }
    if (new_tab) {
      chrome::NewTab(browser_, NewTabTypes::kNewTabButton);
    }
    return;
  }
  location_bar->SetArcomiumNewTab(new_tab);
  // Focus opens the command bar (UpdateEditing).
  location_bar->FocusLocation(/*is_user_initiated=*/true,
                              /*clear_focus_if_failed=*/false);
}

LocationBarView* ArcomiumAddressBar::GetEditableLocationBar() const {
  if (location_bar_) {
    return location_bar_;
  }
  if (!GetWidget() || !IsDrawn()) {
    return nullptr;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  ToolbarView* toolbar = browser_view && browser_view->IsToolbarVisible()
                             ? browser_view->toolbar()
                             : nullptr;
  return toolbar ? toolbar->location_bar_view() : nullptr;
}

void ArcomiumAddressBar::OnDidChangeFocus(views::View* focused_before,
                                          views::View* focused_now) {
  // Focus going nowhere, as when the window deactivates, keeps the field: the
  // address gets focus back with the window.
  if (!moving_ && focused_now && (editing_ || GetEditableLocationBar())) {
    ScheduleUpdateEditing();
  }
}

void ArcomiumAddressBar::OnWindowClick() {
  if (!editing_ || !editor_ || !click_monitor_) {
    return;
  }
  // Clicks in the command bar, its suggestions or another floating widget of
  // the window are theirs.
  views::Widget* top = editor_->parent();
  const gfx::Point point = click_monitor_->GetLastMouseLocation();
  const views::Widget::Widgets children =
      top ? views::Widget::GetAllChildWidgets(top->GetNativeView())
          : views::Widget::Widgets();
  for (views::Widget* child : children) {
    if (child != top && child->IsVisible() &&
        child->GetWindowBoundsInScreen().Contains(point)) {
      return;
    }
  }
  // Posted: the click goes on to what it hit first.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcomiumAddressBar::CloseCommandBar,
                                weak_factory_.GetWeakPtr()));
}

void ArcomiumAddressBar::CloseCommandBar() {
  if (!editing_) {
    return;
  }
  // Leaving the field drops what was typed, as in Arc; the page gets the
  // keyboard back.
  editing_location_bar_->Revert();
  StopEditing();
  if (content::WebContents* contents =
          browser_->GetTabStripModel()->GetActiveWebContents()) {
    contents->Focus();
  }
}

void ArcomiumAddressBar::OnWidgetDestroying(views::Widget* widget) {
  StopEditing();
  editor_->RemoveObserver(this);
}

void ArcomiumAddressBar::ScheduleUpdateEditing() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcomiumAddressBar::UpdateEditing,
                                weak_factory_.GetWeakPtr()));
}

void ArcomiumAddressBar::UpdateEditing() {
  LocationBarView* location_bar =
      editing_ ? editing_location_bar_.get() : GetEditableLocationBar();
  if (!location_bar || !focus_manager_) {
    return;
  }
  const bool focused = location_bar->Contains(focus_manager_->GetFocusedView());
  if (focused && !editing_) {
    StartEditing(location_bar);
  } else if (!focused && editing_ && focus_manager_->GetFocusedView()) {
    // Leaving the field drops what was typed, as in Arc.
    location_bar->Revert();
    StopEditing();
  }
}

void ArcomiumAddressBar::StartEditing(LocationBarView* location_bar) {
  // The panel floats over the whole window, not over the sidebar's widget.
  views::Widget* widget = GetWidget() ? GetWidget()->GetTopLevelWidget()
                                      : nullptr;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!widget || !browser_view || !IsDrawn()) {
    return;
  }
  if (!editor_) {
    editor_ = std::make_unique<ThemeCopyingWidget>(widget);
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_CONTROL);
    params.name = "ArcomiumAddressEditor";
    params.parent = widget->GetNativeView();
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    // Not activatable, like the find bar: the window keeps the keyboard and
    // its focus manager routes it here.
    editor_->Init(std::move(params));
    // The panel's top: the suggestions' color, a 1 px rim and round corners,
    // around the address inset as far as the suggestions' frame reaches out
    // of it, so the two make one panel.
    auto contents = std::make_unique<ArcomiumCommandBarPanel>();
    contents->SetLayoutManager(std::make_unique<views::FillLayout>());
    contents->SetBorder(views::CreateEmptyBorder(
        RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets()));
    editor_->SetContentsView(std::move(contents));
    editor_->AddObserver(this);
  }

  // Where Arc puts its command bar.
  const gfx::Rect area =
      browser_view->ConvertRectToWidget(browser_view->GetLocalBounds());
  const int width =
      std::min(kCommandBarWidth, area.width() - 2 * kCommandBarMargin);
  gfx::Rect bounds(
      area.x() + (area.width() - width + 1) / 2,
      area.y() + std::max(kCommandBarMargin,
                          (area.height() - kCommandBarHeight) / 2 +
                              kCommandBarOffset),
      width, kCommandBarInputHeight);
  const bool compact = std::exchange(compact_command_bar_, false) &&
                       location_bar == location_bar_;
#if BUILDFLAG(IS_MAC)
  if (compact) {
    // Over the field, in the panel's widget's coordinates.
    gfx::Rect field(size());
    field.Inset(gfx::Insets::VH(kVerticalInset, kHorizontalInset));
    views::View::ConvertRectToScreen(this, &field);
    const gfx::Vector2d to_area =
        area.origin() - browser_view->GetBoundsInScreen().origin();
    field += to_area;
    // Its dark outer ring then lands where Arc's does: 8 pt in, 44 pt down.
    bounds = gfx::Rect(field.x() + 1, field.y() - 2,
                       std::min(kCompactCommandBarWidth,
                                area.right() - kCommandBarMargin - field.x() -
                                    1),
                       kCompactCommandBarInputHeight);
  }
#endif

  if (location_bar == location_bar_) {
    // What the field shows stays drawn while it is away (OnPaint).
    OmniboxViewViews* omnibox = location_bar->omnibox_view();
    field_text_ = std::u16string(omnibox->GetText());
    field_placeholder_ = field_text_.empty();
    field_font_ = omnibox->GetFontList();
    if (field_placeholder_) {
      field_text_ = std::u16string(omnibox->GetPlaceholderText());
      field_font_ = field_font_.DeriveWithSizeDelta(-1);
    }
#if BUILDFLAG(IS_MAC)
    location_bar->SetCanProcessEventsWithinSubtree(true);
#endif
  }
  moving_ = true;
  editing_ = true;
  editing_location_bar_ = location_bar;
  if (location_bar != location_bar_) {
    // The toolbar keeps the address's place while it is away.
    views::View* toolbar = location_bar->parent();
    auto placeholder = std::make_unique<views::View>();
    if (const views::FlexSpecification* flex =
            location_bar->GetProperty(views::kFlexBehaviorKey)) {
      placeholder->SetProperty(views::kFlexBehaviorKey, *flex);
    }
    if (const gfx::Insets* margins =
            location_bar->GetProperty(views::kMarginsKey)) {
      placeholder->SetProperty(views::kMarginsKey, *margins);
    }
    edit_placeholder_ = toolbar->AddChildViewAt(
        std::move(placeholder), *toolbar->GetIndexOf(location_bar));
    edit_placeholder_observation_.Observe(edit_placeholder_);
  }
  editor_->GetContentsView()->AddChildView(location_bar);
  editor_->SetBounds(bounds);
  editor_->Show();
  moving_ = false;
  if (!click_watcher_) {
    click_watcher_ = std::make_unique<ClickWatcher>(this);
  }
  click_monitor_ = views::EventMonitor::CreateWindowMonitor(
      click_watcher_.get(), widget->GetNativeWindow(),
      {ui::EventType::kMousePressed, ui::EventType::kTouchPressed});
  location_bar->SetArcomiumCommandBarCompact(compact);
  location_bar->SetArcomiumCommandBar(true);
  static_cast<ArcomiumCommandBarPanel*>(editor_->GetContentsView())
      ->set_draws_divider(!compact);
  UpdateCopyButton();

  location_bar->FocusLocation(/*is_user_initiated=*/true,
                              /*clear_focus_if_failed=*/false);
  // An empty field (a new tab, an empty Space) asks for rows itself: focus
  // moved with the field to the panel drops the on-focus suggestions. Arc for
  // macOS' compact command bar opens empty too, listing the open tabs.
  // So does a new split's pane on "Choose a tab" (arcomium_split.h).
  if (compact || location_bar->ArcomiumOpensNewTab() ||
      location_bar->omnibox_view()->GetText().empty() ||
      arcomium::IsSplitChooser(
          browser_->GetTabStripModel()->GetActiveWebContents())) {
    location_bar->omnibox_view()->SetUserText(std::u16string());
  } else {
    location_bar->omnibox_view()->SelectAll(/*reversed=*/true);
  }
  SchedulePaint();
  if (command_bar_callback_) {
    command_bar_callback_.Run(true, location_bar->ArcomiumOpensNewTab());
  }
}

void ArcomiumAddressBar::StopEditing() {
  if (!editing_) {
    return;
  }
  click_monitor_.reset();
  moving_ = true;
  editing_ = false;
  LocationBarView* location_bar = editing_location_bar_;
  editing_location_bar_ = nullptr;
  location_bar->SetArcomiumCommandBar(false);
  location_bar->SetArcomiumCommandBarCompact(false);
  location_bar->SetArcomiumNewTab(false);
#if !BUILDFLAG(IS_MAC)
  // The command bar dims the placeholder; the top bar's pill shows it bright
  // again, as in Arc for Windows. The color is set only with the text.
  location_bar->omnibox_view()->InstallPlaceholderText();
#endif
  if (location_bar == location_bar_) {
    AddChildViewAt(location_bar, 0);
#if BUILDFLAG(IS_MAC)
    location_bar->SetCanProcessEventsWithinSubtree(false);
#endif
  } else if (edit_placeholder_) {
    views::View* placeholder = edit_placeholder_;
    views::View* toolbar = placeholder->parent();
    edit_placeholder_observation_.Reset();
    edit_placeholder_ = nullptr;
    toolbar->AddChildViewAt(location_bar, *toolbar->GetIndexOf(placeholder));
    toolbar->RemoveChildViewT(placeholder);
  }
  editor_->Hide();
  moving_ = false;
  InvalidateLayout();
  SchedulePaint();
  if (command_bar_callback_) {
    command_bar_callback_.Run(false, false);
  }
}

void ArcomiumAddressBar::ScheduleUpdatePlacement() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcomiumAddressBar::UpdatePlacement,
                                weak_factory_.GetWeakPtr()));
}

void ArcomiumAddressBar::UpdatePlacement() {
  const bool sidebar_drawn = GetWidget() && IsDrawn();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  // A new tab's command bar waiting for the location bar opens now.
  absl::Cleanup open_pending = [this] {
    if (std::exchange(pending_new_tab_, false)) {
      if (GetEditableLocationBar()) {
        OpenCommandBar(/*new_tab=*/true);
      } else {
        chrome::NewTab(browser_, NewTabTypes::kNewTabButton);
      }
    }
  };
  if (sidebar_drawn && browser_view && !browser_view->IsToolbarVisible()) {
    TakeLocationBar();
    return;
  }
  ReturnLocationBar();
  // A toolbar stays beside the sidebar (Windows): the location bar stays in
  // it, as compact as here, with the site centred, as in Arc for Windows.
  ToolbarView* toolbar = browser_view ? browser_view->toolbar() : nullptr;
  if (LocationBarView* location_bar =
          toolbar ? toolbar->location_bar_view() : nullptr) {
    location_bar->SetArcomiumCompact(sidebar_drawn);
    location_bar->SetArcomiumHovered(sidebar_drawn);
    location_bar->SetArcomiumCentered(sidebar_drawn);
  }
}

void ArcomiumAddressBar::TakeLocationBar() {
  if (location_bar_) {
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  ToolbarView* toolbar = browser_view ? browser_view->toolbar() : nullptr;
  LocationBarView* location_bar =
      toolbar ? toolbar->location_bar_view() : nullptr;
  if (!location_bar || location_bar->parent() != toolbar) {
    return;
  }
  auto placeholder = std::make_unique<views::View>();
  if (const views::FlexSpecification* flex =
          location_bar->GetProperty(views::kFlexBehaviorKey)) {
    placeholder->SetProperty(views::kFlexBehaviorKey, *flex);
  }
  toolbar_placeholder_ = toolbar->AddChildViewAt(
      std::move(placeholder), *toolbar->GetIndexOf(location_bar));
  placeholder_observation_.Observe(toolbar_placeholder_);
  location_bar_ = AddChildViewAt(location_bar, 0);
#if BUILDFLAG(IS_MAC)
  // Arc for macOS' field is one button: the address bar takes its clicks.
  // Windows keeps the location bar's own (its site icon and warnings).
  location_bar_->SetCanProcessEventsWithinSubtree(false);
#endif
  location_bar_->SetArcomiumCompact(true);
  location_bar_->SetArcomiumHovered(hovered_);
  PreferredSizeChanged();
}

void ArcomiumAddressBar::ReturnLocationBar() {
  if (!location_bar_) {
    return;
  }
  StopEditing();
  LocationBarView* location_bar = location_bar_;
  location_bar_ = nullptr;
#if BUILDFLAG(IS_MAC)
  location_bar->SetCanProcessEventsWithinSubtree(true);
#endif
  location_bar->SetArcomiumCompact(false);
  views::View* placeholder = toolbar_placeholder_;
  views::View* toolbar = placeholder ? placeholder->parent() : nullptr;
  placeholder_observation_.Reset();
  toolbar_placeholder_ = nullptr;
  if (!toolbar) {
    return;
  }
  toolbar->AddChildViewAt(location_bar, *toolbar->GetIndexOf(placeholder));
  toolbar->RemoveChildViewT(placeholder);
  copy_button_->SetVisible(false);
  PreferredSizeChanged();
}

void ArcomiumAddressBar::UpdateCopyButton() {
  // As in Arc: not while the command bar has the field, nor on an empty
  // Space's blank page, which has no address to copy.
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  copy_button_->SetVisible(
      hovered_ && location_bar_ && !editing_ && contents &&
      !TabStripModel::IsArcomiumEmptySpace(contents));
  if (location_bar_) {
    location_bar_->SetArcomiumHovered(hovered_);
  }
}

void ArcomiumAddressBar::CopyAddress() {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!contents || contents->GetVisibleURL().is_empty()) {
    return;
  }
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::UTF8ToUTF16(contents->GetVisibleURL().spec()));
}

BEGIN_METADATA(ArcomiumAddressBar)
END_METADATA
