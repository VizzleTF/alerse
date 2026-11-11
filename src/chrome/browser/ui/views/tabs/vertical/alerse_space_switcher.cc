// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/alerse_space_switcher.h"

#include "chrome/browser/ui/views/tabs/vertical/alerse_glyphs.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_popover.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_space_color.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_tooltip.h"

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
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/tabs/alerse_space_folder.h"
#include "chrome/browser/ui/tabs/alerse_space_list.h"
#include "chrome/browser/ui/tabs/alerse_space_registry.h"
#include "chrome/browser/ui/tabs/alerse_window_stack.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/browser_commands.h"
#include "ui/events/event.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/tabs/alerse_space_command_hook.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/alerse_frame_color.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/gfx/image/image_skia.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_sidebar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/tabs/public/tab_interface.h"
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
// The menu bar's Spaces menu's icons, about 12.5 pt as Arc's, not the
// sidebar's 14.5.
constexpr float kMenuIconScale = 0.87f;
constexpr float kHoverPlateRadius = 8;
constexpr SkColor kHoverPlate = SkColorSetARGB(0x11, 0xFF, 0xFF, 0xFF);
constexpr int kDotDiameter = 6;
constexpr int kSpaceIconSize = 16;
// The colors (chrome/browser/ui/color/alerse_color_mixer.cc).
constexpr ui::ColorId kActiveIconColor = kColorAlerseSpaceIconActive;
constexpr ui::ColorId kHoveredIconColor = kColorAlerseSpaceIconHovered;
constexpr ui::ColorId kIconColor = kColorAlerseSpaceIcon;
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
// A folder's open state (alerse_sidebar_view.cc).
constexpr char kFolderOpenMetaKey[] = "arcomium.open";
#if BUILDFLAG(IS_MAC)
constexpr int kAddPopoverWidth = 210;
#else
// Room for Windows' spelled-out shortcuts, "Ctrl+Shift+=".
constexpr int kAddPopoverWidth = 250;
#endif

// The icons a Space can have, under the names Arc gives them.
struct SpaceIconChoice {
  const char* name;
  int label;
  // Null for the icons drawn here.
  const gfx::VectorIcon* icon;
};

const SpaceIconChoice kSpaceIcons[] = {
    {"sunny", IDS_ALERSE_SPACE_ICON_SUN, nullptr},
    {"book", IDS_ALERSE_SPACE_ICON_BOOK, &kMenuBookIcon},
    {"home", IDS_ALERSE_SPACE_ICON_HOME, &vector_icons::kHomeIcon},
    {"code", IDS_ALERSE_SPACE_ICON_CODE, &vector_icons::kCodeIcon},
    {"globe", IDS_ALERSE_SPACE_ICON_GLOBE, &vector_icons::kGlobeIcon},
    {"bolt", IDS_ALERSE_SPACE_ICON_BOLT, &kBoltIcon},
    {"star", IDS_ALERSE_SPACE_ICON_STAR, &kStarIcon},
    {"chat", IDS_ALERSE_SPACE_ICON_CHAT, &vector_icons::kChatIcon},
    {"mail", IDS_ALERSE_SPACE_ICON_MAIL, &vector_icons::kMailIcon},
    {"lightbulb", IDS_ALERSE_SPACE_ICON_IDEA, &vector_icons::kLightbulbIcon},
    {"rocket", IDS_ALERSE_SPACE_ICON_ROCKET, &kRocketLaunchIcon},
    {"pets", IDS_ALERSE_SPACE_ICON_PETS, &vector_icons::kPetsIcon},
    {"flight", IDS_ALERSE_SPACE_ICON_TRAVEL, &vector_icons::kFlightIcon},
    {"dot", IDS_ALERSE_SPACE_ICON_DOT, nullptr},
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
AlerseSidebarView* FindSidebar(views::Widget* widget) {
  std::vector<views::View*> views;
  if (widget && widget->GetRootView()) {
    views.push_back(widget->GetRootView());
  }
  while (!views.empty()) {
    views::View* view = views.back();
    views.pop_back();
    if (auto* sidebar = views::AsViewClass<AlerseSidebarView>(view)) {
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
  return command_id == IDC_ALERSE_PIN_TAB ||
         command_id == IDC_ALERSE_NEW_FOLDER ||
         command_id == IDC_ALERSE_CLEAR_TODAY ||
         command_id == IDC_ALERSE_TOGGLE_PINNED ||
         command_id == IDC_ALERSE_REVEAL_TAB ||
         command_id == IDC_ALERSE_COPY_URL_QUOTE ||
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
  const SkColor ink = alerse::SidebarInk(view, id);
  return alerse::PaintsInactiveFrame(view)
             ? color_utils::AlphaBlend(ink, alerse::FrameColor(view), 0.5f)
             : ink;
}

void PaintHoverPlate(const views::Button* button, gfx::Canvas* canvas) {
  if (!button->IsMouseHovered()) {
    return;
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(alerse::SidebarInk(button, kHoverPlate));
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
            std::string space)
      : views::Button(std::move(callback)),
        icon_(std::move(icon)),
        active_(active),
        space_(std::move(space)) {
    SetTooltipText(title);
    GetViewAccessibility().SetName(title);
    SetPreferredSize(kButtonBox);
  }

  // kDefaultSpace for the window's own Space, "profile:<directory>" for
  // another one.
  const std::string& space() const { return space_; }

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
    alerse::PaintSpaceIcon(canvas, gfx::PointF(), icon_, ink, emoji_alpha);
  }

  void StateChanged(ButtonState old_state) override { SchedulePaint(); }

 private:
  std::optional<SkColor> hover_tint_;
  const std::string icon_;
  const bool active_;
  const std::string space_;
};

BEGIN_METADATA(SpaceIcon)
END_METADATA

// The Space bar's left button: Downloads (where Arc has its archive, which
// Alerse does not keep). An arrow down into a tray, in the stroke, size
// and grey of the "+" beside it; opens chrome://downloads as
// IDC_SHOW_DOWNLOADS does.
class DownloadsButton : public views::Button {
  METADATA_HEADER(DownloadsButton, views::Button)

 public:
  explicit DownloadsButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    const std::u16string name =
        l10n_util::GetStringUTF16(IDS_ALERSE_DOWNLOADS);
    // Named with its shortcut in Arc for macOS' pill: Shift-Command-J there,
    // Chromium's Ctrl+J on Windows and Linux.
    alerse::SetArcTooltip(
        this, name, std::u16string(),
#if BUILDFLAG(IS_MAC)
        ui::Accelerator(ui::VKEY_J, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN));
#else
        ui::Accelerator(ui::VKEY_J, ui::EF_CONTROL_DOWN));
#endif
    // Kept under the pointer at the window's bottom, as Arc's Library pill.
    alerse::SetArcTooltipBelowPointer(this);
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
      flags.setColor(alerse::SidebarInk(this, SkColorSetRGB(0xA8, 0xA8, 0xA8)));
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

namespace alerse {

// `icon` if a Space can show it, else the sun for home and the dot for the
// other Spaces.
std::string ValidSpaceIcon(const std::string& icon, bool home) {
  if (FindSpaceIcon(icon) || alerse::IsArcGlyph(icon) ||
      alerse::IsEmojiIcon(icon)) {
    return icon;
  }
  return home ? "sunny" : "dot";
}

std::string GetSpaceIconName(const bookmarks::BookmarkNode* folder,
                             const std::string& space) {
  std::string icon;
  if (folder) {
    folder->GetMetaInfo(kSpaceIconMetaKey, &icon);
  }
  return ValidSpaceIcon(icon, space == kDefaultSpace);
}

void PaintSpaceIcon(gfx::Canvas* canvas,
                    const gfx::PointF& center,
                    const std::string& name,
                    SkColor color,
                    uint8_t emoji_alpha) {
  if (alerse::IsEmojiIcon(name)) {
    alerse::PaintEmojiIcon(canvas, center, name, emoji_alpha);
    return;
  }
  // Arc's sun and (on macOS) book are drawn here already; the rest of Arc's
  // set by the glyph painter, and Alerse's first icons as before.
  if (name != "sunny" && name != "book" &&
      alerse::PaintArcGlyph(canvas, center, name, color)) {
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

}  // namespace alerse

AlerseSpaceSwitcher::AlerseSpaceSwitcher(BrowserWindowInterface* browser)
    : browser_(browser) {
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kBarInsets));
  alerse::SpaceCommandHook() = &AlerseSpaceSwitcher::RunSpaceCommand;
  alerse::SpaceCommandStateHook() =
      &AlerseSpaceSwitcher::GetSpaceCommandState;
  alerse::MenuBarListHook() = &AlerseSpaceSwitcher::GetMenuBarList;
  alerse::MenuBarRunHook() = &AlerseSpaceSwitcher::RunMenuBarEntry;
  alerse::ShownSpaceTitleHook() = &AlerseSpaceSwitcher::GetShownSpaceTitle;
  if (bookmarks::BookmarkModel* bookmarks =
          BookmarkModelFactory::GetForBrowserContext(browser->GetProfile())) {
    bookmark_observation_.Observe(bookmarks);
  }
  if (ProfileManager* manager = g_browser_process->profile_manager()) {
    profiles_observation_.Observe(&manager->GetProfileAttributesStorage());
  }
  spaces_subscription_ = alerse::SpacesChangedCallbacks().Add(
      base::BindRepeating(&AlerseSpaceSwitcher::ScheduleRebuild,
                          base::Unretained(this)));
  Rebuild();
}

void AlerseSpaceSwitcher::OnProfileAdded(const base::FilePath& profile_path) {
  ScheduleRebuild();
}

void AlerseSpaceSwitcher::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const std::u16string& profile_name) {
  ScheduleRebuild();
}

void AlerseSpaceSwitcher::ShowSpaceOfPath(const base::FilePath& path) {
  alerse::ShowSpaceInStack(browser_, path);
}

base::FilePath AlerseSpaceSwitcher::PathOfSpace(
    const std::string& space) const {
  constexpr char kPrefix[] = "profile:";
  if (space == kDefaultSpace) {
    return browser_->GetProfile()->GetOriginalProfile()->GetPath();
  }
  ProfileManager* manager = g_browser_process->profile_manager();
  if (!manager || !space.starts_with(kPrefix)) {
    return base::FilePath();
  }
  return manager->user_data_dir().Append(
      base::FilePath::FromUTF8Unsafe(space.substr(strlen(kPrefix))));
}

AlerseSpaceSwitcher::~AlerseSpaceSwitcher() = default;

// static
bool AlerseSpaceSwitcher::RunSpaceCommand(BrowserWindowInterface* browser,
                                            int command_id) {
  if (command_id == IDC_ALERSE_NEW_SPACE) {
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
    if (auto* sidebar = views::AsViewClass<AlerseSidebarView>(view);
        sidebar && sidebar_command) {
      sidebar->RunMenuBarCommand(command_id);
      return true;
    }
    if (auto* switcher = views::AsViewClass<AlerseSpaceSwitcher>(view);
        switcher && !sidebar_command) {
      if (command_id == IDC_ALERSE_RENAME_SPACE ||
          command_id == IDC_ALERSE_EDIT_SPACE_THEME) {
        // As the shown Space's menu does.
        if (switcher->active_space_ < 0 ||
            switcher->active_space_ >=
                static_cast<int>(switcher->space_refs_.size())) {
          return true;
        }
        switcher->menu_space_ =
            switcher->space_refs_[switcher->active_space_].space;
        // No menu was opened: the theme picker opens over the Space bar, not
        // at a stale point (the screen's corner).
        switcher->menu_point_ = switcher->GetBoundsInScreen().origin();
        if (command_id == IDC_ALERSE_RENAME_SPACE) {
          switcher->RenameMenuSpace();
        } else {
          switcher->EditMenuSpaceColor();
        }
        return true;
      }
      const bool forward = command_id == IDC_ALERSE_NEXT_SPACE;
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
alerse::SpaceCommandState AlerseSpaceSwitcher::GetSpaceCommandState(
    BrowserWindowInterface* browser,
    int command_id) {
  using alerse::SpaceCommandState;
  if (browser->GetProfile()->IsOffTheRecord()) {
    return SpaceCommandState::kUnavailable;
  }
  if (command_id == IDC_ALERSE_NEW_SPACE) {
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
    if (auto* sidebar = views::AsViewClass<AlerseSidebarView>(view);
        sidebar && sidebar_command) {
      return sidebar->GetMenuBarCommandState(command_id);
    }
    if (auto* switcher = views::AsViewClass<AlerseSpaceSwitcher>(view);
        switcher && !sidebar_command) {
      if (!switcher->IsDrawn()) {
        return SpaceCommandState::kUnavailable;
      }
      if (command_id == IDC_ALERSE_RENAME_SPACE ||
          command_id == IDC_ALERSE_EDIT_SPACE_THEME) {
        return switcher->active_space_ >= 0 &&
                       switcher->active_space_ <
                           static_cast<int>(switcher->space_refs_.size())
                   ? SpaceCommandState::kAvailable
                   : SpaceCommandState::kUnavailable;
      }
      return switcher->CanRotateSpace(command_id == IDC_ALERSE_NEXT_SPACE)
                 ? SpaceCommandState::kAvailable
                 : SpaceCommandState::kUnavailable;
    }
    for (views::View* child : view->children()) {
      views.push_back(child);
    }
  }
  return SpaceCommandState::kUnavailable;
}

namespace {

AlerseSpaceSwitcher* FindSpaceSwitcherIn(BrowserWindowInterface* browser) {
  std::vector<views::View*> views;
  if (BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser)) {
    views.push_back(browser_view);
  }
  while (!views.empty()) {
    views::View* view = views.back();
    views.pop_back();
    if (auto* switcher = views::AsViewClass<AlerseSpaceSwitcher>(view)) {
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
std::vector<alerse::MenuBarEntry> AlerseSpaceSwitcher::GetMenuBarList(
    BrowserWindowInterface* browser,
    alerse::MenuBarList list) {
  std::vector<alerse::MenuBarEntry> entries;
  if (list == alerse::MenuBarList::kSpaces) {
    AlerseSpaceSwitcher* switcher = FindSpaceSwitcherIn(browser);
    if (!switcher || browser->GetProfile()->IsOffTheRecord()) {
      return entries;
    }
    for (size_t i = 0; i < switcher->menu_bar_spaces_.size(); ++i) {
      const MenuBarSpace& space = switcher->menu_bar_spaces_[i];
      alerse::MenuBarEntry entry;
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
        alerse::PaintSpaceIcon(&canvas, gfx::PointF(), space.icon,
                                 space.tint.value_or(SK_ColorBLACK));
      }
      entry.icon = gfx::ImageSkia::CreateFromBitmap(canvas.GetBitmap(), 2.0f);
      entry.icon_is_template =
          !alerse::IsEmojiIcon(space.icon) && !space.tint;
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
    alerse::MenuBarEntry entry;
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
std::u16string AlerseSpaceSwitcher::GetShownSpaceTitle(
    BrowserWindowInterface* browser) {
  AlerseSpaceSwitcher* switcher = FindSpaceSwitcherIn(browser);
  if (!switcher || browser->GetProfile()->IsOffTheRecord() ||
      switcher->active_space_ < 0 ||
      switcher->active_space_ >=
          static_cast<int>(switcher->menu_bar_spaces_.size())) {
    return std::u16string();
  }
  return switcher->menu_bar_spaces_[switcher->active_space_].title;
}

// static
void AlerseSpaceSwitcher::RunMenuBarEntry(BrowserWindowInterface* browser,
                                            alerse::MenuBarList list,
                                            size_t index) {
  if (list == alerse::MenuBarList::kSpaces) {
    AlerseSpaceSwitcher* switcher = FindSpaceSwitcherIn(browser);
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

// static
void AlerseSpaceSwitcher::ShowActiveSpaceMenuIn(
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
    if (auto* switcher = views::AsViewClass<AlerseSpaceSwitcher>(view)) {
      switcher->ShowActiveSpaceMenu(point, source_type);
      return;
    }
    for (views::View* child : view->children()) {
      views.push_back(child);
    }
  }
}

// static
void AlerseSpaceSwitcher::CreateSpace(BrowserWindowInterface* browser,
                                        const std::u16string& name,
                                        SpaceCreatedCallback created) {
  if (browser->GetProfile()->IsOffTheRecord()) {
    return;
  }
  // A Space is a profile: Chromium makes its directory, and the window's
  // stack shows it once it has loaded.
  const int stack = alerse::WindowStacks::Get().StackOf(browser);
  ProfileManager::CreateMultiProfileAsync(
      name.empty() ? u"Space " + base::NumberToString16(
                                     alerse::ListSpaces().size() + 1)
                   : name,
      profiles::GetPlaceholderAvatarIndex(), /*is_hidden=*/false,
      base::BindOnce(
          [](int stack, base::FilePath from_path, SpaceCreatedCallback created,
             Profile* profile) {
            if (!profile) {
              return;
            }
            if (created) {
              std::move(created).Run(profile);
            }
            alerse::WindowStacks& stacks = alerse::WindowStacks::Get();
            Profile* from_profile =
                g_browser_process->profile_manager()->GetProfileByPath(
                    from_path);
            if (BrowserWindowInterface* from =
                    from_profile ? stacks.FindMember(stack, from_profile)
                                 : nullptr) {
              stacks.ShowSpace(from, profile);
            }
          },
          stack, browser->GetProfile()->GetPath(), std::move(created)));
}

void AlerseSpaceSwitcher::BookmarkModelChanged() {
  ScheduleRebuild();
}

void AlerseSpaceSwitcher::BookmarkMetaInfoChanged(
    const bookmarks::BookmarkNode* node) {
  ScheduleRebuild();
}

void AlerseSpaceSwitcher::BookmarkModelBeingDeleted() {
  bookmark_observation_.Reset();
}

void AlerseSpaceSwitcher::ScheduleRebuild() {
  // Coalesced into one pending rebuild. Its own factory: invalidating the one
  // the delete dialog holds would drop it.
  rebuild_weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AlerseSpaceSwitcher::Rebuild,
                                rebuild_weak_factory_.GetWeakPtr()));
}

bool AlerseSpaceSwitcher::RotateSpace(bool forward) {
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

bool AlerseSpaceSwitcher::CanRotateSpace(bool forward) const {
  const int next = active_space_ + (forward ? 1 : -1);
  return active_space_ >= 0 && next >= 0 &&
         next < static_cast<int>(space_actions_.size());
}

std::optional<AlerseSpaceRef> AlerseSpaceSwitcher::GetNeighborSpace(
    bool forward) const {
  if (!CanRotateSpace(forward)) {
    return std::nullopt;
  }
  return space_refs_[active_space_ + (forward ? 1 : -1)];
}

void AlerseSpaceSwitcher::Rebuild() {
  add_button_ = nullptr;
  RemoveAllChildViews();

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
  auto add_icon = [&](base::RepeatingClosure callback, std::string icon,
                      bool active, const std::u16string& title,
                      std::string space, const std::string& color_value) {
    if (active) {
      active_space_ = static_cast<int>(space_actions_.size());
    }
    space_actions_.push_back(callback);
    space_refs_.push_back(AlerseSpaceRef{
        space, space == kDefaultSpace ? std::u16string() : title});
    menu_bar_spaces_.push_back({title, icon, std::nullopt});
    auto* view = icons->AddChildView(std::make_unique<SpaceIcon>(
        views::Button::PressedCallback(std::move(callback)), std::move(icon),
        active, title, std::move(space)));
    // Arc for macOS' pill names the Space and its shortcut, "home" ^ 1.
    view->SetTooltipText(std::u16string());
    std::optional<ui::Accelerator> shortcut;
    if (space_actions_.size() <= 9) {
      shortcut = SpaceAccelerator(space_actions_.size() - 1);
    }
    alerse::SetArcTooltip(view, title, std::u16string(), shortcut);
    // Measured on Arc: a teal theme's book turns #96B6BC.
    if (const std::optional<alerse::SpaceColor> space_color =
            alerse::ParseSpaceColor(color_value);
        space_color && space_color->saturation > 0) {
      color_utils::HSL hsl;
      hsl.h = space_color->hue / 360.0;
      hsl.s = 0.45 * std::clamp(space_color->saturation, 0.0, 1.0);
      hsl.l = 0.66;
      view->SetHoverTint(color_utils::HSLToSkColor(hsl, 255));
      menu_bar_spaces_.back().tint = color_utils::HSLToSkColor(hsl, 255);
    }
    view->GetViewAccessibility().SetName(title);
    view->set_context_menu_controller(this);
  };

  // A Space is a profile (alerse_space_registry.h). This window shows one;
  // its name, icon and color are its home folder's, and the registry gets a
  // copy for the Space bars of the other Spaces' windows.
  Profile* profile = browser_->GetProfile();
  const base::FilePath own = profile->GetOriginalProfile()->GetPath();
  if (const bookmarks::BookmarkModel* bookmarks =
          BookmarkModelFactory::GetForBrowserContext(profile);
      bookmarks && bookmarks->loaded() && !profile->IsOffTheRecord()) {
    const alerse::SpaceInfo self = alerse::GetSpaceOfProfile(profile);
    std::u16string name = self.home ? u"home" : self.name;
    std::string icon;
    std::string color;
    if (const bookmarks::BookmarkNode* home = SpaceFolder()) {
      if (!home->GetTitle().empty()) {
        name = home->GetTitle();
      }
      home->GetMetaInfo(kSpaceIconMetaKey, &icon);
      home->GetMetaInfo(alerse::kSpaceColorMetaKey, &color);
    }
    alerse::UpdateSpaceCopy(own, name, icon, color);
    alerse::RecordSpaceId(profile);
    if (self.home) {
      // Home's bookmarks carry the list of Spaces to the other devices.
      alerse::UpdateSpaceList(
          BookmarkModelFactory::GetForBrowserContext(profile));
    }
  }
  // An incognito window is outside the stacks and shows no Spaces.
  const std::vector<alerse::SpaceInfo> spaces =
      profile->IsOffTheRecord() ? std::vector<alerse::SpaceInfo>()
                                : alerse::ListSpaces();
  for (const alerse::SpaceInfo& space : spaces) {
    const bool active = space.path == own;
    add_icon(base::BindRepeating(&AlerseSpaceSwitcher::ShowSpaceOfPath,
                                 base::Unretained(this), space.path),
             alerse::ValidSpaceIcon(space.icon, space.home), active,
             space.name,
             active ? std::string(kDefaultSpace)
                    : "profile:" + space.path.BaseName().AsUTF8Unsafe(),
             space.color);
  }
  ApplyActiveSpaceColor();

  auto add = std::make_unique<AddButton>(base::BindRepeating(
      &AlerseSpaceSwitcher::ShowAddMenu, base::Unretained(this)));
  add->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_ALERSE_NEW_SPACE));
  add_button_ = AddChildView(std::move(add));
  add_button_->set_context_menu_controller(this);

  PreferredSizeChanged();
  // The window is named after the shown Space (BrowserView::GetWindowTitle).
  if (GetWidget()) {
    GetWidget()->UpdateWindowTitle();
  }
}

const bookmarks::BookmarkNode* AlerseSpaceSwitcher::SpaceFolder() const {
  const bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  return bookmarks && bookmarks->loaded() ? alerse::FindSpaceFolder(bookmarks)
                                          : nullptr;
}

void AlerseSpaceSwitcher::AddedToWidget() {
  ApplyActiveSpaceColor();
  // Alt+1 to Alt+9 show the Spaces in the row's order, home first, as in Arc
  // for Windows; Control+1 to Control+9 on macOS, as in Arc there (Option
  // with a digit types a character).
  for (size_t i = 0; i < 9; ++i) {
    AddAccelerator(SpaceAccelerator(i));
  }
}

bool AlerseSpaceSwitcher::AcceleratorPressed(
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

void AlerseSpaceSwitcher::ApplyActiveSpaceColor() {
  if (!GetWidget()) {
    return;
  }
  std::optional<alerse::SpaceColor> color;
  std::string value;
  if (const bookmarks::BookmarkNode* folder = SpaceFolder();
      folder && folder->GetMetaInfo(alerse::kSpaceColorMetaKey, &value)) {
    color = alerse::ParseSpaceColor(value);
  }
  // The first color a window gets is set at once; a switch of Space after it
  // blends into the next Space's color.
  const bool animate = !!color_transition_;
  if (!color_transition_) {
    color_transition_ = std::make_unique<alerse::SpaceColorTransition>();
  }
  color_transition_->AnimateTo(GetWidget(), color, animate);
}

void AlerseSpaceSwitcher::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  // As in Arc, "+" offers the shown Space's menu too.
  if (source == add_button_) {
    ShowActiveSpaceMenu(point, source_type);
    return;
  }
  if (const SpaceIcon* icon = views::AsViewClass<SpaceIcon>(source)) {
    ShowSpaceMenu(icon->space(), point, source_type);
  }
}

void AlerseSpaceSwitcher::ShowActiveSpaceMenu(
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (active_space_ < 0 ||
      active_space_ >= static_cast<int>(space_refs_.size())) {
    return;
  }
  ShowSpaceMenu(space_refs_[active_space_].space, point, source_type);
}

void AlerseSpaceSwitcher::ShowSpaceMenu(
    const std::string& space,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (!GetWidget()) {
    return;
  }
  // The previous menu's runner refers to the models replaced below.
  menu_runner_.reset();
  menu_space_ = space;
  menu_point_ = point;
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  // Arc's Space menu, wherever it opens.
  // Change Space Icon... opens Arc's picker. Arc for macOS' Set Profile,
  // Live Folders, Share, Export and Manage Spaces are left out (Arc's
  // account and services), but not the separators around them.
  menu_model_->AddItem(kChangeIconCommand,
                       l10n_util::GetStringUTF16(IDS_ALERSE_CHANGE_SPACE_ICON));
  menu_model_->AddItem(kRenameSpaceCommand,
                       l10n_util::GetStringUTF16(IDS_ALERSE_RENAME_SPACE));
  menu_model_->AddItem(kEditColorCommand,
                       l10n_util::GetStringUTF16(IDS_ALERSE_EDIT_THEME_COLOR));
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_->AddItem(kNewFolderCommand,
                       l10n_util::GetStringUTF16(IDS_ALERSE_NEW_FOLDER));
  // Collapse All Folders while one is open, else Expand All Folders; dimmed
  // without folders.
  const FolderState folders =
      GetFolderState(menu_space_ == kDefaultSpace ? SpaceFolder() : nullptr);
  menu_model_->AddItem(
      kToggleFoldersCommand,
      l10n_util::GetStringUTF16(folders.any_open
                                    ? IDS_ALERSE_COLLAPSE_ALL_FOLDERS
                                    : IDS_ALERSE_EXPAND_ALL_FOLDERS));
  menu_model_->SetEnabledAt(menu_model_->GetItemCount() - 1, folders.any);
  // home is where tabs outside any Space go; it cannot be deleted, so the
  // item Arc always has is dimmed there.
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_->AddItem(kDeleteSpaceCommand,
                       l10n_util::GetStringUTF16(IDS_ALERSE_DELETE_SPACE));
  menu_model_->SetEnabledAt(
      menu_model_->GetItemCount() - 1,
      !alerse::IsHomeSpacePath(PathOfSpace(menu_space_)));
  // The icon, name, theme and folders are the Space's own profile's: only
  // the window of that Space edits them.
  if (menu_space_ != kDefaultSpace) {
    for (int command : {kChangeIconCommand, kRenameSpaceCommand,
                        kEditColorCommand, kNewFolderCommand,
                        kToggleFoldersCommand}) {
      if (std::optional<size_t> index =
              menu_model_->GetIndexOfCommandId(command)) {
        menu_model_->SetEnabledAt(*index, false);
      }
    }
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kBubbleTopRight,
                          source_type);
}

void AlerseSpaceSwitcher::ShowAddMenu(const ui::Event& event) {
  views::View* button = add_button_;
  if (!button || !button->GetWidget()) {
    return;
  }
  // Arc for macOS' popover over its "+": New Space, New Folder | New Split,
  // New Tab. Its Easel and Boost rows are Arc's own features, left out.
  auto* add = static_cast<AddButton*>(button);
  if (alerse::ArcPopoverJustClosed(add)) {
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
  auto run = [](base::WeakPtr<AlerseSpaceSwitcher> switcher, int command) {
    if (switcher) {
      switcher->ExecuteCommand(command, 0);
    }
  };
  // As in Arc, New Split is dimmed on an empty Space.
  const tabs::TabInterface* tab =
      browser_->GetTabStripModel()->GetActiveTab();
  const bool page = tab && tab->GetContents() &&
                    tab->GetContents()->GetVisibleURL().ref() !=
                        "arcomium-space";
  std::vector<alerse::PopoverItem> items;
  items.emplace_back(alerse::PopoverIcon::kNewSpace,
                     l10n_util::GetStringUTF16(IDS_ALERSE_NEW_SPACE),
                     std::nullopt, true,
                     base::BindOnce(run, weak_factory_.GetWeakPtr(),
                                    kNewSpaceCommand));
  items.emplace_back(alerse::PopoverIcon::kNewFolder,
                     l10n_util::GetStringUTF16(IDS_ALERSE_NEW_FOLDER),
                     std::nullopt, true,
                     base::BindOnce(run, weak_factory_.GetWeakPtr(),
                                    kNewFolderCommand));
  items.emplace_back();
  items.emplace_back(alerse::PopoverIcon::kNewSplit,
                     l10n_util::GetStringUTF16(IDS_ALERSE_NEW_SPLIT),
                     shortcut(IDC_NEW_SPLIT_TAB),
                     page && chrome::IsCommandEnabled(browser_,
                                                      IDC_NEW_SPLIT_TAB),
                     base::BindOnce(run, weak_factory_.GetWeakPtr(),
                                    kNewSplitCommand));
  items.emplace_back(alerse::PopoverIcon::kNewTab,
                     l10n_util::GetStringUTF16(IDS_ALERSE_NEW_TAB),
                     shortcut(IDC_NEW_TAB), true,
                     base::BindOnce(run, weak_factory_.GetWeakPtr(),
                                    kNewTabCommand));
  add->SetOpen(true);
  alerse::ShowArcPopover(add, std::move(items), /*above=*/true,
                           kAddPopoverWidth, add->GetClosedCallback());
}

void AlerseSpaceSwitcher::MenuClosed(ui::SimpleMenuModel* source) {
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

void AlerseSpaceSwitcher::ExecuteCommand(int command_id, int event_flags) {
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
      if (auto* sidebar = views::AsViewClass<AlerseSidebarView>(view)) {
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

void AlerseSpaceSwitcher::ToggleMenuSpaceFolders() {
  const bookmarks::BookmarkNode* space = SpaceFolder();
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

void AlerseSpaceSwitcher::ActivateMenuSpace() {
  if (menu_space_ != kDefaultSpace) {
    ShowSpaceOfPath(PathOfSpace(menu_space_));
  }
}

void AlerseSpaceSwitcher::EditMenuSpaceColor() {
  ActivateMenuSpace();
  // Once the sidebar shows the Space: the picker edits the active one.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<AlerseSpaceSwitcher> switcher) {
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
                                 views::AsViewClass<AlerseSidebarView>(
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
AlerseSpaceSwitcher::GetOrAddMenuSpaceFolder() {
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (!bookmarks || !bookmarks->loaded()) {
    return nullptr;
  }
  // Only the window of a Space edits its folder.
  if (menu_space_ != kDefaultSpace) {
    return nullptr;
  }
  return alerse::GetOrAddSpaceFolder(
      bookmarks, alerse::GetSpaceOfProfile(browser_->GetProfile()).name);
}

void AlerseSpaceSwitcher::ShowMenuSpaceIconPicker() {
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
  AlerseSidebarView* sidebar_view = FindSidebar(GetWidget());
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
          icon && icon->space() == menu_space_) {
        anchor = icon->GetBoundsInScreen();
        arrow_x = anchor.CenterPoint().x();
        break;
      }
      for (views::View* child : view->children()) {
        views.push_back(child);
      }
    }
  }
  alerse::ShowArcIconPicker(
      this, anchor, arrow_x, alerse::IconPickerKind::kSpace,
      alerse::GetSpaceIconName(
          folder, alerse::GetSpaceOfProfile(browser_->GetProfile()).home
                      ? std::string(kDefaultSpace)
                      : std::string()),
      base::BindOnce(
          [](base::WeakPtr<AlerseSpaceSwitcher> switcher, int64_t id,
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
                    views::AsViewClass<AlerseSidebarView>(sidebar->view())) {
              view->SetSpaceTitleHighlighted(false);
            }
          },
          std::move(sidebar)));
}

void AlerseSpaceSwitcher::RenameMenuSpace() {
  const bookmarks::BookmarkNode* folder = GetOrAddMenuSpaceFolder();
  if (!folder || !GetWidget()) {
    return;
  }
  // The Space's name is its folder's; the registry takes a copy on save
  // (BookmarkNodeChanged).
  renaming_folder_ = folder->id();
  renaming_space_ = menu_space_;
  // Titled for the Space, not Chromium's "Edit folder name"
  // (look/space-rename-title.patch).
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::EditNode(folder);
  details.alerse_title_id = IDS_ALERSE_SPACE_EDITOR_TITLE;
  BookmarkEditor::Show(GetWidget()->GetNativeWindow(), browser_->GetProfile(),
                       details, BookmarkEditor::NO_TREE);
}

void AlerseSpaceSwitcher::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  ScheduleRebuild();
}

void AlerseSpaceSwitcher::ConfirmDeleteMenuSpace() {
  const base::FilePath path = PathOfSpace(menu_space_);
  if (path.empty() || alerse::IsHomeSpacePath(path) || !GetWidget()) {
    return;
  }
  std::u16string name;
  for (const alerse::SpaceInfo& space : alerse::ListSpaces()) {
    if (space.path == path) {
      name = space.name;
    }
  }
  chrome::ShowQuestionMessageBoxAsync(
      GetWidget()->GetNativeWindow(),
      l10n_util::GetStringUTF16(IDS_ALERSE_DELETE_SPACE_TITLE),
      name.empty()
          ? l10n_util::GetStringUTF16(IDS_ALERSE_DELETE_UNNAMED_SPACE_BODY)
          : l10n_util::GetStringFUTF16(IDS_ALERSE_DELETE_SPACE_BODY, name),
      base::BindOnce(&AlerseSpaceSwitcher::DeleteSpace,
                     weak_factory_.GetWeakPtr(), menu_space_));
}

void AlerseSpaceSwitcher::DeleteSpace(const std::string& space,
                                        chrome::MessageBoxResult result) {
  const base::FilePath path = PathOfSpace(space);
  if (result != chrome::MESSAGE_BOX_RESULT_YES || path.empty() ||
      alerse::IsHomeSpacePath(path)) {
    return;
  }
  alerse::RecordSpaceDeleted(path);
  // Deleting a Space deletes its profile: Chromium closes its windows and
  // removes its data.
  base::OnceClosure remove = base::BindOnce(
      [](base::FilePath path) {
        g_browser_process->profile_manager()
            ->GetDeleteProfileHelper()
            .MaybeScheduleProfileForDeletion(
                path, base::DoNothing(),
                ProfileMetrics::DELETE_PROFILE_SETTINGS);
      },
      path);
  if (space != kDefaultSpace) {
    std::move(remove).Run();
    return;
  }
  // The shown Space: home takes its place first, or closing the visible
  // window would close the whole stack.
  ProfileManager* manager = g_browser_process->profile_manager();
  alerse::ShowSpaceInStack(
      browser_, manager->user_data_dir().AppendASCII(chrome::kInitialProfile),
      base::IgnoreArgs<BrowserWindowInterface*>(std::move(remove)));
}

BEGIN_METADATA(AlerseSpaceSwitcher)
END_METADATA
