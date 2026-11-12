// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/alerse_site_controls.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/page_info/page_info_navigation_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/page_info/page_info.h"
#include "components/page_info/page_info_ui.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_motion.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "components/zoom/zoom_controller.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "build/build_config.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/translate/core/browser/language_state.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/tabs/vertical/alerse_share_mac.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_window_mac.h"
#endif

namespace alerse_site_controls {

ExtensionMenu::ExtensionMenu(ToolbarActionViewModel* action,
                             base::RepeatingClosure on_closed)
    : action_(action), on_closed_(std::move(on_closed)) {}

ExtensionMenu::~ExtensionMenu() = default;

void ExtensionMenu::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  // The toolbar button's menu, which has Pin and Unpin.
  ui::MenuModel* model = action_->GetContextMenu(
      extensions::ExtensionContextMenuModel::ContextMenuSource::kToolbarAction);
  if (!model || !source->GetWidget()) {
    return;
  }
  adapter_ = std::make_unique<views::MenuModelAdapter>(model, on_closed_);
  runner_ = std::make_unique<views::MenuRunner>(
      adapter_->CreateMenu(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  runner_->RunMenuAt(source->GetWidget(), nullptr,
                     gfx::Rect(point, gfx::Size()),
                     views::MenuAnchorPosition::kTopLeft, source_type);
}

std::unique_ptr<ExtensionMenu> CreateExtensionMenu(
    ToolbarActionViewModel* action,
    base::RepeatingClosure on_closed) {
  return std::make_unique<ExtensionMenu>(action, std::move(on_closed));
}

namespace {

// Arc for macOS' Site Control Center, measured at 1440x900: a 236 pt panel
// whose left edge is 117 pt left of the sliders button's middle, an arrow
// 9 pt high under that button, 13 pt corners, 10 pt margins, four 48x32 pt
// tiles a row 8 pt apart.
constexpr int kWidth = 236;
constexpr int kMargin = 10;
constexpr int kGap = 8;
constexpr int kSpacing = 10;
constexpr int kColumns = 4;
constexpr int kTileWidth = (kWidth - 2 * kMargin - (kColumns - 1) * kGap) /
                           kColumns;
constexpr int kTileHeight = 32;
constexpr float kTileRadius = 8;
constexpr float kPanelRadius = 13;
constexpr int kArrowHeight = 9;
constexpr float kArrowHalfWidth = 8;
constexpr int kArrowX = 117;
// From the button's bottom edge to the arrow's tip.
constexpr int kArrowDrop = 3;
constexpr int kRowHeight = 32;
constexpr int kMaxPermissionRows = 6;

// The panel is Arc's vibrant one, as the icon pickers' (alerse_popover.cc):
// on macOS half of #565656 over a blur of what is behind the window.
#if BUILDFLAG(IS_MAC)
constexpr SkColor kPanel = SkColorSetARGB(0x80, 0x56, 0x56, 0x56);
#else
constexpr SkColor kPanel = SkColorSetARGB(0xF7, 0x42, 0x42, 0x42);
#endif
constexpr SkColor kRimDark = SkColorSetARGB(0xB0, 0x00, 0x00, 0x00);
constexpr SkColor kRimLight = SkColorSetARGB(0x30, 0xFF, 0xFF, 0xFF);
constexpr SkColor kText = SkColorSetRGB(0xE6, 0xE6, 0xE6);
constexpr SkColor kHeading = SkColorSetRGB(0xD9, 0xD9, 0xD9);
constexpr SkColor kDimText = SkColorSetRGB(0x9B, 0x9B, 0x9B);
constexpr SkColor kDisabledInk = SkColorSetRGB(0x6A, 0x6A, 0x6A);
// The page's actions sit on near black tiles, the extensions on a veil.
constexpr SkColor kActionTile = SkColorSetARGB(0xE0, 0x14, 0x14, 0x14);
constexpr SkColor kActionTileHot = SkColorSetARGB(0xE0, 0x2C, 0x2C, 0x2C);
constexpr SkColor kTile = SkColorSetARGB(0x16, 0xFF, 0xFF, 0xFF);
constexpr SkColor kTileHot = SkColorSetARGB(0x2C, 0xFF, 0xFF, 0xFF);
constexpr SkColor kBadge = SkColorSetRGB(0xE4, 0xE4, 0xE4);
constexpr SkColor kBadgeInk = SkColorSetRGB(0x2B, 0x2B, 0x2B);
// A Settings row's disc: white when the setting is on, grey when off.
constexpr SkColor kDiscOn = SkColorSetRGB(0xED, 0xED, 0xED);
constexpr SkColor kDiscOff = SkColorSetARGB(0xC0, 0x78, 0x78, 0x78);
constexpr SkColor kDiscInk = SkColorSetRGB(0x2B, 0x2B, 0x2B);
constexpr SkColor kSeparator = SkColorSetARGB(0x30, 0xFF, 0xFF, 0xFF);
constexpr SkColor kMoreDisc = SkColorSetARGB(0x24, 0x00, 0x00, 0x00);

enum MenuCommand {
  kSiteSettings = 1,
  kExtensionSiteAccess,
  kManageExtensions,
};

// Glyphs Chromium has no icon for on every version, drawn here.
enum class Glyph {
  kNone,
  kShare,
  kCamera,
  kDots,
  kKey,
  kTranslate,
};

gfx::FontList Font(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.DeriveWithSizeDelta(size - base.GetFontSize())
      .DeriveWithWeight(weight);
}

// Draws `glyph` in an 18 pt box centred on `center`.
void PaintGlyph(gfx::Canvas* canvas,
                Glyph glyph,
                const gfx::PointF& center,
                SkColor color) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(1.5f);
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
  const float x = center.x();
  const float y = center.y();
  switch (glyph) {
    case Glyph::kNone:
      break;
    case Glyph::kShare: {
      // An open tray and an arrow leaving it upward.
      SkPathBuilder tray;
      tray.moveTo(x - 2.5f, y - 2);
      tray.lineTo(x - 5, y - 2);
      tray.lineTo(x - 5, y + 7);
      tray.lineTo(x + 5, y + 7);
      tray.lineTo(x + 5, y - 2);
      tray.lineTo(x + 2.5f, y - 2);
      canvas->DrawPath(tray.detach(), flags);
      SkPathBuilder arrow;
      arrow.moveTo(x, y + 2.5f);
      arrow.lineTo(x, y - 7);
      arrow.moveTo(x - 3, y - 4.5f);
      arrow.lineTo(x, y - 7.5f);
      arrow.lineTo(x + 3, y - 4.5f);
      canvas->DrawPath(arrow.detach(), flags);
      break;
    }
    case Glyph::kCamera: {
      SkPathBuilder body;
      body.moveTo(x - 3.5f, y - 4);
      body.lineTo(x - 2, y - 6);
      body.lineTo(x + 2, y - 6);
      body.lineTo(x + 3.5f, y - 4);
      canvas->DrawPath(body.detach(), flags);
      canvas->DrawRoundRect(gfx::RectF(x - 7.5f, y - 4, 15, 10.5f), 2.5f,
                            flags);
      canvas->DrawCircle(gfx::PointF(x, y + 1.25f), 2.75f, flags);
      break;
    }
    case Glyph::kDots:
      flags.setStyle(cc::PaintFlags::kFill_Style);
      for (const float dx : {-5.0f, 0.0f, 5.0f}) {
        canvas->DrawCircle(gfx::PointF(x + dx, y), 1.4f, flags);
      }
      break;
    case Glyph::kKey: {
      canvas->DrawCircle(gfx::PointF(x - 3.5f, y), 3, flags);
      SkPathBuilder shaft;
      shaft.moveTo(x - 0.5f, y);
      shaft.lineTo(x + 7, y);
      shaft.moveTo(x + 6.5f, y);
      shaft.lineTo(x + 6.5f, y + 3);
      shaft.moveTo(x + 3.5f, y);
      shaft.lineTo(x + 3.5f, y + 2.5f);
      canvas->DrawPath(shaft.detach(), flags);
      break;
    }
    case Glyph::kTranslate: {
      // "A" beside a speech line: two scripts.
      SkPathBuilder letter;
      letter.moveTo(x + 0.5f, y + 6);
      letter.lineTo(x + 4, y - 2.5f);
      letter.lineTo(x + 7.5f, y + 6);
      letter.moveTo(x + 2, y + 3);
      letter.lineTo(x + 6, y + 3);
      canvas->DrawPath(letter.detach(), flags);
      SkPathBuilder sign;
      sign.moveTo(x - 7.5f, y - 4.5f);
      sign.lineTo(x + 0.5f, y - 4.5f);
      sign.moveTo(x - 3.5f, y - 6.5f);
      sign.lineTo(x - 3.5f, y - 4.5f);
      sign.moveTo(x - 1, y - 4.5f);
      sign.quadTo(x - 2.5f, y + 0.5f, x - 7, y + 2.5f);
      sign.moveTo(x - 6, y - 2);
      sign.quadTo(x - 4, y + 1.5f, x - 0.5f, y + 2.5f);
      canvas->DrawPath(sign.detach(), flags);
      break;
    }
  }
}

// A rounded tile: a page action, an extension or "add".
class Tile : public views::ImageButton {
  METADATA_HEADER(Tile, views::ImageButton)

 public:
  Tile(PressedCallback callback, const std::u16string& name, bool action)
      : views::ImageButton(std::move(callback)), action_(action) {
    SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    SetPreferredSize(gfx::Size(kTileWidth, kTileHeight));
    SetTooltipText(name);
    GetViewAccessibility().SetName(name);
  }

  void SetGlyph(Glyph glyph) { glyph_ = glyph; }

  // A round button on a darker disc, as Arc's "...".
  void SetDisc() { disc_ = true; }

  void SetPinned(bool pinned) { pinned_ = pinned; }
  bool pinned() const { return pinned_; }

  // views::ImageButton:
  void StateChanged(ButtonState old_state) override {
    views::ImageButton::StateChanged(old_state);
    SchedulePaint();
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    const bool hot =
        GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    if (disc_) {
      flags.setColor(hot ? kTile : kMoreDisc);
      canvas->DrawCircle(gfx::RectF(GetLocalBounds()).CenterPoint(),
                         height() / 2.0f, flags);
      return;
    }
    flags.setColor(action_ ? (hot ? kActionTileHot : kActionTile)
                           : (hot ? kTileHot : kTile));
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), kTileRadius, flags);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    views::ImageButton::PaintButtonContents(canvas);
    PaintGlyph(canvas, glyph_, gfx::RectF(GetLocalBounds()).CenterPoint(),
               GetState() == STATE_DISABLED ? kDisabledInk : kText);
  }

 private:
  const bool action_;
  Glyph glyph_ = Glyph::kNone;
  bool disc_ = false;
  bool pinned_ = false;
};

BEGIN_METADATA(Tile)
END_METADATA

// The pins of pinned extensions, over the tiles' lower right corners, a
// little outside them.
class Badges : public views::View {
  METADATA_HEADER(Badges, views::View)

 public:
  void SetTiles(std::vector<raw_ptr<Tile>> tiles) {
    tiles_ = std::move(tiles);
    SchedulePaint();
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kBadge);
    for (Tile* tile : tiles_) {
      if (!tile->pinned()) {
        continue;
      }
      const gfx::Rect bounds = views::View::ConvertRectToTarget(
          tile, this, tile->GetLocalBounds());
      const gfx::PointF center(bounds.right() - 4, bounds.bottom() - 4);
      canvas->DrawCircle(center, 6.5f, flags);
      const gfx::ImageSkia pin = gfx::CreateVectorIcon(kKeepIcon, 9, kBadgeInk);
      canvas->DrawImageInt(pin, std::round(center.x() - 4.5f),
                           std::round(center.y() - 4.5f));
    }
  }

 private:
  std::vector<raw_ptr<Tile>> tiles_;
};

BEGIN_METADATA(Badges)
END_METADATA

// A row of the Settings section, as Arc's: a 32 pt disc with a glyph, white
// while the setting is on, and a name over its state.
class SettingRow : public views::Button {
  METADATA_HEADER(SettingRow, views::Button)

 public:
  SettingRow(PressedCallback callback,
             const gfx::VectorIcon* icon,
             Glyph glyph,
             const std::u16string& title,
             const std::u16string& state,
             bool on)
      : views::Button(std::move(callback)), icon_(icon), glyph_(glyph) {
    SetPreferredSize(gfx::Size(kWidth - 2 * kMargin, kRowHeight));
    Update(title, state, on);
  }

  void Update(const std::u16string& title,
              const std::u16string& state,
              bool on) {
    title_ = title;
    state_ = state;
    on_ = on;
    SetTooltipText(title);
    GetViewAccessibility().SetName(state.empty() ? title
                                                 : title + u", " + state);
    SchedulePaint();
  }

  void SetIcon(const gfx::VectorIcon* icon) {
    icon_ = icon;
    SchedulePaint();
  }

  // views::Button:
  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    if (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED) {
      flags.setColor(SkColorSetA(SK_ColorWHITE, 0x12));
      gfx::RectF plate(GetLocalBounds());
      plate.Outset(gfx::OutsetsF::VH(3, 4));
      canvas->DrawRoundRect(plate, kTileRadius, flags);
    }
    const gfx::PointF disc(kRowHeight / 2.0f, height() / 2.0f);
    flags.setColor(on_ ? kDiscOn : kDiscOff);
    canvas->DrawCircle(disc, kRowHeight / 2.0f, flags);
    if (icon_) {
      const gfx::ImageSkia image = gfx::CreateVectorIcon(*icon_, 16, kDiscInk);
      canvas->DrawImageInt(image, std::round(disc.x() - 8),
                           std::round(disc.y() - 8));
    } else {
      PaintGlyph(canvas, glyph_, disc, kDiscInk);
    }
    const int text_x = kRowHeight + kGap;
    const int text_width = width() - text_x;
    static const gfx::FontList title_font = Font(14, gfx::Font::Weight::SEMIBOLD);
    static const gfx::FontList state_font = Font(13, gfx::Font::Weight::NORMAL);
    if (state_.empty()) {
      canvas->DrawStringRectWithFlags(
          title_, title_font, kText,
          gfx::Rect(text_x, 0, text_width, height()),
          gfx::Canvas::TEXT_ALIGN_LEFT);
      return;
    }
    canvas->DrawStringRectWithFlags(
        title_, title_font, kText, gfx::Rect(text_x, -1, text_width, 18),
        gfx::Canvas::TEXT_ALIGN_LEFT);
    canvas->DrawStringRectWithFlags(
        state_, state_font, kDimText, gfx::Rect(text_x, 16, text_width, 16),
        gfx::Canvas::TEXT_ALIGN_LEFT);
  }

 private:
  raw_ptr<const gfx::VectorIcon> icon_;
  const Glyph glyph_;
  std::u16string title_;
  std::u16string state_;
  bool on_ = false;
};

BEGIN_METADATA(SettingRow)
END_METADATA

// The footer's connection button: a lock or a warning and a word on a plate.
class SecureButton : public views::Button {
  METADATA_HEADER(SecureButton, views::Button)

 public:
  explicit SecureButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetSecure(true);
  }

  void SetSecure(bool secure) {
    secure_ = secure;
    text_ = l10n_util::GetStringUTF16(secure ? IDS_ALERSE_SECURE
                                             : IDS_ALERSE_NOT_SECURE);
    GetViewAccessibility().SetName(text_);
    SetPreferredSize(gfx::Size(
        10 + 14 + 6 + gfx::GetStringWidth(text_, TextFont()) + 11, kRowHeight));
    PreferredSizeChanged();
    SchedulePaint();
  }

  // views::Button:
  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const bool hot =
        GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(hot ? kTileHot : kTile);
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), kTileRadius, flags);
    const gfx::ImageSkia icon = gfx::CreateVectorIcon(
        secure_ ? vector_icons::kLockIcon
                : vector_icons::kNotSecureWarningOldIcon,
        14, kText);
    canvas->DrawImageInt(icon, 10, (height() - 14) / 2);
    canvas->DrawStringRectWithFlags(
        text_, TextFont(), kText, gfx::Rect(30, 0, width() - 30, height()),
        gfx::Canvas::TEXT_ALIGN_LEFT);
  }

 private:
  static const gfx::FontList& TextFont() {
    static const gfx::FontList font = Font(13, gfx::Font::Weight::SEMIBOLD);
    return font;
  }

  bool secure_ = true;
  std::u16string text_;
};

BEGIN_METADATA(SecureButton)
END_METADATA

std::unique_ptr<views::View> MakeRow() {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), kGap));
  return row;
}

std::unique_ptr<views::Label> MakeHeading(const std::u16string& text) {
  auto label = std::make_unique<views::Label>(text);
  label->SetFontList(Font(13, gfx::Font::Weight::BOLD));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(kHeading);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

class SiteControlsView;
SiteControlsView* g_shown = nullptr;

// The panel: it paints itself, arrow and all, inside a bubble whose own
// frame is left invisible.
class SiteControlsView : public views::View,
                         public PageInfoUI,
                         public content::WebContentsObserver,
                         public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(SiteControlsView, views::View)

 public:
  SiteControlsView(BrowserWindowInterface* browser,
                   ExtensionsToolbarDesktop* extensions,
                   const gfx::Rect& anchor_in_screen)
      : browser_(browser->GetWeakPtr()),
        extensions_(extensions),
        anchor_in_screen_(anchor_in_screen) {
    SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        kArrowHeight + kMargin, kMargin, kMargin, kMargin)));
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), kSpacing));

    content::WebContents* contents =
        browser->GetTabStripModel()->GetActiveWebContents();
    if (contents) {
      url_ = contents->GetLastCommittedURL();
    }

    AddActions();
    AddExtensions();
    settings_heading_ = AddChildView(
        MakeHeading(l10n_util::GetStringUTF16(IDS_ALERSE_SITE_PERMISSIONS)));
    settings_ = AddChildView(std::make_unique<views::View>());
    settings_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), kSpacing));
    AddPageStates(contents);
    UpdateSettingsVisibility();
    auto* separator = AddChildView(std::make_unique<views::View>());
    separator->SetPreferredSize(gfx::Size(kWidth - 2 * kMargin, 1));
    separator->SetBackground(views::CreateSolidBackground(kSeparator));
    separator->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(4, 0, 0, 0));
    AddFooter();

    // The pins lie over the tiles' corners, a little outside them.
    badges_ = AddChildView(std::make_unique<Badges>());
    badges_->SetProperty(views::kViewIgnoredByLayoutKey, true);
    badges_->SetCanProcessEventsWithinSubtree(false);
    UpdatePins();

    // Closed when the tab goes or leaves the page: the page info presenter
    // holds the tab and would change another site's permissions.
    Observe(contents);
    if (contents && url_.SchemeIsHTTPOrHTTPS()) {
      ui_delegate_ =
          std::make_unique<ChromePageInfoUiDelegate>(contents, url_);
      presenter_ = std::make_unique<PageInfo>(
          std::make_unique<ChromePageInfoDelegate>(contents), contents, url_);
      presenter_->InitializeUiState(this, base::DoNothing());
    }
  }

  // Where the arrow's tip is, from the view's left edge.
  void set_arrow_x(int arrow_x) { arrow_x_ = arrow_x; }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    // The presenter points at the tab: it goes now, the bubble with it once
    // it closes.
    presenter_.reset();
    ui_delegate_.reset();
    Observe(nullptr);
    CloseSoon();
  }
  void PrimaryPageChanged(content::Page& page) override { CloseSoon(); }

  void CloseSoon() {
    if (views::Widget* widget = GetWidget()) {
      widget->Close();
    }
  }

  ~SiteControlsView() override {
    if (g_shown == this) {
      g_shown = nullptr;
    }
    // The presenter points at the delegates and at this view; it goes first.
    presenter_.reset();
    permission_rows_.clear();
    extension_tiles_.clear();
    settings_heading_ = nullptr;
    settings_ = nullptr;
    secure_button_ = nullptr;
    more_button_ = nullptr;
    badges_ = nullptr;
    // The tiles point at their context menus: they go before them.
    RemoveAllChildViews();
    context_menus_.clear();
    if (after_close_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(after_close_));
    }
  }

  // Closes, then runs `action` once this view is gone, so that what it opens
  // (a popup, a screenshot) neither anchors to it nor shows it.
  void CloseThen(base::OnceClosure action) {
    after_close_ = std::move(action);
    GetWidget()->Close();
  }

  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    const SkPath outline = Outline();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kPanel);
    canvas->DrawPath(outline, flags);
    // Arc's rim: a dark line outside a faint light one, round the arrow too:
    // strokes along the outline, clipped to its inside.
    gfx::ScopedCanvas clip(canvas);
    canvas->ClipPath(outline, true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
    flags.setStrokeWidth(3);
    flags.setColor(kRimLight);
    canvas->DrawPath(outline, flags);
    flags.setStrokeWidth(1);
    flags.setColor(kRimDark);
    canvas->DrawPath(outline, flags);
  }

  void Layout(PassKey) override {
    LayoutSuperclass<views::View>(this);
    if (badges_) {
      badges_->SetBoundsRect(GetLocalBounds());
    }
  }

  // PageInfoUI:
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override {
    if (!settings_) {
      return;
    }
    const bool first = permission_rows_.empty();
    int count = 0;
    for (const auto& permission : permission_info_list) {
      auto it = permission_rows_.find(permission.type);
      if (it != permission_rows_.end()) {
        it->second.info = permission;
        UpdatePermissionRow(it->second);
        continue;
      }
      if (!first || count++ == kMaxPermissionRows) {
        continue;
      }
      PermissionRow& row = permission_rows_[permission.type];
      row.info = permission;
      row.view = settings_->AddChildView(std::make_unique<SettingRow>(
          base::BindRepeating(&SiteControlsView::TogglePermission,
                              base::Unretained(this), permission.type),
          nullptr, Glyph::kNone, std::u16string(), std::u16string(), false));
      UpdatePermissionRow(row);
    }
    UpdateSettingsVisibility();
    Resize();
  }

  void SetIdentityInfo(const IdentityInfo& identity_info) override {
    const std::unique_ptr<SecurityDescription> description =
        GetSecurityDescription(identity_info);
    const bool secure =
        description &&
        description->summary_style != SecuritySummaryColor::RED &&
        identity_info.connection_status ==
            PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
    secure_button_->SetSecure(secure);
    secure_button_->SetVisible(true);
    InvalidateLayout();
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    base::WeakPtr<BrowserWindowInterface> browser = browser_;
    const GURL url = url_;
    switch (command_id) {
      case kSiteSettings:
        CloseThen(base::BindOnce(
            [](base::WeakPtr<BrowserWindowInterface> browser, GURL url) {
              if (browser) {
                chrome::ShowSiteSettings(browser.get(), url);
              }
            },
            browser, url));
        break;
      case kExtensionSiteAccess:
        CloseThen(base::BindOnce(
            [](std::unique_ptr<views::ViewTracker> extensions) {
              if (auto* container = static_cast<ExtensionsToolbarDesktop*>(
                      extensions->view())) {
                if (ExtensionsToolbarButton* button =
                        container->GetExtensionsButton()) {
                  button->ToggleExtensionsMenu();
                }
              }
            },
            std::make_unique<views::ViewTracker>(extensions_)));
        break;
      case kManageExtensions:
        CloseThen(base::BindOnce(
            [](base::WeakPtr<BrowserWindowInterface> browser) {
              if (browser) {
                chrome::ShowExtensions(browser.get());
              }
            },
            browser));
        break;
    }
  }

 private:
  struct PermissionRow {
    PageInfo::PermissionInfo info;
    raw_ptr<SettingRow> view = nullptr;
  };

  // The panel's shape: a rounded rectangle under an arrow.
  SkPath Outline() const {
    const float top = kArrowHeight;
    const SkPath path =
        SkPath::RRect(SkRect::MakeLTRB(0, top, width(), height()),
                      kPanelRadius, kPanelRadius);
    SkPathBuilder arrow;
    arrow.moveTo(arrow_x_ - kArrowHalfWidth, top + 0.5f);
    arrow.lineTo(arrow_x_ - 1.5f, 1.2f);
    arrow.quadTo(arrow_x_, 0, arrow_x_ + 1.5f, 1.2f);
    arrow.lineTo(arrow_x_ + kArrowHalfWidth, top + 0.5f);
    arrow.close();
    SkPath outline;
    return Op(path, arrow.detach(), kUnion_SkPathOp, &outline) ? outline : path;
  }

  void Resize() {
    if (views::Widget* widget = GetWidget()) {
      if (views::DialogDelegate* dialog =
              widget->widget_delegate()->AsDialogDelegate()) {
        if (views::BubbleDialogDelegate* bubble =
                dialog->AsBubbleDialogDelegate()) {
          bubble->SizeToContents();
        }
      }
    }
  }

  void UpdateSettingsVisibility() {
    const bool any = !settings_->children().empty();
    settings_heading_->SetVisible(any);
    settings_->SetVisible(any);
  }

  void UpdatePermissionRow(PermissionRow& row) {
    const ui::ImageModel icon = PageInfoViewFactory::GetPermissionIcon(row.info);
    row.view->SetIcon(icon.IsVectorIcon() ? icon.GetVectorIcon().vector_icon()
                                          : nullptr);
    row.view->Update(
        PageInfoUI::PermissionTypeToUIString(row.info.type),
        PageInfoUI::PermissionStateToUIString(ui_delegate_.get(), row.info),
        PageInfoUI::IsToggleOn(row.info));
    // A setting from a policy or an extension is not the user's to change.
    row.view->SetEnabled(row.info.source ==
                         content_settings::SettingSource::kUser);
  }

  // A click on a permission's row turns it on or off, as Arc's rows do.
  void TogglePermission(ContentSettingsType type) {
    auto it = permission_rows_.find(type);
    if (it == permission_rows_.end() || !presenter_) {
      return;
    }
    PageInfo::PermissionInfo& info = it->second.info;
    PageInfoUI::ToggleBetweenAllowAndBlock(info);
    UpdatePermissionRow(it->second);
    presenter_->OnSitePermissionChanged(info.type, info.setting,
                                        info.requesting_origin,
                                        info.is_one_time);
  }

  // What the page did that the toolbar's icons would tell, first in the
  // Settings section: the compact address field hides those icons, and a
  // blocked pop-up or a zoomed page would otherwise leave no trace. One row
  // for the zoom, while it is not the default, one for each content setting
  // icon Chromium would show now (pop-ups blocked, a download blocked, the
  // clipboard read...), one when the page can be translated and one when
  // the password manager has something for it. A click on the zoom's row
  // sets it back; a click on another opens Chromium's own bubble for it
  // under the address field.
  void AddPageStates(content::WebContents* contents) {
    if (!contents) {
      return;
    }
    const zoom::ZoomController* zoom =
        zoom::ZoomController::FromWebContents(contents);
    if (zoom && !zoom->IsAtDefaultZoom()) {
      settings_->AddChildView(std::make_unique<SettingRow>(
          base::BindRepeating(&SiteControlsView::RunCommand,
                              base::Unretained(this), IDC_ZOOM_NORMAL),
          &kZoomInIcon, Glyph::kNone,
          l10n_util::GetStringFUTF16(
              IDS_ALERSE_SITE_ZOOM,
              base::NumberToString16(zoom->GetZoomPercent())),
          l10n_util::GetStringUTF16(IDS_ZOOM_SET_DEFAULT), true));
    }
    const std::vector<std::unique_ptr<ContentSettingImageModel>> models =
        ContentSettingImageModel::GenerateContentSettingImageModels();
    for (size_t i = 0; i < models.size(); ++i) {
      models[i]->Update(contents);
      if (!models[i]->is_visible() || models[i]->get_tooltip().empty()) {
        continue;
      }
      settings_->AddChildView(std::make_unique<SettingRow>(
          base::BindRepeating(&SiteControlsView::ShowContentSetting,
                              base::Unretained(this), i),
          models[i]->icon(), Glyph::kNone, models[i]->get_tooltip(),
          std::u16string(), true));
    }
    ChromeTranslateClient* translate =
        ChromeTranslateClient::FromWebContents(contents);
    if (translate && translate->GetLanguageState().translate_enabled() &&
        browser_ && chrome::IsCommandEnabled(browser_.get(), IDC_SHOW_TRANSLATE)) {
      settings_->AddChildView(std::make_unique<SettingRow>(
          base::BindRepeating(&SiteControlsView::RunCommand,
                              base::Unretained(this), IDC_SHOW_TRANSLATE),
          nullptr, Glyph::kTranslate,
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TRANSLATE), std::u16string(),
          true));
    }
    const ManagePasswordsUIController* passwords =
        ManagePasswordsUIController::FromWebContents(contents);
    if (passwords &&
        passwords->GetState() != password_manager::ui::INACTIVE_STATE &&
        browser_ &&
        chrome::IsCommandEnabled(browser_.get(),
                                 IDC_MANAGE_PASSWORDS_FOR_PAGE)) {
      settings_->AddChildView(std::make_unique<SettingRow>(
          base::BindRepeating(&SiteControlsView::RunCommand,
                              base::Unretained(this),
                              IDC_MANAGE_PASSWORDS_FOR_PAGE),
          nullptr, Glyph::kKey,
          l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE),
          std::u16string(), true));
    }
  }

  // Opens Chromium's bubble of the `index`th content setting icon, under
  // the address field, once this panel has gone.
  void ShowContentSetting(size_t index) {
    CloseThen(base::BindOnce(
        [](base::WeakPtr<BrowserWindowInterface> browser, size_t index) {
          BrowserView* browser_view =
              browser ? BrowserView::GetBrowserViewForBrowser(browser.get())
                      : nullptr;
          LocationBarView* location_bar =
              browser_view ? browser_view->GetLocationBarView() : nullptr;
          content::WebContents* contents =
              browser ? browser->GetTabStripModel()->GetActiveWebContents()
                      : nullptr;
          if (!location_bar || !location_bar->GetWidget() || !contents) {
            return;
          }
          std::vector<std::unique_ptr<ContentSettingImageModel>> models =
              ContentSettingImageModel::GenerateContentSettingImageModels();
          if (index >= models.size()) {
            return;
          }
          models[index]->Update(contents);
          if (!models[index]->is_visible()) {
            return;
          }
          auto* bubble = new ContentSettingBubbleContents(
              models[index]->CreateBubbleModel(
                  location_bar->GetContentSettingBubbleModelDelegate(),
                  contents),
              contents, views::BubbleAnchor(location_bar),
              views::BubbleBorder::TOP_LEFT);
          views::BubbleDialogDelegateView::CreateBubble(bubble)->Show();
        },
        browser_, index));
  }

  // Arc's top row: Share, Boost, Screenshot, Capture. Alerse has no boosts
  // and no captures: Share (macOS' picker; Copy Link elsewhere), Find,
  // Screenshot and Reading Mode.
  void AddActions() {
    struct Action {
      int command;
      raw_ptr<const gfx::VectorIcon> icon;
      Glyph glyph;
      int name;
    };
    const Action actions[] = {
#if BUILDFLAG(IS_MAC)
        {0, nullptr, Glyph::kShare, IDS_ALERSE_SHARE},
#else
        {IDC_COPY_URL, &vector_icons::kAlerseLinkIcon, Glyph::kNone,
         IDS_ALERSE_COPY_LINK},
#endif
        {IDC_FIND, &kFindInPageIcon, Glyph::kNone, IDS_ALERSE_FIND_IN_PAGE},
        {IDC_SHARING_HUB_SCREENSHOT, nullptr, Glyph::kCamera,
         IDS_ALERSE_SCREENSHOT},
        {IDC_SHOW_READING_MODE, &kMenuBookIcon, Glyph::kNone,
         IDS_ALERSE_READING_MODE},
    };
    views::View* row = AddChildView(MakeRow());
    for (const Action& action : actions) {
      auto* tile = row->AddChildView(std::make_unique<Tile>(
          action.command
              ? base::BindRepeating(&SiteControlsView::RunCommand,
                                    base::Unretained(this), action.command)
              : base::BindRepeating(&SiteControlsView::Share,
                                    base::Unretained(this)),
          l10n_util::GetStringUTF16(action.name), /*action=*/true));
      tile->SetGlyph(action.glyph);
      if (action.icon) {
        tile->SetImageModel(
            views::Button::STATE_NORMAL,
            ui::ImageModel::FromVectorIcon(*action.icon, kText, 18));
        tile->SetImageModel(
            views::Button::STATE_DISABLED,
            ui::ImageModel::FromVectorIcon(*action.icon, kDisabledInk, 18));
      }
      tile->SetEnabled(
          action.command
              ? browser_ &&
                    chrome::IsCommandEnabled(browser_.get(), action.command)
              : url_.is_valid() && !url_.is_empty());
    }
  }

  void AddExtensions() {
    AddChildView(MakeHeading(l10n_util::GetStringUTF16(IDS_ALERSE_EXTENSIONS)));
    ExtensionsToolbarViewModel* model = extensions_->GetToolbarViewModel();
    content::WebContents* contents =
        browser_ ? browser_->GetTabStripModel()->GetActiveWebContents()
                 : nullptr;
    // Pinned ones first, in their order, then the rest by name.
    std::vector<ToolbarActionsModel::ActionId> ids;
    if (model && model->AreActionsInitialized()) {
      ids = model->GetPinnedActionIds();
      std::vector<ToolbarActionsModel::ActionId> rest;
      for (const auto& id : model->GetAllActionIds()) {
        // Only ones with a model: the comparator below needs every name.
        if (!std::ranges::contains(ids, id) && model->GetActionModelForId(id)) {
          rest.push_back(id);
        }
      }
      std::ranges::sort(rest, [model](const auto& a, const auto& b) {
        return model->GetActionModelForId(a)->GetActionName() <
               model->GetActionModelForId(b)->GetActionName();
      });
      ids.insert(ids.end(), rest.begin(), rest.end());
    }

    // The rows of tiles keep 8 pt between them, less than the sections'.
    views::View* grid = AddChildView(std::make_unique<views::View>());
    grid->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), kGap));
    views::View* row = nullptr;
    auto next_tile_row = [&]() {
      if (!row || row->children().size() == kColumns) {
        row = grid->AddChildView(MakeRow());
      }
      return row;
    };
    for (const auto& id : ids) {
      ToolbarActionViewModel* action = model->GetActionModelForId(id);
      if (!action) {
        continue;
      }
      auto* tile = next_tile_row()->AddChildView(std::make_unique<Tile>(
          base::BindRepeating(&SiteControlsView::RunExtension,
                              base::Unretained(this), id),
          action->GetActionName(), /*action=*/false));
      tile->SetImageModel(views::Button::STATE_NORMAL,
                          action->GetIcon(contents, gfx::Size(20, 20)));
      auto menu = CreateExtensionMenu(
          action, base::BindRepeating(&SiteControlsView::UpdatePins,
                                      base::Unretained(this)));
      tile->set_context_menu_controller(menu.get());
      context_menus_.push_back(std::move(menu));
      extension_tiles_.emplace_back(id, tile);
    }
    auto* add = next_tile_row()->AddChildView(std::make_unique<Tile>(
        base::BindRepeating(&SiteControlsView::ExecuteCommand,
                            base::Unretained(this), kManageExtensions, 0),
        l10n_util::GetStringUTF16(IDS_ALERSE_MANAGE_EXTENSIONS),
        /*action=*/false));
    add->SetImageModel(views::Button::STATE_NORMAL,
                       ui::ImageModel::FromVectorIcon(kAddIcon, kText, 18));
    UpdatePins();
  }

  void AddFooter() {
    views::View* footer = AddChildView(std::make_unique<views::View>());
    auto* layout = footer->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    secure_button_ = footer->AddChildView(
        std::make_unique<SecureButton>(base::BindRepeating(
            &SiteControlsView::OpenPageInfo, base::Unretained(this),
            std::nullopt)));
    secure_button_->SetVisible(false);

    auto* spacer = footer->AddChildView(std::make_unique<views::View>());
    layout->SetFlexForView(spacer, 1);

    // Arc's "..." lies on its side, on a darker disc.
    auto more = std::make_unique<Tile>(
        base::BindRepeating(&SiteControlsView::ShowMoreMenu,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_ALERSE_MORE), /*action=*/false);
    more->SetGlyph(Glyph::kDots);
    more->SetDisc();
    more->SetPreferredSize(gfx::Size(kRowHeight, kRowHeight));
    more_button_ = footer->AddChildView(std::move(more));
  }

  void UpdatePins() {
    if (!browser_) {
      return;
    }
    ToolbarActionsModel* model =
        ToolbarActionsModel::Get(browser_->GetProfile());
    for (auto& [id, tile] : extension_tiles_) {
      tile->SetPinned(model && model->IsActionPinned(id));
    }
    if (badges_) {
      std::vector<raw_ptr<Tile>> tiles;
      for (auto& [id, tile] : extension_tiles_) {
        tiles.push_back(tile);
      }
      badges_->SetTiles(std::move(tiles));
    }
  }

  void RunCommand(int command) {
    // A screenshot waits for the bubble to leave the screen.
    const base::TimeDelta delay = command == IDC_SHARING_HUB_SCREENSHOT
                                      ? base::Milliseconds(250)
                                      : base::TimeDelta();
    CloseThen(base::BindOnce(
        [](base::WeakPtr<BrowserWindowInterface> browser, int command,
           base::TimeDelta delay) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE,
              base::BindOnce(
                  [](base::WeakPtr<BrowserWindowInterface> browser,
                     int command) {
                    if (browser) {
                      chrome::ExecuteCommand(browser.get(), command);
                    }
                  },
                  browser, command),
              delay);
        },
        browser_, command, delay));
  }

  // The system's share picker for the page's link, at the sliders button.
  void Share() {
#if BUILDFLAG(IS_MAC)
    CloseThen(base::BindOnce(
        [](base::WeakPtr<BrowserWindowInterface> browser, GURL url,
           gfx::Rect anchor) {
          BrowserView* browser_view =
              browser ? BrowserView::GetBrowserViewForBrowser(browser.get())
                      : nullptr;
          if (browser_view && browser_view->GetWidget()) {
            alerse::ShowSharePicker({url}, browser_view->GetWidget(), anchor);
          }
        },
        browser_, url_, anchor_in_screen_));
#endif
  }

  void RunExtension(const ToolbarActionsModel::ActionId& id) {
    CloseThen(base::BindOnce(
        [](std::unique_ptr<views::ViewTracker> extensions,
           ToolbarActionsModel::ActionId id) {
          auto* container =
              static_cast<ExtensionsToolbarDesktop*>(extensions->view());
          if (!container || !container->GetToolbarViewModel()) {
            return;
          }
          container->GetToolbarViewModel()->ExecuteUserAction(
              id, ToolbarActionViewModel::InvocationSource::kMenuEntry);
        },
        std::make_unique<views::ViewTracker>(extensions_), id));
  }

  void OpenPageInfo(std::optional<ContentSettingsType> type) {
    CloseThen(base::BindOnce(
        [](base::WeakPtr<BrowserWindowInterface> browser,
           std::optional<ContentSettingsType> type) {
          if (!browser) {
            return;
          }
          if (content::WebContents* contents =
                  browser->GetTabStripModel()->GetActiveWebContents()) {
            ShowPageInfoDialog(contents, base::DoNothing(),
                               bubble_anchor_util::Anchor::kLocationBar, type);
          }
        },
        browser_, type));
  }

  void ShowMoreMenu() {
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    if (url_.SchemeIsHTTPOrHTTPS()) {
      menu_model_->AddItem(kSiteSettings, l10n_util::GetStringUTF16(IDS_ALERSE_SITE_SETTINGS));
    }
    menu_model_->AddItem(kExtensionSiteAccess,
                         l10n_util::GetStringUTF16(IDS_ALERSE_EXTENSION_SITE_ACCESS));
    menu_model_->AddItem(kManageExtensions,
                         l10n_util::GetStringUTF16(IDS_ALERSE_MANAGE_EXTENSIONS));
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
    menu_runner_->RunMenuAt(GetWidget(), nullptr,
                            more_button_->GetAnchorBoundsInScreen(),
                            views::MenuAnchorPosition::kTopRight,
                            ui::mojom::MenuSourceType::kMouse);
  }

  base::WeakPtr<BrowserWindowInterface> browser_;
  raw_ptr<ExtensionsToolbarDesktop> extensions_;
  GURL url_;
  // The sliders button, for what a tile opens after the panel has gone.
  const gfx::Rect anchor_in_screen_;
  int arrow_x_ = kArrowX;
  base::OnceClosure after_close_;

  std::unique_ptr<ChromePageInfoUiDelegate> ui_delegate_;
  std::unique_ptr<PageInfo> presenter_;

  raw_ptr<views::View> settings_heading_ = nullptr;
  raw_ptr<views::View> settings_ = nullptr;
  base::flat_map<ContentSettingsType, PermissionRow> permission_rows_;
  raw_ptr<SecureButton> secure_button_ = nullptr;
  raw_ptr<Tile> more_button_ = nullptr;
  raw_ptr<Badges> badges_ = nullptr;
  std::vector<std::pair<ToolbarActionsModel::ActionId, raw_ptr<Tile>>>
      extension_tiles_;
  std::vector<std::unique_ptr<ExtensionMenu>> context_menus_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

BEGIN_METADATA(SiteControlsView)
END_METADATA

// The bubble that holds the panel: its top left corner goes where Arc's
// panel starts, 117 pt left of the anchor's middle (as far as the screen
// allows), the arrow's tip just under the anchor.
class SiteControlsBubble : public views::BubbleDialogDelegate {
 public:
  explicit SiteControlsBubble(views::View* anchor)
      : views::BubbleDialogDelegate(anchor, views::BubbleBorder::TOP_LEFT) {}

  static int PanelLeft(const gfx::Rect& anchor) {
    const gfx::Rect work = display::Screen::Get()
                               ->GetDisplayNearestPoint(anchor.CenterPoint())
                               .work_area();
    return std::clamp(anchor.CenterPoint().x() - kArrowX, work.x() + 4,
                      std::max(work.x() + 4, work.right() - 4 - kWidth));
  }

  // views::BubbleDialogDelegate:
  gfx::Rect GetAnchorRect() const override {
    const gfx::Rect anchor = views::BubbleDialogDelegate::GetAnchorRect();
    return gfx::Rect(PanelLeft(anchor), anchor.bottom() + kArrowDrop, 0, 0);
  }
};

}  // namespace

views::Widget* Show(views::View* anchor,
                    BrowserWindowInterface* browser,
                    ExtensionsToolbarDesktop* extensions) {
  Hide();
  const gfx::Rect anchor_in_screen = anchor->GetBoundsInScreen();
  auto delegate = std::make_unique<SiteControlsBubble>(anchor);
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetShowCloseButton(false);
  delegate->SetShowTitle(false);
  delegate->SetTitle(l10n_util::GetStringUTF16(IDS_ALERSE_SITE_CONTROLS));
  // The panel paints itself: the bubble's frame shows nothing, and the
  // window's own shadow follows the panel's shape.
  delegate->set_margins(gfx::Insets());
  delegate->set_shadow(views::BubbleBorder::NO_SHADOW);
  delegate->set_corner_radius(0);
  delegate->SetBackgroundColor(SK_ColorTRANSPARENT);
  delegate->set_fixed_width(kWidth);
  auto view = std::make_unique<SiteControlsView>(browser, extensions,
                                                 anchor_in_screen);
  view->set_arrow_x(anchor_in_screen.CenterPoint().x() -
                    SiteControlsBubble::PanelLeft(anchor_in_screen));
  g_shown = delegate->SetContentsView(std::move(view));
  views::Widget* widget = views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(delegate), views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
#if BUILDFLAG(IS_MAC)
  // Arc's vibrancy: the panel blurs what is behind it.
  alerse::BlurBehindPopover(widget);
#endif
  alerse::motion::FadeInWidget(widget, /*activate=*/true);
  return widget;
}

bool IsShowing() {
  return g_shown != nullptr;
}

void Hide() {
  if (g_shown && g_shown->GetWidget()) {
    g_shown->GetWidget()->Close();
  }
  g_shown = nullptr;
}

}  // namespace alerse_site_controls
