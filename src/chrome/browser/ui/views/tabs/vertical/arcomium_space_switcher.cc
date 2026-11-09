// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/arcomium_space_switcher.h"

#include "chrome/browser/ui/views/tabs/vertical/arcomium_glyphs.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_popover.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_space_color.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_tooltip.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "base/token.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/browser_commands.h"
#include "ui/events/event.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/tabs/arcomium_space_command_hook.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/arcomium_spaces.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/arcomium_frame_color.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/gfx/image/image_skia.h"
#endif
#include "chrome/browser/ui/views/tabs/vertical/arcomium_sidebar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/color/color_provider.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

// Arc's Space bar: Downloads at the left (where Arc has its archive), the
// Spaces' icons in the middle (the active one bright), "+" at the right edge.
// Arc for macOS' Space bar: 34x32 pt Space buttons side by side, centred on
// the sidebar; Downloads and "+" centred 24 pt from its sides (the region
// already keeps 12 pt), 13 pt icons; a hovered button gets a faint plate as
// wide as the button.
constexpr gfx::Size kButtonBox(34, 32);
// Downloads' and "+"'s plates are 32x32 pt, as tall as the Spaces'.
constexpr gfx::Size kSideButtonBox(32, 32);
// The bar spans the sidebar but for 8 pt at each side (the region's bottom
// padding there), where Arc's plates end.
constexpr gfx::Insets kBarInsets = gfx::Insets::TLBR(2, 0, 0, 0);
constexpr int kIconGap = 0;
// The sun, rays and all, is 16.8 wide drawn for the sidebar.
constexpr float kIconScale = 13 / 16.8f;
#if BUILDFLAG(IS_MAC)
// The menu bar's Spaces menu's icons, about 12.5 pt as Arc's, not the
// sidebar's 14.5.
constexpr float kMenuIconScale = 0.87f;
#endif
constexpr float kHoverPlateRadius = 8;
constexpr SkColor kHoverPlate = SkColorSetARGB(0x11, 0xFF, 0xFF, 0xFF);
constexpr int kDotDiameter = 6;
constexpr int kSpaceIconSize = 16;
// The colors (chrome/browser/ui/color/arcomium_color_mixer.cc).
constexpr ui::ColorId kActiveIconColor = kColorArcomiumSpaceIconActive;
constexpr ui::ColorId kHoveredIconColor = kColorArcomiumSpaceIconHovered;
constexpr ui::ColorId kIconColor = kColorArcomiumSpaceIcon;
// Space icons a folder records; home is a sun until chosen otherwise.
constexpr char kSpaceIconMetaKey[] = "arcomium.space_icon";
constexpr char kDefaultSpace[] = "default";
// Context menu commands.
constexpr int kEditColorCommand = 1;
constexpr int kDeleteSpaceCommand = 2;
// The plus button's menu.
constexpr int kNewSpaceCommand = 3;
constexpr int kNewFolderCommand = 4;
constexpr int kNewTabCommand = 5;
constexpr int kRenameSpaceCommand = 6;
constexpr int kNewSplitCommand = 7;
constexpr int kChangeIconCommand = 8;
constexpr int kToggleFoldersCommand = 9;
// A folder's open state (arcomium_sidebar_view.cc).
constexpr char kFolderOpenMetaKey[] = "arcomium.open";
#if BUILDFLAG(IS_MAC)
constexpr int kAddPopoverWidth = 210;
#else
// Room for Windows' spelled-out shortcuts, "Ctrl+Shift+=".
constexpr int kAddPopoverWidth = 250;
#endif

// Meta info of a Space's bookmark folder: its Space id, and the saved tab
// group it syncs as.
constexpr char kSpaceMetaKey[] = "arcomium.space";
constexpr char kSavedGroupMetaKey[] = "arcomium.group";

constexpr tab_groups::TabGroupColorId kSpaceColors[] = {
    tab_groups::TabGroupColorId::kBlue,   tab_groups::TabGroupColorId::kPurple,
    tab_groups::TabGroupColorId::kGreen,  tab_groups::TabGroupColorId::kPink,
    tab_groups::TabGroupColorId::kOrange, tab_groups::TabGroupColorId::kCyan,
    tab_groups::TabGroupColorId::kYellow, tab_groups::TabGroupColorId::kRed,
};

// The icons a Space can have, under the names Arc gives them.
struct SpaceIconChoice {
  const char* name;
  int label;
  // Null for the icons drawn here.
  const gfx::VectorIcon* icon;
};

const SpaceIconChoice kSpaceIcons[] = {
    {"sunny", IDS_ARCOMIUM_SPACE_ICON_SUN, nullptr},
    {"book", IDS_ARCOMIUM_SPACE_ICON_BOOK, &kMenuBookIcon},
    {"home", IDS_ARCOMIUM_SPACE_ICON_HOME, &vector_icons::kHomeIcon},
    {"code", IDS_ARCOMIUM_SPACE_ICON_CODE, &vector_icons::kCodeIcon},
    {"globe", IDS_ARCOMIUM_SPACE_ICON_GLOBE, &vector_icons::kGlobeIcon},
    {"bolt", IDS_ARCOMIUM_SPACE_ICON_BOLT, &kBoltIcon},
    {"star", IDS_ARCOMIUM_SPACE_ICON_STAR, &kStarIcon},
    {"chat", IDS_ARCOMIUM_SPACE_ICON_CHAT, &vector_icons::kChatIcon},
    {"mail", IDS_ARCOMIUM_SPACE_ICON_MAIL, &vector_icons::kMailIcon},
    {"lightbulb", IDS_ARCOMIUM_SPACE_ICON_IDEA, &vector_icons::kLightbulbIcon},
    {"rocket", IDS_ARCOMIUM_SPACE_ICON_ROCKET, &kRocketLaunchIcon},
    {"pets", IDS_ARCOMIUM_SPACE_ICON_PETS, &vector_icons::kPetsIcon},
    {"flight", IDS_ARCOMIUM_SPACE_ICON_TRAVEL, &vector_icons::kFlightIcon},
    {"dot", IDS_ARCOMIUM_SPACE_ICON_DOT, nullptr},
};

// Whether a Space's pinned tree has folders, and whether one is open.
struct FolderState {
  bool any = false;
  bool any_open = false;
};

FolderState GetFolderState(const bookmarks::BookmarkNode* space) {
  FolderState state;
  if (!space) {
    return state;
  }
  std::vector<const bookmarks::BookmarkNode*> folders = {space};
  while (!folders.empty()) {
    const bookmarks::BookmarkNode* folder = folders.back();
    folders.pop_back();
    for (const auto& child : folder->children()) {
      if (child->is_folder()) {
        state.any = true;
        std::string open;
        state.any_open |=
            child->GetMetaInfo(kFolderOpenMetaKey, &open) && open == "1";
        folders.push_back(child.get());
      }
    }
  }
  return state;
}

// The sidebar in `widget`'s window.
ArcomiumSidebarView* FindSidebar(views::Widget* widget) {
  std::vector<views::View*> views;
  if (widget && widget->GetRootView()) {
    views.push_back(widget->GetRootView());
  }
  while (!views.empty()) {
    views::View* view = views.back();
    views.pop_back();
    if (auto* sidebar = views::AsViewClass<ArcomiumSidebarView>(view)) {
      return sidebar;
    }
    for (views::View* child : view->children()) {
      views.push_back(child);
    }
  }
  return nullptr;
}

const SpaceIconChoice* FindSpaceIcon(const std::string& name) {
  for (const SpaceIconChoice& choice : kSpaceIcons) {
    if (name == choice.name) {
      return &choice;
    }
  }
  return nullptr;
}

// Draws the Space icon `name` centered on `center`.
void PaintSpaceIcon(gfx::Canvas* canvas,
                    const gfx::PointF& center,
                    const std::string& name,
                    SkColor color) {
  const SpaceIconChoice* choice = FindSpaceIcon(name);
  if (name == "book") {
    // Arc for macOS' book: two filled pages open at a spine gap, their tops
    // dipping and their bottoms sagging towards it.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color);
    for (const float side : {-1.0f, 1.0f}) {
      SkPathBuilder page;
      page.moveTo(center.x() + side * 0.8f, center.y() - 5.7f)
          .lineTo(center.x() + side * 8.4f, center.y() - 7.2f)
          .lineTo(center.x() + side * 8.4f, center.y() + 5.2f)
          .lineTo(center.x() + side * 0.8f, center.y() + 7.1f)
          .close();
      canvas->DrawPath(page.detach(), flags);
    }
    return;
  }
  if (choice && choice->icon) {
    const gfx::ImageSkia image =
        gfx::CreateVectorIcon(*choice->icon, kSpaceIconSize, color);
    canvas->DrawImageInt(image, std::round(center.x() - kSpaceIconSize / 2.0f),
                         std::round(center.y() - kSpaceIconSize / 2.0f));
    return;
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  if (name == "sunny") {
    // Arc's sun: a disc with eight short rays.
    canvas->DrawCircle(center, 3.5f, flags);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.6f);
    flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
    for (int i = 0; i < 8; ++i) {
      const float angle = i * std::numbers::pi_v<float> / 4;
      const gfx::Vector2dF ray(std::cos(angle), std::sin(angle));
      canvas->DrawLine(center + gfx::ScaleVector2d(ray, 5.8f),
                       center + gfx::ScaleVector2d(ray, 7.6f), flags);
    }
    return;
  }
  canvas->DrawCircle(center, kDotDiameter / 2.0f, flags);
}

// The menu bar's commands that the sidebar runs; the others are the Space
// bar's. Command-1 to -9 (IDC_SELECT_TAB_0 to _LAST_TAB) go to its items on
// macOS, as in Arc.
bool IsSidebarCommand(int command_id) {
  return command_id == IDC_ARCOMIUM_PIN_TAB ||
         command_id == IDC_ARCOMIUM_NEW_FOLDER ||
         command_id == IDC_ARCOMIUM_CLEAR_TODAY ||
         command_id == IDC_ARCOMIUM_TOGGLE_PINNED ||
         command_id == IDC_ARCOMIUM_REVEAL_TAB ||
         (command_id >= IDC_SELECT_TAB_0 && command_id <= IDC_SELECT_LAST_TAB);
}

// The shortcut that shows the Space at `index` in the row.
ui::Accelerator SpaceAccelerator(size_t index) {
  return ui::Accelerator(
      static_cast<ui::KeyboardCode>(ui::VKEY_1 + index),
#if BUILDFLAG(IS_MAC)
      ui::EF_CONTROL_DOWN
#else
      ui::EF_ALT_DOWN
#endif
  );
}

// The Space bar's icons: Arc for macOS dims them in an inactive window to
// half their contrast with its lighter frame (#A0A0A0 to #747474, the shown
// Space's #F0F0F1 to #969696).
SkColor IconInk(const views::View* view, ui::ColorId id) {
  const SkColor ink = arcomium::SidebarInk(view, id);
  return arcomium::PaintsInactiveFrame(view)
             ? color_utils::AlphaBlend(ink, arcomium::FrameColor(view), 0.5f)
             : ink;
}

void PaintHoverPlate(const views::Button* button, gfx::Canvas* canvas) {
  if (!button->IsMouseHovered()) {
    return;
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(arcomium::SidebarInk(button, kHoverPlate));
  canvas->DrawRoundRect(gfx::RectF(button->GetLocalBounds()),
                        kHoverPlateRadius, flags);
}

// A Space: its icon, bright while it is the active one and dimmed otherwise.
class SpaceIcon : public views::Button {
  METADATA_HEADER(SpaceIcon, views::Button)

 public:
  SpaceIcon(PressedCallback callback,
            std::string icon,
            bool active,
            const std::u16string& title,
            std::string space,
            std::string sync_id)
      : views::Button(std::move(callback)),
        icon_(std::move(icon)),
        active_(active),
        space_(std::move(space)),
        sync_id_(std::move(sync_id)) {
    SetTooltipText(title);
    GetViewAccessibility().SetName(title);
    SetPreferredSize(kButtonBox);
  }

  // The Space's id, or "" for a saved Space without a tab group here.
  const std::string& space() const { return space_; }
  // The saved tab group of a Space without a tab group here.
  const std::string& sync_id() const { return sync_id_; }

  // Arc for macOS tints a hovered Space's icon with a light shade of the
  // Space's theme color.
  void SetHoverTint(SkColor tint) { hover_tint_ = tint; }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const ui::ColorId color = active_            ? kActiveIconColor
                              : IsMouseHovered() ? kHoveredIconColor
                                                 : kIconColor;
    const SkColor ink = !active_ && IsMouseHovered() && hover_tint_
                            ? *hover_tint_
                            : IconInk(this, color);
    const gfx::PointF center = gfx::RectF(GetContentsBounds()).CenterPoint();
    // An emoji fades like the glyphs do on the dark theme, on every theme.
    const uint8_t emoji_alpha = active_ ? 255 : IsMouseHovered() ? 150 : 90;
    PaintHoverPlate(this, canvas);
    // Arc for macOS' Space icons are 13 pt, smaller than the sidebar's.
    gfx::ScopedCanvas scoped_canvas(canvas);
    canvas->sk_canvas()->translate(center.x(), center.y());
    canvas->sk_canvas()->scale(kIconScale, kIconScale);
    arcomium::PaintSpaceIcon(canvas, gfx::PointF(), icon_, ink, emoji_alpha);
  }

  void StateChanged(ButtonState old_state) override { SchedulePaint(); }

 private:
  std::optional<SkColor> hover_tint_;
  const std::string icon_;
  const bool active_;
  const std::string space_;
  const std::string sync_id_;
};

BEGIN_METADATA(SpaceIcon)
END_METADATA

// The Space bar's left button: Downloads (where Arc has its archive, which
// Arcomium does not keep). An arrow down into a tray, in the stroke, size
// and grey of the "+" beside it; opens chrome://downloads as
// IDC_SHOW_DOWNLOADS does.
class DownloadsButton : public views::Button {
  METADATA_HEADER(DownloadsButton, views::Button)

 public:
  explicit DownloadsButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    const std::u16string name =
        l10n_util::GetStringUTF16(IDS_ARCOMIUM_DOWNLOADS);
    // Named with its shortcut in Arc for macOS' pill: Shift-Command-J there,
    // Chromium's Ctrl+J on Windows and Linux.
    arcomium::SetArcTooltip(
        this, name, std::u16string(),
#if BUILDFLAG(IS_MAC)
        ui::Accelerator(ui::VKEY_J, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN));
#else
        ui::Accelerator(ui::VKEY_J, ui::EF_CONTROL_DOWN));
#endif
    // Kept under the pointer at the window's bottom, as Arc's Library pill.
    arcomium::SetArcTooltipBelowPointer(this);
    GetViewAccessibility().SetName(name);
    SetPreferredSize(kSideButtonBox);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::PointF c = gfx::RectF(GetContentsBounds()).CenterPoint();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
    flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
    PaintHoverPlate(this, canvas);
    // Grey under the pointer too, as the "+"; the plate shows the hover.
    flags.setColor(IconInk(this, kHoveredIconColor));
    // The "+"'s 1.7 pt stroke over a 13.5 pt square.
    flags.setStrokeWidth(1.7f);
    constexpr float kHalf = 6.75f;
    constexpr float kHead = 3.4f;
    const float arrow_top = c.y() - kHalf;
    const float arrow_tip = c.y() + 2.2f;
    const float tray_top = c.y() + 1.2f;
    SkPathBuilder arrow;
    arrow.moveTo(c.x(), arrow_top)
        .lineTo(c.x(), arrow_tip)
        .moveTo(c.x() - kHead, arrow_tip - kHead)
        .lineTo(c.x(), arrow_tip)
        .lineTo(c.x() + kHead, arrow_tip - kHead);
    canvas->DrawPath(arrow.detach(), flags);
    SkPathBuilder tray;
    tray.moveTo(c.x() - kHalf, tray_top)
        .lineTo(c.x() - kHalf, c.y() + kHalf)
        .lineTo(c.x() + kHalf, c.y() + kHalf)
        .lineTo(c.x() + kHalf, tray_top);
    canvas->DrawPath(tray.detach(), flags);
  }

  void StateChanged(ButtonState old_state) override { SchedulePaint(); }
};

BEGIN_METADATA(DownloadsButton)
END_METADATA

// Arc for macOS' "+": a 13.5 pt cross 1.7 pt thick in Downloads' grey,
// on the Space bar's hover plate.
class AddButton : public views::Button {
  METADATA_HEADER(AddButton, views::Button)

 public:
  explicit AddButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetPreferredSize(kSideButtonBox);
  }

  // While its popover is open the "+" is a cross on the plate, as in Arc.
  // The popover has the pointer's events meanwhile, so whether the pointer
  // is on the button (its plate) is polled.
  void SetOpen(bool open) {
    open_ = open;
    if (open) {
      hovered_ = IsMouseHovered();
      hover_poll_.Start(FROM_HERE, base::Milliseconds(50),
                        base::BindRepeating(&AddButton::PollHover,
                                            base::Unretained(this)));
    } else {
      hover_poll_.Stop();
    }
    SchedulePaint();
  }

  base::OnceClosure GetClosedCallback() {
    return base::BindOnce(&AddButton::SetOpen, weak_factory_.GetWeakPtr(),
                          false);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::PointF c = gfx::RectF(GetContentsBounds()).CenterPoint();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    if (open_) {
      // The plate only while the pointer is on it: Arc's goes once the
      // pointer is off the button or in the popover.
      PaintHoverPlate(this, canvas);
      // An 11 pt cross, a shade brighter than the "+": #A8A8A8, as Arc's.
      flags.setColor(arcomium::SidebarInk(this, SkColorSetRGB(0xA8, 0xA8, 0xA8)));
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(1.7f);
      flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
      for (const float side : {-1.0f, 1.0f}) {
        canvas->DrawLine(c + gfx::Vector2dF(-4.6f, -4.6f * side),
                         c + gfx::Vector2dF(4.6f, 4.6f * side), flags);
      }
      return;
    }
    PaintHoverPlate(this, canvas);
    flags.setColor(IconInk(this, kHoveredIconColor));
    canvas->DrawRoundRect(gfx::RectF(c.x() - 6.75f, c.y() - 0.85f, 13.5f, 1.7f),
                          0.85f, flags);
    canvas->DrawRoundRect(gfx::RectF(c.x() - 0.85f, c.y() - 6.75f, 1.7f, 13.5f),
                          0.85f, flags);
  }

  void StateChanged(ButtonState old_state) override { SchedulePaint(); }

 private:
  void PollHover() {
    if (const bool hovered = IsMouseHovered(); hovered != hovered_) {
      hovered_ = hovered;
      SchedulePaint();
    }
  }

  bool open_ = false;
  bool hovered_ = false;
  base::RepeatingTimer hover_poll_;
  base::WeakPtrFactory<AddButton> weak_factory_{this};
};

BEGIN_METADATA(AddButton)
END_METADATA

}  // namespace

namespace arcomium {

std::string GetSpaceIconName(const bookmarks::BookmarkNode* folder,
                             const std::string& space) {
  std::string icon;
  if (folder && folder->GetMetaInfo(kSpaceIconMetaKey, &icon) &&
      (FindSpaceIcon(icon) || arcomium::IsArcGlyph(icon) ||
       arcomium::IsEmojiIcon(icon))) {
    return icon;
  }
  return space == kDefaultSpace ? "sunny" : "dot";
}

void PaintSpaceIcon(gfx::Canvas* canvas,
                    const gfx::PointF& center,
                    const std::string& name,
                    SkColor color,
                    uint8_t emoji_alpha) {
  if (arcomium::IsEmojiIcon(name)) {
    arcomium::PaintEmojiIcon(canvas, center, name, emoji_alpha);
    return;
  }
  // Arc's sun and (on macOS) book are drawn here already; the rest of Arc's
  // set by the glyph painter, and Arcomium's first icons as before.
  if (name != "sunny" && name != "book" &&
      arcomium::PaintArcGlyph(canvas, center, name, color)) {
    return;
  }
  ::PaintSpaceIcon(canvas, center, name, color);
}

std::vector<std::string> GetSpaceIconNames() {
  std::vector<std::string> names;
  for (const SpaceIconChoice& choice : kSpaceIcons) {
    names.push_back(choice.name);
  }
  return names;
}

}  // namespace arcomium

ArcomiumSpaceSwitcher::ArcomiumSpaceSwitcher(BrowserWindowInterface* browser)
    : browser_(browser), model_(browser->GetTabStripModel()) {
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kBarInsets));
  model_->AddObserver(this);
  arcomium::SpaceCommandHook() = &ArcomiumSpaceSwitcher::RunSpaceCommand;
  arcomium::SpaceCommandStateHook() =
      &ArcomiumSpaceSwitcher::GetSpaceCommandState;
#if BUILDFLAG(IS_MAC)
  arcomium::MenuBarListHook() = &ArcomiumSpaceSwitcher::GetMenuBarList;
  arcomium::MenuBarRunHook() = &ArcomiumSpaceSwitcher::RunMenuBarEntry;
  arcomium::ShownSpaceTitleHook() = &ArcomiumSpaceSwitcher::GetShownSpaceTitle;
#endif
  if (!browser->GetProfile()->IsOffTheRecord()) {
    sync_service_ = tab_groups::TabGroupSyncServiceFactory::GetForProfile(
        browser->GetProfile());
  }
  if (sync_service_) {
    sync_observation_.Observe(sync_service_);
  }
  if (bookmarks::BookmarkModel* bookmarks =
          BookmarkModelFactory::GetForBrowserContext(browser->GetProfile())) {
    bookmark_observation_.Observe(bookmarks);
  }
  Rebuild();
}

ArcomiumSpaceSwitcher::~ArcomiumSpaceSwitcher() = default;

// static
bool ArcomiumSpaceSwitcher::RunSpaceCommand(BrowserWindowInterface* browser,
                                            int command_id) {
  if (command_id == IDC_ARCOMIUM_NEW_SPACE) {
    CreateSpace(browser);
    return true;
  }
  std::vector<views::View*> views;
  if (BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser)) {
    views.push_back(browser_view);
  }
  const bool sidebar_command = IsSidebarCommand(command_id);
  while (!views.empty()) {
    views::View* view = views.back();
    views.pop_back();
    if (auto* sidebar = views::AsViewClass<ArcomiumSidebarView>(view);
        sidebar && sidebar_command) {
      sidebar->RunMenuBarCommand(command_id);
      return true;
    }
    if (auto* switcher = views::AsViewClass<ArcomiumSpaceSwitcher>(view);
        switcher && !sidebar_command) {
      if (command_id == IDC_ARCOMIUM_RENAME_SPACE ||
          command_id == IDC_ARCOMIUM_EDIT_SPACE_THEME) {
        // As the shown Space's menu does.
        if (switcher->active_space_ < 0 ||
            switcher->active_space_ >=
                static_cast<int>(switcher->space_refs_.size())) {
          return true;
        }
        switcher->menu_space_ =
            switcher->space_refs_[switcher->active_space_].space;
        switcher->menu_sync_id_.clear();
        // No menu was opened: the theme picker opens over the Space bar, not
        // at a stale point (the screen's corner).
        switcher->menu_point_ = switcher->GetBoundsInScreen().origin();
        if (command_id == IDC_ARCOMIUM_RENAME_SPACE) {
          switcher->RenameMenuSpace();
        } else {
          switcher->EditMenuSpaceColor();
        }
        return true;
      }
      const bool forward = command_id == IDC_ARCOMIUM_NEXT_SPACE;
      if (switcher->IsDrawn() && switcher->CanRotateSpace(forward)) {
        switcher->RotateSpace(forward);
      }
      return true;
    }
    for (views::View* child : view->children()) {
      views.push_back(child);
    }
  }
  return false;
}

// static
arcomium::SpaceCommandState ArcomiumSpaceSwitcher::GetSpaceCommandState(
    BrowserWindowInterface* browser,
    int command_id) {
  using arcomium::SpaceCommandState;
  if (browser->GetProfile()->IsOffTheRecord()) {
    return SpaceCommandState::kUnavailable;
  }
  if (command_id == IDC_ARCOMIUM_NEW_SPACE) {
    return SpaceCommandState::kAvailable;
  }
  const bool sidebar_command = IsSidebarCommand(command_id);
  std::vector<views::View*> views;
  if (BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser)) {
    views.push_back(browser_view);
  }
  while (!views.empty()) {
    views::View* view = views.back();
    views.pop_back();
    if (auto* sidebar = views::AsViewClass<ArcomiumSidebarView>(view);
        sidebar && sidebar_command) {
      return sidebar->GetMenuBarCommandState(command_id);
    }
    if (auto* switcher = views::AsViewClass<ArcomiumSpaceSwitcher>(view);
        switcher && !sidebar_command) {
      if (!switcher->IsDrawn()) {
        return SpaceCommandState::kUnavailable;
      }
      if (command_id == IDC_ARCOMIUM_RENAME_SPACE ||
          command_id == IDC_ARCOMIUM_EDIT_SPACE_THEME) {
        return switcher->active_space_ >= 0 &&
                       switcher->active_space_ <
                           static_cast<int>(switcher->space_refs_.size())
                   ? SpaceCommandState::kAvailable
                   : SpaceCommandState::kUnavailable;
      }
      return switcher->CanRotateSpace(command_id == IDC_ARCOMIUM_NEXT_SPACE)
                 ? SpaceCommandState::kAvailable
                 : SpaceCommandState::kUnavailable;
    }
    for (views::View* child : view->children()) {
      views.push_back(child);
    }
  }
  return SpaceCommandState::kUnavailable;
}

#if BUILDFLAG(IS_MAC)
namespace {

ArcomiumSpaceSwitcher* FindSpaceSwitcherIn(BrowserWindowInterface* browser) {
  std::vector<views::View*> views;
  if (BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser)) {
    views.push_back(browser_view);
  }
  while (!views.empty()) {
    views::View* view = views.back();
    views.pop_back();
    if (auto* switcher = views::AsViewClass<ArcomiumSpaceSwitcher>(view)) {
      return switcher;
    }
    for (views::View* child : view->children()) {
      views.push_back(child);
    }
  }
  return nullptr;
}

ExtensionsToolbarViewModel* GetExtensionsModel(
    BrowserWindowInterface* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  ExtensionsToolbarDesktop* container =
      browser_view && browser_view->toolbar()
          ? browser_view->toolbar()->extensions_container()
          : nullptr;
  return container ? container->GetToolbarViewModel() : nullptr;
}

// The window's extensions by name, as Arc's Extensions menu lists them.
std::vector<ToolbarActionsModel::ActionId> GetSortedExtensions(
    ExtensionsToolbarViewModel* model) {
  std::vector<ToolbarActionsModel::ActionId> ids;
  if (!model || !model->AreActionsInitialized()) {
    return ids;
  }
  for (const auto& id : model->GetAllActionIds()) {
    if (model->GetActionModelForId(id)) {
      ids.push_back(id);
    }
  }
  std::ranges::sort(ids, [model](const auto& a, const auto& b) {
    return model->GetActionModelForId(a)->GetActionName() <
           model->GetActionModelForId(b)->GetActionName();
  });
  return ids;
}

}  // namespace

// static
std::vector<arcomium::MenuBarEntry> ArcomiumSpaceSwitcher::GetMenuBarList(
    BrowserWindowInterface* browser,
    arcomium::MenuBarList list) {
  std::vector<arcomium::MenuBarEntry> entries;
  if (list == arcomium::MenuBarList::kSpaces) {
    ArcomiumSpaceSwitcher* switcher = FindSpaceSwitcherIn(browser);
    if (!switcher || browser->GetProfile()->IsOffTheRecord()) {
      return entries;
    }
    for (size_t i = 0; i < switcher->menu_bar_spaces_.size(); ++i) {
      const MenuBarSpace& space = switcher->menu_bar_spaces_[i];
      arcomium::MenuBarEntry entry;
      entry.title = space.title;
      entry.checked = static_cast<int>(i) == switcher->active_space_;
      // Arc's menu shows each Space with its icon, smaller than the
      // sidebar's (about 12 pt): a glyph in the menu's ink, or in the light
      // shade of the Space's theme (Arc's Work book is light blue); an emoji
      // in its own colors.
      gfx::Canvas canvas(gfx::Size(16, 16), 2.0f, /*is_opaque=*/false);
      {
        gfx::ScopedCanvas scoped(&canvas);
        canvas.sk_canvas()->translate(8, 8);
        canvas.sk_canvas()->scale(kMenuIconScale, kMenuIconScale);
        arcomium::PaintSpaceIcon(&canvas, gfx::PointF(), space.icon,
                                 space.tint.value_or(SK_ColorBLACK));
      }
      entry.icon = gfx::ImageSkia::CreateFromBitmap(canvas.GetBitmap(), 2.0f);
      entry.icon_is_template =
          !arcomium::IsEmojiIcon(space.icon) && !space.tint;
      entries.push_back(std::move(entry));
    }
    return entries;
  }
  ExtensionsToolbarViewModel* model = GetExtensionsModel(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  content::WebContents* contents =
      browser->GetTabStripModel()->GetActiveWebContents();
  for (const auto& id : GetSortedExtensions(model)) {
    ToolbarActionViewModel* action = model->GetActionModelForId(id);
    arcomium::MenuBarEntry entry;
    entry.title = action->GetActionName();
    const ui::ImageModel image = action->GetIcon(contents, gfx::Size(16, 16));
    if (!image.IsEmpty() && browser_view && browser_view->GetColorProvider()) {
      entry.icon = image.Rasterize(browser_view->GetColorProvider());
    }
    entries.push_back(std::move(entry));
  }
  return entries;
}

// static
std::u16string ArcomiumSpaceSwitcher::GetShownSpaceTitle(
    BrowserWindowInterface* browser) {
  ArcomiumSpaceSwitcher* switcher = FindSpaceSwitcherIn(browser);
  if (!switcher || browser->GetProfile()->IsOffTheRecord() ||
      switcher->active_space_ < 0 ||
      switcher->active_space_ >=
          static_cast<int>(switcher->menu_bar_spaces_.size())) {
    return std::u16string();
  }
  return switcher->menu_bar_spaces_[switcher->active_space_].title;
}

// static
void ArcomiumSpaceSwitcher::RunMenuBarEntry(BrowserWindowInterface* browser,
                                            arcomium::MenuBarList list,
                                            size_t index) {
  if (list == arcomium::MenuBarList::kSpaces) {
    ArcomiumSpaceSwitcher* switcher = FindSpaceSwitcherIn(browser);
    if (switcher && index < switcher->space_actions_.size() &&
        static_cast<int>(index) != switcher->active_space_) {
      // A copy: switching rebuilds the row and its callbacks.
      base::RepeatingClosure action = switcher->space_actions_[index];
      action.Run();
    }
    return;
  }
  // As the site controls run an extension: its popup or its action.
  ExtensionsToolbarViewModel* model = GetExtensionsModel(browser);
  const std::vector<ToolbarActionsModel::ActionId> ids =
      GetSortedExtensions(model);
  if (index < ids.size()) {
    model->ExecuteUserAction(
        ids[index], ToolbarActionViewModel::InvocationSource::kMenuEntry);
  }
}
#endif  // BUILDFLAG(IS_MAC)

// static
void ArcomiumSpaceSwitcher::ShowActiveSpaceMenuIn(
    BrowserWindowInterface* browser,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  std::vector<views::View*> views;
  if (BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser)) {
    views.push_back(browser_view);
  }
  while (!views.empty()) {
    views::View* view = views.back();
    views.pop_back();
    if (auto* switcher = views::AsViewClass<ArcomiumSpaceSwitcher>(view)) {
      switcher->ShowActiveSpaceMenu(point, source_type);
      return;
    }
    for (views::View* child : view->children()) {
      views.push_back(child);
    }
  }
}

// static
void ArcomiumSpaceSwitcher::CreateSpace(BrowserWindowInterface* browser) {
  Profile* profile = browser->GetProfile();
  TabStripModel* model = browser->GetTabStripModel();
  if (profile->IsOffTheRecord() || !model->SupportsTabGroups()) {
    return;
  }
  const std::string space =
      base::ToLowerASCII(base::Token::CreateRandom().ToString());

  const size_t count = model->group_model()->ListTabGroups().size();
  // Name the Space after a Space imported from Arc that has no Arcomium Space
  // yet; the sidebar then adopts its pinned folder.
  std::u16string title = u"Space " + base::NumberToString16(count + 1);
  if (bookmarks::BookmarkModel* bookmarks =
          BookmarkModelFactory::GetForBrowserContext(profile);
      bookmarks && bookmarks->loaded()) {
    for (const auto& folder : bookmarks->other_node()->children()) {
      std::string value;
      if (folder->is_folder() &&
          folder->GetMetaInfo("arcomium.space", &value) &&
          value.starts_with("arc:")) {
        title = folder->GetTitle();
        break;
      }
    }
  }
  OpenSpace(browser, space, title);
  chrome::FocusLocationBar(browser);
}

// static
void ArcomiumSpaceSwitcher::OpenSpace(BrowserWindowInterface* browser,
                                      const std::string& space,
                                      const std::u16string& title) {
  Profile* profile = browser->GetProfile();
  TabStripModel* model = browser->GetTabStripModel();
  if (profile->IsOffTheRecord() || !model->SupportsTabGroups()) {
    return;
  }
  // A blank tab with no history: the Space's hidden one, so it opens empty
  // (TabStripModel::IsArcomiumEmptySpace()).
  const GURL url("about:blank#arcomium-space");
  content::WebContents::CreateParams create_params(
      profile, arcomium::CreateSpaceSiteInstance(profile, url, space));
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(create_params);
  contents->GetController().LoadURL(url, content::Referrer(),
                                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                    std::string());
  // Marked before it shows, or it takes the address bar's focus: an empty
  // Space waits for a bookmark or for typing.
  TabStripModel::MarkArcomiumEmptySpace(contents.get());
  model->AddWebContents(std::move(contents), -1,
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                        AddTabTypes::ADD_ACTIVE);

  const size_t count = model->group_model()->ListTabGroups().size();
  const tab_groups::TabGroupId group =
      model->AddToNewGroup({model->active_index()});
  model->ChangeTabGroupVisuals(
      group, tab_groups::TabGroupVisualData(
                 title, kSpaceColors[count % std::size(kSpaceColors)]));
  model->EnterFocusMode(group);
}

void ArcomiumSpaceSwitcher::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }
  // A tab still in the strip, and the Space it is in.
  auto space_of = [this](tabs::TabInterface* tab)
      -> std::optional<std::optional<tab_groups::TabGroupId>> {
    if (!tab || model_->GetIndexOfTab(tab) == TabStripModel::kNoTab) {
      return std::nullopt;
    }
    return tab->GetGroup();
  };
  const auto old_space = space_of(selection.old_tab);
  const auto new_space = space_of(selection.new_tab);
  // Switching Spaces activates a tab of the new Space that the user did not
  // pick, so the new tab only counts while moving within one Space; the one
  // left behind counts when leaving it.
  if (new_space && (!old_space || *old_space == *new_space)) {
    last_active_tabs_[*new_space] = selection.new_tab->GetHandle();
  } else if (old_space && new_space) {
    last_active_tabs_[*old_space] = selection.old_tab->GetHandle();
  }
}

void ArcomiumSpaceSwitcher::RestoreActiveTab(
    std::optional<tab_groups::TabGroupId> space) {
  if (model_->GetFocusedGroup() != space) {
    return;
  }
  auto it = last_active_tabs_.find(space);
  tabs::TabInterface* tab = it != last_active_tabs_.end() ? it->second.Get()
                                                          : nullptr;
  int index = tab && tab->GetGroup() == space ? model_->GetIndexOfTab(tab)
                                              : TabStripModel::kNoTab;
  // Home never visited yet: its first tab, not a tab of the Space just left.
  if (index == TabStripModel::kNoTab && !space) {
    for (int i = 0; i < model_->count(); ++i) {
      if (!model_->GetTabAtIndex(i)->GetGroup()) {
        index = i;
        break;
      }
    }
  }
  if (index != TabStripModel::kNoTab && index != model_->active_index()) {
    model_->ActivateTabAt(index);
  }
}

void ArcomiumSpaceSwitcher::OnTabGroupChanged(const TabGroupChange& change) {
  if (change.type == TabGroupChange::kCreated ||
      change.type == TabGroupChange::kVisualsChanged ||
      change.type == TabGroupChange::kMoved ||
      change.type == TabGroupChange::kClosed) {
    ScheduleRebuild();
  }
}

void ArcomiumSpaceSwitcher::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  ScheduleRebuild();
  // Posted: the tab strip does not take changes from its observers.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcomiumSpaceSwitcher::RestoreActiveTab,
                                weak_factory_.GetWeakPtr(),
                                new_focused_group_id));
}

void ArcomiumSpaceSwitcher::OnInitialized() {
  sync_initialized_ = true;
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  // A saved group changes on every navigation in it; the icons only show its
  // title, color, whether it is open here and whether it is empty.
  std::string signature = base::StrCat(
      {base::UTF16ToUTF8(group.title()), "|",
       base::NumberToString(static_cast<int>(group.color())), "|",
       group.local_group_id() ? "open" : "closed", "|",
       group.saved_tabs().empty() ? "empty" : "tabs"});
  std::string& known = group_signatures_[group.saved_guid()];
  if (known == signature) {
    return;
  }
  known = std::move(signature);
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::OnTabGroupRemoved(
    const base::Uuid& sync_id,
    tab_groups::TriggerSource source) {
  group_signatures_.erase(sync_id);
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::BookmarkModelChanged() {
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::BookmarkMetaInfoChanged(
    const bookmarks::BookmarkNode* node) {
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::BookmarkModelBeingDeleted() {
  bookmark_observation_.Reset();
}

void ArcomiumSpaceSwitcher::RetryRebuildLater() {
  // One retry at a time, however many rebuilds ask for it.
  if (!retry_timer_.IsRunning()) {
    retry_timer_.Start(FROM_HERE, base::Seconds(2),
                       base::BindOnce(&ArcomiumSpaceSwitcher::ScheduleRebuild,
                                      base::Unretained(this)));
  }
}

void ArcomiumSpaceSwitcher::ScheduleRebuild() {
  // Coalesced into one pending rebuild. Its own factory: invalidating the one
  // the delete dialog and RestoreActiveTab() hold would drop them.
  rebuild_weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcomiumSpaceSwitcher::Rebuild,
                                rebuild_weak_factory_.GetWeakPtr()));
}

bool ArcomiumSpaceSwitcher::RotateSpace(bool forward) {
  const int next = active_space_ + (forward ? 1 : -1);
  if (active_space_ < 0 || next < 0 ||
      next >= static_cast<int>(space_actions_.size())) {
    return false;
  }
  // A copy: switching rebuilds the row.
  base::RepeatingClosure action = space_actions_[next];
  action.Run();
  return true;
}

bool ArcomiumSpaceSwitcher::CanRotateSpace(bool forward) const {
  const int next = active_space_ + (forward ? 1 : -1);
  return active_space_ >= 0 && next >= 0 &&
         next < static_cast<int>(space_actions_.size());
}

std::optional<ArcomiumSpaceRef> ArcomiumSpaceSwitcher::GetNeighborSpace(
    bool forward) const {
  if (!CanRotateSpace(forward)) {
    return std::nullopt;
  }
  return space_refs_[active_space_ + (forward ? 1 : -1)];
}

void ArcomiumSpaceSwitcher::Rebuild() {
  // Opening one changes the tab strip, which schedules another rebuild.
  if (OpenSavedSpacesHere() || MergeDuplicateSpaces()) {
    return;
  }
  add_button_ = nullptr;
  RemoveAllChildViews();
  TabGroupModel* groups = model_->group_model();
  if (!groups) {
    return;
  }

  std::vector<tab_groups::TabGroupId> spaces = groups->ListTabGroups();
  std::ranges::sort(spaces, {}, [groups](const tab_groups::TabGroupId& id) {
    return groups->GetTabGroup(id)->ListTabs().start();
  });
  const std::optional<tab_groups::TabGroupId> focused =
      model_->GetFocusedGroup();

  AddChildView(std::make_unique<DownloadsButton>(base::BindRepeating(
      [](BrowserWindowInterface* browser) { chrome::ShowDownloads(browser); },
      base::Unretained(browser_.get()))));
  views::View* icons = AddChildView(std::make_unique<views::View>());
  icons->SetLayoutManager(std::make_unique<views::BoxLayout>(
                              views::BoxLayout::Orientation::kHorizontal,
                              gfx::Insets(), kIconGap))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout_->SetFlexForView(icons, 1);

  space_actions_.clear();
  space_refs_.clear();
  menu_bar_spaces_.clear();
  active_space_ = -1;
  auto add_icon = [&](base::RepeatingClosure callback,
                      std::string icon, bool active,
                      const std::u16string& title, std::string space,
                      std::string sync_id) {
    if (active) {
      active_space_ = static_cast<int>(space_actions_.size());
    }
    space_actions_.push_back(callback);
    space_refs_.push_back(ArcomiumSpaceRef{
        space, space == kDefaultSpace ? std::u16string() : title, std::nullopt});
    menu_bar_spaces_.push_back({title, icon, std::nullopt});
    auto* view = icons->AddChildView(std::make_unique<SpaceIcon>(
        views::Button::PressedCallback(std::move(callback)), std::move(icon),
        active, title, std::move(space),
        std::move(sync_id)));
    // Arc for macOS' pill names the Space and its shortcut, "home" ^ 1.
    view->SetTooltipText(std::u16string());
    std::optional<ui::Accelerator> shortcut;
    if (space_actions_.size() <= 9) {
      shortcut = SpaceAccelerator(space_actions_.size() - 1);
    }
    arcomium::SetArcTooltip(view, title, std::u16string(), shortcut);
    // Measured on Arc: a teal theme's book turns #96B6BC.
    std::string color_value;
    if (const bookmarks::BookmarkNode* folder =
            FindSpaceFolder(view->space(), view->sync_id());
        folder &&
        folder->GetMetaInfo(arcomium::kSpaceColorMetaKey, &color_value)) {
      if (const std::optional<arcomium::SpaceColor> space_color =
              arcomium::ParseSpaceColor(color_value);
          space_color && space_color->saturation > 0) {
        color_utils::HSL hsl;
        hsl.h = space_color->hue / 360.0;
        hsl.s = 0.45 * std::clamp(space_color->saturation, 0.0, 1.0);
        hsl.l = 0.66;
        view->SetHoverTint(color_utils::HSLToSkColor(hsl, 255));
        menu_bar_spaces_.back().tint = color_utils::HSLToSkColor(hsl, 255);
      }
    }
    view->GetViewAccessibility().SetName(title);
    view->set_context_menu_controller(this);
  };

  // The tabs outside of any Space form the first, default Space.
  add_icon(base::BindRepeating(
               [](TabStripModel* model) {
                 model->ExitFocusMode(
                     TabGroupFocusExitReason::kActiveTabGroupOperation);
               },
               base::Unretained(model_.get())),
           GetSpaceIcon(kDefaultSpace, std::string()), !focused.has_value(),
           u"home", kDefaultSpace, std::string());
  for (const tab_groups::TabGroupId& id : spaces) {
    const tab_groups::TabGroupVisualData* visuals =
        groups->GetTabGroup(id)->visual_data();
    const std::string space =
        arcomium::GetSpaceOfGroupOrFolder(browser_->GetProfile(), model_, id)
            .value_or(std::string());
    add_icon(base::BindRepeating(&ArcomiumSpaceSwitcher::SwitchTo,
                                 base::Unretained(this), id),
             GetSpaceIcon(space, std::string()), focused == id,
             visuals->title().empty() ? u"Space" : visuals->title(), space,
             std::string());
    space_refs_.back().group = id;
    space_refs_.back().title = visuals->title();
  }
  // Saved Spaces without a tab group here: from other devices, or closed.
  std::set<std::string> shown_saved;
  if (sync_service_) {
    // Only once its storage partition is known: opened before, its tabs would
    // stay out of the Space's partition.
    const std::set<std::string> linked = GetLinkedSavedGroups();
    for (const tab_groups::SavedTabGroup& saved :
         sync_service_->GetAllGroups()) {
      const std::string sync_id = saved.saved_guid().AsLowercaseString();
      if (saved.local_group_id() || saved.is_shared_tab_group() ||
          saved.saved_tabs().empty() || !linked.contains(sync_id)) {
        continue;
      }
      shown_saved.insert(sync_id);
      add_icon(base::BindRepeating(&ArcomiumSpaceSwitcher::OpenSavedSpace,
                                   base::Unretained(this), saved.saved_guid()),
               GetSpaceIcon(std::string(), sync_id), /*active=*/false,
               saved.title().empty() ? u"Space" : saved.title(), std::string(),
               sync_id);
    }
  }
  // Spaces whose tabs all closed, and with them their tab group: their
  // bookmark folder is left, and they open from it again, as in Arc, where a
  // Space stays until it is deleted.
  if (const bookmarks::BookmarkModel* bookmarks =
          BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
      bookmarks && bookmarks->loaded()) {
    std::set<std::string> open_spaces;
    for (const tab_groups::TabGroupId& id : spaces) {
      if (std::optional<std::string> space =
              arcomium::GetSpaceOfGroupOrFolder(browser_->GetProfile(),
                                                model_, id)) {
        open_spaces.insert(*space);
      }
    }
    for (const auto& folder : bookmarks->other_node()->children()) {
      std::string space;
      std::string saved;
      if (!folder->is_folder() ||
          !folder->GetMetaInfo(kSpaceMetaKey, &space) || space.empty() ||
          space == kDefaultSpace || space.starts_with("arc:") ||
          open_spaces.contains(space) ||
          shown_saved.contains(
              arcomium::SavedGroupGuidForSpace(space).AsLowercaseString()) ||
          (folder->GetMetaInfo(kSavedGroupMetaKey, &saved) &&
           shown_saved.contains(saved))) {
        continue;
      }
      add_icon(base::BindRepeating(&ArcomiumSpaceSwitcher::OpenSpace,
                                   base::Unretained(browser_.get()), space,
                                   folder->GetTitle()),
               GetSpaceIcon(space, std::string()), /*active=*/false,
               folder->GetTitle().empty() ? u"Space" : folder->GetTitle(),
               space, std::string());
    }
  }
  LinkSpaceFolders();
  ApplyActiveSpaceColor();

  auto add = std::make_unique<AddButton>(base::BindRepeating(
      &ArcomiumSpaceSwitcher::ShowAddMenu, base::Unretained(this)));
  add->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_SPACE));
  add_button_ = AddChildView(std::move(add));
  add_button_->set_context_menu_controller(this);

  PreferredSizeChanged();
#if BUILDFLAG(IS_MAC)
  // The window is named after the shown Space (BrowserView::GetWindowTitle).
  if (GetWidget()) {
    GetWidget()->UpdateWindowTitle();
  }
#endif
}

const bookmarks::BookmarkNode* ArcomiumSpaceSwitcher::FindSpaceFolder(
    const std::string& space,
    const std::string& sync_id) const {
  const bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (!bookmarks || !bookmarks->loaded()) {
    return nullptr;
  }
  for (const auto& folder : bookmarks->other_node()->children()) {
    std::string value;
    if (!folder->is_folder()) {
      continue;
    }
    if (!folder->GetMetaInfo(kSpaceMetaKey, &value)) {
      continue;
    }
    if ((!space.empty() && value == space) ||
        (!sync_id.empty() && arcomium::IsGroupSpace(value) &&
         arcomium::SavedGroupGuidForSpace(value).AsLowercaseString() ==
             sync_id)) {
      return folder.get();
    }
    if (!sync_id.empty() && folder->GetMetaInfo(kSavedGroupMetaKey, &value) &&
        value == sync_id) {
      return folder.get();
    }
  }
  return nullptr;
}

std::string ArcomiumSpaceSwitcher::GetSpaceIcon(
    const std::string& space,
    const std::string& sync_id) const {
  return arcomium::GetSpaceIconName(FindSpaceFolder(space, sync_id), space);
}

void ArcomiumSpaceSwitcher::AddedToWidget() {
  ApplyActiveSpaceColor();
  // Alt+1 to Alt+9 show the Spaces in the row's order, home first, as in Arc
  // for Windows; Control+1 to Control+9 on macOS, as in Arc there (Option
  // with a digit types a character).
  for (size_t i = 0; i < 9; ++i) {
    AddAccelerator(SpaceAccelerator(i));
  }
}

bool ArcomiumSpaceSwitcher::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  const int index = accelerator.key_code() - ui::VKEY_1;
  if (index < 0 || index > 8 ||
      accelerator != SpaceAccelerator(static_cast<size_t>(index)) ||
      index >= static_cast<int>(space_actions_.size())) {
    return false;
  }
  if (index != active_space_) {
    // A copy: switching rebuilds the row and its callbacks.
    base::RepeatingClosure action = space_actions_[index];
    action.Run();
  }
  return true;
}

void ArcomiumSpaceSwitcher::ApplyActiveSpaceColor() {
  if (!GetWidget()) {
    return;
  }
  std::string space = kDefaultSpace;
  if (std::optional<tab_groups::TabGroupId> focused =
          model_->GetFocusedGroup()) {
    space = arcomium::GetSpaceOfGroupOrFolder(browser_->GetProfile(), model_,
                                              *focused)
                .value_or(std::string());
  }
  std::optional<arcomium::SpaceColor> color;
  std::string value;
  if (const bookmarks::BookmarkNode* folder =
          space.empty() ? nullptr : FindSpaceFolder(space, std::string());
      folder && folder->GetMetaInfo(arcomium::kSpaceColorMetaKey, &value)) {
    color = arcomium::ParseSpaceColor(value);
  }
  // The first color a window gets is set at once; a switch of Space after it
  // blends into the next Space's color.
  const bool animate = !!color_transition_;
  if (!color_transition_) {
    color_transition_ = std::make_unique<arcomium::SpaceColorTransition>();
  }
  color_transition_->AnimateTo(GetWidget(), color, animate);
}

void ArcomiumSpaceSwitcher::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  // As in Arc, "+" offers the shown Space's menu too.
  if (source == add_button_) {
    ShowActiveSpaceMenu(point, source_type);
    return;
  }
  if (const SpaceIcon* icon = views::AsViewClass<SpaceIcon>(source)) {
    ShowSpaceMenu(icon->space(), icon->sync_id(), point, source_type);
  }
}

void ArcomiumSpaceSwitcher::ShowActiveSpaceMenu(
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (active_space_ < 0 ||
      active_space_ >= static_cast<int>(space_refs_.size())) {
    return;
  }
  ShowSpaceMenu(space_refs_[active_space_].space, std::string(), point,
                source_type);
}

void ArcomiumSpaceSwitcher::ShowSpaceMenu(
    const std::string& space,
    const std::string& sync_id,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (!GetWidget()) {
    return;
  }
  // The previous menu's runner refers to the models replaced below.
  menu_runner_.reset();
  menu_space_ = space;
  menu_sync_id_ = sync_id;
  menu_point_ = point;
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  // Arc's Space menu, wherever it opens.
  // Change Space Icon... opens Arc's picker. Arc for macOS' Set Profile,
  // Live Folders, Share, Export and Manage Spaces are left out (Arc's
  // account and services), but not the separators around them.
  menu_model_->AddItem(kChangeIconCommand,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_CHANGE_SPACE_ICON));
  menu_model_->AddItem(kRenameSpaceCommand,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_RENAME_SPACE));
  menu_model_->AddItem(kEditColorCommand,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_EDIT_THEME_COLOR));
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_->AddItem(kNewFolderCommand,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_FOLDER));
  // Collapse All Folders while one is open, else Expand All Folders; dimmed
  // without folders.
  const FolderState folders =
      GetFolderState(FindSpaceFolder(menu_space_, menu_sync_id_));
  menu_model_->AddItem(
      kToggleFoldersCommand,
      l10n_util::GetStringUTF16(folders.any_open
                                    ? IDS_ARCOMIUM_COLLAPSE_ALL_FOLDERS
                                    : IDS_ARCOMIUM_EXPAND_ALL_FOLDERS));
  menu_model_->SetEnabledAt(menu_model_->GetItemCount() - 1, folders.any);
  // home is where tabs outside any Space go; it cannot be deleted, so the
  // item Arc always has is dimmed there.
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_->AddItem(kDeleteSpaceCommand,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_DELETE_SPACE));
  menu_model_->SetEnabledAt(menu_model_->GetItemCount() - 1,
                            menu_space_ != kDefaultSpace);
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kBubbleTopRight,
                          source_type);
}

void ArcomiumSpaceSwitcher::ShowAddMenu(const ui::Event& event) {
  views::View* button = add_button_;
  if (!button || !button->GetWidget()) {
    return;
  }
  // Arc for macOS' popover over its "+": New Space, New Folder | New Split,
  // New Tab. Its Easel and Boost rows are Arc's own features, left out.
  auto* add = static_cast<AddButton*>(button);
  if (arcomium::ArcPopoverJustClosed(add)) {
    return;
  }
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser_);
  auto shortcut = [view](int command) -> std::optional<ui::Accelerator> {
    ui::Accelerator accelerator;
    if (view && view->GetAccelerator(command, &accelerator)) {
      return accelerator;
    }
    return std::nullopt;
  };
  auto run = [](base::WeakPtr<ArcomiumSpaceSwitcher> switcher, int command) {
    if (switcher) {
      switcher->ExecuteCommand(command, 0);
    }
  };
  // As in Arc, New Split is dimmed on an empty Space.
  const tabs::TabInterface* tab = model_->GetActiveTab();
  const bool page = tab && tab->GetContents() &&
                    tab->GetContents()->GetVisibleURL().ref() !=
                        "arcomium-space";
  std::vector<arcomium::PopoverItem> items;
  items.emplace_back(arcomium::PopoverIcon::kNewSpace,
                     l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_SPACE),
                     std::nullopt, true,
                     base::BindOnce(run, weak_factory_.GetWeakPtr(),
                                    kNewSpaceCommand));
  items.emplace_back(arcomium::PopoverIcon::kNewFolder,
                     l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_FOLDER),
                     std::nullopt, true,
                     base::BindOnce(run, weak_factory_.GetWeakPtr(),
                                    kNewFolderCommand));
  items.emplace_back();
  items.emplace_back(arcomium::PopoverIcon::kNewSplit,
                     l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_SPLIT),
                     shortcut(IDC_NEW_SPLIT_TAB),
                     page && chrome::IsCommandEnabled(browser_,
                                                      IDC_NEW_SPLIT_TAB),
                     base::BindOnce(run, weak_factory_.GetWeakPtr(),
                                    kNewSplitCommand));
  items.emplace_back(arcomium::PopoverIcon::kNewTab,
                     l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_TAB),
                     shortcut(IDC_NEW_TAB), true,
                     base::BindOnce(run, weak_factory_.GetWeakPtr(),
                                    kNewTabCommand));
  add->SetOpen(true);
  arcomium::ShowArcPopover(add, std::move(items), /*above=*/true,
                           kAddPopoverWidth, add->GetClosedCallback());
}

void ArcomiumSpaceSwitcher::MenuClosed(ui::SimpleMenuModel* source) {
  // The context menu took the pointer's events: the button it opened on
  // never saw the pointer leave and would keep its hover plate.
  std::vector<views::View*> views = {this};
  while (!views.empty()) {
    views::View* view = views.back();
    views.pop_back();
    if (auto* button = views::AsViewClass<views::Button>(view)) {
      button->SetState(button->IsMouseHovered() ? views::Button::STATE_HOVERED
                                                : views::Button::STATE_NORMAL);
      button->SchedulePaint();
    }
    for (views::View* child : view->children()) {
      views.push_back(child);
    }
  }
}

void ArcomiumSpaceSwitcher::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == kNewSpaceCommand) {
    CreateSpace(browser_);
    return;
  }
  if (command_id == kNewTabCommand) {
    chrome::ExecuteCommand(browser_, IDC_NEW_TAB);
    return;
  }
  if (command_id == kNewSplitCommand) {
    chrome::ExecuteCommand(browser_, IDC_NEW_SPLIT_TAB);
    return;
  }
  if (command_id == kNewFolderCommand) {
    // The sidebar beside this row, in the same window.
    views::View* root = GetWidget() ? GetWidget()->GetRootView() : nullptr;
    std::vector<views::View*> views;
    if (root) {
      views.push_back(root);
    }
    while (!views.empty()) {
      views::View* view = views.back();
      views.pop_back();
      if (auto* sidebar = views::AsViewClass<ArcomiumSidebarView>(view)) {
        sidebar->NewFolder();
        return;
      }
      for (views::View* child : view->children()) {
        views.push_back(child);
      }
    }
    return;
  }
  if (command_id == kEditColorCommand) {
    EditMenuSpaceColor();
    return;
  }
  if (command_id == kDeleteSpaceCommand) {
    ConfirmDeleteMenuSpace();
    return;
  }
  if (command_id == kRenameSpaceCommand) {
    RenameMenuSpace();
    return;
  }
  if (command_id == kChangeIconCommand) {
    ShowMenuSpaceIconPicker();
    return;
  }
  if (command_id == kToggleFoldersCommand) {
    ToggleMenuSpaceFolders();
  }
}

void ArcomiumSpaceSwitcher::ToggleMenuSpaceFolders() {
  const bookmarks::BookmarkNode* space =
      FindSpaceFolder(menu_space_, menu_sync_id_);
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (!space || !bookmarks) {
    return;
  }
  // As the menu said: closes them all while one is open, else opens all.
  const std::string value = GetFolderState(space).any_open ? "0" : "1";
  std::vector<const bookmarks::BookmarkNode*> folders = {space};
  while (!folders.empty()) {
    const bookmarks::BookmarkNode* folder = folders.back();
    folders.pop_back();
    for (const auto& child : folder->children()) {
      if (!child->is_folder()) {
        continue;
      }
      folders.push_back(child.get());
      std::string open;
      child->GetMetaInfo(kFolderOpenMetaKey, &open);
      if ((open == "1") != (value == "1")) {
        bookmarks->SetNodeMetaInfo(child.get(), kFolderOpenMetaKey, value);
      }
    }
  }
}

std::optional<tab_groups::TabGroupId> ArcomiumSpaceSwitcher::FindGroupOfSpace(
    const std::string& space) const {
  if (space.empty() || !model_->group_model()) {
    return std::nullopt;
  }
  for (const tab_groups::TabGroupId& id :
       model_->group_model()->ListTabGroups()) {
    if (arcomium::GetSpaceOfGroupOrFolder(browser_->GetProfile(), model_, id) ==
        space) {
      return id;
    }
  }
  return std::nullopt;
}

void ArcomiumSpaceSwitcher::ActivateMenuSpace() {
  if (menu_space_ == kDefaultSpace) {
    model_->ExitFocusMode(TabGroupFocusExitReason::kActiveTabGroupOperation);
    return;
  }
  if (std::optional<tab_groups::TabGroupId> group =
          FindGroupOfSpace(menu_space_)) {
    SwitchTo(*group);
    return;
  }
  if (!menu_sync_id_.empty()) {
    OpenSavedSpace(base::Uuid::ParseLowercase(menu_sync_id_));
    return;
  }
  if (const bookmarks::BookmarkNode* folder =
          FindSpaceFolder(menu_space_, std::string())) {
    OpenSpace(browser_, menu_space_, folder->GetTitle());
  }
}

void ArcomiumSpaceSwitcher::EditMenuSpaceColor() {
  ActivateMenuSpace();
  // Once the sidebar shows the Space: the picker edits the active one.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<ArcomiumSpaceSwitcher> switcher) {
                       if (!switcher || !switcher->GetWidget()) {
                         return;
                       }
                       // The sidebar is in the same window.
                       std::vector<views::View*> views = {
                           switcher->GetWidget()->GetRootView()};
                       while (!views.empty()) {
                         views::View* view = views.back();
                         views.pop_back();
                         if (auto* sidebar =
                                 views::AsViewClass<ArcomiumSidebarView>(
                                     view)) {
                           sidebar->EditSpaceColorAt(switcher->menu_point_);
                           return;
                         }
                         for (views::View* child : view->children()) {
                           views.push_back(child);
                         }
                       }
                     },
                     weak_factory_.GetWeakPtr()));
}

const bookmarks::BookmarkNode*
ArcomiumSpaceSwitcher::GetOrAddMenuSpaceFolder() {
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (!bookmarks || !bookmarks->loaded()) {
    return nullptr;
  }
  const bookmarks::BookmarkNode* folder =
      FindSpaceFolder(menu_space_, menu_sync_id_);
  if (!folder && !menu_space_.empty()) {
    // The Space has no folder yet: home before anything was pinned to it.
    bookmarks::BookmarkNode::MetaInfoMap meta;
    meta[kSpaceMetaKey] = menu_space_;
    std::u16string title = u"Space";
    if (menu_space_ == kDefaultSpace) {
      title = u"home";
    } else if (std::optional<tab_groups::TabGroupId> group =
                   FindGroupOfSpace(menu_space_);
               group && !model_->group_model()
                             ->GetTabGroup(*group)
                             ->visual_data()
                             ->title()
                             .empty()) {
      title = model_->group_model()->GetTabGroup(*group)->visual_data()->title();
    }
    folder = bookmarks->AddFolder(bookmarks->other_node(),
                                  bookmarks->other_node()->children().size(),
                                  title, &meta);
  }
  return folder;
}

void ArcomiumSpaceSwitcher::ShowMenuSpaceIconPicker() {
  const bookmarks::BookmarkNode* folder = GetOrAddMenuSpaceFolder();
  if (!folder || !GetWidget()) {
    return;
  }
  // As in Arc, under the Space's name in the sidebar when that is the shown
  // Space; else over the Space's button in the bar, else where the menu was.
  gfx::Rect anchor(menu_point_, gfx::Size());
  int arrow_x = menu_point_.x();
  auto sidebar = std::make_unique<views::ViewTracker>();
  const bool shown = active_space_ >= 0 &&
                     active_space_ < static_cast<int>(space_refs_.size()) &&
                     space_refs_[active_space_].space == menu_space_;
  ArcomiumSidebarView* sidebar_view = FindSidebar(GetWidget());
  if (shown && sidebar_view &&
      sidebar_view->GetSpaceTitleAnchor(&anchor, &arrow_x)) {
    // Arc does not light the name for its picker, nor shows its tooltip.
    sidebar->SetView(sidebar_view);
    sidebar_view->SetSpaceTitleHighlighted(true);
  } else {
    std::vector<views::View*> views = {this};
    while (!views.empty()) {
      views::View* view = views.back();
      views.pop_back();
      if (const auto* icon = views::AsViewClass<SpaceIcon>(view);
          icon && (menu_space_.empty() ? icon->sync_id() == menu_sync_id_
                                       : icon->space() == menu_space_)) {
        anchor = icon->GetBoundsInScreen();
        arrow_x = anchor.CenterPoint().x();
        break;
      }
      for (views::View* child : view->children()) {
        views.push_back(child);
      }
    }
  }
  arcomium::ShowArcIconPicker(
      this, anchor, arrow_x, arcomium::IconPickerKind::kSpace,
      arcomium::GetSpaceIconName(folder, menu_space_),
      base::BindOnce(
          [](base::WeakPtr<ArcomiumSpaceSwitcher> switcher, int64_t id,
             const std::string& name) {
            if (!switcher) {
              return;
            }
            bookmarks::BookmarkModel* bookmarks =
                BookmarkModelFactory::GetForBrowserContext(
                    switcher->browser_->GetProfile());
            const bookmarks::BookmarkNode* node =
                bookmarks ? bookmarks::GetBookmarkNodeByID(bookmarks, id)
                          : nullptr;
            if (!node) {
              return;
            }
            // The trash: home goes back to its sun, others to the dot.
            if (name.empty()) {
              bookmarks->DeleteNodeMetaInfo(node, kSpaceIconMetaKey);
            } else {
              bookmarks->SetNodeMetaInfo(node, kSpaceIconMetaKey, name);
            }
          },
          weak_factory_.GetWeakPtr(), folder->id()),
      base::BindOnce(
          [](std::unique_ptr<views::ViewTracker> sidebar) {
            if (auto* view =
                    views::AsViewClass<ArcomiumSidebarView>(sidebar->view())) {
              view->SetSpaceTitleHighlighted(false);
            }
          },
          std::move(sidebar)));
}

void ArcomiumSpaceSwitcher::RenameMenuSpace() {
  const bookmarks::BookmarkNode* folder = GetOrAddMenuSpaceFolder();
  if (!folder || !GetWidget()) {
    return;
  }
  // The Space's name is its folder's; its tab group takes it on save
  // (BookmarkNodeChanged).
  renaming_folder_ = folder->id();
  renaming_space_ = menu_space_;
  // Titled for the Space, not Chromium's "Edit folder name"
  // (look/space-rename-title.patch).
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::EditNode(folder);
  details.arcomium_title_id = IDS_ARCOMIUM_SPACE_EDITOR_TITLE;
  BookmarkEditor::Show(GetWidget()->GetNativeWindow(), browser_->GetProfile(),
                       details, BookmarkEditor::NO_TREE);
}

void ArcomiumSpaceSwitcher::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  if (node->id() == renaming_folder_) {
    if (std::optional<tab_groups::TabGroupId> group =
            FindGroupOfSpace(renaming_space_)) {
      const tab_groups::TabGroupVisualData* visuals =
          model_->group_model()->GetTabGroup(*group)->visual_data();
      model_->ChangeTabGroupVisuals(
          *group, tab_groups::TabGroupVisualData(node->GetTitle(),
                                                 visuals->color(),
                                                 visuals->is_collapsed()));
    }
  }
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::ConfirmDeleteMenuSpace() {
  if (menu_space_ == kDefaultSpace || !GetWidget()) {
    return;
  }
  std::u16string body =
      l10n_util::GetStringUTF16(IDS_ARCOMIUM_DELETE_UNNAMED_SPACE_BODY);
  if (const bookmarks::BookmarkNode* folder =
          FindSpaceFolder(menu_space_, menu_sync_id_);
      folder && !folder->GetTitle().empty()) {
    body = l10n_util::GetStringFUTF16(IDS_ARCOMIUM_DELETE_SPACE_BODY,
                                      folder->GetTitle());
  }
  chrome::ShowQuestionMessageBoxAsync(
      GetWidget()->GetNativeWindow(),
      l10n_util::GetStringUTF16(IDS_ARCOMIUM_DELETE_SPACE_TITLE), body,
      base::BindOnce(&ArcomiumSpaceSwitcher::DeleteSpace,
                     weak_factory_.GetWeakPtr(), menu_space_, menu_sync_id_));
}

void ArcomiumSpaceSwitcher::DeleteSpace(const std::string& space,
                                        const std::string& sync_id,
                                        chrome::MessageBoxResult result) {
  if (result != chrome::MESSAGE_BOX_RESULT_YES || space == kDefaultSpace) {
    return;
  }
  // Found before the folder goes: an open Space is known by its folder too.
  const bookmarks::BookmarkNode* folder = FindSpaceFolder(space, sync_id);
  const int64_t folder_id = folder ? folder->id() : 0;
  std::optional<base::Uuid> saved;
  if (!sync_id.empty()) {
    saved = base::Uuid::ParseLowercase(sync_id);
  }
  if (std::optional<tab_groups::TabGroupId> group = FindGroupOfSpace(space)) {
    if (sync_service_) {
      if (std::optional<tab_groups::SavedTabGroup> saved_group =
              sync_service_->GetGroup(*group)) {
        saved = saved_group->saved_guid();
      }
    }
    model_->CloseAllTabsInGroup(*group);
  }
  if (saved && saved->is_valid() && sync_service_ &&
      sync_service_->GetGroup(*saved)) {
    sync_service_->RemoveGroup(*saved);
  }
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (bookmarks && bookmarks->loaded() && folder_id) {
    if (const bookmarks::BookmarkNode* node =
            bookmarks::GetBookmarkNodeByID(bookmarks, folder_id)) {
      bookmarks->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                        FROM_HERE);
    }
  }
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::SwitchTo(const tab_groups::TabGroupId& group) {
  if (model_->group_model() && model_->group_model()->ContainsTabGroup(group)) {
    model_->EnterFocusMode(group);
  }
}

void ArcomiumSpaceSwitcher::OpenSavedSpace(const base::Uuid& sync_id) {
  if (!sync_service_) {
    return;
  }
  std::optional<tab_groups::TabGroupId> group = sync_service_->OpenTabGroup(
      sync_id, std::make_unique<tab_groups::TabGroupActionContextDesktop>(
                   browser_, tab_groups::OpeningSource::kOpenedFromRevisitUi));
  if (group) {
    SwitchTo(*group);
  }
}

bool ArcomiumSpaceSwitcher::OpenSavedSpacesHere() {
  if (!sync_service_ || !sync_initialized_ || !model_->group_model() ||
      model_->closing_all()) {
    return false;
  }
  const bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (SessionRestore::IsRestoring(browser_->GetProfile()) || !bookmarks ||
      !bookmarks->loaded()) {
    // Restore brings back the Spaces open before, and the Spaces' folders
    // tell which saved groups are Spaces: asked again after both. Not left to
    // the bookmark model's load notification, which may come first.
    RetryRebuildLater();
    return false;
  }
  // Each saved Space is looked at once: one open here already, or one this
  // window opened, is not opened again when its group closes, as all do
  // when the window closes. Only once its storage partition is known, as for
  // the switcher's icons.
  const std::set<std::string> linked = GetLinkedSavedGroups();
  std::vector<base::Uuid> to_open;
  for (const tab_groups::SavedTabGroup& saved : sync_service_->GetAllGroups()) {
    const std::string sync_id = saved.saved_guid().AsLowercaseString();
    if (saved.is_shared_tab_group() || saved.saved_tabs().empty() ||
        !linked.contains(sync_id) || !opened_saved_.insert(sync_id).second ||
        saved.local_group_id()) {
      continue;
    }
    to_open.push_back(saved.saved_guid());
  }
  if (to_open.empty()) {
    return false;
  }
  // Opening a saved group leaves the focused one and activates its tabs; the
  // Space and tab shown stay as they were.
  const std::optional<tab_groups::TabGroupId> focused =
      model_->GetFocusedGroup();
  tabs::TabInterface* const active = model_->GetActiveTab();
  for (const base::Uuid& sync_id : to_open) {
    sync_service_->OpenTabGroup(
        sync_id, std::make_unique<tab_groups::TabGroupActionContextDesktop>(
                     browser_, tab_groups::OpeningSource::kOpenedFromRevisitUi));
  }
  if (focused && model_->group_model()->ContainsTabGroup(*focused)) {
    model_->EnterFocusMode(*focused);
  } else if (!focused && model_->GetFocusedGroup()) {
    model_->ExitFocusMode(TabGroupFocusExitReason::kActiveTabGroupOperation);
  }
  if (active) {
    if (const int index = model_->GetIndexOfTab(active);
        index != TabStripModel::kNoTab && index != model_->active_index()) {
      model_->ActivateTabAt(index);
    }
  }
  return true;
}

bool ArcomiumSpaceSwitcher::MergeDuplicateSpaces() {
  // Once per profile and run, once session restore has opened every window:
  // before that, groups still missing would look like nothing to merge, and
  // deleting a group deletes it on every device.
  static base::NoDestructor<std::set<const Profile*>> merged;
  Profile* const profile = browser_->GetProfile();
  TabGroupModel* const groups = model_->group_model();
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(profile);
  if (!sync_service_ || !sync_initialized_ || !groups || !bookmarks ||
      merged->contains(profile)) {
    return false;
  }
  if (SessionRestore::IsRestoring(profile) || !bookmarks->loaded()) {
    // Asked again once restore is over and the folders are known: nothing
    // else may rebuild the row.
    RetryRebuildLater();
    return false;
  }
  merged->insert(profile);

  bool changed = false;
  for (const auto& folder : bookmarks->other_node()->children()) {
    std::string space;
    if (!folder->is_folder() || !folder->GetMetaInfo(kSpaceMetaKey, &space) ||
        !arcomium::IsGroupSpace(space)) {
      continue;
    }
    const base::Uuid canonical = arcomium::SavedGroupGuidForSpace(space);
    // The Space's groups here: by their saved group's id, or by the
    // partition of their tabs for a group no folder names.
    std::vector<tab_groups::TabGroupId> mine;
    for (const tab_groups::TabGroupId& id : groups->ListTabGroups()) {
      const std::optional<tab_groups::SavedTabGroup> saved =
          sync_service_->GetGroup(id);
      if (!saved) {
        continue;
      }
      std::optional<std::string> owner =
          arcomium::GetSpaceOfSavedGroup(profile, saved->saved_guid());
      if (!owner) {
        owner = arcomium::GetSpaceOfGroup(model_, id);
      }
      if (owner == space) {
        mine.push_back(id);
      }
    }
    if (mine.empty() ||
        (mine.size() == 1 &&
         sync_service_->GetGroup(mine[0])->saved_guid() == canonical)) {
      continue;
    }
    LOG(WARNING) << "Arcomium: merging " << mine.size()
                 << " tab groups of Space " << space;

    // The group that stays: the one with the derived id, or else the largest,
    // saved again under the derived id.
    std::optional<tab_groups::TabGroupId> kept;
    for (const tab_groups::TabGroupId& id : mine) {
      if (sync_service_->GetGroup(id)->saved_guid() == canonical) {
        kept = id;
      }
    }
    if (!kept) {
      if (const std::optional<tab_groups::SavedTabGroup> derived =
              sync_service_->GetGroup(canonical);
          derived && !derived->saved_tabs().empty()) {
        // Saved elsewhere with tabs but not open here: OpenSavedSpacesHere()
        // opens such a group before this runs, unless it was closed here.
        continue;
      }
      // Saved again under the derived id. If an empty group has that id
      // already, the tabs join it (TabGroupSyncServiceImpl::AddGroup()).
      kept = *std::ranges::max_element(
          mine, {}, [groups](const tab_groups::TabGroupId& id) {
            return groups->GetTabGroup(id)->tab_count();
          });
      sync_service_->UnsaveGroup(*kept);
      sync_service_->SaveGroup(
          tab_groups::SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
              *kept));
    }

    // Tabs of the other groups move into it, but for pages it has already;
    // their saved groups go.
    std::set<GURL> urls;
    const gfx::Range kept_tabs = groups->GetTabGroup(*kept)->ListTabs();
    for (size_t i = kept_tabs.start(); i < kept_tabs.end(); ++i) {
      urls.insert(
          model_->GetWebContentsAt(static_cast<int>(i))->GetVisibleURL());
    }
    for (const tab_groups::TabGroupId& id : mine) {
      if (id == *kept) {
        continue;
      }
      const base::Uuid old_guid = sync_service_->GetGroup(id)->saved_guid();
      std::vector<tabs::TabInterface*> tabs;
      const gfx::Range range = groups->GetTabGroup(id)->ListTabs();
      for (size_t i = range.start(); i < range.end(); ++i) {
        tabs.push_back(model_->GetTabAtIndex(static_cast<int>(i)));
      }
      for (tabs::TabInterface* tab : tabs) {
        const int index = model_->GetIndexOfTab(tab);
        if (index == TabStripModel::kNoTab) {
          continue;
        }
        content::WebContents* contents = tab->GetContents();
        if (TabStripModel::IsArcomiumEmptySpace(contents) ||
            !urls.insert(contents->GetVisibleURL()).second) {
          model_->CloseWebContentsAt(index, TabCloseTypes::CLOSE_NONE);
        } else {
          model_->AddToExistingGroup({index}, *kept, /*add_to_end=*/true);
        }
      }
      if (sync_service_->GetGroup(old_guid)) {
        sync_service_->RemoveGroup(old_guid);
      }
    }
    changed = true;
  }
  return changed;
}

std::set<std::string> ArcomiumSpaceSwitcher::GetLinkedSavedGroups() const {
  std::set<std::string> linked;
  const bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (!bookmarks || !bookmarks->loaded()) {
    return linked;
  }
  // A Space's group has the id derived from it; a folder may still name an
  // older one, until MergeDuplicateSpaces() folds it in.
  for (const auto& folder : bookmarks->other_node()->children()) {
    std::string space;
    if (!folder->is_folder() || !folder->GetMetaInfo(kSpaceMetaKey, &space) ||
        !arcomium::IsGroupSpace(space)) {
      continue;
    }
    linked.insert(arcomium::SavedGroupGuidForSpace(space).AsLowercaseString());
    std::string group;
    if (folder->GetMetaInfo(kSavedGroupMetaKey, &group)) {
      linked.insert(group);
    }
  }
  return linked;
}

void ArcomiumSpaceSwitcher::LinkSpaceFolders() {
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  TabGroupModel* groups = model_->group_model();
  if (!sync_service_ || !bookmarks || !bookmarks->loaded() || !groups) {
    return;
  }
  for (const tab_groups::TabGroupId& id : groups->ListTabGroups()) {
    const std::optional<std::string> space =
        arcomium::GetSpaceOfGroupOrFolder(browser_->GetProfile(), model_, id);
    const std::optional<tab_groups::SavedTabGroup> saved =
        sync_service_->GetGroup(id);
    if (!space || !saved) {
      continue;
    }
    const bookmarks::BookmarkNode* folder = nullptr;
    for (const auto& child : bookmarks->other_node()->children()) {
      std::string value;
      if (child->is_folder() && child->GetMetaInfo(kSpaceMetaKey, &value) &&
          value == *space) {
        folder = child.get();
        break;
      }
    }
    const std::u16string& title =
        groups->GetTabGroup(id)->visual_data()->title();
    if (!folder) {
      // Adopt a folder imported from Arc for a Space of the same name, as the
      // sidebar does.
      for (const auto& child : bookmarks->other_node()->children()) {
        std::string value;
        if (child->is_folder() && child->GetTitle() == title &&
            child->GetMetaInfo(kSpaceMetaKey, &value) &&
            value.starts_with("arc:")) {
          folder = child.get();
          bookmarks->SetNodeMetaInfo(folder, kSpaceMetaKey, *space);
          break;
        }
      }
    }
    if (!folder) {
      bookmarks::BookmarkNode::MetaInfoMap meta;
      meta[kSpaceMetaKey] = *space;
      bookmarks->AddFolder(bookmarks->other_node(),
                           bookmarks->other_node()->children().size(),
                           title.empty() ? u"Space" : title, &meta);
      continue;
    }
    // The folder names no saved group: the group's id derives from the Space
    // (arcomium::SavedGroupGuidForSpace()). Devices writing their own group
    // here took the link from one another and made duplicate groups.
  }
}

BEGIN_METADATA(ArcomiumSpaceSwitcher)
END_METADATA
