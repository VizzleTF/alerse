// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/alerse_address_bar.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/alerse_split.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/alerse_frame_color.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
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
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/views/extensions/alerse_site_controls.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_motion.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_tooltip.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/ink_drop.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_space_color.h"
#include "ui/base/models/image_model.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/controls/button/button.h"
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
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"

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
// Arc for macOS' hover buttons in the sidebar's field, measured: site
// controls centred 20 pt from the field's end, Copy URL and a single pinned
// extension 28 pt apart before it, glyphs #ACACAC; the address fades out
// before them. The button under the pointer has a 28 pt plate with 8 pt
// corners, #4D4D4F on the hovered field's #3F3F40.
constexpr int kHoverButtonPitch = 28;
constexpr int kHoverButtonEndCenter = 20;
constexpr SkColor kHoverIconColor = SkColorSetRGB(0xAC, 0xAC, 0xAC);
constexpr float kHoverPlateRadius = 8;
constexpr SkAlpha kHoverPlateShade = 0x13;
constexpr int kTextFadeWidth = 16;
// Two or more pinned extensions are tiles under the field, as in Arc for
// macOS: 27 pt high, 8 apart and 9 below the field, up to four in a row,
// the rows evened out (five are three and two); the field's shade.
constexpr int kTileHeight = 27;
constexpr int kTileGap = 8;
constexpr int kTileRowGap = 9;
constexpr int kTilesPerRow = 4;
constexpr float kTileRadius = 8;
constexpr SkAlpha kTileShade = 0x1B;
constexpr SkAlpha kTileHoverShade = 0x25;
// An extension's image is asked for at the toolbar button's size, as
// Chromium's toolbar does: the 16 pt icon sits in its middle and the badge
// (a blocker's count, unread mail) in its corner. Asked for at 16 pt, the
// 14 pt badge would cover the icon.
constexpr int kExtensionImageSize = 28;
// Mouse events say when the pointer comes to the sidebar; this poll only
// catches an exit that no event told of, and runs while it may be needed.
constexpr base::TimeDelta kSidebarHoverPoll = base::Milliseconds(100);
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
class AlerseCommandBarPanel : public views::View {
  METADATA_HEADER(AlerseCommandBarPanel, views::View)

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

BEGIN_METADATA(AlerseCommandBarPanel)
END_METADATA

// A shade off the frame, as the field and Arc's tiles are.
SkColor FieldShade(const views::View* view, SkAlpha alpha) {
  return color_utils::BlendTowardMaxContrast(alerse::FrameColor(view),
                                              alpha);
}

// A pinned extension's tile under the field.
class ExtensionTile : public views::Button {
  METADATA_HEADER(ExtensionTile, views::Button)

 public:
  ExtensionTile(PressedCallback callback,
                const std::u16string& name,
                ui::ImageModel icon)
      : views::Button(std::move(callback)), icon_(std::move(icon)) {
    SetTooltipText(name);
    GetViewAccessibility().SetName(name);
    SetInstallFocusRingOnFocus(false);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(
        FieldShade(this, IsMouseHovered() ? kTileHoverShade : kTileShade));
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), kTileRadius, flags);
    if (icon_.IsEmpty() || !GetColorProvider()) {
      return;
    }
    const gfx::ImageSkia image = icon_.Rasterize(GetColorProvider());
    // Bottom-aligned when the image is taller than the tile, so the badge
    // at its foot is whole.
    canvas->DrawImageInt(image, (width() - image.width()) / 2,
                         std::min((height() - image.height()) / 2,
                                  height() - image.height()));
  }

  void StateChanged(ButtonState old_state) override { SchedulePaint(); }

 private:
  const ui::ImageModel icon_;
};

BEGIN_METADATA(ExtensionTile)
END_METADATA

// Fades the field's address out into its shade before the hover buttons.
class TextFade : public views::View {
  METADATA_HEADER(TextFade, views::View)

 public:
  void set_shade(SkAlpha shade) {
    if (shade_ != shade) {
      shade_ = shade;
      SchedulePaint();
    }
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const SkColor color = FieldShade(this, shade_);
    cc::PaintFlags flags;
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(), gfx::Point(kTextFadeWidth, 0), SkColorSetA(color, 0),
        color));
    canvas->DrawRect(gfx::Rect(0, 0, kTextFadeWidth, height()), flags);
    canvas->FillRect(gfx::Rect(kTextFadeWidth, 0,
                               std::max(0, width() - kTextFadeWidth), height()),
                     color);
  }

 private:
  SkAlpha shade_ = kTileShade;
};

BEGIN_METADATA(TextFade)
END_METADATA

// Arc for macOS' glyphs on the field's hover buttons, traced from its
// screen: SF Symbols' link (two chain links at 45 degrees) and
// slider.horizontal.below.square (a 15.3 pt rounded square, two slider
// lines with hollow knobs), 1.2 pt strokes.
// After a copy, the link turns into a check mark for a moment.
enum class HoverGlyph { kLink, kSliders, kCheck };

class HoverGlyphSource : public gfx::CanvasImageSource {
 public:
  HoverGlyphSource(HoverGlyph glyph, SkColor color)
      : gfx::CanvasImageSource(gfx::Size(18, 18)),
        glyph_(glyph),
        color_(color) {}

  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.2f);
    flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
    flags.setColor(color_);
    if (glyph_ == HoverGlyph::kCheck) {
      const SkPath check = SkPathBuilder()
                               .moveTo(4.2f, 9.4f)
                               .lineTo(7.6f, 12.8f)
                               .lineTo(13.8f, 5.6f)
                               .detach();
      flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
      flags.setStrokeWidth(1.5f);
      canvas->DrawPath(check, flags);
      return;
    }
    if (glyph_ == HoverGlyph::kSliders) {
      const gfx::RectF square(1.35f, 1.35f, 15.3f, 15.3f);
      canvas->DrawRoundRect(square, 3.4f, flags);
      const float left = square.x() + 3.75f;
      const float right = square.x() + 11.9f;
      for (const auto [y, knob] : {std::pair(square.y() + 5.6f, 9.0f),
                                   std::pair(square.y() + 9.9f, 6.6f)}) {
        const float knob_x = square.x() + knob;
        canvas->DrawLine(gfx::PointF(left, y), gfx::PointF(knob_x - 1.5f, y),
                         flags);
        canvas->DrawLine(gfx::PointF(knob_x + 1.5f, y), gfx::PointF(right, y),
                         flags);
        canvas->DrawCircle(gfx::PointF(knob_x, y), 1.5f, flags);
      }
      return;
    }
    // Two links, each a capsule, along the rising diagonal and overlapping
    // at the middle.
    gfx::ScopedCanvas scoped(canvas);
    canvas->sk_canvas()->translate(9, 9);
    canvas->sk_canvas()->rotate(-45);
    for (const float offset : {-2.9f, 2.9f}) {
      canvas->DrawRoundRect(gfx::RectF(offset - 4.75f, -2.8f, 9.5f, 5.6f),
                            2.8f, flags);
    }
  }

 private:
  const HoverGlyph glyph_;
  const SkColor color_;
};

// A button of the field: Arc's plate under the pointer, no ink drop.
class HoverButton : public views::ImageButton {
  METADATA_HEADER(HoverButton, views::ImageButton)

 public:
  using views::ImageButton::ImageButton;

  void OnPaintBackground(gfx::Canvas* canvas) override {
    if (GetState() != STATE_HOVERED && GetState() != STATE_PRESSED) {
      return;
    }
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetA(
        color_utils::GetColorWithMaxContrast(alerse::FrameColor(this)),
        kHoverPlateShade));
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), kHoverPlateRadius,
                          flags);
  }

  void StateChanged(ButtonState old_state) override {
    views::ImageButton::StateChanged(old_state);
    SchedulePaint();
  }
};

BEGIN_METADATA(HoverButton)
END_METADATA

std::unique_ptr<views::ImageButton> MakeHoverButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    int name_id,
    std::optional<ui::Accelerator> shortcut = std::nullopt) {
  auto button = std::make_unique<HoverButton>(std::move(callback));
  views::ConfigureVectorImageButton(button.get());
  ui::ImageModel glyph =
      ui::ImageModel::FromVectorIcon(icon, kHoverIconColor, kCopyIconSize);
  if (&icon == &vector_icons::kAlerseLinkIcon ||
      &icon == &vector_icons::kAlerseTuneIcon) {
    glyph = ui::ImageModel::FromImageSkia(
        gfx::CanvasImageSource::MakeImageSkia<HoverGlyphSource>(
            &icon == &vector_icons::kAlerseLinkIcon ? HoverGlyph::kLink
                                                      : HoverGlyph::kSliders,
            kHoverIconColor));
  }
  for (const auto state : {views::Button::STATE_NORMAL,
                           views::Button::STATE_HOVERED,
                           views::Button::STATE_PRESSED}) {
    button->SetImageModel(state, glyph);
  }
  // Its own plate under the pointer, and Arc's pill for the name with the
  // shortcut's keycaps, as Arc's.
  views::InkDrop::Get(button.get())
      ->SetMode(views::InkDropHost::InkDropMode::OFF);
  const std::u16string name = l10n_util::GetStringUTF16(name_id);
  alerse::SetArcTooltip(button.get(), name, std::u16string(), shortcut);
  button->GetViewAccessibility().SetName(name);
  button->SetVisible(false);
  return button;
}

// Whether the extension's icon says something on this page: badge text, or
// the mark of an extension that was kept from running here.
bool HasNotice(Profile* profile,
               const std::string& id,
               content::WebContents* contents) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)
          ->enabled_extensions()
          .GetByID(id);
  if (!extension || !contents) {
    return false;
  }
  const extensions::ExtensionAction* action =
      extensions::ExtensionActionManager::Get(profile)->GetExtensionAction(
          *extension);
  const int tab_id = sessions::SessionTabHelper::IdForTab(contents).id();
  return (action && !action->GetDisplayBadgeText(tab_id).empty()) ||
         extensions::SitePermissionsHelper(profile).HasBeenBlocked(*extension,
                                                                   contents);
}

}  // namespace

class AlerseAddressBar::ClickWatcher : public ui::EventObserver {
 public:
  explicit ClickWatcher(AlerseAddressBar* bar) : bar_(bar) {}

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override { bar_->OnWindowClick(); }

 private:
  raw_ptr<AlerseAddressBar> bar_;
};

// Hears the pointer come to and leave the sidebar: a handler on the region
// sees the events of every view in it.
class AlerseAddressBar::SidebarHoverWatcher : public ui::EventHandler {
 public:
  explicit SidebarHoverWatcher(AlerseAddressBar* bar) : bar_(bar) {}

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    switch (event->type()) {
      case ui::EventType::kMouseEntered:
      case ui::EventType::kMouseMoved:
        bar_->SetSidebarHovered(true);
        break;
      case ui::EventType::kMouseExited:
        // To another view of the sidebar, or away from it.
        bar_->PollSidebarHover();
        break;
      default:
        break;
    }
  }

 private:
  raw_ptr<AlerseAddressBar> bar_;
};

AlerseAddressBar::AlerseAddressBar(BrowserWindowInterface* browser)
    : browser_(browser) {
  SetNotifyEnterExitOnChild(true);
  // The location bar paints to a layer; so do these, to show over it.
  auto fade = std::make_unique<TextFade>();
  fade->SetPaintToLayer();
  fade->layer()->SetFillsBoundsOpaquely(false);
  fade->SetCanProcessEventsWithinSubtree(false);
  fade->SetVisible(false);
  text_fade_ = AddChildView(std::move(fade));
  pinned_button_ = AddChildView(MakeHoverButton(
      views::Button::PressedCallback(), vector_icons::kAlerseLinkIcon,
      IDS_ALERSE_EXTENSIONS));
  copy_button_ = AddChildView(MakeHoverButton(
      base::BindRepeating(&AlerseAddressBar::CopyAddress,
                          base::Unretained(this)),
      // Arc's name and keys: "Copy URL", Shift-Command-C.
      vector_icons::kAlerseLinkIcon, IDS_ALERSE_MENU_COPY_URL,
      ui::Accelerator(ui::VKEY_C,
#if BUILDFLAG(IS_MAC)
                      ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN
#else
                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN
#endif
                      )));
  site_button_ = AddChildView(MakeHoverButton(
      base::BindRepeating(&AlerseAddressBar::ShowSiteControls,
                          base::Unretained(this)),
      vector_icons::kAlerseTuneIcon, IDS_ALERSE_SITE_CONTROL_CENTER));
  for (views::ImageButton* button :
       {pinned_button_.get(), copy_button_.get(), site_button_.get()}) {
    button->SetPaintToLayer();
    button->layer()->SetFillsBoundsOpaquely(false);
  }
}

AlerseAddressBar::~AlerseAddressBar() {
  // Where the command bar edits the toolbar's own bar, `location_bar_` is
  // null and ReturnLocationBar() alone would leave that bar in the editor.
  StopEditing();
  ReturnLocationBar();
  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
  }
  if (editor_) {
    editor_->RemoveObserver(this);
  }
  if (extensions_model_) {
    extensions_model_->RemoveObserver(this);
  }
}

gfx::Size AlerseAddressBar::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
#if BUILDFLAG(IS_MAC)
  const int height = kFieldHeight;
#else
  const int height = location_bar_ ? location_bar_->GetPreferredSize().height()
                                   : 0;
#endif
  return gfx::Size(
      0, location_bar_ ? height + 2 * kVerticalInset +
                             GetTileRows() * (kTileRowGap + kTileHeight)
                       : 0);
}

gfx::Rect AlerseAddressBar::GetFieldBounds() const {
#if BUILDFLAG(IS_MAC)
  const int field_height = kFieldHeight;
#else
  const int field_height =
      location_bar_ ? location_bar_->GetPreferredSize().height() : 0;
#endif
  return gfx::Rect(kHorizontalInset, kVerticalInset,
                   std::max(0, width() - 2 * kHorizontalInset), field_height);
}

int AlerseAddressBar::GetTileRows() const {
  const int count = static_cast<int>(extension_tiles_.size());
  return count < 2 ? 0 : (count + kTilesPerRow - 1) / kTilesPerRow;
}

void AlerseAddressBar::Layout(PassKey) {
  if (!location_bar_) {
    return;
  }
  const gfx::Rect field = GetFieldBounds();
  if (!editing_) {
    location_bar_->SetBoundsRect(field);
  }
  // From the field's end: site controls, Copy Link, a single pinned
  // extension.
  int center_x = field.right() - kHoverButtonEndCenter;
  int leftmost = field.right();
  for (views::ImageButton* button :
       {site_button_.get(), copy_button_.get(), pinned_button_.get()}) {
    // As large as the plate; the pinned extension's holds its 28 pt image.
    const int size = kHoverButtonPitch;
    button->SetBounds(center_x - size / 2,
                      field.y() + (field.height() - size) / 2, size, size);
    // A place is taken only by a button that shows: a pinned extension
    // shown alone, for its badge, stands at the field's end.
    if (button == pinned_button_.get() ? pinned_shown_ : buttons_shown_) {
      leftmost = button->x();
      center_x -= kHoverButtonPitch;
    }
  }
  // The fade runs from before the leftmost button to the field's rounded end.
  text_fade_->SetBounds(leftmost - kTextFadeWidth, field.y() + 4,
                        field.right() - 6 - (leftmost - kTextFadeWidth),
                        std::max(0, field.height() - 8));
  // The tiles, rows evened out.
  const int count = static_cast<int>(extension_tiles_.size());
  const int rows = GetTileRows();
  if (rows > 0) {
    const int columns = (count + rows - 1) / rows;
    const int tile_width =
        (field.width() - (columns - 1) * kTileGap) / columns;
    for (int i = 0; i < count; ++i) {
      const int row = i / columns;
      const int column = i % columns;
      extension_tiles_[i]->SetBounds(
          field.x() + column * (tile_width + kTileGap),
          field.bottom() + kTileRowGap + row * (kTileHeight + kTileRowGap),
          tile_width, kTileHeight);
    }
  }
}

void AlerseAddressBar::VisibilityChanged(views::View* starting_from,
                                           bool is_visible) {
  ScheduleUpdatePlacement();
}

void AlerseAddressBar::AddedToWidget() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_) {
    focus_manager_->AddFocusChangeListener(this);
  }
  ScheduleUpdatePlacement();
  // The sidebar: the vertical tab strip's region this field sits in.
  views::View* sidebar = this;
  for (views::View* view = parent(); view; view = view->parent()) {
    if (view->GetClassName() == "VerticalTabStripRegionView") {
      sidebar = view;
      break;
    }
  }
  hover_region_.SetView(sidebar);
  if (!hover_watcher_) {
    hover_watcher_ = std::make_unique<SidebarHoverWatcher>(this);
  }
  sidebar->AddPreTargetHandler(hover_watcher_.get());
  window_observation_.Observe(GetWidget());
  PollSidebarHover();
}

void AlerseAddressBar::SetSidebarHovered(bool hovered) {
  if (hovered != sidebar_hovered_) {
    sidebar_hovered_ = hovered;
    UpdateCopyButton();
  }
  // While hovered, an exit may go unheard (a popup over the sidebar, rows
  // rebuilt under the pointer).
  bool poll = hovered;
#if BUILDFLAG(IS_MAC)
  // An inactive window gets no mouse moves on macOS.
  const views::Widget* widget = GetWidget();
  poll = poll || (widget && widget->IsVisible() && !widget->IsActive());
#endif
  if (!poll) {
    sidebar_hover_poll_.Stop();
  } else if (!sidebar_hover_poll_.IsRunning()) {
    sidebar_hover_poll_.Start(
        FROM_HERE, kSidebarHoverPoll,
        base::BindRepeating(&AlerseAddressBar::PollSidebarHover,
                            base::Unretained(this)));
  }
}

void AlerseAddressBar::PollSidebarHover() {
  const views::View* sidebar = hover_region_.view();
  SetSidebarHovered(sidebar && GetWidget() && GetWidget()->IsVisible() &&
                    sidebar->IsDrawn() &&
                    sidebar->GetBoundsInScreen().Contains(
                        display::Screen::Get()->GetCursorScreenPoint()));
}

void AlerseAddressBar::OnWidgetActivationChanged(views::Widget* widget,
                                                   bool active) {
  if (widget != editor_.get()) {
    PollSidebarHover();
  }
}

void AlerseAddressBar::OnWidgetVisibilityChanged(views::Widget* widget,
                                                   bool visible) {
  if (widget != editor_.get()) {
    PollSidebarHover();
  }
}

void AlerseAddressBar::RemovedFromWidget() {
  sidebar_hover_poll_.Stop();
  window_observation_.Reset();
  if (hover_region_.view() && hover_watcher_) {
    hover_region_.view()->RemovePreTargetHandler(hover_watcher_.get());
  }
  hover_region_.SetView(nullptr);
  StopEditing();
  ReturnLocationBar();
  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = nullptr;
  }
}

void AlerseAddressBar::OnMouseEntered(const ui::MouseEvent& event) {
  hovered_ = true;
  UpdateCopyButton();
}

void AlerseAddressBar::OnMouseExited(const ui::MouseEvent& event) {
  hovered_ = false;
  UpdateCopyButton();
}

bool AlerseAddressBar::OnMousePressed(const ui::MouseEvent& event) {
#if BUILDFLAG(IS_MAC)
  return location_bar_ && !editing_ && event.IsOnlyLeftMouseButton();
#else
  return false;
#endif
}

void AlerseAddressBar::OnMouseReleased(const ui::MouseEvent& event) {
#if BUILDFLAG(IS_MAC)
  // The field takes the press (its location bar takes no events); a release
  // on one of its hover buttons runs that button instead of the command bar.
  if (location_bar_ && !editing_) {
    if (site_button_->GetVisible() &&
        site_button_->bounds().Contains(event.location())) {
      ShowSiteControls();
      return;
    }
    if (copy_button_->GetVisible() &&
        copy_button_->bounds().Contains(event.location())) {
      CopyAddress();
      return;
    }
    if (pinned_button_->GetVisible() && pinned_ids_.size() == 1 &&
        pinned_button_->bounds().Contains(event.location())) {
      RunExtension(pinned_ids_[0]);
      return;
    }
  }
  if (location_bar_ && !editing_ && GetFieldBounds().Contains(event.location())) {
    compact_command_bar_ = true;
    OpenCommandBar(/*new_tab=*/false);
  }
#endif
}

void AlerseAddressBar::OnPaint(gfx::Canvas* canvas) {
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
      color_utils::BlendTowardMaxContrast(alerse::FrameColor(this), 0x1B));
  canvas->DrawRoundRect(gfx::RectF(field), 12, flags);
  canvas->DrawImageInt(
      gfx::CreateVectorIcon(vector_icons::kAlerseSearchIcon, 16,
                            colors->GetColor(kColorAlerseSidebarNewTab)),
      field.x() + 10, field.y() + (field.height() - 16) / 2);
  canvas->DrawStringRect(
      field_text_, field_font_,
      colors->GetColor(field_placeholder_ ? kColorOmniboxTextDimmed
                                          : kColorOmniboxText),
      gfx::Rect(field.x() + 34, field.y(), std::max(0, field.width() - 42),
                field.height()));
#endif
}

void AlerseAddressBar::OnViewRemovedFromWidget(views::View* observed_view) {
  // As the window closes, or as fullscreen moves the toolbar to another
  // widget: the field takes the model again when it next takes the location
  // bar.
  if (observed_view == extensions_container_) {
    ReleaseExtensions(/*model_exists=*/true);
  }
}

void AlerseAddressBar::ReleaseExtensions(bool model_exists) {
  if (model_exists && extensions_model_) {
    extensions_model_->RemoveObserver(this);
  }
  extensions_model_ = nullptr;
  extensions_container_observation_.Reset();
  extensions_container_ = nullptr;
  // The menus point at the model's actions: they go with it. Emptied first:
  // a raw_ptr must not outlive its view.
  for (views::View* tile : std::exchange(extension_tiles_, {})) {
    RemoveChildViewT(tile);
  }
  pinned_button_->set_context_menu_controller(nullptr);
  pinned_button_->SetCallback(views::Button::PressedCallback());
  extension_menus_.clear();
  pinned_ids_.clear();
}

void AlerseAddressBar::OnViewIsDeleting(views::View* observed_view) {
  if (observed_view == extensions_container_) {
    // The container's destructor has freed the model already.
    ReleaseExtensions(/*model_exists=*/false);
    extensions_container_gone_ = true;
    return;
  }
  if (observed_view == edit_placeholder_) {
    edit_placeholder_observation_.Reset();
    edit_placeholder_ = nullptr;
    return;
  }
  placeholder_observation_.Reset();
  toolbar_placeholder_ = nullptr;
}

void AlerseAddressBar::OpenCommandBar(bool new_tab) {
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
  location_bar->SetAlerseNewTab(new_tab);
  // Focus opens the command bar (UpdateEditing).
  location_bar->FocusLocation(/*is_user_initiated=*/true,
                              /*clear_focus_if_failed=*/false);
}

LocationBarView* AlerseAddressBar::GetEditableLocationBar() const {
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

void AlerseAddressBar::OnDidChangeFocus(views::View* focused_before,
                                          views::View* focused_now) {
  // Focus going nowhere, as when the window deactivates, keeps the field: the
  // address gets focus back with the window.
  if (!moving_ && focused_now && (editing_ || GetEditableLocationBar())) {
    ScheduleUpdateEditing();
  }
}

void AlerseAddressBar::OnWindowClick() {
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
      FROM_HERE, base::BindOnce(&AlerseAddressBar::CloseCommandBar,
                                weak_factory_.GetWeakPtr()));
}

void AlerseAddressBar::CloseCommandBar() {
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

void AlerseAddressBar::OnWidgetDestroying(views::Widget* widget) {
  if (widget != editor_.get()) {
    // The window itself.
    window_observation_.Reset();
    return;
  }
  StopEditing();
  editor_->RemoveObserver(this);
}

void AlerseAddressBar::ScheduleUpdateEditing() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AlerseAddressBar::UpdateEditing,
                                weak_factory_.GetWeakPtr()));
}

void AlerseAddressBar::UpdateEditing() {
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

void AlerseAddressBar::StartEditing(LocationBarView* location_bar) {
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
    params.name = "AlerseAddressEditor";
    params.parent = widget->GetNativeView();
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    // Not activatable, like the find bar: the window keeps the keyboard and
    // its focus manager routes it here.
    editor_->Init(std::move(params));
    // The panel's top: the suggestions' color, a 1 px rim and round corners,
    // around the address inset as far as the suggestions' frame reaches out
    // of it, so the two make one panel.
    auto contents = std::make_unique<AlerseCommandBarPanel>();
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
  alerse::motion::FadeInWidget(editor_.get(), /*activate=*/true);
  moving_ = false;
  if (!click_watcher_) {
    click_watcher_ = std::make_unique<ClickWatcher>(this);
  }
  click_monitor_ = views::EventMonitor::CreateWindowMonitor(
      click_watcher_.get(), widget->GetNativeWindow(),
      {ui::EventType::kMousePressed, ui::EventType::kTouchPressed});
  location_bar->SetAlerseCommandBarCompact(compact);
  location_bar->SetAlerseCommandBar(true);
  static_cast<AlerseCommandBarPanel*>(editor_->GetContentsView())
      ->set_draws_divider(!compact);
  UpdateCopyButton();

  location_bar->FocusLocation(/*is_user_initiated=*/true,
                              /*clear_focus_if_failed=*/false);
  // An empty field (a new tab, an empty Space) asks for rows itself: focus
  // moved with the field to the panel drops the on-focus suggestions. So
  // does a new split's pane on "Choose a tab" (alerse_split.h). On a page,
  // the compact command bar (a click on the field) and the wide one
  // (Command-L) both open on the page's address, selected, as Arc's: typing
  // replaces it and Command-C copies it.
  if (location_bar->AlerseOpensNewTab() ||
      location_bar->omnibox_view()->GetText().empty() ||
      alerse::IsSplitChooser(
          browser_->GetTabStripModel()->GetActiveWebContents())) {
    location_bar->omnibox_view()->SetUserText(std::u16string());
  } else {
    location_bar->omnibox_view()->SelectAll(/*reversed=*/true);
  }
  SchedulePaint();
  if (command_bar_callback_) {
    command_bar_callback_.Run(true, location_bar->AlerseOpensNewTab());
  }
}

void AlerseAddressBar::StopEditing() {
  if (!editing_) {
    return;
  }
  click_monitor_.reset();
  moving_ = true;
  editing_ = false;
  LocationBarView* location_bar = editing_location_bar_;
  editing_location_bar_ = nullptr;
  location_bar->SetAlerseCommandBar(false);
  location_bar->SetAlerseCommandBarCompact(false);
  location_bar->SetAlerseNewTab(false);
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

void AlerseAddressBar::ScheduleUpdatePlacement() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AlerseAddressBar::UpdatePlacement,
                                weak_factory_.GetWeakPtr()));
}

void AlerseAddressBar::UpdatePlacement() {
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
    location_bar->SetAlerseCompact(sidebar_drawn);
    location_bar->SetAlerseHovered(sidebar_drawn);
    location_bar->SetAlerseCentered(sidebar_drawn);
  }
}

void AlerseAddressBar::TakeLocationBar() {
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
  location_bar_->SetAlerseCompact(true);
  location_bar_->SetAlerseHovered(hovered_);
  ObserveExtensions();
  UpdateCopyButton();
  PreferredSizeChanged();
}

void AlerseAddressBar::ReturnLocationBar() {
  if (!location_bar_) {
    return;
  }
  StopEditing();
  LocationBarView* location_bar = location_bar_;
  location_bar_ = nullptr;
#if BUILDFLAG(IS_MAC)
  location_bar->SetCanProcessEventsWithinSubtree(true);
#endif
  location_bar->SetAlerseCompact(false);
  views::View* placeholder = toolbar_placeholder_;
  views::View* toolbar = placeholder ? placeholder->parent() : nullptr;
  placeholder_observation_.Reset();
  toolbar_placeholder_ = nullptr;
  if (!toolbar) {
    return;
  }
  toolbar->AddChildViewAt(location_bar, *toolbar->GetIndexOf(placeholder));
  toolbar->RemoveChildViewT(placeholder);
  location_bar->SetAlerseTrailingInset(0);
  UpdateCopyButton();
  PreferredSizeChanged();
}

void AlerseAddressBar::UpdateCopyButton() {
  // As in Arc: while the pointer is anywhere on the sidebar; not while the
  // command bar has the field, nor on an empty Space's blank page, which has
  // no address to copy.
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  const bool show = (hovered_ || sidebar_hovered_) && location_bar_ &&
                    !editing_ && contents &&
                    !TabStripModel::IsAlerseEmptySpace(contents);
  // They fade: popping in as the pointer crosses the sidebar's edge reads
  // as a flicker.
  buttons_shown_ = show;
  pinned_shown_ = pinned_ids_.size() == 1 && location_bar_ && !editing_ &&
                  contents && !TabStripModel::IsAlerseEmptySpace(contents) &&
                  (show || pinned_notice_);
  alerse::motion::SetShown(copy_button_, show);
  alerse::motion::SetShown(site_button_, show);
  // A single pinned extension stays in the field without the pointer while
  // it has something to say (a badge, or it was blocked on this page): shown
  // only on hover, an ad blocker's count would never be seen.
  alerse::motion::SetShown(pinned_button_, pinned_shown_);
  alerse::motion::SetShown(text_fade_, show || pinned_shown_);
  static_cast<TextFade*>(text_fade_.get())
      ->set_shade(hovered_ ? kTileHoverShade : kTileShade);
  InvalidateLayout();
  if (location_bar_) {
    location_bar_->SetAlerseHovered(hovered_);
    // The address runs on under the fade rather than eliding before it.
    location_bar_->SetAlerseTrailingInset(0);
  }
}

void AlerseAddressBar::ShowSiteControls() {
  // Anchored to the sliders, as Arc's; through the extensions button, which
  // owns the panel, else the location bar's own way (page info).
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  ExtensionsToolbarDesktop* container =
      browser_view && browser_view->toolbar()
          ? browser_view->toolbar()->extensions_container()
          : nullptr;
  if (container && container->GetExtensionsButton()) {
    container->GetExtensionsButton()->ToggleAlerseSiteControls(site_button_);
    return;
  }
  if (location_bar_) {
    location_bar_->ShowAlerseSiteControlsForField();
  }
}

void AlerseAddressBar::ObserveExtensions() {
  if (extensions_model_ || extensions_container_gone_) {
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  ExtensionsToolbarDesktop* container =
      browser_view && browser_view->toolbar()
          ? browser_view->toolbar()->extensions_container()
          : nullptr;
  extensions_model_ = container ? container->GetToolbarViewModel() : nullptr;
  if (!extensions_model_) {
    return;
  }
  extensions_model_->AddObserver(this);
  extensions_container_ = container;
  extensions_container_observation_.Observe(container);
  UpdateExtensions();
}

void AlerseAddressBar::UpdateExtensions() {
  std::vector<std::string> ids;
  if (extensions_model_ && extensions_model_->AreActionsInitialized()) {
    for (const auto& id : extensions_model_->GetPinnedActionIds()) {
      if (extensions_model_->GetActionModelForId(id)) {
        ids.push_back(id);
      }
    }
  }
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  const int rows_before = GetTileRows();
  pinned_ids_ = ids;
  // Emptied first: a raw_ptr must not outlive its view.
  for (views::View* tile : std::exchange(extension_tiles_, {})) {
    RemoveChildViewT(tile);
  }
  pinned_button_->set_context_menu_controller(nullptr);
  extension_menus_.clear();
  pinned_notice_ = false;
  if (ids.size() == 1) {
    ToolbarActionViewModel* action =
        extensions_model_->GetActionModelForId(ids[0]);
    const ui::ImageModel icon = action->GetIcon(
        contents, gfx::Size(kExtensionImageSize, kExtensionImageSize));
    for (const auto state : {views::Button::STATE_NORMAL,
                             views::Button::STATE_HOVERED,
                             views::Button::STATE_PRESSED}) {
      pinned_button_->SetImageModel(state, icon);
    }
    pinned_button_->SetCallback(
        base::BindRepeating(&AlerseAddressBar::RunExtension,
                            base::Unretained(this), ids[0]));
    alerse::SetArcTooltip(pinned_button_, action->GetActionName());
    pinned_notice_ = HasNotice(browser_->GetProfile(), ids[0], contents);
    // Its menu on a right click (Unpin, Options, Remove...), as Arc's.
    extension_menus_.push_back(
        alerse_site_controls::CreateExtensionMenu(action, base::DoNothing()));
    pinned_button_->set_context_menu_controller(extension_menus_.back().get());
    pinned_button_->GetViewAccessibility().SetName(action->GetActionName());
  } else if (ids.size() > 1) {
    for (const auto& id : ids) {
      ToolbarActionViewModel* action =
          extensions_model_->GetActionModelForId(id);
      auto tile = std::make_unique<ExtensionTile>(
          base::BindRepeating(&AlerseAddressBar::RunExtension,
                              base::Unretained(this), id),
          action->GetActionName(),
          action->GetIcon(contents,
                          gfx::Size(kExtensionImageSize, kExtensionImageSize)));
      extension_menus_.push_back(
          alerse_site_controls::CreateExtensionMenu(action, base::DoNothing()));
      tile->set_context_menu_controller(extension_menus_.back().get());
      extension_tiles_.push_back(AddChildView(std::move(tile)));
    }
  }
  UpdateCopyButton();
  if (GetTileRows() != rows_before) {
    PreferredSizeChanged();
  }
  InvalidateLayout();
  SchedulePaint();
}

void AlerseAddressBar::RunExtension(const ToolbarActionsModel::ActionId& id) {
  if (extensions_model_) {
    extensions_model_->ExecuteUserAction(
        id, ToolbarActionViewModel::InvocationSource::kMenuEntry);
  }
}

void AlerseAddressBar::OnActionsInitialized() {
  UpdateExtensions();
}

void AlerseAddressBar::OnActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  UpdateExtensions();
}

void AlerseAddressBar::OnActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  UpdateExtensions();
}

void AlerseAddressBar::OnActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  // The icon follows the page (an extension's badge or its disabled look).
  if (std::ranges::contains(pinned_ids_, action_id)) {
    UpdateExtensions();
  }
}

void AlerseAddressBar::OnPinnedActionsChanged() {
  UpdateExtensions();
}

void AlerseAddressBar::OnActiveWebContentsChanged(
    bool is_same_document,
    content::WebContents* web_contents) {
  if (!is_same_document && !pinned_ids_.empty()) {
    UpdateExtensions();
  }
  // The buttons follow the page: an empty Space has none.
  UpdateCopyButton();
}

void AlerseAddressBar::CopyAddress() {
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!contents || contents->GetVisibleURL().is_empty()) {
    return;
  }
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::UTF8ToUTF16(contents->GetVisibleURL().spec()));
  // Shows that it copied: a check mark in place of the link for a moment.
  ShowCopied(true);
  copy_feedback_timer_.Start(
      FROM_HERE, base::Milliseconds(1500),
      base::BindOnce(&AlerseAddressBar::ShowCopied, base::Unretained(this),
                     false));
}

void AlerseAddressBar::ShowCopied(bool copied) {
  const ui::ImageModel image = ui::ImageModel::FromImageSkia(
      gfx::CanvasImageSource::MakeImageSkia<HoverGlyphSource>(
          copied ? HoverGlyph::kCheck : HoverGlyph::kLink, kHoverIconColor));
  for (const auto state : {views::Button::STATE_NORMAL,
                           views::Button::STATE_HOVERED,
                           views::Button::STATE_PRESSED}) {
    copy_button_->SetImageModel(state, image);
  }
  if (!copied || !copy_button_->layer()) {
    return;
  }
  // The check mark grows in: the answer to the click.
  ui::Layer* layer = copy_button_->layer();
  layer->SetOpacity(0.0f);
  if (!alerse::motion::Move(alerse::motion::kQuick).is_zero()) {
    layer->SetTransform(gfx::TransformAboutPivot(
        gfx::PointF(copy_button_->GetLocalBounds().CenterPoint()),
        gfx::Transform::MakeScale(0.7f)));
  }
  views::AnimationBuilder()
      .SetPreemptionStrategy(alerse::motion::kRetarget)
      .Once()
      .SetDuration(alerse::motion::Fade(alerse::motion::kQuick))
      .SetOpacity(layer, 1.0f, alerse::motion::kEnter)
      .SetTransform(layer, gfx::Transform(), alerse::motion::kEnter);
}

BEGIN_METADATA(AlerseAddressBar)
END_METADATA
