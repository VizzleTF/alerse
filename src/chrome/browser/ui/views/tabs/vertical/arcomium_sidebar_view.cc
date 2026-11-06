// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/arcomium_sidebar_view.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_address_bar.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "build/build_config.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/arcomium_spaces.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_mini_player.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/tabs/public/tab_group.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/vector_icons/vector_icons.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/animation/animation_builder.h"
#include "base/functional/callback_helpers.h"
#include "ui/compositor/layer_animator.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// The text of a dragged row's data, which only this view accepts.
constexpr char16_t kRowDragData[] = u"arcomium-sidebar-row";

constexpr char kSpaceMetaKey[] = "arcomium.space";
constexpr char kOpenMetaKey[] = "arcomium.open";
constexpr char kIconMetaKey[] = "arcomium.icon";
constexpr char kFolderIconMetaKey[] = "arcomium.folder_icon";
constexpr char kDefaultSpace[] = "default";

// Measured on Arc for Windows at 100% scale. Rows are 43.3 px apart, so row
// heights alternate between 43 and 44.
constexpr float kRowPitch = 43.3f;
constexpr int kIconX = 16;
constexpr int kTextX = 44;
constexpr int kFolderGlyphSize = 18;
constexpr int kIconSize = 16;
constexpr int kIndent = 9;
constexpr int kSpaceLabelHeight = 21;
constexpr int kSpaceLabelTextHeight = 13;
constexpr int kSpaceLabelX = 18;
constexpr int kDividerInset = 9;
constexpr int kDividerTop = 11;
constexpr int kDividerBottom = 9;
constexpr int kItemFontSize = 14;
constexpr int kLabelFontSize = 12;
constexpr int kRowBackgroundInsetX = 8;
constexpr int kRowBackgroundInsetY = 4;
constexpr float kRowBackgroundRadius = 8;

// The colors (chrome/browser/ui/color/arcomium_color_mixer.cc).
constexpr ui::ColorId kItemColor = kColorArcomiumSidebarItem;
constexpr ui::ColorId kFolderColor = kColorArcomiumSidebarFolder;
constexpr ui::ColorId kSubtleColor = kColorArcomiumSidebarSubtle;
constexpr ui::ColorId kNewTabColor = kColorArcomiumSidebarNewTab;
constexpr ui::ColorId kPlusColor = kColorArcomiumSidebarPlus;
constexpr ui::ColorId kDividerColor = kColorArcomiumSidebarDivider;
constexpr ui::ColorId kGlyphColor = kColorArcomiumSidebarGlyph;
constexpr ui::ColorId kGlyphFillColor = kColorArcomiumSidebarGlyphFill;
constexpr ui::ColorId kHoverColor = kColorArcomiumSidebarHover;
constexpr ui::ColorId kActiveColor = kColorArcomiumSidebarActive;

// Animations: folders opening and closing, the folder popup fading in.
constexpr base::TimeDelta kFolderAnimationDuration = base::Milliseconds(150);
constexpr base::TimeDelta kPopupFadeDuration = base::Milliseconds(120);

// Folder popup: hover delay, size, color and pointer polling.
constexpr base::TimeDelta kFolderPopupDelay = base::Milliseconds(200);
constexpr int kPopupWidth = 260;
constexpr int kPopupPadding = 4;
constexpr ui::ColorId kPopupColor = kColorArcomiumSidebarPopup;
constexpr base::TimeDelta kPopupPollInterval = base::Milliseconds(30);
// Polls the pointer may spend outside before the popup closes.
constexpr int kPopupOutsideTicks = 10;

enum Command {
  kPinTab = 1,
  kNewFolder,
  kRename,
  kRemove,
  kCloseTab,
  kCopyLink,
  kDuplicate,
  kMoveTo,
  kEditColor,
  kToggleMute,
  kSplitView,
  kEditPinnedPage,
  kReplacePinnedUrl,
  kEditPinnedItem,
  // "Move to" submenu: one command per Space from here.
  kMoveToFirst = 100,
};

// Arc draws its sidebar with Segoe UI Variable: "Text" optical size,
// semilight for items and semibold for folders; "Small" for the Space name.
// Segoe UI is the fallback before Windows 11.
constexpr auto kSemilight = static_cast<gfx::Font::Weight>(350);

gfx::FontList ArcFont(const char* family, int size, gfx::Font::Weight weight) {
#if BUILDFLAG(IS_WIN)
  return gfx::FontList({family, "Segoe UI"}, gfx::Font::NORMAL, size, weight);
#else
  // Elsewhere Arc uses the system font. macOS fonts come in the standard
  // weights only, so semilight is regular there.
  const gfx::FontList system;
  return system.DeriveWithSizeDelta(size - system.GetFontSize())
      .DeriveWithWeight(weight == kSemilight ? gfx::Font::Weight::NORMAL
                                             : weight);
#endif
}

gfx::FontList ItemFont(bool bold) {
  return ArcFont("Segoe UI Variable Text", kItemFontSize,
                 bold ? gfx::Font::Weight::SEMIBOLD : kSemilight);
}

std::unique_ptr<views::Label> MakeLabel(const std::u16string& text,
                                        const gfx::FontList& font,
                                        ui::ColorId color) {
  auto label = std::make_unique<views::Label>(text);
  label->SetFontList(font);
  label->SetEnabledColor(color);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetElideBehavior(gfx::ELIDE_TAIL);
  label->SetSubpixelRenderingEnabled(false);
  return label;
}

// Arc's folder glyph: a light frame with a thick top edge. A folder holding
// open tabs shows two dashes inside; otherwise its custom Arc icon, if any.
class FolderGlyph : public views::View {
  METADATA_HEADER(FolderGlyph, views::View)

 public:
  FolderGlyph(bool has_open_tabs, std::string custom_icon)
      : has_open_tabs_(has_open_tabs), custom_icon_(std::move(custom_icon)) {
    SetPreferredSize(gfx::Size(kFolderGlyphSize, kFolderGlyphSize));
    SetCanProcessEventsWithinSubtree(false);
  }

  // Arc's folder is a 17 px square, larger than a favicon.
  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const SkColor glyph = arcomium::SidebarInk(this, kGlyphColor);
    const SkColor glyph_fill = arcomium::SidebarInk(this, kGlyphFillColor);
    flags.setColor(glyph);
    canvas->DrawRoundRect(gfx::RectF(0.5f, 1, 16.5f, 16.5f), 3, flags);
    flags.setColor(glyph_fill);
    canvas->DrawRoundRect(gfx::RectF(2, 5, 13.5f, 11), 1.5f, flags);
    flags.setColor(glyph);
    if (has_open_tabs_) {
      canvas->DrawRect(gfx::RectF(4, 9.5f, 3.5f, 1.6f), flags);
      canvas->DrawRect(gfx::RectF(10, 9.5f, 3.5f, 1.6f), flags);
    } else if (custom_icon_ == "chatBubbleEllipses") {
      canvas->DrawRoundRect(gfx::RectF(5, 7.5f, 7.5f, 5.5f), 2.5f, flags);
      canvas->DrawRect(gfx::RectF(6, 12.5f, 2, 2), flags);
      flags.setColor(glyph_fill);
      for (float x : {6.6f, 8.2f, 9.8f}) {
        canvas->DrawRect(gfx::RectF(x, 9.8f, 1, 1), flags);
      }
    } else if (custom_icon_ == "code") {
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(1.2f);
      SkPathBuilder left;
      left.moveTo(7, 8).lineTo(5, 10.5f).lineTo(7, 13);
      SkPathBuilder right;
      right.moveTo(10.5f, 8).lineTo(12.5f, 10.5f).lineTo(10.5f, 13);
      canvas->DrawPath(left.detach(), flags);
      canvas->DrawPath(right.detach(), flags);
    }
  }

 private:
  const bool has_open_tabs_;
  const std::string custom_icon_;
};

BEGIN_METADATA(FolderGlyph)
END_METADATA

// The thin "+" of the New Tab row.
class PlusGlyph : public views::View {
  METADATA_HEADER(PlusGlyph, views::View)

 public:
  PlusGlyph() {
    SetPreferredSize(gfx::Size(kIconSize, kIconSize));
    SetCanProcessEventsWithinSubtree(false);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(arcomium::SidebarInk(this, kPlusColor));
    canvas->DrawRect(gfx::RectF(3, 7.4f, 12, 1.2f), flags);
    canvas->DrawRect(gfx::RectF(8.4f, 2.5f, 1.2f, 12), flags);
  }
};

BEGIN_METADATA(PlusGlyph)
END_METADATA

// The 1px divider between the pinned area and the tabs.
class Divider : public views::View {
  METADATA_HEADER(Divider, views::View)

 public:
  Divider() {
    SetPreferredSize(gfx::Size(0, kDividerTop + 1 + kDividerBottom));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(
        gfx::Rect(kDividerInset, kDividerTop, width() - 2 * kDividerInset, 1),
        arcomium::SidebarInk(this, kDividerColor));
  }
};

BEGIN_METADATA(Divider)
END_METADATA

std::unique_ptr<views::View> MakeImageIcon(const ui::ImageModel& image) {
  auto view = std::make_unique<views::ImageView>(image);
  view->SetImageSize(gfx::Size(kIconSize, kIconSize));
  view->SetCanProcessEventsWithinSubtree(false);
  return view;
}

const bookmarks::BookmarkNode* FindSpaceFolder(
    const bookmarks::BookmarkNode* parent,
    const std::string& space) {
  for (const auto& child : parent->children()) {
    std::string value;
    if (child->is_folder() && child->GetMetaInfo(kSpaceMetaKey, &value) &&
        value == space) {
      return child.get();
    }
  }
  return nullptr;
}

std::string FolderIcon(const bookmarks::BookmarkNode* folder) {
  std::string value;
  folder->GetMetaInfo(kFolderIconMetaKey, &value);
  return value;
}

bool IsOpen(const bookmarks::BookmarkNode* folder) {
  std::string value;
  return folder->GetMetaInfo(kOpenMetaKey, &value) && value == "1";
}

}  // namespace

// One line of the sidebar: icon, optional "/", title and, for tabs, a close
// button shown on hover.
class ArcomiumSidebarView::Row : public views::Button {
  METADATA_HEADER(Row, views::Button)

 public:
  Row(PressedCallback callback,
      std::unique_ptr<views::View> icon,
      const std::u16string& title,
      int depth,
      const gfx::FontList& font,
      ui::ColorId color,
      bool slash,
      bool active)
      : views::Button(std::move(callback)),
        depth_(depth),
        active_(active),
        color_(color),
        height_(static_cast<int>(kRowPitch)) {
    icon_ = AddChildView(std::move(icon));
    if (slash) {
      slash_ = AddChildView(MakeLabel(u"/", font, kSubtleColor));
    }
    label_ = AddChildView(MakeLabel(title, font, color));
    SetTooltipText(title);
    GetViewAccessibility().SetName(title);
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    SetInstallFocusRingOnFocus(false);
  }

  void SetCloseCallback(PressedCallback callback) {
    auto close = views::CreateVectorImageButtonWithNativeTheme(
        std::move(callback), vector_icons::kCloseIcon, 18);
    close->SetTooltipText(l10n_util::GetStringUTF16(IDS_ARCOMIUM_CLOSE_TAB));
    close->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ARCOMIUM_CLOSE_TAB));
    close_ = AddChildView(std::move(close));
    // As in Arc, shown only while the row is hovered.
    close_->SetVisible(false);
    // Hovering the close button keeps the row hovered.
    SetNotifyEnterExitOnChild(true);
  }

  void SetHoverCallback(base::RepeatingCallback<void(bool)> callback) {
    hover_callback_ = std::move(callback);
  }

  void set_height(int height) { height_ = height; }

  std::u16string title() const { return std::u16string(label_->GetText()); }

  void SetDragDoneCallback(base::RepeatingClosure callback) {
    drag_done_callback_ = std::move(callback);
  }

  void OnDragDone() override {
    views::Button::OnDragDone();
    if (drag_done_callback_) {
      drag_done_callback_.Run();
    }
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(0, height_);
  }

  void AddedToWidget() override {
    views::Button::AddedToWidget();
    adopt_hover_ = true;
  }

  // The folder popup shows without activation, and macOS sends an inactive
  // window no mouse moves: its rows take their hover from its pointer polls.
  void SetPolledHover(bool hovered) {
    if (polled_hover_ != hovered) {
      polled_hover_ = hovered;
      SchedulePaint();
    }
  }

  void Layout(PassKey) override {
    // The sidebar rebuilds its rows whenever a tab changes (a title, a load):
    // a new row under the pointer is hovered at once, close button included,
    // instead of blinking until the pointer moves.
    if (adopt_hover_ && !size().IsEmpty()) {
      adopt_hover_ = false;
      if (IsMouseHovered()) {
        SetAnimateOnStateChange(false);
        SetState(STATE_HOVERED);
        SetAnimateOnStateChange(true);
        if (close_) {
          close_->SetVisible(true);
        }
      }
    }
    const int shift = depth_ * kIndent;
    const int center_y = height_ / 2;
    const gfx::Size icon_size = icon_->GetPreferredSize();
    icon_->SetBounds(kIconX + shift - (icon_size.width() - kIconSize) / 2,
                     center_y - icon_size.height() / 2, icon_size.width(),
                     icon_size.height());
    int x = kTextX + shift;
    int right = width() - kRowBackgroundInsetX;
    if (close_) {
      const gfx::Size size = close_->GetPreferredSize();
      close_->SetBounds(right - size.width() - 4, center_y - size.height() / 2,
                        size.width(), size.height());
      if (close_->GetVisible()) {
        right = close_->x() - 2;
      }
    }
    if (slash_) {
      const int w = slash_->GetPreferredSize().width();
      slash_->SetBounds(x, 0, w, height_);
      x += w + 5;
    }
    label_->SetBounds(x, 0, std::max(0, right - x), height_);
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::Button::OnMouseEntered(event);
    UpdateHover(true);
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    label_->SetEnabledColor(arcomium::SidebarInk(this, color_));
    if (slash_) {
      slash_->SetEnabledColor(arcomium::SidebarInk(this, kSubtleColor));
    }
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    views::Button::OnMouseExited(event);
    UpdateHover(false);
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    // The hover highlight fades with the button's hover animation.
    const double hover =
        polled_hover_ ? 1.0 : hover_animation().GetCurrentValue();
    if (!active_ && hover <= 0) {
      return;
    }
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const SkColor hover_color = arcomium::SidebarInk(this, kHoverColor);
    flags.setColor(active_ ? arcomium::SidebarInk(this, kActiveColor)
                           : SkColorSetA(hover_color,
                                         static_cast<U8CPU>(std::round(
                                             SkColorGetA(hover_color) *
                                             hover))));
    gfx::RectF bounds(0, 0, width(), height_);
    bounds.Inset(gfx::InsetsF::VH(kRowBackgroundInsetY, kRowBackgroundInsetX));
    canvas->DrawRoundRect(bounds, kRowBackgroundRadius, flags);
  }

 private:
  void UpdateHover(bool hovered) {
    if (close_) {
      close_->SetVisible(hovered);
      InvalidateLayout();
    }
    SchedulePaint();
    if (hover_callback_) {
      hover_callback_.Run(hovered);
    }
  }

  const int depth_;
  const bool active_;
  const ui::ColorId color_;
  int height_;
  raw_ptr<views::View> icon_ = nullptr;
  raw_ptr<views::Label> slash_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageButton> close_ = nullptr;
  bool adopt_hover_ = false;
  bool polled_hover_ = false;
  base::RepeatingCallback<void(bool)> hover_callback_;
  base::RepeatingClosure drag_done_callback_;
};

BEGIN_METADATA(ArcomiumSidebarView, Row)
END_METADATA

ArcomiumSidebarView::ArcomiumSidebarView(BrowserWindowInterface* browser)
    : browser_(browser),
      model_(
          BookmarkModelFactory::GetForBrowserContext(browser->GetProfile())) {
  // The address bar stays at the top, the rows scroll, the mini player stays
  // at the bottom.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  address_bar_ = AddChildView(std::make_unique<ArcomiumAddressBar>(browser_));
  // Clips the rows while they slide between Spaces.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);
  auto* scroll = AddChildView(std::make_unique<views::ScrollView>());
  scroll_ = scroll;
  // A layer for the Space slide.
  scroll->SetPaintToLayer();
  scroll->layer()->SetFillsBoundsOpaquely(false);
  layout->SetFlexForView(scroll, 1);
  scroll->SetBackgroundColor(std::nullopt);
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll->ClipHeightTo(0, 100000);
  auto rows = std::make_unique<views::View>();
  rows->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  rows->set_context_menu_controller(this);
  // And the empty space below them: Arc's "Edit Theme Color".
  set_context_menu_controller(this);
  rows_ = scroll->SetContents(std::move(rows));
  AddChildView(std::make_unique<ArcomiumMiniPlayer>(browser_));
  drop_indicator_ = AddChildView(std::make_unique<views::View>());
  drop_indicator_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  drop_indicator_->SetCanProcessEventsWithinSubtree(false);
  drop_indicator_->SetVisible(false);

  if (model_) {
    model_->AddObserver(this);
  }
  browser_->GetTabStripModel()->AddObserver(this);
  folder_animation_.SetSlideDuration(kFolderAnimationDuration);
  folder_animation_.SetTweenType(gfx::Tween::LINEAR);
  Rebuild();
}

ArcomiumSidebarView::~ArcomiumSidebarView() {
  CloseFolderPopup();
  if (model_) {
    model_->RemoveObserver(this);
  }
}

void ArcomiumSidebarView::BookmarkModelChanged() {
  ScheduleRebuild();
}

void ArcomiumSidebarView::BookmarkMetaInfoChanged(
    const bookmarks::BookmarkNode* node) {
  ScheduleRebuild();
}

void ArcomiumSidebarView::BookmarkModelBeingDeleted() {
  model_->RemoveObserver(this);
  model_ = nullptr;
}

void ArcomiumSidebarView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  ScheduleRebuild();
}

void ArcomiumSidebarView::OnTabChangedAt(tabs::TabInterface* tab,
                                         TabChangeType change_type) {
  // A tab of a Space not shown changes nothing here (its title, its loading),
  // unless a pinned item shown is bound to it.
  if (tab && tab->GetGroup() != GetActiveGroup()) {
    const tabs::TabInterface::Handle handle = tab->GetHandle();
    if (std::ranges::none_of(bound_, [&handle](const auto& entry) {
          return entry.second == handle;
        })) {
      return;
    }
  }
  ScheduleRebuild();
}

void ArcomiumSidebarView::OnTabGroupChanged(const TabGroupChange& change) {
  ScheduleRebuild();
}

void ArcomiumSidebarView::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  ScheduleRebuild();
}

std::optional<tab_groups::TabGroupId> ArcomiumSidebarView::GetActiveGroup()
    const {
  return preview_space_ ? preview_space_->group
                        : browser_->GetTabStripModel()->GetFocusedGroup();
}

std::string ArcomiumSidebarView::GetActiveSpace() const {
  if (preview_space_) {
    return preview_space_->space;
  }
  const TabStripModel* tabs = browser_->GetTabStripModel();
  if (std::optional<tab_groups::TabGroupId> group = tabs->GetFocusedGroup()) {
    if (std::optional<std::string> space = arcomium::GetSpaceOfGroupOrFolder(
            browser_->GetProfile(), tabs, *group)) {
      return *space;
    }
  }
  return kDefaultSpace;
}

std::u16string ArcomiumSidebarView::GetActiveSpaceTitle() const {
  if (preview_space_) {
    return preview_space_->title;
  }
  const TabStripModel* tabs = browser_->GetTabStripModel();
  std::optional<tab_groups::TabGroupId> group = tabs->GetFocusedGroup();
  if (group && tabs->group_model() &&
      tabs->group_model()->ContainsTabGroup(*group)) {
    const std::u16string& title =
        tabs->group_model()->GetTabGroup(*group)->visual_data()->title();
    if (!title.empty()) {
      return title;
    }
  }
  return std::u16string();
}

const bookmarks::BookmarkNode* ArcomiumSidebarView::GetSpaceFolder(
    bool create) {
  return GetSpaceFolderFor(GetActiveSpace(), GetActiveSpaceTitle(), create);
}

const bookmarks::BookmarkNode* ArcomiumSidebarView::GetSpaceFolderFor(
    const std::string& space,
    const std::u16string& title,
    bool create) {
  if (!model_ || !model_->loaded()) {
    return nullptr;
  }
  const bookmarks::BookmarkNode* folder =
      FindSpaceFolder(model_->other_node(), space);
  if (!folder && !title.empty()) {
    // Adopt a folder imported for a Space of the same name.
    for (const auto& child : model_->other_node()->children()) {
      std::string value;
      if (child->is_folder() && child->GetTitle() == title &&
          child->GetMetaInfo(kSpaceMetaKey, &value) &&
          value.starts_with("arc:")) {
        folder = child.get();
        model_->SetNodeMetaInfo(folder, kSpaceMetaKey, space);
        break;
      }
    }
  }
  if (!folder && create) {
    bookmarks::BookmarkNode::MetaInfoMap meta;
    meta[kSpaceMetaKey] = space;
    folder = model_->AddFolder(
        model_->other_node(), model_->other_node()->children().size(),
        title.empty() ? std::u16string(u"home") : title, &meta);
  }
  return folder;
}

const bookmarks::BookmarkNode* ArcomiumSidebarView::GetNode(int64_t id) const {
  if (!model_ || !id) {
    return nullptr;
  }
  return bookmarks::GetBookmarkNodeByID(model_, id);
}

bool ArcomiumSidebarView::IsInActiveSpace(const tabs::TabInterface* tab) const {
  const std::optional<tab_groups::TabGroupId> focused = GetActiveGroup();
  if (!focused && preview_space_ && preview_space_->space != kDefaultSpace) {
    // A closed or synced Space has no tabs here.
    return false;
  }
  return focused ? tab->GetGroup() == focused : !tab->GetGroup().has_value();
}

tabs::TabInterface* ArcomiumSidebarView::GetBoundTab(int64_t id) const {
  auto it = bound_.find(id);
  return it == bound_.end() ? nullptr : it->second.Get();
}

bool ArcomiumSidebarView::IsBound(const tabs::TabInterface* tab) const {
  for (const auto& [id, handle] : bound_) {
    if (handle.Get() == tab) {
      return true;
    }
  }
  return false;
}

void ArcomiumSidebarView::BindTabsByUrl(const bookmarks::BookmarkNode* folder) {
  TabStripModel* tabs = browser_->GetTabStripModel();
  for (const auto& child : folder->children()) {
    const bookmarks::BookmarkNode* node = child.get();
    if (node->is_folder()) {
      BindTabsByUrl(node);
      continue;
    }
    if (GetBoundTab(node->id())) {
      continue;
    }
    for (int i = 0; i < tabs->count(); ++i) {
      tabs::TabInterface* tab = tabs->GetTabAtIndex(i);
      if (IsInActiveSpace(tab) && !IsBound(tab) &&
          tab->GetContents()->GetLastCommittedURL() == node->url()) {
        bound_[node->id()] = tab->GetHandle();
        break;
      }
    }
  }
}

bool ArcomiumSidebarView::HasBoundDescendant(
    const bookmarks::BookmarkNode* folder) const {
  for (const auto& child : folder->children()) {
    if (child->is_folder() ? HasBoundDescendant(child.get())
                           : GetBoundTab(child->id()) != nullptr) {
      return true;
    }
  }
  return false;
}

ui::ImageModel ArcomiumSidebarView::GetBookmarkIcon(
    const bookmarks::BookmarkNode* node) {
  const gfx::Image& favicon = model_->GetFavicon(node);
  if (!favicon.IsEmpty()) {
    return ui::ImageModel::FromImage(favicon);
  }
  if (auto it = imported_icons_.find(node->id()); it != imported_icons_.end()) {
    return it->second;
  }
  std::string encoded;
  if (node->GetMetaInfo(kIconMetaKey, &encoded)) {
    if (std::optional<std::vector<uint8_t>> png = base::Base64Decode(encoded)) {
      SkBitmap bitmap = gfx::PNGCodec::Decode(*png);
      if (!bitmap.isNull()) {
        // Arc caches favicons at 32px; they are drawn at 16 DIP.
        const float scale = bitmap.width() / static_cast<float>(kIconSize);
        ui::ImageModel icon = ui::ImageModel::FromImageSkia(
            gfx::ImageSkia::CreateFromBitmap(bitmap, scale));
        imported_icons_[node->id()] = icon;
        return icon;
      }
    }
  }
  return ui::ImageModel::FromImage(favicon::GetDefaultFavicon());
}

bool ArcomiumSidebarView::HasOwnIcon(const bookmarks::BookmarkNode* node) {
  std::string unused;
  return !model_->GetFavicon(node).IsEmpty() ||
         node->GetMetaInfo(kIconMetaKey, &unused);
}

ui::ImageModel ArcomiumSidebarView::GetTabIcon(tabs::TabInterface* tab) const {
  ui::ImageModel icon = TabUIHelper::From(tab)->GetFavicon();
  return icon.IsEmpty()
             ? ui::ImageModel::FromImage(favicon::GetDefaultFavicon())
             : icon;
}

void ArcomiumSidebarView::ScheduleRebuild() {
  if (drag_source_) {
    // A rebuild would destroy the row being dragged.
    if (GetWidget() && GetWidget()->dragged_view()) {
      rebuild_after_drag_ = true;
      return;
    }
    // The drag ended without reaching the row's OnDragDone().
    drag_source_.reset();
    rebuild_after_drag_ = false;
  }
  weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcomiumSidebarView::Rebuild,
                                weak_factory_.GetWeakPtr()));
}

void ArcomiumSidebarView::PrepareSpaceSlide(bool forward, int distance) {
  pending_slide_ = forward ? 1 : -1;
  pending_slide_distance_ = distance;
  pending_slide_from_ = GetActiveSpace();
  pending_slide_until_ = base::TimeTicks::Now() + base::Seconds(1);
}

void ArcomiumSidebarView::CancelSpaceSlide() {
  pending_slide_ = 0;
}

std::unique_ptr<views::View> ArcomiumSidebarView::BuildSpacePreview(
    const ArcomiumSpaceRef& space) {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  container->SetCanProcessEventsWithinSubtree(false);
  // BuildRows() as for the active Space, into the container, leaving the
  // active Space's state as it was.
  views::View* rows = rows_;
  rows_ = container.get();
  auto targets = std::move(row_targets_);
  row_targets_.clear();
  auto bound = bound_;
  const double pitch_offset = row_pitch_offset_;
  row_pitch_offset_ = 0;
  preview_space_ = space;
  BuildRows();
  preview_space_.reset();
  row_pitch_offset_ = pitch_offset;
  bound_ = std::move(bound);
  row_targets_ = std::move(targets);
  rows_ = rows;
  return container;
}

void ArcomiumSidebarView::HideSpacePreview() {
  drop_preview_after_rebuild_ = false;
  space_preview_side_ = 0;
  if (space_preview_) {
    views::View* preview = space_preview_;
    space_preview_ = nullptr;
    RemoveChildViewT(preview);
  }
  if (scroll_ && scroll_->layer()) {
    scroll_->layer()->GetAnimator()->StopAnimating();
    scroll_->layer()->SetTransform(gfx::Transform());
    scroll_->layer()->SetOpacity(1.0f);
  }
}

void ArcomiumSidebarView::TrackSpaceSwipe(
    float dx,
    bool forward,
    const std::optional<ArcomiumSpaceRef>& neighbor) {
  if (!scroll_ || !scroll_->layer()) {
    return;
  }
  ui::Layer* layer = scroll_->layer();
  layer->GetAnimator()->StopAnimating();
  if (space_preview_ && space_preview_->layer()) {
    space_preview_->layer()->GetAnimator()->StopAnimating();
  }
  pending_slide_ = 0;
  drop_preview_after_rebuild_ = false;
  const int width = std::max(1, scroll_->width());
  // The side the neighbor stands on: right of these rows going forward.
  const int side = neighbor ? (forward ? 1 : -1) : 0;
  if (side != space_preview_side_) {
    if (space_preview_) {
      views::View* preview = space_preview_;
      space_preview_ = nullptr;
      RemoveChildViewT(preview);
    }
    space_preview_side_ = side;
    if (side) {
      auto frame = std::make_unique<views::View>();
      frame->SetProperty(views::kViewIgnoredByLayoutKey, true);
      frame->SetCanProcessEventsWithinSubtree(false);
      frame->SetPaintToLayer();
      frame->layer()->SetFillsBoundsOpaquely(false);
      frame->layer()->SetMasksToBounds(true);
      std::unique_ptr<views::View> rows = BuildSpacePreview(*neighbor);
      rows->SetBounds(0, 0, width, rows->GetHeightForWidth(width));
      frame->AddChildView(std::move(rows));
      frame->SetBoundsRect(scroll_->bounds());
      space_preview_ = AddChildView(std::move(frame));
    }
  }
  float offset = side ? std::clamp(dx, -1.0f * width, 1.0f * width)
                      : std::clamp(dx * 0.2f, -24.0f, 24.0f);
  gfx::Transform transform;
  transform.Translate(offset, 0);
  layer->SetTransform(transform);
  layer->SetOpacity(1.0f);
  if (space_preview_) {
    gfx::Transform beside;
    beside.Translate(offset + side * width, 0);
    space_preview_->layer()->SetTransform(beside);
  }
}

void ArcomiumSidebarView::EndSpaceSwipe(bool commit,
                                        bool forward,
                                        base::OnceClosure switch_space) {
  if (!scroll_ || !scroll_->layer()) {
    return;
  }
  commit = commit && space_preview_;
  const int width = std::max(1, scroll_->width());
  const int side = space_preview_side_;
  // Quick, and quicker the less of the way is left.
  const float done =
      std::abs(scroll_->layer()->transform().To2dTranslation().x()) / width;
  const float left = commit ? 1.0f - done : done;
  const base::TimeDelta duration =
      base::Milliseconds(std::clamp(260.0f * left, 70.0f, 220.0f));
  gfx::Transform rows_to;
  gfx::Transform preview_to;
  if (commit) {
    rows_to.Translate(-side * width, 0);
  } else {
    preview_to.Translate(side * width, 0);
  }
  base::OnceClosure finish;
  if (commit) {
    finish = base::BindOnce(
        [](base::WeakPtr<ArcomiumSidebarView> sidebar,
           base::OnceClosure switch_space) {
          if (!sidebar) {
            return;
          }
          // The preview stays in place until the switched Space's rows
          // replace it.
          sidebar->drop_preview_after_rebuild_ = true;
          std::move(switch_space).Run();
          base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE,
              base::BindOnce(&ArcomiumSidebarView::RestoreSlide, sidebar),
              base::Milliseconds(500));
        },
        slide_weak_factory_.GetWeakPtr(), std::move(switch_space));
  } else {
    finish = base::BindOnce(&ArcomiumSidebarView::HideSpacePreview,
                            slide_weak_factory_.GetWeakPtr());
  }
  auto [on_ended, on_aborted] = base::SplitOnceCallback(std::move(finish));
  views::AnimationBuilder builder;
  builder.OnEnded(std::move(on_ended))
      .OnAborted(std::move(on_aborted))
      .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                 IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  auto& step = builder.Once().SetDuration(duration).SetTransform(
      scroll_.get(), rows_to, gfx::Tween::EASE_OUT);
  if (space_preview_) {
    step.SetTransform(space_preview_.get(), preview_to, gfx::Tween::EASE_OUT);
  }
}

void ArcomiumSidebarView::RestoreSlide() {
  if (drop_preview_after_rebuild_) {
    HideSpacePreview();
  }
}

void ArcomiumSidebarView::RunPendingSlide() {
  if (drop_preview_after_rebuild_ && !preview_space_) {
    // The swipe's Space is in: its rows where its preview was.
    scroll_->ScrollToPosition(scroll_->vertical_scroll_bar(), 0);
    HideSpacePreview();
    return;
  }
  if (!pending_slide_ || !scroll_ || !scroll_->layer()) {
    return;
  }
  if (base::TimeTicks::Now() > pending_slide_until_) {
    pending_slide_ = 0;
    return;
  }
  if (GetActiveSpace() == pending_slide_from_) {
    return;
  }
  // Quick, as Arc's: in from the side the fingers moved away from, fading
  // in.
  constexpr base::TimeDelta kSlideDuration = base::Milliseconds(170);
  gfx::Transform start;
  start.Translate(pending_slide_ * pending_slide_distance_, 0);
  pending_slide_ = 0;
  scroll_->layer()->SetTransform(start);
  scroll_->layer()->SetOpacity(0.0f);
  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                 IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kSlideDuration)
      .SetTransform(scroll_.get(), gfx::Transform(), gfx::Tween::EASE_OUT)
      .SetOpacity(scroll_.get(), 1.0f, gfx::Tween::EASE_OUT);
}

void ArcomiumSidebarView::Rebuild() {
  absl::Cleanup slide = [this] { RunPendingSlide(); };
  hover_timer_.Stop();
  FinishFolderAnimation();
  folder_anim_.reset();
  rows_->RemoveAllChildViews();
  row_targets_.clear();
  row_pitch_offset_ = 0;
  std::erase_if(bound_, [this](const auto& entry) {
    return !entry.second.Get() || !GetNode(entry.first);
  });

  BuildRows();
  TabStripModel* tabs = browser_->GetTabStripModel();
  // It goes once its Space has a tab again and it is not shown.
  for (int i = 0; i < tabs->count(); ++i) {
    tabs::TabInterface* hidden = tabs->GetTabAtIndex(i);
    if (hidden == tabs->GetActiveTab() ||
        !TabStripModel::IsArcomiumEmptySpace(hidden->GetContents())) {
      continue;
    }
    for (int j = 0; j < tabs->count(); ++j) {
      tabs::TabInterface* other = tabs->GetTabAtIndex(j);
      if (other != hidden && other->GetGroup() == hidden->GetGroup() &&
          !TabStripModel::IsArcomiumEmptySpace(other->GetContents())) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(&ArcomiumSidebarView::OnTabClosePressed,
                           drop_weak_factory_.GetWeakPtr(),
                           hidden->GetHandle()));
        break;
      }
    }
  }

  expanding_folder_ = 0;
  if (folder_anim_ && folder_anim_->to > folder_anim_->from) {
    ApplyFolderAnimation(0);
    folder_animation_.Reset(0);
    folder_animation_.Show();
  } else {
    folder_anim_.reset();
  }

  // Keeps the folder popup on its folder's new row.
  if (folder_popup_delegate_) {
    views::View* anchor = nullptr;
    for (const auto& [row, target] : row_targets_) {
      const bookmarks::BookmarkNode* node = GetNode(target.node);
      if (target.node == folder_popup_id_ && node && node->is_folder() &&
          !IsOpen(node)) {
        anchor = row;
      }
    }
    if (anchor) {
      folder_popup_delegate_->SetAnchorView(anchor);
    } else {
      CloseFolderPopup();
    }
  }

  PreferredSizeChanged();
}

void ArcomiumSidebarView::BuildRows() {
  const bookmarks::BookmarkNode* folder = GetSpaceFolder(false);

  // Space name.
  std::u16string title = GetActiveSpaceTitle();
  if (title.empty()) {
    title = folder ? folder->GetTitle() : u"home";
  }
  auto* space_label = rows_->AddChildView(MakeLabel(
      title, ArcFont("Segoe UI Variable Small", kLabelFontSize, kSemilight),
      kSubtleColor));
  space_label->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      2, kSpaceLabelX, kSpaceLabelHeight - kSpaceLabelTextHeight - 2, 0)));
  space_label->SetPreferredSize(gfx::Size(0, kSpaceLabelHeight));

  if (folder) {
    BindTabsByUrl(folder);
    AddPinnedRows(folder, 0);
  }

  rows_->AddChildView(std::make_unique<Divider>());

  // Arc's New Tab opens its command bar; what is picked there opens in a
  // new tab.
  AddRow(std::make_unique<Row>(
      base::BindRepeating(
          [](ArcomiumAddressBar* address_bar) {
            address_bar->OpenCommandBar(/*new_tab=*/true);
          },
          base::Unretained(address_bar_.get())),
      std::make_unique<PlusGlyph>(),
      l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_TAB), 0, ItemFont(false),
      kNewTabColor, /*slash=*/false, /*active=*/false));

  TabStripModel* tabs = browser_->GetTabStripModel();
  for (int i = 0; i < tabs->count(); ++i) {
    tabs::TabInterface* tab = tabs->GetTabAtIndex(i);
    // An empty Space's hidden tab has no row.
    if (IsInActiveSpace(tab) && !IsBound(tab) &&
        !TabStripModel::IsArcomiumEmptySpace(tab->GetContents())) {
      AddTabRow(tab);
    }
  }
}

ArcomiumSidebarView::Row* ArcomiumSidebarView::AddRow(
    std::unique_ptr<Row> row,
    int64_t node,
    tabs::TabInterface::Handle tab) {
  // Spread the fractional pitch so rows never drift from Arc's grid.
  const int top = static_cast<int>(std::lround(row_pitch_offset_));
  row_pitch_offset_ += kRowPitch;
  row->set_height(static_cast<int>(std::lround(row_pitch_offset_)) - top);
  Row* added = rows_->AddChildView(std::move(row));
  added->set_context_menu_controller(this);
  row_targets_[added] = RowTarget{node, tab};
  if (node || tab.Get()) {
    added->set_drag_controller(this);
    added->SetDragDoneCallback(base::BindRepeating(
        &ArcomiumSidebarView::OnRowDragDone, base::Unretained(this)));
  }
  return added;
}

void ArcomiumSidebarView::AddPinnedRows(const bookmarks::BookmarkNode* folder,
                                        int depth) {
  for (const auto& child : folder->children()) {
    const bookmarks::BookmarkNode* node = child.get();
    if (!node->is_folder()) {
      AddBookmarkRow(node, depth);
      continue;
    }
    const bool open = IsOpen(node);
    const int64_t id = node->id();
    Row* row =
        AddRow(std::make_unique<Row>(
                   base::BindRepeating(&ArcomiumSidebarView::OnFolderPressed,
                                       base::Unretained(this), id),
                   std::make_unique<FolderGlyph>(
                       !open && HasBoundDescendant(node), FolderIcon(node)),
                   node->GetTitle(), depth, ItemFont(true), kFolderColor,
                   /*slash=*/false, /*active=*/false),
               id);
    row->SetHoverCallback(
        base::BindRepeating(&ArcomiumSidebarView::OnFolderHovered,
                            base::Unretained(this), id, row));
    if (open) {
      const size_t first = rows_->children().size();
      AddPinnedRows(node, depth + 1);
      if (id == expanding_folder_) {
        FolderAnimation anim;
        anim.first = first;
        anim.count = rows_->children().size() - first;
        anim.from = expand_from_;
        for (size_t i = first; i < rows_->children().size(); ++i) {
          anim.to += rows_->children()[i]->GetPreferredSize().height();
        }
        folder_anim_ = anim;
      }
    } else {
      AddBoundTabRows(node, depth + 1);
    }
  }
}

void ArcomiumSidebarView::AddBoundTabRows(const bookmarks::BookmarkNode* folder,
                                          int depth) {
  for (const auto& child : folder->children()) {
    if (child->is_folder()) {
      AddBoundTabRows(child.get(), depth);
    } else if (GetBoundTab(child->id())) {
      AddBookmarkRow(child.get(), depth);
    }
  }
}

void ArcomiumSidebarView::AddBookmarkRow(const bookmarks::BookmarkNode* node,
                                         int depth) {
  tabs::TabInterface* tab = GetBoundTab(node->id());
  // As in Arc, a pinned item keeps its own icon; the tab's favicon is only a
  // fallback for items without one.
  ui::ImageModel icon = GetBookmarkIcon(node);
  bool slash = false;
  if (tab) {
    if (!HasOwnIcon(node)) {
      icon = GetTabIcon(tab);
    }
    const GURL& url = tab->GetContents()->GetLastCommittedURL();
    slash = url.is_valid() && url != node->url();
  }
  Row* row =
      AddRow(std::make_unique<Row>(
                 base::BindRepeating(&ArcomiumSidebarView::OnBookmarkPressed,
                                     base::Unretained(this), node->id()),
                 MakeImageIcon(icon), node->GetTitle(), depth, ItemFont(false),
                 kItemColor, slash, tab && tab->IsActivated()),
             node->id(), tab ? tab->GetHandle() : tabs::TabInterface::Handle());
  if (tab) {
    row->SetCloseCallback(
        base::BindRepeating(&ArcomiumSidebarView::OnTabClosePressed,
                            base::Unretained(this), tab->GetHandle()));
  }
}

void ArcomiumSidebarView::AddTabRow(tabs::TabInterface* tab) {
  const tabs::TabInterface::Handle handle = tab->GetHandle();
  Row* row = AddRow(
      std::make_unique<Row>(
          base::BindRepeating(
              [](ArcomiumSidebarView* view, tabs::TabInterface::Handle handle,
                 const ui::Event& event) {
                if (event.flags() & ui::EF_MIDDLE_MOUSE_BUTTON) {
                  view->OnTabClosePressed(handle);
                } else {
                  view->OnTabPressed(handle);
                }
              },
              base::Unretained(this), handle),
          MakeImageIcon(GetTabIcon(tab)), TabUIHelper::From(tab)->GetTitle(), 0,
          ItemFont(false), kItemColor, /*slash=*/false, tab->IsActivated()),
      0, handle);
  row->SetCloseCallback(base::BindRepeating(
      &ArcomiumSidebarView::OnTabClosePressed, base::Unretained(this), handle));
}

void ArcomiumSidebarView::OnBookmarkPressed(int64_t id) {
  if (const bookmarks::BookmarkNode* node = GetNode(id)) {
    OpenBookmark(node);
  }
}

void ArcomiumSidebarView::OnFolderPressed(int64_t id) {
  hover_timer_.Stop();
  CloseFolderPopup();
  FinishFolderAnimation();
  const bookmarks::BookmarkNode* node = GetNode(id);
  if (!node) {
    return;
  }
  if (IsOpen(node)) {
    StartFolderCollapse(node);
    return;
  }
  // The rows grow in from the height its open tabs took while closed.
  expanding_folder_ = id;
  expand_from_ = 0;
  for (const auto& [row, target] : row_targets_) {
    const bookmarks::BookmarkNode* item = GetNode(target.node);
    if (item && item != node && item->HasAncestor(node)) {
      expand_from_ += row->GetPreferredSize().height();
    }
  }
  model_->SetNodeMetaInfo(node, kOpenMetaKey, "1");
}

void ArcomiumSidebarView::StartFolderCollapse(
    const bookmarks::BookmarkNode* folder) {
  // The rows under the folder's row, down to the first that is not in it;
  // once closed, only its open tabs remain.
  FolderAnimation anim;
  const auto& children = rows_->children();
  bool found = false;
  for (size_t i = 0; i < children.size(); ++i) {
    auto it = row_targets_.find(children[i]);
    const bookmarks::BookmarkNode* item =
        it == row_targets_.end() ? nullptr : GetNode(it->second.node);
    if (!found) {
      if (item == folder) {
        found = true;
        anim.first = i + 1;
      }
      continue;
    }
    if (!item || !item->HasAncestor(folder)) {
      break;
    }
    const int height = children[i]->GetPreferredSize().height();
    ++anim.count;
    anim.from += height;
    if (!item->is_folder() && GetBoundTab(item->id())) {
      anim.to += height;
    }
  }
  if (anim.from <= anim.to) {
    model_->SetNodeMetaInfo(folder, kOpenMetaKey, "0");
    return;
  }
  folder_anim_ = anim;
  collapsing_folder_ = folder->id();
  folder_animation_.Reset(0);
  folder_animation_.Show();
}

void ArcomiumSidebarView::ApplyFolderAnimation(double progress) {
  const FolderAnimation& anim = *folder_anim_;
  const auto& children = rows_->children();
  if (anim.first + anim.count > children.size()) {
    return;
  }
  const double eased =
      gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN, progress);
  const int shown = static_cast<int>(
      std::round(anim.from + (anim.to - anim.from) * eased));
  // The folder's rows, revealed top to bottom like a curtain.
  int offset = 0;
  int full = 0;
  for (size_t i = anim.first; i < anim.first + anim.count; ++i) {
    views::View* child = children[i];
    const int height = child->GetPreferredSize().height();
    full += height;
    if (!child->layer()) {
      child->SetPaintToLayer();
      child->layer()->SetFillsBoundsOpaquely(false);
    }
    const int visible = std::clamp(shown - offset, 0, height);
    offset += height;
    child->layer()->SetOpacity(visible > 0 ? 1 : 0);
    // An empty clip rect means no clipping.
    child->layer()->SetClipRect(visible < height
                                    ? gfx::Rect(0, 0, width(), visible)
                                    : gfx::Rect());
  }
  // Everything below moves with the curtain's edge.
  const gfx::Transform shift = gfx::Transform::MakeTranslation(0, shown - full);
  for (size_t i = anim.first + anim.count; i < children.size(); ++i) {
    views::View* child = children[i];
    if (!child->layer()) {
      child->SetPaintToLayer();
      child->layer()->SetFillsBoundsOpaquely(false);
    }
    child->layer()->SetTransform(shift);
  }
}

void ArcomiumSidebarView::ResetFolderAnimation() {
  if (!folder_anim_) {
    return;
  }
  const auto& children = rows_->children();
  for (size_t i = folder_anim_->first; i < children.size(); ++i) {
    if (ui::Layer* layer = children[i]->layer()) {
      layer->SetOpacity(1);
      layer->SetClipRect(gfx::Rect());
      layer->SetTransform(gfx::Transform());
    }
  }
  folder_anim_.reset();
}

void ArcomiumSidebarView::FinishFolderAnimation() {
  folder_animation_.End();
  if (const int64_t id = std::exchange(collapsing_folder_, 0)) {
    if (const bookmarks::BookmarkNode* folder = GetNode(id)) {
      model_->SetNodeMetaInfo(folder, kOpenMetaKey, "0");
    }
  }
}

void ArcomiumSidebarView::AnimationProgressed(const gfx::Animation* animation) {
  if (folder_anim_) {
    ApplyFolderAnimation(folder_animation_.GetCurrentValue());
  }
}

void ArcomiumSidebarView::AnimationEnded(const gfx::Animation* animation) {
  if (collapsing_folder_) {
    // Closing the folder rebuilds the rows where the animation left them.
    FinishFolderAnimation();
  } else {
    ResetFolderAnimation();
  }
}

void ArcomiumSidebarView::OnTabPressed(tabs::TabInterface::Handle handle) {
  TabStripModel* tabs = browser_->GetTabStripModel();
  if (tabs::TabInterface* tab = handle.Get()) {
    const int index = tabs->GetIndexOfTab(tab);
    if (index != TabStripModel::kNoTab) {
      tabs->ActivateTabAt(index);
    }
  }
}

void ArcomiumSidebarView::OnTabClosePressed(tabs::TabInterface::Handle handle) {
  TabStripModel* tabs = browser_->GetTabStripModel();
  if (tabs::TabInterface* tab = handle.Get()) {
    const int index = tabs->GetIndexOfTab(tab);
    if (index != TabStripModel::kNoTab) {
      tabs->CloseWebContentsAt(index, TabCloseTypes::CLOSE_USER_GESTURE);
    }
  }
}

void ArcomiumSidebarView::OpenBookmark(const bookmarks::BookmarkNode* node) {
  if (tabs::TabInterface* tab = GetBoundTab(node->id())) {
    OnTabPressed(tab->GetHandle());
    return;
  }
  NavigateParams params(browser_, node->url(),
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.group = browser_->GetTabStripModel()->GetFocusedGroup();
  Navigate(&params);
  if (params.navigated_or_inserted_contents) {
    if (tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(
            params.navigated_or_inserted_contents)) {
      bound_[node->id()] = tab->GetHandle();
    }
  }
  ScheduleRebuild();
}

void ArcomiumSidebarView::PinTab(tabs::TabInterface* tab,
                                 const bookmarks::BookmarkNode* folder) {
  if (!folder) {
    folder = GetSpaceFolder(true);
  }
  if (!tab || !folder) {
    return;
  }
  content::WebContents* contents = tab->GetContents();
  if (!contents->GetLastCommittedURL().is_valid()) {
    return;
  }
  const bookmarks::BookmarkNode* node =
      model_->AddNewURL(folder, folder->children().size(), contents->GetTitle(),
                        contents->GetLastCommittedURL());
  if (node) {
    bound_[node->id()] = tab->GetHandle();
  }
}

void ArcomiumSidebarView::AddFolder(const bookmarks::BookmarkNode* parent) {
  if (!parent) {
    parent = GetSpaceFolder(true);
  }
  if (!parent || !GetWidget()) {
    return;
  }
  model_->SetNodeMetaInfo(parent, kOpenMetaKey, "1");
  BookmarkEditor::Show(
      GetWidget()->GetNativeWindow(), browser_->GetProfile(),
      BookmarkEditor::EditDetails::AddFolder(parent, parent->children().size()),
      BookmarkEditor::NO_TREE);
}

void ArcomiumSidebarView::OnFolderHovered(int64_t id,
                                          views::View* row,
                                          bool hovered) {
  if (!hovered) {
    hover_timer_.Stop();
    return;
  }
  if (folder_popup_) {
    // Switches at once while a popup is open.
    if (folder_popup_id_ != id) {
      ShowFolderPopup(id, row);
    }
    return;
  }
  hover_timer_.Start(FROM_HERE, kFolderPopupDelay,
                     base::BindOnce(&ArcomiumSidebarView::ShowFolderPopup,
                                    weak_factory_.GetWeakPtr(), id, row));
}

void ArcomiumSidebarView::ShowFolderPopup(int64_t id, views::View* row) {
  CloseFolderPopup();
  const bookmarks::BookmarkNode* node = GetNode(id);
  if (!node || IsOpen(node) || node->children().empty() || !row->GetWidget() ||
      !row->IsMouseHovered() || drag_source_ ||
      (menu_runner_ && menu_runner_->IsRunning())) {
    return;
  }
  folder_popup_open_.clear();
  auto contents = std::make_unique<views::View>();
  contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  FillFolderPopup(contents.get(), node);

  // A bubble right of the row, shown without taking focus. It must be able to
  // activate: Windows drops clicks on bubbles that cannot.
  auto delegate = std::make_unique<views::BubbleDialogDelegate>(
      row, views::BubbleBorder::LEFT_TOP);
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetShowCloseButton(false);
  delegate->set_close_on_deactivate(false);
  delegate->set_margins(gfx::Insets::VH(kPopupPadding, 0));
  delegate->SetBackgroundColor(arcomium::SidebarInk(this, kPopupColor));
  delegate->SetContentsView(std::move(contents));
  folder_popup_ = views::BubbleDialogDelegate::CreateBubble(
      delegate.get(), base::BindOnce(&ArcomiumSidebarView::OnFolderPopupClosed,
                                     drop_weak_factory_.GetWeakPtr()));
  folder_popup_delegate_ = std::move(delegate);
  ui::Layer* layer = folder_popup_->GetLayer();
  if (layer) {
    layer->SetOpacity(0);
  }
  folder_popup_->ShowInactive();
  if (layer) {
    ui::ScopedLayerAnimationSettings fade(layer->GetAnimator());
    fade.SetTransitionDuration(kPopupFadeDuration);
    fade.SetTweenType(gfx::Tween::EASE_OUT);
    layer->SetOpacity(1);
  }
  folder_popup_id_ = id;
  folder_popup_outside_ticks_ = 0;
  folder_popup_timer_.Start(FROM_HERE, kPopupPollInterval, this,
                            &ArcomiumSidebarView::CheckFolderPopupHover);
}

void ArcomiumSidebarView::FillFolderPopup(
    views::View* contents,
    const bookmarks::BookmarkNode* folder) {
  contents->RemoveAllChildViews();
  AddFolderPopupRows(contents, folder, 0);
  int height = 0;
  for (const views::View* child : contents->children()) {
    height += child->GetPreferredSize().height();
  }
  contents->SetPreferredSize(gfx::Size(kPopupWidth, height));
}

void ArcomiumSidebarView::ToggleFolderPopupFolder(int64_t id) {
  if (!folder_popup_open_.erase(id)) {
    folder_popup_open_.insert(id);
  }
  // Later: the pressed row is among those rebuilt.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcomiumSidebarView::RefreshFolderPopup,
                                drop_weak_factory_.GetWeakPtr()));
}

void ArcomiumSidebarView::RefreshFolderPopup() {
  const bookmarks::BookmarkNode* folder = GetNode(folder_popup_id_);
  if (!folder_popup_ || !folder) {
    return;
  }
  FillFolderPopup(folder_popup_delegate_->GetContentsView(), folder);
  folder_popup_delegate_->SizeToContents();
}

void ArcomiumSidebarView::AddFolderPopupRows(
    views::View* container,
    const bookmarks::BookmarkNode* folder,
    int depth) {
  for (const auto& child : folder->children()) {
    const bookmarks::BookmarkNode* node = child.get();
    if (node->is_folder()) {
      container->AddChildView(std::make_unique<Row>(
          base::BindRepeating(&ArcomiumSidebarView::ToggleFolderPopupFolder,
                              base::Unretained(this), node->id()),
          std::make_unique<FolderGlyph>(false, FolderIcon(node)),
          node->GetTitle(), depth, ItemFont(true), kFolderColor,
          /*slash=*/false, /*active=*/false));
      if (folder_popup_open_.contains(node->id())) {
        AddFolderPopupRows(container, node, depth + 1);
      }
      continue;
    }
    container->AddChildView(std::make_unique<Row>(
        base::BindRepeating(
            [](ArcomiumSidebarView* sidebar, int64_t id) {
              sidebar->OnBookmarkPressed(id);
              sidebar->CloseFolderPopup();
            },
            base::Unretained(this), node->id()),
        MakeImageIcon(GetBookmarkIcon(node)), node->GetTitle(), depth,
        ItemFont(false), kItemColor, /*slash=*/false, /*active=*/false));
  }
}

void ArcomiumSidebarView::CheckFolderPopupHover() {
  if (!folder_popup_) {
    CloseFolderPopup();
    return;
  }
  const gfx::Point point = display::Screen::Get()->GetCursorScreenPoint();
  const gfx::Rect bubble = folder_popup_->GetWindowBoundsInScreen();
  bool inside = bubble.Contains(point);
  if (views::View* anchor = folder_popup_delegate_->GetAnchorView()) {
    // The row, and the gap between it and the popup.
    const gfx::Rect row = anchor->GetBoundsInScreen();
    const gfx::Rect bridge(row.x(), row.y(),
                           std::max(row.width(), bubble.x() - row.x()),
                           row.height());
    inside = inside || bridge.Contains(point);
  }
  for (views::View* child :
       folder_popup_delegate_->GetContentsView()->children()) {
    static_cast<Row*>(child)->SetPolledHover(
        child->GetBoundsInScreen().Contains(point));
  }
  folder_popup_outside_ticks_ = inside ? 0 : folder_popup_outside_ticks_ + 1;
  if (folder_popup_outside_ticks_ >= kPopupOutsideTicks) {
    CloseFolderPopup();
  }
}

namespace {

// Deletes a bubble's widget, then its delegate, in one task: a delegate left
// without its widget still hears its anchor widget's theme changes (a Space
// switch), and crashes on them.
void DeleteBubbleSoon(std::unique_ptr<views::Widget> widget,
                      std::unique_ptr<views::BubbleDialogDelegate> delegate) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<views::Widget> widget,
             std::unique_ptr<views::BubbleDialogDelegate> delegate) {
            widget.reset();
            delegate.reset();
          },
          std::move(widget), std::move(delegate)));
}

}  // namespace

void ArcomiumSidebarView::CloseFolderPopup() {
  folder_popup_timer_.Stop();
  folder_popup_id_ = 0;
  if (!folder_popup_) {
    return;
  }
  // Deleted later: this may run from inside the popup's own events.
  folder_popup_->Hide();
  DeleteBubbleSoon(std::move(folder_popup_),
                   std::move(folder_popup_delegate_));
}

void ArcomiumSidebarView::OnFolderPopupClosed(
    views::Widget::ClosedReason reason) {
  CloseFolderPopup();
}

void ArcomiumSidebarView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  hover_timer_.Stop();
  menu_node_ = 0;
  menu_point_ = point;
  menu_tab_ = tabs::TabInterface::Handle();
  CloseFolderPopup();
  // The previous menu's runner refers to the models replaced below.
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  const auto it = row_targets_.find(source);
  if (it != row_targets_.end()) {
    menu_node_ = it->second.node;
    menu_tab_ = it->second.tab;
  }
  const bookmarks::BookmarkNode* node = GetNode(menu_node_);
  tabs::TabInterface* tab = menu_tab_.Get();

  // "Move to": the other Spaces.
  menu_spaces_ = GetOtherSpaces();
  move_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  for (size_t i = 0; i < menu_spaces_.size(); ++i) {
    move_menu_model_->AddItem(kMoveToFirst + static_cast<int>(i),
                              menu_spaces_[i].title);
  }
  auto add_move_to = [this] {
    if (!menu_spaces_.empty()) {
      menu_model_->AddSubMenu(kMoveTo, l10n_util::GetStringUTF16(IDS_ARCOMIUM_MOVE_TO),
                               move_menu_model_.get());
    }
  };

  // A tab's own items: its sound when it plays or is muted, and a split
  // with the current tab.
  auto add_tab_items = [this](tabs::TabInterface* tab) {
    content::WebContents* contents = tab ? tab->GetContents() : nullptr;
    if (!contents) {
      return;
    }
    if (contents->IsCurrentlyAudible() || contents->IsAudioMuted()) {
      menu_model_->AddItem(
          kToggleMute, l10n_util::GetStringUTF16(contents->IsAudioMuted()
                                                     ? IDS_ARCOMIUM_UNMUTE_TAB
                                                     : IDS_ARCOMIUM_MUTE_TAB));
    }
    if (!tab->IsSplit()) {
      menu_model_->AddItem(
          kSplitView, l10n_util::GetStringUTF16(IDS_ARCOMIUM_OPEN_IN_SPLIT_VIEW));
    }
  };

  if (node && node->is_folder()) {
    menu_model_->AddItem(kPinTab,
                         l10n_util::GetStringUTF16(IDS_ARCOMIUM_PIN_CURRENT_TAB_HERE));
    menu_model_->AddItem(kNewFolder,
                         l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_FOLDER_INSIDE));
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kRename, l10n_util::GetStringUTF16(IDS_ARCOMIUM_RENAME));
    add_move_to();
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kRemove, l10n_util::GetStringUTF16(IDS_ARCOMIUM_DELETE_FOLDER));
  } else if (node) {
    menu_model_->AddItem(kCopyLink, l10n_util::GetStringUTF16(IDS_ARCOMIUM_COPY_LINK));
    menu_model_->AddItem(kRename, l10n_util::GetStringUTF16(IDS_ARCOMIUM_RENAME));
    add_tab_items(tab);
    // The pinned page: what the tab shows now, or edited by hand.
    pinned_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    if (tab && tab->GetContents() &&
        tab->GetContents()->GetLastCommittedURL().is_valid() &&
        tab->GetContents()->GetLastCommittedURL() != node->url()) {
      pinned_menu_model_->AddItem(
          kReplacePinnedUrl,
          l10n_util::GetStringUTF16(IDS_ARCOMIUM_REPLACE_PINNED_URL));
    }
    pinned_menu_model_->AddItem(
        kEditPinnedItem, l10n_util::GetStringUTF16(IDS_ARCOMIUM_EDIT_PINNED_ITEM));
    menu_model_->AddSubMenu(kEditPinnedPage,
                            l10n_util::GetStringUTF16(IDS_ARCOMIUM_EDIT_PINNED_PAGE),
                            pinned_menu_model_.get());
    menu_model_->AddItem(kDuplicate, l10n_util::GetStringUTF16(IDS_ARCOMIUM_DUPLICATE));
    add_move_to();
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kRemove, l10n_util::GetStringUTF16(IDS_ARCOMIUM_UNPIN));
    if (tab) {
      menu_model_->AddItem(kCloseTab, l10n_util::GetStringUTF16(IDS_ARCOMIUM_CLOSE_TAB));
    }
  } else if (tab) {
    menu_model_->AddItem(kCopyLink, l10n_util::GetStringUTF16(IDS_ARCOMIUM_COPY_LINK));
    add_tab_items(tab);
    menu_model_->AddItem(kDuplicate, l10n_util::GetStringUTF16(IDS_ARCOMIUM_DUPLICATE));
    menu_model_->AddItem(kPinTab, l10n_util::GetStringUTF16(IDS_ARCOMIUM_PIN_TO_SPACE));
    add_move_to();
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kCloseTab, l10n_util::GetStringUTF16(IDS_ARCOMIUM_CLOSE_TAB));
  } else {
    menu_model_->AddItem(kNewFolder, l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_FOLDER));
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kEditColor,
                         l10n_util::GetStringUTF16(IDS_ARCOMIUM_EDIT_THEME_COLOR));
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(source->GetWidget(), nullptr,
                          gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

std::vector<ArcomiumSidebarView::SpaceTarget>
ArcomiumSidebarView::GetOtherSpaces() const {
  std::vector<SpaceTarget> spaces;
  TabStripModel* tabs = browser_->GetTabStripModel();
  const std::optional<tab_groups::TabGroupId> focused = tabs->GetFocusedGroup();
  if (focused) {
    spaces.push_back({std::nullopt, kDefaultSpace, u"home"});
  }
  TabGroupModel* groups = tabs->group_model();
  if (!groups) {
    return spaces;
  }
  // In the Space bar's order.
  std::vector<tab_groups::TabGroupId> ids = groups->ListTabGroups();
  std::ranges::sort(ids, {}, [groups](const tab_groups::TabGroupId& id) {
    return groups->GetTabGroup(id)->ListTabs().start();
  });
  for (const tab_groups::TabGroupId& id : ids) {
    std::optional<std::string> space = arcomium::GetSpaceOfGroup(tabs, id);
    if (id == focused || !space) {
      continue;
    }
    const std::u16string& title = groups->GetTabGroup(id)->visual_data()->title();
    spaces.push_back({id, *space, title.empty() ? u"Space" : title});
  }
  return spaces;
}

void ArcomiumSidebarView::MoveToSpace(const SpaceTarget& target) {
  const bookmarks::BookmarkNode* node = GetNode(menu_node_);
  tabs::TabInterface* tab = menu_tab_.Get();
  if (!node) {
    if (tab) {
      MoveTabToSpace(tab, target);
    }
    return;
  }
  const bookmarks::BookmarkNode* folder =
      GetSpaceFolderFor(target.space, target.title, /*create=*/true);
  if (!folder || folder == node || folder->HasAncestor(node)) {
    return;
  }
  // A pinned item's open tab stays behind in this Space's storage, so it
  // closes; the item opens again in the target Space.
  tabs::TabInterface* bound = node->is_folder() ? nullptr : GetBoundTab(node->id());
  model_->Move(node, folder, folder->children().size());
  if (bound) {
    bound_.erase(node->id());
    OnTabClosePressed(bound->GetHandle());
  }
}

void ArcomiumSidebarView::MoveTabToSpace(tabs::TabInterface* tab,
                                         const SpaceTarget& target) {
  TabStripModel* tabs = browser_->GetTabStripModel();
  content::WebContents* old_contents = tab->GetContents();
  GURL url = old_contents->GetLastCommittedURL();
  if (!url.is_valid()) {
    url = old_contents->GetVisibleURL();
  }
  // Each Space has its own storage, so the page opens anew in the target's.
  Profile* profile = browser_->GetProfile();
  content::WebContents::CreateParams create_params(
      profile, target.group
                   ? arcomium::CreateSpaceSiteInstance(profile, url, target.space)
                   : tab_util::GetSiteInstanceForNewTab(profile, url));
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(create_params);
  contents->GetController().LoadURL(url, content::Referrer(),
                                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                    std::string());
  int index = tabs->count();
  if (target.group && tabs->group_model() &&
      tabs->group_model()->ContainsTabGroup(*target.group)) {
    index = static_cast<int>(
        tabs->group_model()->GetTabGroup(*target.group)->ListTabs().end());
  }
  tabs->AddWebContents(std::move(contents), index,
                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL, AddTabTypes::ADD_NONE,
                       target.group);

  // The Space it leaves stays, empty, as in Arc.
  {
    tabs::TabInterface* const leaving[] = {tab};
    tabs->ArcomiumKeepSpaces(leaving);
  }
  OnTabClosePressed(tab->GetHandle());
}

void ArcomiumSidebarView::DuplicateItem() {
  const bookmarks::BookmarkNode* node = GetNode(menu_node_);
  tabs::TabInterface* tab = menu_tab_.Get();
  TabStripModel* tabs = browser_->GetTabStripModel();
  if (tab) {
    // Keeps the page's history.
    chrome::DuplicateTabAt(browser_, tabs->GetIndexOfTab(tab));
    return;
  }
  if (node && node->is_url()) {
    NavigateParams params(browser_, node->url(),
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.group = tabs->GetFocusedGroup();
    Navigate(&params);
  }
}

void ArcomiumSidebarView::CopyLink() {
  const bookmarks::BookmarkNode* node = GetNode(menu_node_);
  tabs::TabInterface* tab = menu_tab_.Get();
  GURL url;
  if (node && node->is_url()) {
    url = node->url();
  } else if (tab) {
    url = tab->GetContents()->GetLastCommittedURL();
  }
  if (url.is_valid()) {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(base::UTF8ToUTF16(url.spec()));
  }
}

void ArcomiumSidebarView::ExecuteCommand(int command_id, int event_flags) {
  if (command_id >= kMoveToFirst) {
    const size_t index = static_cast<size_t>(command_id - kMoveToFirst);
    if (index < menu_spaces_.size()) {
      MoveToSpace(menu_spaces_[index]);
    }
    return;
  }
  const bookmarks::BookmarkNode* node = GetNode(menu_node_);
  tabs::TabInterface* tab = menu_tab_.Get();
  switch (command_id) {
    case kCopyLink:
      CopyLink();
      break;
    case kDuplicate:
      DuplicateItem();
      break;
    case kPinTab:
      PinTab(tab ? tab : browser_->GetTabStripModel()->GetActiveTab(),
             node && node->is_folder() ? node : nullptr);
      break;
    case kNewFolder:
      AddFolder(node && node->is_folder() ? node : nullptr);
      break;
    case kRename:
      if (node && GetWidget()) {
        BookmarkEditor::Show(GetWidget()->GetNativeWindow(),
                             browser_->GetProfile(),
                             BookmarkEditor::EditDetails::EditNode(node),
                             BookmarkEditor::NO_TREE);
      }
      break;
    case kRemove:
      if (node) {
        model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kOther,
                       FROM_HERE);
      }
      break;
    case kCloseTab:
      if (tab) {
        OnTabClosePressed(tab->GetHandle());
      }
      break;
    case kEditColor:
      ShowColorPicker();
      break;
    case kToggleMute:
      if (tab && tab->GetContents()) {
        SetTabAudioMuted(tab->GetContents(),
                         !tab->GetContents()->IsAudioMuted(),
                         TabMutedReason::kAudioIndicator, std::string());
      }
      break;
    case kSplitView:
      if (tab && !tab->IsSplit()) {
        TabStripModel* model = browser_->GetTabStripModel();
        const int index = model->GetIndexOfTab(tab);
        if (index == TabStripModel::kNoTab) {
          break;
        }
        if (tab == model->GetActiveTab()) {
          chrome::ExecuteCommand(browser_, IDC_NEW_SPLIT_TAB);
        } else {
          model->ExecuteAddToNewSplitCommand(
              index, split_tabs::SplitTabLayout::kSideBySide);
        }
      }
      break;
    case kReplacePinnedUrl:
      if (node && !node->is_folder() && tab && tab->GetContents()) {
        const GURL url = tab->GetContents()->GetLastCommittedURL();
        if (url.is_valid()) {
          model_->SetURL(node, url,
                         bookmarks::metrics::BookmarkEditSource::kUser);
        }
      }
      break;
    case kEditPinnedItem:
      if (node && GetWidget()) {
        BookmarkEditor::Show(GetWidget()->GetNativeWindow(),
                             browser_->GetProfile(),
                             BookmarkEditor::EditDetails::EditNode(node),
                             BookmarkEditor::NO_TREE);
      }
      break;
  }
}

void ArcomiumSidebarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  // Each step of a Space's color transition re-themes the window; the rows
  // recolor themselves, and they are rebuilt once it settles.
  if (rows_ && rows_->children().empty()) {
    ScheduleRebuild();
    return;
  }
  theme_rebuild_timer_.Start(
      FROM_HERE, base::Milliseconds(250),
      base::BindOnce(&ArcomiumSidebarView::ScheduleRebuild,
                     base::Unretained(this)));
}

void ArcomiumSidebarView::EditSpaceColorAt(const gfx::Point& screen_point) {
  menu_point_ = screen_point;
  ShowColorPicker();
}

void ArcomiumSidebarView::ShowColorPicker() {
  CloseColorPicker();
  if (!GetWidget()) {
    return;
  }
  arcomium::SpaceColor initial;
  std::string value;
  if (const bookmarks::BookmarkNode* folder = GetSpaceFolder(false);
      folder && folder->GetMetaInfo(arcomium::kSpaceColorMetaKey, &value)) {
    initial = arcomium::ParseSpaceColor(value).value_or(initial);
  }
  // Arc's picker opens where the menu was, and closes when it loses focus.
  auto delegate = std::make_unique<views::BubbleDialogDelegate>(
      nullptr, views::BubbleBorder::TOP_LEFT);
  delegate->set_parent_window(GetWidget()->GetNativeView());
  delegate->SetAnchorRect(gfx::Rect(menu_point_, gfx::Size()));
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetShowCloseButton(false);
  delegate->set_margins(gfx::Insets());
  delegate->SetBackgroundColor(arcomium::kSpaceColorPickerBackground);
  delegate->SetContentsView(arcomium::CreateSpaceColorPicker(
      initial, base::BindRepeating(&ArcomiumSidebarView::OnSpaceColorPicked,
                                   drop_weak_factory_.GetWeakPtr())));
  color_picker_ = views::BubbleDialogDelegate::CreateBubble(
      delegate.get(), base::BindOnce(&ArcomiumSidebarView::OnColorPickerClosed,
                                     drop_weak_factory_.GetWeakPtr()));
  color_picker_delegate_ = std::move(delegate);
  color_picker_->Show();
}

void ArcomiumSidebarView::CloseColorPicker() {
  if (!color_picker_) {
    return;
  }
  // Deleted later: this may run from inside the picker's own events.
  color_picker_->Hide();
  DeleteBubbleSoon(std::move(color_picker_),
                   std::move(color_picker_delegate_));
}

void ArcomiumSidebarView::OnColorPickerClosed(
    views::Widget::ClosedReason reason) {
  CloseColorPicker();
}

void ArcomiumSidebarView::OnSpaceColorPicked(const arcomium::SpaceColor& color,
                                             bool commit) {
  // Shown at once; stored (and synced) once a drag ends.
  arcomium::ApplySpaceColor(GetWidget(), color);
  if (commit) {
    if (const bookmarks::BookmarkNode* folder = GetSpaceFolder(true)) {
      model_->SetNodeMetaInfo(folder, arcomium::kSpaceColorMetaKey,
                              arcomium::SerializeSpaceColor(color));
    }
  }
}

void ArcomiumSidebarView::WriteDragDataForView(views::View* sender,
                                               const gfx::Point& press_pt,
                                               ui::OSExchangeData* data) {
  auto it = row_targets_.find(sender);
  if (it == row_targets_.end()) {
    return;
  }
  drag_source_ = it->second;
  // The rows only move within the sidebar; the text marks the data as ours.
  data->SetString(kRowDragData);
  button_drag_utils::SetDragImage(GURL(), static_cast<Row*>(sender)->title(),
                                  gfx::ImageSkia(), &press_pt, data);
}

int ArcomiumSidebarView::GetDragOperationsForView(views::View* sender,
                                                  const gfx::Point& p) {
  return row_targets_.contains(sender) ? ui::DragDropTypes::DRAG_MOVE
                                       : ui::DragDropTypes::DRAG_NONE;
}

bool ArcomiumSidebarView::CanStartDragForView(views::View* sender,
                                              const gfx::Point& press_pt,
                                              const gfx::Point& p) {
  return views::View::ExceededDragThreshold(p - press_pt);
}

bool ArcomiumSidebarView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::STRING;
  return true;
}

bool ArcomiumSidebarView::CanDrop(const ui::OSExchangeData& data) {
  return drag_source_ && data.GetString() == kRowDragData;
}

int ArcomiumSidebarView::OnDragUpdated(const ui::DropTargetEvent& event) {
  const std::optional<DropTarget> target = GetDropTarget(event.location());
  ShowDropIndicator(target);
  return target ? ui::DragDropTypes::DRAG_MOVE : ui::DragDropTypes::DRAG_NONE;
}

void ArcomiumSidebarView::OnDragExited() {
  ShowDropIndicator(std::nullopt);
}

views::View::DropCallback ArcomiumSidebarView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  std::optional<DropTarget> target = GetDropTarget(event.location());
  ShowDropIndicator(std::nullopt);
  if (!target) {
    return base::NullCallback();
  }
  return base::BindOnce(&ArcomiumSidebarView::PerformDrop,
                        drop_weak_factory_.GetWeakPtr(), *target);
}

std::optional<ArcomiumSidebarView::DropTarget>
ArcomiumSidebarView::GetDropTarget(const gfx::Point& point) {
  if (!drag_source_ || !model_) {
    return std::nullopt;
  }
  const bookmarks::BookmarkNode* source_node = GetNode(drag_source_->node);
  tabs::TabInterface* source_tab =
      source_node ? nullptr : drag_source_->tab.Get();
  if (!source_node && !source_tab) {
    return std::nullopt;
  }
  gfx::Point p = point;
  views::View::ConvertPointToTarget(this, rows_, &p);

  // A line under `row`, or over it, in this view's coordinates.
  auto line = [this](const views::View* row, bool below) {
    const gfx::Rect rect =
        views::View::ConvertRectToTarget(rows_, this, row->bounds());
    const int y = below ? rect.bottom() : rect.y();
    return gfx::Rect(kRowBackgroundInsetX, y - 1,
                     width() - 2 * kRowBackgroundInsetX, 2);
  };

  TabStripModel* tabs = browser_->GetTabStripModel();
  const bookmarks::BookmarkNode* space_folder = GetSpaceFolder(false);
  bool in_tabs = false;
  const views::View* last_tab_row = nullptr;
  for (views::View* child : rows_->children()) {
    const bool hit = p.y() >= child->y() && p.y() < child->bounds().bottom();
    if (views::IsViewClass<Divider>(child)) {
      if (hit) {
        // The end of the Space's pinned items.
        DropTarget target;
        target.parent = space_folder ? space_folder->id() : 0;
        target.index = space_folder ? space_folder->children().size() : 0;
        target.indicator = line(child, false);
        return target;
      }
      in_tabs = true;
      continue;
    }
    auto it = row_targets_.find(child);
    if (it == row_targets_.end()) {
      if (hit && !in_tabs) {
        // The Space's name: the top of its pinned items.
        DropTarget target;
        target.parent = space_folder ? space_folder->id() : 0;
        target.indicator = line(child, true);
        return target;
      }
      continue;
    }
    const RowTarget& row = it->second;
    if (in_tabs && row.tab.Get()) {
      last_tab_row = child;
    }
    if (!hit) {
      continue;
    }
    const bool lower = p.y() >= child->y() + child->height() / 2;
    DropTarget target;
    if (!in_tabs) {
      const bookmarks::BookmarkNode* node = GetNode(row.node);
      if (!node || node == source_node) {
        return std::nullopt;
      }
      const int quarter = child->height() / 4;
      if (node->is_folder() && p.y() >= child->y() + quarter &&
          p.y() < child->bounds().bottom() - quarter) {
        target.parent = node->id();
        target.index = node->children().size();
        target.into_folder = true;
        gfx::Rect rect =
            views::View::ConvertRectToTarget(rows_, this, child->bounds());
        rect.Inset(gfx::Insets::VH(kRowBackgroundInsetY, kRowBackgroundInsetX));
        target.indicator = rect;
      } else if (node->is_folder() && lower && IsOpen(node)) {
        // Below an open folder is its first item.
        target.parent = node->id();
        target.index = 0;
        target.indicator = line(child, true);
      } else {
        target.parent = node->parent()->id();
        target.index =
            node->parent()->GetIndexOf(node).value() + (lower ? 1 : 0);
        target.indicator = line(child, lower);
      }
      // A folder cannot go into itself.
      const bookmarks::BookmarkNode* parent = GetNode(target.parent);
      if (source_node && parent && parent->HasAncestor(source_node)) {
        return std::nullopt;
      }
      return target;
    }
    // The tabs: pinned folders cannot become tabs.
    if (source_node && source_node->is_folder()) {
      return std::nullopt;
    }
    if (tabs::TabInterface* tab = row.tab.Get()) {
      target.tab_index = tabs->GetIndexOfTab(tab) + (lower ? 1 : 0);
      target.indicator = line(child, lower);
      return target;
    }
    // The New Tab row: before the Space's first tab, the next tab row.
    target.indicator = line(child, true);
    target.tab_index = tabs->count();
    bool after = false;
    for (views::View* next : rows_->children()) {
      if (next == child) {
        after = true;
        continue;
      }
      auto next_it = row_targets_.find(next);
      if (after && next_it != row_targets_.end() && next_it->second.tab.Get()) {
        target.tab_index = tabs->GetIndexOfTab(next_it->second.tab.Get());
        break;
      }
    }
    return target;
  }

  // Below the last row: after the Space's last tab.
  if (in_tabs && !(source_node && source_node->is_folder())) {
    DropTarget target;
    target.tab_index = tabs->count();
    if (last_tab_row) {
      target.tab_index =
          tabs->GetIndexOfTab(row_targets_.at(last_tab_row).tab.Get()) + 1;
      target.indicator = line(last_tab_row, true);
    }
    return target;
  }
  return std::nullopt;
}

void ArcomiumSidebarView::ShowDropIndicator(
    const std::optional<DropTarget>& target) {
  if (!target || target->indicator.IsEmpty()) {
    drop_indicator_->SetVisible(false);
    return;
  }
  drop_indicator_->SetBackground(
      target->into_folder
          ? views::CreateRoundedRectBackground(
                arcomium::SidebarInk(this, kActiveColor),
                                               kRowBackgroundRadius)
          : views::CreateSolidBackground(arcomium::SidebarInk(this, kItemColor)));
  drop_indicator_->SetBoundsRect(target->indicator);
  drop_indicator_->SetVisible(true);
}

void ArcomiumSidebarView::PerformDrop(
    DropTarget target,
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  output_drag_op = ui::mojom::DragOperation::kNone;
  if (!drag_source_ || !model_) {
    return;
  }
  const bookmarks::BookmarkNode* node = GetNode(drag_source_->node);
  tabs::TabInterface* tab = node ? nullptr : drag_source_->tab.Get();

  if (!target.tab_index) {
    // Into the pinned tree.
    const bookmarks::BookmarkNode* parent =
        target.parent ? GetNode(target.parent) : GetSpaceFolder(true);
    if (!parent) {
      return;
    }
    if (node) {
      if (parent == node || parent->HasAncestor(node)) {
        return;
      }
      model_->Move(node, parent, target.index);
    } else if (tab) {
      // Pins the tab where it was dropped.
      content::WebContents* contents = tab->GetContents();
      if (!contents->GetLastCommittedURL().is_valid()) {
        return;
      }
      const bookmarks::BookmarkNode* pinned = model_->AddNewURL(
          parent, std::min(target.index, parent->children().size()),
          contents->GetTitle(), contents->GetLastCommittedURL());
      if (pinned) {
        bound_[pinned->id()] = tab->GetHandle();
      }
    }
  } else if (node) {
    // Unpins the item; its tab stays open, or opens.
    if (node->is_folder()) {
      return;
    }
    tabs::TabInterface* bound = GetBoundTab(node->id());
    bound_.erase(node->id());
    if (bound) {
      MoveTabToSlot(bound, *target.tab_index);
    } else {
      NavigateParams params(browser_, node->url(),
                            ui::PAGE_TRANSITION_AUTO_BOOKMARK);
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      params.group = browser_->GetTabStripModel()->GetFocusedGroup();
      Navigate(&params);
    }
    model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                   FROM_HERE);
  } else if (tab) {
    MoveTabToSlot(tab, *target.tab_index);
  }
  output_drag_op = ui::mojom::DragOperation::kMove;
  ScheduleRebuild();
}

void ArcomiumSidebarView::MoveTabToSlot(tabs::TabInterface* tab, int slot) {
  TabStripModel* tabs = browser_->GetTabStripModel();
  const int from = tabs->GetIndexOfTab(tab);
  if (from == TabStripModel::kNoTab) {
    return;
  }
  const int to =
      std::clamp(slot > from ? slot - 1 : slot, 0, tabs->count() - 1);
  if (to != from) {
    // Within the Space's group, which stays contiguous.
    tabs->MoveWebContentsAt(from, to, /*select_after_move=*/false,
                            tabs->GetFocusedGroup());
  }
}

void ArcomiumSidebarView::OnRowDragDone() {
  drag_source_.reset();
  ShowDropIndicator(std::nullopt);
  if (std::exchange(rebuild_after_drag_, false)) {
    ScheduleRebuild();
  }
}

BEGIN_METADATA(ArcomiumSidebarView)
END_METADATA
