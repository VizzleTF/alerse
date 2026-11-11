// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/alerse_sidebar_view.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_address_bar.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "build/build_config.h"
#include "base/functional/bind.h"
#include "base/i18n/unicodestring.h"
#include "base/location.h"
#include "base/uuid.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/alerse_space_folder.h"
#include "chrome/browser/ui/tabs/alerse_space_registry.h"
#include "chrome/browser/ui/tabs/alerse_window_stack.h"
#include "chrome/browser/ui/tabs/alerse_spaces.h"
#include "chrome/browser/ui/tabs/alerse_sidebar_drag.h"
#include "chrome/browser/ui/tabs/alerse_split.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/alerse_command_bar_hook.h"
#include "chrome/browser/ui/views/frame/alerse_frame_color.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_glyphs.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_mini_player.h"
#include "ui/views/view_utils.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_popover.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_space_switcher.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_tooltip.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_alert.h"
#include "components/tabs/public/tab_group.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/vector_icons/vector_icons.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/token.h"
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/views/paint_info.h"
#include "ui/base/models/image_model.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/display/screen.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/canvas_image_source.h"
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
#include "ui/views/focus/focus_manager.h"
#include "ui/views/metrics.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "base/i18n/case_conversion.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/reldatefmt.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/tabs/vertical/alerse_share_mac.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_window_mac.h"
#endif

namespace {

// The text of a dragged row's data, which only this view accepts.
constexpr char16_t kRowDragData[] = u"alerse-sidebar-row";

constexpr char kSpaceMetaKey[] = "arcomium.space";
constexpr char kOpenMetaKey[] = "arcomium.open";
constexpr char kIconMetaKey[] = "arcomium.icon";
constexpr char kFolderIconMetaKey[] = "arcomium.folder_icon";
// A pinned item's or favorite's glyph from Change Icon..., in place of its
// favicon: a Space icon's name.
constexpr char kGlyphMetaKey[] = "arcomium.glyph";
// A pinned item the user named (Rename..., or Arc's own name imported).
constexpr char kRenamedMetaKey[] = "arcomium.renamed";
// The favorites' folder, beside the Spaces' folders; and a Space folder whose
// pinned tabs are collapsed under its name.
constexpr char kFavoritesMetaKey[] = "arcomium.favorites";
constexpr char kCollapsedMetaKey[] = "arcomium.collapsed";
constexpr char kDefaultSpace[] = "default";

// Arc's favorites: tiles above the Space's name, 47 high, 8 apart, up to
// four in a row (measured on Arc for macOS: two tiles 70x47 in its 148 pt).
constexpr int kFavoriteHeight = 47;
constexpr int kFavoriteGap = 8;
constexpr int kFavoritesPerRow = 4;
constexpr int kFavoritesTop = 4;
constexpr int kFavoritesBottom = 6;
constexpr float kFavoriteRadius = 12;
// Shades off the frame: #2F2F2F, hovered #383839 on Arc's #161616.
constexpr SkAlpha kFavoriteShade = 0x1B;
constexpr SkAlpha kFavoriteHoverShade = 0x25;
constexpr SkAlpha kFavoriteActiveShade = 0x40;

// Measured on Arc for macOS (in pt), used on every platform (a point is a
// DIP on Windows and Linux): rows 41 apart with a 148x36 highlight
// with 12 pt corners, favicons at x = 18, 19 pt folders, nested rows 16 in.
// The Space's name is a row of its own with the Space's icon; no divider
// before New Tab.
constexpr float kRowPitch = 41;
constexpr int kIconX = 18;
constexpr int kFolderGlyphSize = 22;
constexpr int kOpenFolderGlyphWidth = 24;
constexpr float kFolderGlyphScale = 19 / 16.5f;
constexpr float kFolderOutlineScale = 19 / 18.0f;
constexpr SkColor kFolderInside = SkColorSetRGB(0x05, 0x05, 0x05);
// Space bar's icons are 13 pt; the Space's name's 15 pt, in the active white.
constexpr float kSpaceTitleIconScale = 15 / 17.0f;
// Text ends 11 pt short of the highlight's edge, where Arc elides it.
constexpr int kTextEndInset = 11;
constexpr int kIndent = 16;
constexpr float kRowBackgroundInsetY = 2.5f;
constexpr float kRowBackgroundRadius = 12;
constexpr int kSpaceTitleTextX = 44;
constexpr int kSpaceTitleIconX = 26;
constexpr int kSpaceTitleFontSize = 13;
constexpr SkColor kSpaceTitleColor = SkColorSetRGB(0x80, 0x80, 0x80);
constexpr int kTextX = 44;
constexpr int kIconSize = 16;
constexpr int kItemFontSize = 14;
constexpr int kRowBackgroundInsetX = 8;
// The speaker of a tab playing sound, and the gap after it.
constexpr int kSpeakerSize = 16;
constexpr int kSpeakerGap = 6;
// A split row's halves: the end half's icon this far after the middle, the
// start half's text ending this far before it.
constexpr int kSplitHalfGap = 4;

// The colors (chrome/browser/ui/color/alerse_color_mixer.cc).
constexpr ui::ColorId kItemColor = kColorAlerseSidebarItem;
constexpr ui::ColorId kFolderColor = kColorAlerseSidebarFolder;
constexpr ui::ColorId kSubtleColor = kColorAlerseSidebarSubtle;
constexpr ui::ColorId kNewTabColor = kColorAlerseSidebarNewTab;
constexpr ui::ColorId kPlusColor = kColorAlerseSidebarPlus;
constexpr ui::ColorId kGlyphColor = kColorAlerseSidebarGlyph;
constexpr ui::ColorId kGlyphFillColor = kColorAlerseSidebarGlyphFill;
constexpr ui::ColorId kHoverColor = kColorAlerseSidebarHover;
constexpr ui::ColorId kActiveColor = kColorAlerseSidebarActive;

// Animations: folders opening and closing, the folder popup fading in.
constexpr base::TimeDelta kFolderAnimationDuration = base::Milliseconds(150);
constexpr base::TimeDelta kPopupFadeDuration = base::Milliseconds(120);

// `duration`, or none where the system asks for reduced motion.
base::TimeDelta Motion(base::TimeDelta duration) {
  return gfx::Animation::PrefersReducedMotion() ? base::TimeDelta() : duration;
}

// Folder popup: hover delay, size, color and pointer polling.
// Arc for macOS shows a folder's links 0.3 to 0.6 s into the hover.
constexpr base::TimeDelta kFolderPopupDelay = base::Milliseconds(400);
// Arc for macOS' popup: 240 pt wide, 4 pt right of the sidebar and centred on
// its folder's row; a 48 pt search row over a line, then rows 45 pt apart
// with the link's title and when it was last opened, the first selected.
// Longer lists scroll inside 320 pt.
constexpr int kPopupWidth = 240;
constexpr views::BubbleBorder::Arrow kPopupArrow =
    views::BubbleBorder::LEFT_CENTER;
constexpr int kPopupGap = 4;
constexpr int kPopupRadius = 12;
constexpr int kPopupSearchHeight = 49;
constexpr int kPopupRowPitch = 45;
constexpr int kPopupMaxHeight = 320;
constexpr int kPopupIconCenterX = 21;
constexpr int kPopupTextX = 36;
constexpr int kPopupTextEndInset = 13;
constexpr SkColor kPopupBackground = SkColorSetRGB(0x1A, 0x1A, 0x19);
// Arc's rim, #3D3D3D outside #4F4F4E, straight over what is behind: the
// popup's window has no macOS shadow, whose dark outline and lightened edge
// Arc's does not show (RemoveWindowShadow()).
constexpr SkColor kPopupOuterRim = SkColorSetRGB(0x3D, 0x3D, 0x3D);
constexpr SkColor kPopupRim = SkColorSetRGB(0x4F, 0x4F, 0x4E);
constexpr SkColor kPopupLine = SkColorSetRGB(0x38, 0x38, 0x37);
constexpr SkColor kPopupSelected = SkColorSetRGB(0x2C, 0x2C, 0x2B);
constexpr SkColor kPopupTitle = SkColorSetRGB(0xD3, 0xD3, 0xD3);
constexpr SkColor kPopupDate = SkColorSetRGB(0x92, 0x92, 0x91);
constexpr SkColor kPopupGlyph = SkColorSetRGB(0x8C, 0x8C, 0x8C);
// The line Arc for macOS draws above the Space bar while the rows overflow:
// 1 pt #212121 on #161616.
constexpr SkAlpha kOverflowLineShade = 0x0C;
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
  kAddFavorite,
  kRemoveFavorite,
  kCopyAllLinks,
  kTurnIntoSpace,
  kChangeIcon,
  kResetPinnedTab,
  kShare,
  kShareFolder,
  // "Move to" submenu: one command per Space from here.
  kMoveToFirst = 100,
  // macOS' "Share" submenu: one command per sharing service from here.
  kShareFirst = 1000,
};

// Arc for macOS draws its sidebar in the system font, the same sizes and
// weights on every platform: SF Pro on macOS; on Windows the system's Segoe UI
// Variable ("Text" optical size; Segoe UI before Windows 11), regular for
// items and semibold for folders, as macOS', not Arc for Windows' semilight.
gfx::FontList ArcFont(int size, gfx::Font::Weight weight) {
#if BUILDFLAG(IS_WIN)
  return gfx::FontList({"Segoe UI Variable Text", "Segoe UI"},
                       gfx::Font::NORMAL, size, weight);
#else
  const gfx::FontList system;
  return system.DeriveWithSizeDelta(size - system.GetFontSize())
      .DeriveWithWeight(weight);
#endif
}

gfx::FontList ItemFont(bool bold) {
  return ArcFont(kItemFontSize, bold ? gfx::Font::Weight::SEMIBOLD
                                     : gfx::Font::Weight::NORMAL);
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
  // An elided label would add the system's tooltip under Arc's pill, which
  // its row shows already.
  label->SetHandlesTooltips(false);
  return label;
}

// Arc's folder glyph: a light frame with a thick top edge. A folder holding
// open tabs shows two dashes inside; otherwise its custom Arc icon, if any.
class FolderGlyph : public views::View {
  METADATA_HEADER(FolderGlyph, views::View)

 public:
  FolderGlyph(bool has_open_tabs, std::string custom_icon, bool open = false)
      : has_open_tabs_(has_open_tabs),
        custom_icon_(std::move(custom_icon)),
        open_(open) {
    // Arc for macOS' open folder is wider, 22.5 pt.
    SetPreferredSize(gfx::Size(open ? kOpenFolderGlyphWidth : kFolderGlyphSize,
                               kFolderGlyphSize));
    SetCanProcessEventsWithinSubtree(false);
  }

  // Arc's folder is a 17 px square, larger than a favicon; on macOS 19 pt.
  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const SkColor glyph = alerse::SidebarInk(this, kGlyphColor);
    const SkColor glyph_fill = alerse::SidebarInk(this, kGlyphFillColor);
    // Arc for macOS fills its folders near black.
    const SkColor inside = alerse::SidebarInk(this, kFolderInside);
    if (open_) {
      PaintOpenFolder(canvas, glyph, inside);
      return;
    }
    // Arc for macOS' folder: one 18.5 pt outline whose tab slants down into
    // the body, grey behind a bright front edge, near black below it.
    {
      gfx::ScopedCanvas scoped(canvas);
      canvas->sk_canvas()->translate(kFolderGlyphSize / 2.0f - 9.25f,
                                     kFolderGlyphSize / 2.0f - 9.3f);
      SkPathBuilder outline;
      outline.moveTo(0.75f, 3.2f)
          .quadTo(0.75f, 0.75f, 3.2f, 0.75f)
          .lineTo(7.4f, 0.75f)
          .quadTo(8.4f, 0.75f, 9.1f, 1.5f)
          .lineTo(9.9f, 2.3f)
          .quadTo(10.3f, 2.75f, 11, 2.75f)
          .lineTo(15.3f, 2.75f)
          .quadTo(17.75f, 2.75f, 17.75f, 5.2f)
          .lineTo(17.75f, 15.3f)
          .quadTo(17.75f, 17.75f, 15.3f, 17.75f)
          .lineTo(3.2f, 17.75f)
          .quadTo(0.75f, 17.75f, 0.75f, 15.3f)
          .close();
      const SkPath path = outline.detach();
      flags.setColor(inside);
      canvas->DrawPath(path, flags);
      canvas->sk_canvas()->save();
      canvas->ClipPath(path, /*do_anti_alias=*/true);
      flags.setColor(SkColorSetA(glyph, 0x8C));
      canvas->DrawRect(gfx::RectF(0, 0, 18.5f, 4.0f), flags);
      flags.setColor(glyph);
      canvas->DrawRect(gfx::RectF(0, 4.0f, 18.5f, 1.5f), flags);
      canvas->sk_canvas()->restore();
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(1.5f);
      flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
      canvas->DrawPath(path, flags);
      flags.setStyle(cc::PaintFlags::kFill_Style);
    }
    // The marks inside follow, drawn for Windows' 16.5 px folder.
    canvas->sk_canvas()->translate(kFolderGlyphSize / 2.0f,
                                   kFolderGlyphSize / 2.0f);
    canvas->sk_canvas()->scale(kFolderOutlineScale, kFolderOutlineScale);
    canvas->sk_canvas()->translate(-9, -9.25f);
    canvas->sk_canvas()->translate(9 - 8.75f * kFolderGlyphScale,
                                   9.25f - 9.25f * kFolderGlyphScale);
    canvas->sk_canvas()->scale(kFolderGlyphScale, kFolderGlyphScale);
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
    } else if (!custom_icon_.empty()) {
      // A glyph picked with Change Icon..., at half size in the body.
      gfx::ScopedCanvas scoped(canvas);
      canvas->sk_canvas()->translate(8.75f, 10.6f);
      canvas->sk_canvas()->scale(0.5f, 0.5f);
      alerse::PaintSpaceIcon(canvas, gfx::PointF(), custom_icon_, glyph);
    }
  }

 private:
  // Arc for macOS' open folder: a grey back panel leaning left behind a
  // slanted front, both outlined. Drawn from a 22.5x15.3 pt trace.
  void PaintOpenFolder(gfx::Canvas* canvas, SkColor glyph, SkColor inside) {
    // Its left edge 2 pt left of the closed folder's, as Arc's.
    canvas->sk_canvas()->translate(2, (height() - 15.3f) / 2 + 0.5f);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
    SkPathBuilder back;
    back.moveTo(0.65f, 0.65f)
        .lineTo(6.6f, 0.65f)
        .lineTo(8.4f, 2.4f)
        .lineTo(14.2f, 2.4f)
        .lineTo(14.6f, 4.0f)
        .lineTo(7.8f, 4.0f)
        .lineTo(4.4f, 14.6f)
        .lineTo(0.65f, 1.5f)
        .close();
    SkPathBuilder front;
    front.moveTo(7.8f, 4.0f)
        .lineTo(21.8f, 4.1f)
        .lineTo(18.3f, 14.65f)
        .lineTo(4.4f, 14.65f)
        .close();
    const SkPath back_path = back.detach();
    const SkPath front_path = front.detach();
    flags.setColor(SkColorSetA(glyph, 0x8C));
    canvas->DrawPath(back_path, flags);
    flags.setColor(inside);
    canvas->DrawPath(front_path, flags);
    flags.setColor(glyph);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.3f);
    canvas->DrawPath(back_path, flags);
    canvas->DrawPath(front_path, flags);
  }

  const bool has_open_tabs_;
  const std::string custom_icon_;
  [[maybe_unused]] const bool open_;
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

  // Arc for macOS brightens the "+" while its row is hovered, and draws it
  // as the text while the row is the command bar's new tab.
  void SetHovered(bool hovered) {
    hovered_ = hovered;
    SchedulePaint();
  }

  void SetActive(bool active) {
    active_ = active;
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(
        active_    ? alerse::SidebarInk(this, kItemColor)
        : hovered_ ? alerse::SidebarInk(this, SkColorSetRGB(0x95, 0x95, 0x95))
                   : alerse::SidebarInk(this, kPlusColor));
    // Arc for macOS' is 11.5 pt, a point left of Windows'.
    canvas->DrawRect(gfx::RectF(2, 7.4f, 11.5f, 1.2f), flags);
    canvas->DrawRect(gfx::RectF(7.15f, 2.25f, 1.2f, 11.5f), flags);
  }

 private:
  bool hovered_ = false;
  bool active_ = false;
};

BEGIN_METADATA(PlusGlyph)
END_METADATA

// A shade off the frame (the Space's tint), as Arc's tiles and plates are.
SkColor FrameShade(const views::View* view, SkAlpha alpha) {
  return color_utils::BlendTowardMaxContrast(alerse::FrameColor(view),
                                              alpha);
}

// The end of the pinned items, for drag and drop. While the Space has tabs
// below, it is Arc's line with Clear at its right end, which closes them.
class Divider : public views::View {
  METADATA_HEADER(Divider, views::View)

 public:
  // `clear` is null while there is nothing to clear: no line then, as in Arc
  // for macOS.
  Divider(base::RepeatingClosure clear, const std::u16string& text)
      : clear_(std::move(clear)), text_(text), font_(ArcFont(12, gfx::Font::Weight::MEDIUM)) {
    if (!clear_) {
      SetPreferredSize(gfx::Size());
      return;
    }
    SetPreferredSize(gfx::Size(0, kHeight));
    GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
    GetViewAccessibility().SetName(text_);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    if (!clear_) {
      return;
    }
    const gfx::Rect button = ButtonBounds();
    const float mid = height() / 2.0f;
    canvas->FillRect(gfx::Rect(kInset, height() / 2,
                               std::max(0, button.x() - kGap - kInset), 1),
                     FrameShade(this, 0x1F));
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.2f);
    const SkColor ink = alerse::SidebarInk(
        this, hovered_ ? SkColorSetRGB(0xC4, 0xC4, 0xC4)
                       : SkColorSetRGB(0x8B, 0x8B, 0x8B));
    flags.setColor(ink);
    // The arrow down: a stem and its head.
    const float x = button.x() + kArrow / 2.0f;
    canvas->DrawLine(gfx::PointF(x, mid - 4.5f), gfx::PointF(x, mid + 4), flags);
    canvas->DrawLine(gfx::PointF(x - 3.5f, mid + 0.5f), gfx::PointF(x, mid + 4),
                     flags);
    canvas->DrawLine(gfx::PointF(x + 3.5f, mid + 0.5f), gfx::PointF(x, mid + 4),
                     flags);
    canvas->DrawStringRectWithFlags(
        text_, font_, ink,
        gfx::Rect(button.x() + kArrow + 4, 0, button.width() - kArrow - 4,
                  height()),
        gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::NO_SUBPIXEL_RENDERING);
  }

  void OnMouseMoved(const ui::MouseEvent& event) override {
    SetHovered(clear_ && ButtonBounds().Contains(event.location()));
  }
  void OnMouseExited(const ui::MouseEvent& event) override { SetHovered(false); }
  bool OnMousePressed(const ui::MouseEvent& event) override {
    return clear_ && event.IsOnlyLeftMouseButton() &&
           ButtonBounds().Contains(event.location());
  }
  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (clear_ && ButtonBounds().Contains(event.location())) {
      // Copied: the rows, and this view with them, are rebuilt as tabs close.
      base::RepeatingClosure clear = clear_;
      clear.Run();
    }
  }

 private:
  static constexpr int kHeight = 28;
  static constexpr int kInset = 12;
  static constexpr int kGap = 10;
  static constexpr int kArrow = 9;

  // The arrow and the word, at the right end.
  gfx::Rect ButtonBounds() const {
    const int width = kArrow + 4 + gfx::Canvas::GetStringWidth(text_, font_);
    return gfx::Rect(this->width() - kInset - width, 0, width, height());
  }

  void SetHovered(bool hovered) {
    if (hovered_ != hovered) {
      hovered_ = hovered;
      SchedulePaint();
    }
  }

  base::RepeatingClosure clear_;
  const std::u16string text_;
  const gfx::FontList font_;
  bool hovered_ = false;
};

BEGIN_METADATA(Divider)
END_METADATA

// Arc for macOS' line over the Space bar while the rows overflow: a point
// high (the indicator's whole height), barely off the frame.
class OverflowLine : public views::View {
  METADATA_HEADER(OverflowLine, views::View)

 public:
  OverflowLine() = default;

  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), color_utils::BlendTowardMaxContrast(
                                           alerse::FrameColor(this),
                                           kOverflowLineShade));
  }
};

BEGIN_METADATA(OverflowLine)
END_METADATA

// Arc for macOS' Space name: a row with the Space's icon, in semibold grey.
// Hovered, it shows a plate, a chevron that collapses the pinned tabs in
// place of the icon, and "..." for the Space's menu at its end.
class SpaceTitle : public views::View {
  METADATA_HEADER(SpaceTitle, views::View)

 public:
  SpaceTitle(const std::u16string& title,
             std::string icon,
             bool collapsed,
             base::RepeatingClosure toggle_collapsed,
             base::RepeatingCallback<void(const gfx::Point&)> show_menu)
      : icon_(std::move(icon)),
        collapsed_(collapsed),
        toggle_collapsed_(std::move(toggle_collapsed)),
        show_menu_(std::move(show_menu)) {
    label_ = AddChildView(MakeLabel(
        title, ArcFont(kSpaceTitleFontSize, gfx::Font::Weight::SEMIBOLD),
        kSubtleColor));
    label_->SetCanProcessEventsWithinSubtree(false);
    GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
    GetViewAccessibility().SetName(title);
    SetPreferredSize(gfx::Size(0, static_cast<int>(kRowPitch)));
  }

  void Layout(PassKey) override {
    // A rebuild under the pointer: hovered at once.
    if (!size().IsEmpty() && !hover_checked_) {
      hover_checked_ = true;
      hovered_ = IsMouseHovered();
      UpdateLabelColor();
    }
    label_->SetBounds(kSpaceTitleTextX, 0,
                      std::max(0, MoreButtonX() - 12 - kSpaceTitleTextX),
                      height());
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    UpdateLabelColor();
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    hovered_ = true;
    UpdateLabelColor();
    SchedulePaint();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    hovered_ = false;
    more_hovered_ = false;
    UpdateLabelColor();
    SchedulePaint();
  }

  void OnMouseMoved(const ui::MouseEvent& event) override {
    const bool more = event.x() >= MoreButtonX() - 12;
    if (more != more_hovered_) {
      more_hovered_ = more;
      SchedulePaint();
    }
  }

  // While the Space's icon picker is open: as in Arc the name is not lit
  // for it (only the pointer lights it, keeping the icon rather than the
  // chevron) and shows no tooltip.
  void SetHighlighted(bool highlighted) {
    highlighted_ = highlighted;
    PollPointer();
    // The picker has the keyboard, and macOS sends this window no mouse
    // moves meanwhile: the pointer is polled, as Arc's name still lights.
    if (highlighted_) {
      pointer_poll_.Start(FROM_HERE, base::Milliseconds(50), this,
                          &SpaceTitle::PollPointer);
    } else {
      pointer_poll_.Stop();
    }
    // The picker covers the name: drop "Space Options" shown before it.
    TooltipTextChanged();
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    return event.IsOnlyLeftMouseButton();
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (!HitTestPoint(event.location())) {
      return;
    }
    if (event.x() < kSpaceTitleTextX - 4) {
      toggle_collapsed_.Run();
    } else if (event.x() >= MoreButtonX() - 12) {
      gfx::Point point(MoreButtonX(), height());
      views::View::ConvertPointToScreen(this, &point);
      show_menu_.Run(point);
    }
  }

  std::u16string GetRenderedTooltipText(const gfx::Point& p) const override {
    if (highlighted_) {
      return std::u16string();
    }
    if (p.x() < kSpaceTitleTextX - 4) {
      return l10n_util::GetStringUTF16(collapsed_ ? IDS_ALERSE_EXPAND_PINNED
                                                  : IDS_ALERSE_COLLAPSE_PINNED);
    }
    if (p.x() >= MoreButtonX() - 12) {
      return l10n_util::GetStringUTF16(IDS_ALERSE_SPACE_OPTIONS);
    }
    return std::u16string();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const gfx::PointF icon_center(kSpaceTitleIconX, height() / 2.0f);
    auto paint_icon = [&] {
      // The Space's icon, 15 pt in the active white.
      gfx::ScopedCanvas scoped(canvas);
      canvas->sk_canvas()->translate(icon_center.x(), icon_center.y());
      canvas->sk_canvas()->scale(kSpaceTitleIconScale, kSpaceTitleIconScale);
      alerse::PaintSpaceIcon(
          canvas, gfx::PointF(), icon_,
          alerse::SidebarInk(this, kColorAlerseSpaceIconActive));
    };
    if (!hovered_) {
      paint_icon();
      return;
    }
    // Arc's plate: 148x36 with 12 pt corners, #2F2F2F on #161616.
    flags.setColor(FrameShade(this, kFavoriteShade));
    gfx::RectF plate(0, 0, width(), height());
    plate.Inset(gfx::InsetsF::TLBR((height() - 36) / 2.0f - 0.5f,
                                   kRowBackgroundInsetX,
                                   (height() - 36) / 2.0f + 0.5f,
                                   kRowBackgroundInsetX));
    canvas->DrawRoundRect(plate, kRowBackgroundRadius, flags);
    // Under the pointer "..." takes a plate of its own: Arc's is 24 pt
    // square, #3F3F3F on the row's #2F2F2F.
    if (more_hovered_) {
      flags.setColor(SkColorSetA(SK_ColorWHITE, 0x14));
      canvas->DrawRoundRect(
          gfx::RectF(MoreButtonX() - 12, height() / 2.0f - 12.5f, 24, 24), 6,
          flags);
    }
    // Arc's chevron and "..." here are #ABABAB, a shade over the Space bar's.
    flags.setColor(alerse::SidebarInk(this, SkColorSetRGB(0xAB, 0xAB, 0xAB)));
    PaintMore(canvas, flags);
    // While its icon picker is open, the name keeps its icon, as Arc's does.
    if (highlighted_) {
      paint_icon();
      return;
    }
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    // Arc's chevron: 11 pt wide, a 1.9 pt stroke.
    flags.setStrokeWidth(1.9f);
    flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
    flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
    // Down while the pinned tabs show, right while they are collapsed.
    SkPathBuilder chevron;
    if (collapsed_) {
      chevron.moveTo(icon_center.x() - 2.25f, icon_center.y() - 4.5f)
          .lineTo(icon_center.x() + 2.25f, icon_center.y())
          .lineTo(icon_center.x() - 2.25f, icon_center.y() + 4.5f);
    } else {
      chevron.moveTo(icon_center.x() - 4.5f, icon_center.y() - 2.25f)
          .lineTo(icon_center.x(), icon_center.y() + 2.25f)
          .lineTo(icon_center.x() + 4.5f, icon_center.y() - 2.25f);
    }
    canvas->DrawPath(chevron.detach(), flags);
  }

 private:
  // "...": three 3 pt dots, 5 pt apart, as the header's.
  void PaintMore(gfx::Canvas* canvas, cc::PaintFlags flags) const {
    flags.setStyle(cc::PaintFlags::kFill_Style);
    for (int i = -1; i <= 1; ++i) {
      canvas->DrawCircle(
          gfx::PointF(MoreButtonX() + 5 * i, height() / 2.0f - 0.5f), 1.5f,
          flags);
    }
  }

  // The centre of "...", 18 pt from the plate's end as Arc's.
  int MoreButtonX() const { return width() - kRowBackgroundInsetX - 18; }

  void PollPointer() {
    // Rebuilt rows are set up before they join the window.
    if (!GetWidget()) {
      return;
    }
    gfx::Point cursor = display::Screen::Get()->GetCursorScreenPoint();
    views::View::ConvertPointFromScreen(this, &cursor);
    const bool hovered = HitTestPoint(cursor);
    const bool more = hovered && cursor.x() >= MoreButtonX() - 12;
    if (hovered != hovered_ || more != more_hovered_) {
      hovered_ = hovered;
      more_hovered_ = more;
      UpdateLabelColor();
      SchedulePaint();
    }
  }

  // Arc brightens the name a little under the pointer.
  void UpdateLabelColor() {
    if (GetColorProvider()) {
      label_->SetEnabledColor(alerse::SidebarInk(
          this, hovered_ ? SkColorSetRGB(0x8D, 0x8D, 0x8D) : kSpaceTitleColor));
    }
  }

  const std::string icon_;
  const bool collapsed_;
  base::RepeatingClosure toggle_collapsed_;
  base::RepeatingCallback<void(const gfx::Point&)> show_menu_;
  raw_ptr<views::Label> label_ = nullptr;
  bool hovered_ = false;
  bool more_hovered_ = false;
  bool highlighted_ = false;
  bool hover_checked_ = false;
  base::RepeatingTimer pointer_poll_;
};

BEGIN_METADATA(SpaceTitle)
END_METADATA

// Arc's favorite: a tile with the site's icon in its middle; lighter while
// hovered or while its tab shows.
class FavoriteTile : public views::Button {
  METADATA_HEADER(FavoriteTile, views::Button)

 public:
  FavoriteTile(PressedCallback callback,
               const ui::ImageModel& icon,
               const std::u16string& title,
               bool active)
      : views::Button(std::move(callback)), active_(active) {
    icon_ = AddChildView(std::make_unique<views::ImageView>(icon));
    icon_->SetImageSize(gfx::Size(kIconSize, kIconSize));
    icon_->SetCanProcessEventsWithinSubtree(false);
    GetViewAccessibility().SetName(title);
    SetInstallFocusRingOnFocus(false);
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON);
  }

  void Layout(PassKey) override {
    icon_->SetBounds((width() - kIconSize) / 2, (height() - kIconSize) / 2,
                     kIconSize, kIconSize);
  }

  void SetDragDoneCallback(base::RepeatingClosure callback) {
    drag_done_callback_ = std::move(callback);
  }

  void OnDragDone() override {
    views::Button::OnDragDone();
    if (drag_done_callback_) {
      drag_done_callback_.Run();
    }
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::Button::OnMouseEntered(event);
    SchedulePaint();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    views::Button::OnMouseExited(event);
    SchedulePaint();
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(FrameShade(this, active_ ? kFavoriteActiveShade
                                    : IsMouseHovered()
                                        ? kFavoriteHoverShade
                                        : kFavoriteShade));
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), kFavoriteRadius,
                          flags);
  }

 private:
  const bool active_;
  raw_ptr<views::ImageView> icon_ = nullptr;
  base::RepeatingClosure drag_done_callback_;
};

BEGIN_METADATA(FavoriteTile)
END_METADATA

// The favorites' grid: rows of up to four tiles filling the sidebar's width.
class FavoritesGrid : public views::View {
  METADATA_HEADER(FavoritesGrid, views::View)

 public:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int count = static_cast<int>(children().size());
    if (!count) {
      return gfx::Size();
    }
    const int rows = (count + kFavoritesPerRow - 1) / kFavoritesPerRow;
    return gfx::Size(0, kFavoritesTop + rows * kFavoriteHeight +
                            (rows - 1) * kFavoriteGap + kFavoritesBottom);
  }

  void Layout(PassKey) override {
    const int count = static_cast<int>(children().size());
    const int columns = std::min(count, kFavoritesPerRow);
    if (!columns) {
      return;
    }
    const float space = width() - 2 * kRowBackgroundInsetX;
    const float tile = (space - (columns - 1) * kFavoriteGap) / columns;
    for (int i = 0; i < count; ++i) {
      const float left = kRowBackgroundInsetX + (i % columns) * (tile + kFavoriteGap);
      const int x = static_cast<int>(std::lround(left));
      const int right = static_cast<int>(std::lround(left + tile));
      children()[i]->SetBounds(
          x, kFavoritesTop + (i / columns) * (kFavoriteHeight + kFavoriteGap),
          right - x, kFavoriteHeight);
    }
  }

  // Where a tile dropped at `point` lands among the tiles.
  size_t GetDropIndex(const gfx::Point& point) const {
    size_t index = 0;
    for (const views::View* child : children()) {
      if (point.y() >= child->bounds().bottom() ||
          (point.y() >= child->y() &&
           point.x() >= child->bounds().CenterPoint().x())) {
        ++index;
      }
    }
    return index;
  }
};

BEGIN_METADATA(FavoritesGrid)
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

// A Space glyph or an emoji as a pinned item's icon (Change Icon...).
class GlyphImageSource : public gfx::CanvasImageSource {
 public:
  GlyphImageSource(std::string name, SkColor color)
      : gfx::CanvasImageSource(gfx::Size(kIconSize, kIconSize)),
        name_(std::move(name)),
        color_(color) {}

  void Draw(gfx::Canvas* canvas) override {
    const gfx::PointF center(kIconSize / 2.0f, kIconSize / 2.0f);
    // An emoji from Arc's Emoji page, in its own colors at the favicon's
    // size.
    if (alerse::IsEmojiIcon(name_)) {
      alerse::PaintEmojiIcon(canvas, center, name_, 255, kEmojiSize);
      return;
    }
    alerse::PaintSpaceIcon(canvas, center, name_, color_);
  }

 private:
  // The emoji's face size: it then fills about the favicon's 16 pt.
  static constexpr int kEmojiSize = 14;
  const std::string name_;
  const SkColor color_;
};

// Arc for macOS' icon for a page without a favicon: an outlined globe, a
// 12.8 pt ring with one meridian and the equator in 1.25 pt strokes, not
// Chromium's filled one.
class BlankPageImageSource : public gfx::CanvasImageSource {
 public:
  explicit BlankPageImageSource(SkColor color)
      : gfx::CanvasImageSource(gfx::Size(kIconSize, kIconSize)),
        color_(color) {}

  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.25f);
    flags.setColor(color_);
    const gfx::PointF center(kIconSize / 2.0f, kIconSize / 2.0f);
    constexpr float kRadius = 6.4f;
    canvas->DrawCircle(center, kRadius, flags);
    canvas->sk_canvas()->drawOval(
        SkRect::MakeLTRB(center.x() - 2.35f, center.y() - kRadius,
                         center.x() + 2.35f, center.y() + kRadius),
        flags);
    canvas->DrawLine(gfx::PointF(center.x() - kRadius, center.y()),
                     gfx::PointF(center.x() + kRadius, center.y()), flags);
  }

 private:
  const SkColor color_;
};

// A page without a favicon of its own.
ui::ImageModel DefaultPageIcon(const views::View* view) {
  const SkColor color = view->GetColorProvider()
                            ? alerse::SidebarInk(
                                  view, SkColorSetRGB(0x8D, 0x8D, 0x8D))
                            : SK_ColorGRAY;
  return ui::ImageModel::FromImageSkia(
      gfx::CanvasImageSource::MakeImageSkia<BlankPageImageSource>(color));
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

// As in Arc, a tab shows its speaker while it plays and as long as it is
// muted. "Plays" is the tab strip's audio alert, which outlasts the short
// silences between tracks.
bool ShowsSpeaker(tabs::TabInterface* tab) {
  content::WebContents* contents = tab->GetContents();
  if (!contents) {
    return false;
  }
  if (contents->IsAudioMuted()) {
    return true;
  }
  const tabs::TabAlertController* alerts = tabs::TabAlertController::From(tab);
  return alerts && alerts->IsAlertActive(tabs::TabAlert::kAudioPlaying);
}

AlerseSidebarView* FindSidebarIn(views::View* view) {
  if (auto* sidebar = views::AsViewClass<AlerseSidebarView>(view)) {
    return sidebar;
  }
  for (views::View* child : view->children()) {
    if (AlerseSidebarView* sidebar = FindSidebarIn(child)) {
      return sidebar;
    }
  }
  return nullptr;
}

// Arc for macOS' popup rim: 1 px #3D3D3D outside 1 px #4F4F4E.
class PopupRimBorder : public views::Border {
 public:
  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped(canvas);
    const float scale = canvas->UndoDeviceScaleFactor();
    gfx::RectF bounds(gfx::ScaleRect(gfx::RectF(view.GetLocalBounds()), scale));
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    for (const auto& [inset, color] :
         {std::pair(0.5f, kPopupOuterRim), std::pair(1.5f, kPopupRim)}) {
      gfx::RectF ring = bounds;
      ring.Inset(inset);
      flags.setColor(alerse::SidebarInk(&view, color));
      canvas->DrawRoundRect(ring, kPopupRadius * scale - inset, flags);
    }
  }
  gfx::Insets GetInsets() const override { return gfx::Insets(); }
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
};

// The popup's search row: a magnifier, then the field.
class PopupSearchRow : public views::View {
  METADATA_HEADER(PopupSearchRow, views::View)

 public:
  explicit PopupSearchRow(std::unique_ptr<views::Textfield> field) {
    field_ = AddChildView(std::move(field));
    SetPreferredSize(gfx::Size(kPopupWidth, kPopupSearchHeight));
  }

  void Layout(PassKey) override {
    const int height = field_->GetPreferredSize().height();
    field_->SetBounds(kPopupTextX, 24 - height / 2 + 1,
                      kPopupWidth - kPopupTextX - kPopupTextEndInset, height);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    // Arc's magnifier: 12 pt, its middle 24.5 pt down and 20.5 pt in.
    gfx::ScopedCanvas scoped(canvas);
    canvas->sk_canvas()->translate(0.5f, 0);
    canvas->DrawImageInt(
        gfx::CreateVectorIcon(vector_icons::kAlerseSearchIcon, 16,
                              alerse::SidebarInk(this, kPopupGlyph)),
        12, 17);
    canvas->FillRect(gfx::Rect(-1, kPopupSearchHeight - 1, width() + 2, 1),
                     alerse::SidebarInk(this, kPopupLine));
  }

 private:
  raw_ptr<views::Textfield> field_ = nullptr;
};

BEGIN_METADATA(PopupSearchRow)
END_METADATA


}  // namespace

// Hands the folder popup's search field's typing and keys to the sidebar.
class AlerseSidebarView::PopupSearchController
    : public views::TextfieldController {
 public:
  PopupSearchController(
      base::RepeatingCallback<void(const std::u16string&)> on_changed,
      base::RepeatingCallback<bool(ui::KeyboardCode)> on_key)
      : on_changed_(std::move(on_changed)), on_key_(std::move(on_key)) {}

  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    on_changed_.Run(new_contents);
  }

  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    return key_event.type() == ui::EventType::kKeyPressed &&
           on_key_.Run(key_event.key_code());
  }

 private:
  base::RepeatingCallback<void(const std::u16string&)> on_changed_;
  base::RepeatingCallback<bool(ui::KeyboardCode)> on_key_;
};

namespace {


// Arc for macOS' popup stands 4 pt off the sidebar.
class FolderPopupBubble : public views::BubbleDialogDelegate {
 public:
  using views::BubbleDialogDelegate::BubbleDialogDelegate;

  gfx::Rect GetAnchorRect() const override {
    gfx::Rect rect = views::BubbleDialogDelegate::GetAnchorRect();
    rect.Inset(gfx::Insets::TLBR(0, 0, 0, -kPopupGap));
    return rect;
  }
};

// When a pinned link was last opened, or else added, as Arc's popup says.
std::u16string LastUsedText(const bookmarks::BookmarkNode* node) {
  const base::Time when = node->date_last_used().is_null()
                              ? node->date_added()
                              : node->date_last_used();
  if (when.is_null()) {
    return std::u16string();
  }
  const base::TimeDelta elapsed =
      std::max(base::TimeDelta(), base::Time::Now() - when);
  // Arc counts in weeks past a week and in months past a month ("3 weeks
  // ago", "2 months ago"); ICU words it in the browser's language.
  const int days = elapsed.InDays();
  UDateRelativeUnit unit;
  int count;
  if (days >= 365) {
    unit = UDAT_RELATIVE_YEARS;
    count = days / 365;
  } else if (days >= 30) {
    unit = UDAT_RELATIVE_MONTHS;
    count = days / 30;
  } else if (days >= 7) {
    unit = UDAT_RELATIVE_WEEKS;
    count = days / 7;
  } else {
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                  ui::TimeFormat::LENGTH_LONG, elapsed);
  }
  UErrorCode status = U_ZERO_ERROR;
  icu::RelativeDateTimeFormatter formatter(
      icu::Locale::getDefault(), nullptr, UDAT_STYLE_LONG,
      UDISPCTX_CAPITALIZATION_FOR_STANDALONE, status);
  icu::UnicodeString text;
  formatter.format(count, UDAT_DIRECTION_LAST, unit, text, status);
  if (U_FAILURE(status)) {
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                  ui::TimeFormat::LENGTH_LONG, elapsed);
  }
  return base::i18n::UnicodeStringToString16(text);
}

// Arc's Copy URL as Quote: the page's selected text as a Markdown quote, then
// the page's link; empty without a selection. (macOS' menu bar has its own
// copy, browser_native_widget_mac.mm.)
std::u16string GetQuote(BrowserWindowInterface* browser) {
  content::WebContents* contents = chrome::GetAlerseCopyableContents(browser);
  content::RenderWidgetHostView* view =
      contents ? contents->GetRenderWidgetHostView() : nullptr;
  if (!view) {
    return std::u16string();
  }
  const std::u16string selection(
      base::TrimWhitespace(view->GetSelectedText(), base::TRIM_ALL));
  if (selection.empty()) {
    return std::u16string();
  }
  std::u16string quote;
  for (std::u16string_view line :
       base::SplitStringPiece(selection, u"\n", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL)) {
    base::StrAppend(&quote, {u"> ", line, u"\n"});
  }
  // The title's brackets are escaped, so the link stays one.
  std::u16string title = contents->GetTitle();
  base::ReplaceChars(title, u"[", u"\\[", &title);
  base::ReplaceChars(title, u"]", u"\\]", &title);
  base::StrAppend(&quote,
                  {u"\n[", title, u"](",
                   base::UTF8ToUTF16(contents->GetVisibleURL().spec()), u")"});
  return quote;
}

// Hides Chromium's split drop zone in every window once a row's drag is over.
// The zone's controller hides it when a drag that started in a page ends
// (BrowserView::HandleDragEnded) or when the drop lands on the zone; a row's
// drag starts in the sidebar, so a drop on the page itself, before the zone's
// delay ran out or outside it, left the zone up, or let its timer show it
// after the drop, until the next drag. Posted, so that a drop on the zone is
// done with it first.
void HideSplitDropZones() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce([] {
        for (BrowserWindowInterface* browser :
             GetAllBrowserWindowInterfaces()) {
          if (BrowserView* view =
                  BrowserView::GetBrowserViewForBrowser(browser)) {
            view->HandleDragEnded();
          }
        }
      }));
}
}  // namespace

// One line of the sidebar: icon, optional "/", for a tab with sound its
// speaker, title and, for tabs, a close button shown on hover.
class AlerseSidebarView::Row : public views::Button {
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
    // Arc for macOS' black pill (alerse_tooltip.h).
    alerse::SetArcTooltip(this, title);
    GetViewAccessibility().SetName(title);
    GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
    GetViewAccessibility().SetIsSelected(active);
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    SetInstallFocusRingOnFocus(false);
  }

  // What a row's button on hover does, as in Arc: a tab's "x" closes it, a
  // pinned item's "-" closes its tab and keeps the item, and the "x" of a
  // pinned item with no tab open removes the item.
  enum class CloseKind { kCloseTab, kClosePinnedTab, kRemovePinned };

  void SetCloseCallback(PressedCallback callback,
                        CloseKind kind = CloseKind::kCloseTab) {
    // The "-" is Chromium's minus drawn as wide and thick as the cross: its
    // bar spans 8.5 of 16, the cross 9.4 of 20.
    const bool minus = kind == CloseKind::kClosePinnedTab;
    const gfx::VectorIcon& icon =
        minus ? kRemoveIcon : vector_icons::kCloseIcon;
    // Arc for macOS' cross: 11 pt in #A9A9A9 (the icon draws 11 of 24),
    // brighter under the pointer; fixed colors, which the native theme's
    // button would replace.
    constexpr int kButtonIconSize = 24;
    const int icon_size = minus ? 21 : kButtonIconSize;
    auto close = std::make_unique<views::ImageButton>(std::move(callback));
    views::ConfigureVectorImageButton(close.get());
    close->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(icon, SkColorSetRGB(0xA9, 0xA9, 0xA9),
                                       icon_size));
    for (const auto state :
         {views::Button::STATE_HOVERED, views::Button::STATE_PRESSED}) {
      close->SetImageModel(
          state, ui::ImageModel::FromVectorIcon(
                     icon, SkColorSetRGB(0xDA, 0xDA, 0xDA), icon_size));
    }
    // The "-" takes the cross's place and plate.
    close->SetMinimumImageSize(gfx::Size(kButtonIconSize, kButtonIconSize));
    const std::u16string name = l10n_util::GetStringUTF16(
        kind == CloseKind::kRemovePinned ? IDS_ALERSE_REMOVE_PINNED_BUTTON
                                         : IDS_ALERSE_CLOSE_TAB_BUTTON);
    // Arc for macOS' black pill (alerse_tooltip.h); the row's own pill
    // gives way to it.
    alerse::SetArcTooltip(close.get(), name);
    close->GetViewAccessibility().SetName(name);
    close_ = AddChildView(std::move(close));
    // As in Arc, shown only while the row is hovered.
    close_->SetVisible(false);
    // Hovering the close button keeps the row hovered.
    SetNotifyEnterExitOnChild(true);
  }

  // The speaker of a tab that plays or is muted, crossed out while muted. It
  // takes its own clicks, so pressing it never activates or drags the row.
  void SetSpeaker(PressedCallback callback, bool muted) {
    auto speaker = std::make_unique<views::ImageButton>(std::move(callback));
    speaker_muted_ = muted;
    speaker->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    speaker->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    speaker->SetPreferredSize(gfx::Size(kSpeakerSize, kSpeakerSize));
    // Names what a click does.
    const std::u16string name = l10n_util::GetStringUTF16(
        muted ? IDS_ALERSE_UNMUTE_TAB : IDS_ALERSE_MUTE_TAB);
    speaker->SetTooltipText(name);
    speaker->GetViewAccessibility().SetName(name);
    speaker->SetInstallFocusRingOnFocus(false);
    // The button takes every press, so a right click on it would otherwise
    // open no menu; it opens the row's.
    speaker->set_context_menu_controller(context_menu_controller());
    speaker_ = AddChildView(std::move(speaker));
    UpdateSpeakerImage();
  }

  views::View* speaker() const { return speaker_; }

  // As in Arc, a click on the icon of a pinned tab that left its saved page
  // (the row shows "/") takes the tab back to it.
  void SetIconCallback(base::RepeatingClosure callback) {
    icon_callback_ = std::move(callback);
  }

  void SetHoverCallback(base::RepeatingCallback<void(bool)> callback) {
    hover_callback_ = std::move(callback);
  }

  // As in Arc, a double click renames a pinned tab. `key` names the item:
  // its first click may open its tab and rebuild the rows, so the second
  // press reaches a new row, which views would not count as a double click.
  void SetDoubleClickCallback(int64_t key,
                              base::RepeatingClosure callback,
                              bool renamed) {
    double_click_key_ = key;
    double_click_callback_ = std::move(callback);
    // Arc for macOS' tooltip says so under the name, but not once the item
    // has a name of its own.
    if (!renamed) {
      alerse::SetArcTooltipDetail(
          this,
          l10n_util::GetStringUTF16(IDS_ALERSE_DOUBLE_CLICK_TO_RENAME));
    }
  }

  void NotifyClick(const ui::Event& event) override {
    if (IsIconClick(event)) {
      icon_callback_.Run();
      return;
    }
    views::Button::NotifyClick(event);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (double_click_callback_ && event.IsOnlyLeftMouseButton() &&
        !IsIconClick(event)) {
      // The last press on an item, kept across rebuilds of the rows.
      static int64_t last_key = 0;
      static base::TimeTicks last_time;
      const base::TimeTicks now = base::TimeTicks::Now();
      const bool second =
          event.GetClickCount() == 2 ||
          (last_key == double_click_key_ &&
           now - last_time <= views::GetDoubleClickInterval());
      last_key = second ? 0 : double_click_key_;
      last_time = now;
      if (second) {
        double_click_callback_.Run();
        return true;
      }
    }
    return views::Button::OnMousePressed(event);
  }

  void set_height(int height) { height_ = height; }

  // Where the row's icon is centred, for a picker's arrow.
  int GetIconCenterX() const { return icon_->bounds().CenterPoint().x(); }

  // One half of a split's row (SplitPair), which paints the plate: the
  // half paints only its hover, and its insets stop at the row's middle.
  void SetSplitHalf(bool start) { split_half_ = start ? 1 : 2; }

  // A row of Arc for macOS' folder popup: smaller text, an optional second
  // line in grey, and only the selected row highlighted.
  void SetPopupStyle(const std::u16string& subtitle) {
    popup_ = true;
    height_ = kPopupRowPitch;
    label_->SetFontList(ArcFont(13, gfx::Font::Weight::NORMAL));
    color_override_ = kPopupTitle;
    if (!subtitle.empty()) {
      subtitle_ = AddChildView(MakeLabel(
          subtitle, ArcFont(11, gfx::Font::Weight::MEDIUM), kSubtleColor));
      subtitle_->SetCanProcessEventsWithinSubtree(false);
    }
  }

  void SetSelected(bool selected) {
    if (selected_ != selected) {
      selected_ = selected;
      GetViewAccessibility().SetIsSelected(selected);
      SchedulePaint();
    }
  }

  // Arc for macOS brightens some rows' text under the pointer.
  void SetHoverLabelColor(SkColor color) { hover_label_color_ = color; }

  std::u16string title() const { return std::u16string(label_->GetText()); }

  // A tab's new title and icon, on a row nothing else of which changed.
  void UpdateTab(const std::u16string& title, const ui::ImageModel& icon) {
    if (label_->GetText() != title) {
      label_->SetText(title);
      alerse::SetArcTooltip(this, title);
      GetViewAccessibility().SetName(title);
    }
    if (auto* image = views::AsViewClass<views::ImageView>(icon_.get())) {
      image->SetImage(icon);
    }
  }

  // Whether the row's speaker shows the tab muted; nullopt without one.
  std::optional<bool> speaker_muted() const {
    return speaker_ ? std::optional<bool>(speaker_muted_) : std::nullopt;
  }

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
        // Without the animation its value stays 0: the highlight shows at
        // once (OnPaintBackground).
        adopted_hover_ = true;
        // The root view never saw the pointer enter this row, so it sends no
        // exit either: the row checks for itself until the pointer leaves.
        adopt_poll_.Start(FROM_HERE, base::Milliseconds(100),
                          base::BindRepeating(&Row::PollAdoptedHover,
                                              base::Unretained(this)));
        if (close_) {
          close_->SetVisible(true);
        }
      }
    }
    if (popup_) {
      LayoutPopupRow();
      return;
    }
    const int shift = depth_ * kIndent +
                      (split_half_ == 2 ? kSplitHalfGap - kRowBackgroundInsetX
                                        : 0);
    const int end_inset =
        split_half_ == 1 ? kSplitHalfGap : kRowBackgroundInsetX;
    const int center_y = height_ / 2;
    const gfx::Size icon_size = icon_->GetPreferredSize();
    icon_->SetBounds(kIconX + shift - (icon_size.width() - kIconSize) / 2,
                     center_y - icon_size.height() / 2, icon_size.width(),
                     icon_size.height());
    // A split row's half is narrow: its title sits closer to the icon and
    // runs to the half's end.
    int x = split_half_ ? kIconX + shift + kIconSize + kSplitHalfGap + 2
                        : kTextX + shift;
    int right = width() - end_inset - (split_half_ ? 0 : kTextEndInset);
    if (close_) {
      const gfx::Size size = close_->GetPreferredSize();
      // Arc for macOS centres its cross 18 pt from the highlight's edge.
      const int close_right = width() - end_inset + 2;
      close_->SetBounds(close_right - size.width() - 4,
                        center_y - size.height() / 2, size.width(),
                        size.height());
      if (close_->GetVisible()) {
        right = close_->x() - 2;
      }
    }
    if (slash_) {
      const int w = slash_->GetPreferredSize().width();
      slash_->SetBounds(x, 0, w, height_);
      x += w + 5;
    }
    if (speaker_) {
      speaker_->SetBounds(x, center_y - kSpeakerSize / 2, kSpeakerSize,
                          kSpeakerSize);
      x += kSpeakerSize + kSpeakerGap;
    }
    label_->SetBounds(x, 0, std::max(0, right - x), height_);
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::Button::OnMouseEntered(event);
    UpdateHover(true);
    // A rebuild or a folder's animation under the pointer can swallow the
    // exit; the row checks for itself until the pointer has left.
    if (!adopt_poll_.IsRunning()) {
      adopt_poll_.Start(FROM_HERE, base::Milliseconds(100),
                        base::BindRepeating(&Row::PollAdoptedHover,
                                            base::Unretained(this)));
    }
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    UpdateLabelColor();
    if (slash_) {
      slash_->SetEnabledColor(alerse::SidebarInk(this, kSubtleColor));
    }
    UpdateSpeakerImage();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    views::Button::OnMouseExited(event);
    adopted_hover_ = false;
    adopt_poll_.Stop();
    UpdateHover(false);
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    // The hover highlight fades with the button's hover animation.
    // On macOS the button's hover animation never advances here, so the
    // highlight follows the pointer without a fade.
    const double hover = polled_hover_ || adopted_hover_ || mouse_hovered_;
    if (popup_) {
      if (selected_) {
        cc::PaintFlags flags;
        flags.setAntiAlias(true);
        flags.setColor(alerse::SidebarInk(this, kPopupSelected));
        canvas->DrawRoundRect(gfx::RectF(4, 0, width() - 8, height_ - 1), 8,
                              flags);
      }
      return;
    }
    if (!active_ && hover <= 0) {
      return;
    }
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const SkColor hover_color = alerse::SidebarInk(this, kHoverColor);
    flags.setColor(active_ ? alerse::SidebarInk(this, kActiveColor)
                           : SkColorSetA(hover_color,
                                         static_cast<U8CPU>(std::round(
                                             SkColorGetA(hover_color) *
                                             hover))));
    gfx::RectF bounds(0, 0, width(), height_);
    // A split row's halves meet at its middle.
    const float start_inset = split_half_ == 2 ? 1 : kRowBackgroundInsetX;
    const float end_inset = split_half_ == 1 ? 1 : kRowBackgroundInsetX;
    // Arc for macOS' plate sits half a point above the row's middle, and
    // a nested row's starts where the row does.
    bounds.Inset(gfx::InsetsF::TLBR(kRowBackgroundInsetY - 0.5f,
                                    start_inset + depth_ * kIndent,
                                    kRowBackgroundInsetY + 0.5f, end_inset));
    canvas->DrawRoundRect(bounds, kRowBackgroundRadius, flags);
  }

 private:
  // Whether `event` is a left click or a tap on the icon or its "/".
  bool IsIconClick(const ui::Event& event) const {
    if (!icon_callback_ || !slash_ || !event.IsLocatedEvent() ||
        (event.IsMouseEvent() && !event.AsMouseEvent()->IsLeftMouseButton())) {
      return false;
    }
    const int x = event.AsLocatedEvent()->location().x();
    return x >= icon_->x() - 6 && x < slash_->bounds().right();
  }

  // In the ink of the "/", which follows the Space's tint.
  void UpdateSpeakerImage() {
    if (!speaker_ || !GetWidget()) {
      return;
    }
    speaker_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            speaker_muted_ ? vector_icons::kVolumeOffIcon
                           : vector_icons::kVolumeUpIcon,
            alerse::SidebarInk(this, kSubtleColor), kSpeakerSize));
  }

  void PollAdoptedHover() {
    if (IsMouseHovered()) {
      return;
    }
    adopt_poll_.Stop();
    adopted_hover_ = false;
    if (GetState() == STATE_HOVERED) {
      SetState(STATE_NORMAL);
    }
    UpdateHover(false);
  }

  void LayoutPopupRow() {
    const int shift = depth_ * kIndent;
    const gfx::Size icon_size = icon_->GetPreferredSize();
    const int center_y = subtitle_ ? 21 : height_ / 2;
    icon_->SetBounds(kPopupIconCenterX + shift - icon_size.width() / 2,
                     center_y - icon_size.height() / 2, icon_size.width(),
                     icon_size.height());
    int x = kPopupTextX + shift;
    if (speaker_) {
      speaker_->SetBounds(x, center_y - kSpeakerSize / 2, kSpeakerSize,
                          kSpeakerSize);
      x += kSpeakerSize + kSpeakerGap;
    }
    const int text_width = std::max(0, width() - kPopupTextEndInset - x);
    if (!subtitle_) {
      label_->SetBounds(x, 0, text_width, height_);
      return;
    }
    // Arc's title caps start 8 pt into the row, the date's 26 pt.
    label_->SetBounds(x, 5, text_width, 16);
    subtitle_->SetBounds(kPopupTextX + shift, 23, text_width, 14);
  }

  void UpdateLabelColor() {
    if (!GetColorProvider()) {
      return;
    }
    if (subtitle_) {
      subtitle_->SetEnabledColor(alerse::SidebarInk(this, kPopupDate));
    }
    if (mouse_hovered_ && hover_label_color_) {
      label_->SetEnabledColor(alerse::SidebarInk(this, *hover_label_color_));
      return;
    }
    if (color_override_) {
      label_->SetEnabledColor(alerse::SidebarInk(this, *color_override_));
      return;
    }
    label_->SetEnabledColor(alerse::SidebarInk(this, color_));
  }

  void UpdateHover(bool hovered) {
    mouse_hovered_ = hovered;
    if (hover_label_color_) {
      UpdateLabelColor();
    }
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
  // 1: a split row's start half, 2: its end half.
  int split_half_ = 0;
  raw_ptr<views::View> icon_ = nullptr;
  raw_ptr<views::Label> slash_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageButton> close_ = nullptr;
  raw_ptr<views::ImageButton> speaker_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
  bool popup_ = false;
  bool selected_ = false;
  std::optional<SkColor> color_override_;
  std::optional<SkColor> hover_label_color_;
  bool speaker_muted_ = false;
  bool adopt_hover_ = false;
  bool polled_hover_ = false;
  bool adopted_hover_ = false;
  bool mouse_hovered_ = false;
  base::RepeatingTimer adopt_poll_;
  base::RepeatingCallback<void(bool)> hover_callback_;
  base::RepeatingClosure double_click_callback_;
  base::RepeatingClosure icon_callback_;
  int64_t double_click_key_ = 0;
  base::RepeatingClosure drag_done_callback_;
};

BEGIN_METADATA(AlerseSidebarView, Row)
END_METADATA

namespace {

// Arc's split row: a split's two pages side by side in one row. Each half is
// a row of its own (clicked, dragged, closed and given a menu alone); the
// pair paints the plate while the split is shown, and a line between them.
class SplitPair : public views::View {
  METADATA_HEADER(SplitPair, views::View)

 public:
  SplitPair(int height, bool active) : height_(height), active_(active) {}

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(0, height_);
  }

  void Layout(PassKey) override {
    if (children().size() != 2) {
      return;
    }
    const int half = width() / 2;
    children()[0]->SetBounds(0, 0, half, height_);
    children()[1]->SetBounds(half, 0, width() - half, height_);
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    if (active_) {
      flags.setColor(alerse::SidebarInk(this, kActiveColor));
      gfx::RectF bounds(0, 0, width(), height_);
      bounds.Inset(gfx::InsetsF::TLBR(kRowBackgroundInsetY - 0.5f,
                                      kRowBackgroundInsetX,
                                      kRowBackgroundInsetY + 0.5f,
                                      kRowBackgroundInsetX));
      canvas->DrawRoundRect(bounds, kRowBackgroundRadius, flags);
    }
    flags.setColor(alerse::SidebarInk(this, kSubtleColor));
    canvas->DrawRect(gfx::RectF(width() / 2 - 0.5f, height_ / 3.0f, 1,
                                height_ / 3.0f),
                     flags);
  }

 private:
  const int height_;
  const bool active_;
};

BEGIN_METADATA(SplitPair)
END_METADATA

}  // namespace

AlerseSidebarView::AlerseSidebarView(BrowserWindowInterface* browser)
    : browser_(browser),
      model_(
          BookmarkModelFactory::GetForBrowserContext(browser->GetProfile())) {
  // The address bar stays at the top, the rows scroll, the mini player stays
  // at the bottom.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  address_bar_ = AddChildView(std::make_unique<AlerseAddressBar>(browser_));
  address_bar_->SetCommandBarCallback(base::BindRepeating(
      &AlerseSidebarView::OnCommandBarChanged, base::Unretained(this)));
  // As in Arc, a right click on the address field offers the Space's menu.
  address_bar_->set_context_menu_controller(this);
  favorites_ = AddChildView(std::make_unique<FavoritesGrid>());
  favorites_->set_context_menu_controller(this);
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
  for (const auto side : {views::OverflowIndicatorAlignment::kTop,
                          views::OverflowIndicatorAlignment::kBottom}) {
    scroll->SetCustomOverflowIndicator(side, std::make_unique<OverflowLine>(),
                                       1, /*fills_opaquely=*/false);
  }
  auto rows = std::make_unique<views::View>();
  rows->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  rows->set_context_menu_controller(this);
  // A screen reader reads the rows as a list and counts them.
  rows->GetViewAccessibility().SetRole(ax::mojom::Role::kList);
  // And the empty space below them: Arc's "Edit Theme Color".
  set_context_menu_controller(this);
  rows_ = scroll->SetContents(std::move(rows));
  AddChildView(std::make_unique<AlerseMiniPlayer>(browser_));
  drop_indicator_ = AddChildView(std::make_unique<views::View>());
  drop_indicator_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  drop_indicator_->SetCanProcessEventsWithinSubtree(false);
  drop_indicator_->SetVisible(false);

  if (model_) {
    model_->AddObserver(this);
  }
  browser_->GetTabStripModel()->AddObserver(this);
  folder_animation_.SetSlideDuration(Motion(kFolderAnimationDuration));
  folder_animation_.SetTweenType(gfx::Tween::LINEAR);
  alerse::NewTabCommandBarHook() = &AlerseSidebarView::OpenNewTabCommandBar;
  alerse::CommandBarHook() = &AlerseSidebarView::OpenSplitCommandBar;
  alerse::SplitDraggedRowHook() = &AlerseSidebarView::SplitDraggedRow;
  Rebuild();
}

// static
bool AlerseSidebarView::OpenNewTabCommandBar(
    BrowserWindowInterface* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  AlerseSidebarView* sidebar =
      browser_view ? FindSidebarIn(browser_view) : nullptr;
  if (!sidebar || !sidebar->IsDrawn()) {
    return false;
  }
  sidebar->address_bar_->OpenCommandBar(/*new_tab=*/true);
  return true;
}

// static
bool AlerseSidebarView::OpenSplitCommandBar(BrowserWindowInterface* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  AlerseSidebarView* sidebar =
      browser_view ? FindSidebarIn(browser_view) : nullptr;
  if (!sidebar || !sidebar->IsDrawn()) {
    return false;
  }
  // The field opens empty on the pane's "Choose a tab" page
  // (AlerseAddressBar::UpdateEditing); what is typed loads in the pane.
  sidebar->address_bar_->OpenCommandBar(/*new_tab=*/false);
  return true;
}

AlerseSidebarView::~AlerseSidebarView() {
  if (drag_source_) {
    EndRowDrag();
  }
  CloseFolderPopup();
  if (model_) {
    model_->RemoveObserver(this);
  }
}

void AlerseSidebarView::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  if (renaming_node_ && node->id() == renaming_node_ &&
      node->GetTitle() != renaming_title_) {
    renaming_node_ = 0;
    if (node->is_url()) {
      model_->SetNodeMetaInfo(node, kRenamedMetaKey, "1");
    }
  }
  BookmarkModelChanged();
}

void AlerseSidebarView::BookmarkModelChanged() {
  ScheduleRebuild();
}

void AlerseSidebarView::BookmarkMetaInfoChanged(
    const bookmarks::BookmarkNode* node) {
  ScheduleRebuild();
}

void AlerseSidebarView::BookmarkModelBeingDeleted() {
  model_->RemoveObserver(this);
  model_ = nullptr;
}

void AlerseSidebarView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // As in Arc, while a new split's pane waits on "Choose a tab", the tab
  // picked in the sidebar or the command bar (Switch to Tab) fills it.
  // Done after this change: the split can be updated only from one of its
  // panes, shown again first.
  if (selection.active_tab_changed() && selection.old_contents &&
      selection.new_contents &&
      alerse::IsSplitChooser(selection.old_contents)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AlerseSidebarView::FillSplitChooser,
                       split_weak_factory_.GetWeakPtr(),
                       selection.old_contents->GetWeakPtr(),
                       selection.new_contents->GetWeakPtr()));
  }
  ScheduleRebuild();
}

void AlerseSidebarView::FillSplitChooser(
    base::WeakPtr<content::WebContents> chooser,
    base::WeakPtr<content::WebContents> picked) {
  TabStripModel* const model = browser_->GetTabStripModel();
  const int chooser_index =
      chooser ? model->GetIndexOfWebContents(chooser.get())
              : TabStripModel::kNoTab;
  const int picked_index = picked ? model->GetIndexOfWebContents(picked.get())
                                  : TabStripModel::kNoTab;
  if (chooser_index == TabStripModel::kNoTab ||
      picked_index == TabStripModel::kNoTab ||
      !alerse::IsSplitChooser(chooser.get())) {
    return;
  }
  tabs::TabInterface* const pane = model->GetTabAtIndex(chooser_index);
  tabs::TabInterface* const tab = model->GetTabAtIndex(picked_index);
  // Only a plain tab of the pane's own Space still shown: another Space's
  // tab or a favorite's would move into this Space, and the split's other
  // pane or another split is just shown.
  if (!pane->IsSplit() || tab->IsSplit() || model->GetActiveTab() != tab ||
      tab->GetGroup() != pane->GetGroup() ||
      tab->IsPinned() != pane->IsPinned()) {
    return;
  }
  model->ActivateTabAt(chooser_index);
  // Puts the tab where the chooser was, closes the chooser and shows the
  // tab in the split.
  model->UpdateTabInSplit(pane, model->GetIndexOfTab(tab),
                          TabStripModel::SplitUpdateType::kReplace);
}

void AlerseSidebarView::OnTabChangedAt(tabs::TabInterface* tab,
                                         TabChangeType change_type) {
  // A page that ticks in its title would otherwise rebuild every row several
  // times a second.
  if (tab && UpdateTabRows(tab)) {
    return;
  }
  ScheduleRebuild();
}

bool AlerseSidebarView::UpdateTabRows(tabs::TabInterface* tab) {
  content::WebContents* contents = tab->GetContents();
  if (!contents) {
    return false;
  }
  const tabs::TabInterface::Handle handle = tab->GetHandle();
  const std::optional<bool> speaker =
      ShowsSpeaker(tab) ? std::optional<bool>(contents->IsAudioMuted())
                        : std::nullopt;
  std::vector<Row*> rows;
  for (const auto& [view, target] : row_targets_) {
    if (!(target.tab == handle)) {
      continue;
    }
    // Only a plain tab's row is its title and icon alone: a pinned item's
    // also shows whether the tab left its address, a favorite is a tile, and
    // a speaker that comes or goes changes the row's layout.
    Row* row = views::AsViewClass<Row>(view);
    if (target.node || !row || row->speaker_muted() != speaker) {
      return false;
    }
    rows.push_back(row);
  }
  if (rows.empty()) {
    return false;
  }
  const std::u16string title = TabUIHelper::From(tab)->GetTitle();
  const ui::ImageModel icon = GetTabIcon(tab);
  for (Row* row : rows) {
    row->UpdateTab(title, icon);
  }
  return true;
}

void AlerseSidebarView::OnTabGroupChanged(const TabGroupChange& change) {
  ScheduleRebuild();
}

std::string AlerseSidebarView::GetActiveSpace() const {
  // The window shows one Space, its profile's; a swipe previews a neighbor.
  return preview_space_ ? preview_space_->space : kDefaultSpace;
}

std::u16string AlerseSidebarView::GetActiveSpaceTitle() const {
  return preview_space_ ? preview_space_->title : std::u16string();
}

const bookmarks::BookmarkNode* AlerseSidebarView::GetSpaceFolder(
    bool create) {
  return GetSpaceFolderFor(GetActiveSpace(), GetActiveSpaceTitle(), create);
}

const bookmarks::BookmarkNode* AlerseSidebarView::GetSpaceFolderFor(
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

const bookmarks::BookmarkNode* AlerseSidebarView::GetNode(int64_t id) const {
  if (!model_ || !id) {
    return nullptr;
  }
  return bookmarks::GetBookmarkNodeByID(model_, id);
}

bool AlerseSidebarView::IsInActiveSpace(const tabs::TabInterface* tab) const {
  // Every tab of the window is the Space's. A previewed neighbor's tabs are
  // in its own window.
  return !preview_space_;
}

tabs::TabInterface* AlerseSidebarView::GetBoundTab(int64_t id) const {
  auto it = bound_.find(id);
  return it == bound_.end() ? nullptr : it->second.Get();
}

bool AlerseSidebarView::IsBound(const tabs::TabInterface* tab) const {
  for (const auto& [id, handle] : bound_) {
    if (handle.Get() == tab) {
      return true;
    }
  }
  return false;
}

void AlerseSidebarView::BindTabsByUuid() {
  if (!model_ || !model_->loaded()) {
    return;
  }
  TabStripModel* tabs = browser_->GetTabStripModel();
  for (int i = 0; i < tabs->count(); ++i) {
    tabs::TabInterface* tab = tabs->GetTabAtIndex(i);
    const base::Uuid uuid = base::Uuid::ParseLowercase(
        alerse::GetPinnedTabUuid(tab->GetContents()));
    if (!uuid.is_valid() || IsBound(tab)) {
      continue;
    }
    // Spaces' folders are local only.
    const bookmarks::BookmarkNode* node = model_->GetNodeByUuid(
        uuid, bookmarks::BookmarkModel::NodeTypeForUuidLookup::
                  kLocalOrSyncableNodes);
    // Only to an item of the Space's folder: one elsewhere has no row, and
    // the tab would be lost from the sidebar. A favorite's tab is pinned.
    if (node && node->is_url() && !GetBoundTab(node->id()) &&
        (IsFavorite(node) ? tab->IsPinned()
                          : GetSpaceOfNode(node) == kDefaultSpace)) {
      bound_[node->id()] = tab->GetHandle();
    }
  }
}

std::string AlerseSidebarView::GetSpaceOfNode(
    const bookmarks::BookmarkNode* node) const {
  const bookmarks::BookmarkNode* folder = node;
  while (folder->parent() && folder->parent() != model_->other_node()) {
    folder = folder->parent();
  }
  std::string space;
  if (!folder->parent() || !folder->is_folder() ||
      !folder->GetMetaInfo(kSpaceMetaKey, &space)) {
    return std::string();
  }
  return space;
}

void AlerseSidebarView::WritePinnedItem(tabs::TabInterface* tab,
                                          const std::string& uuid) {
  content::WebContents* contents = tab->GetContents();
  alerse::SetPinnedTabUuid(contents, uuid);
  SessionService* session =
      SessionServiceFactory::GetForProfile(browser_->GetProfile());
  const SessionID window_id =
      sessions::SessionTabHelper::IdForWindowContainingTab(contents);
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
  if (session && window_id.is_valid() && tab_id.is_valid()) {
    session->AddTabExtraData(window_id, tab_id,
                             alerse::kPinnedTabExtraDataKey, uuid);
  }
}

void AlerseSidebarView::SaveBindings() {
  // Before the bookmarks load, no binding is known: a restored tab's must
  // not be cleared.
  if (!model_ || !model_->loaded()) {
    return;
  }
  std::map<const tabs::TabInterface*, const bookmarks::BookmarkNode*> pinned;
  for (const auto& [id, handle] : bound_) {
    const bookmarks::BookmarkNode* node = GetNode(id);
    if (tabs::TabInterface* tab = handle.Get(); tab && node) {
      pinned[tab] = node;
    }
  }
  TabStripModel* tabs = browser_->GetTabStripModel();
  for (int i = 0; i < tabs->count(); ++i) {
    tabs::TabInterface* tab = tabs->GetTabAtIndex(i);
    content::WebContents* contents = tab->GetContents();
    const std::string current = alerse::GetPinnedTabUuid(contents);
    auto it = pinned.find(tab);
    if (it != pinned.end()) {
      const bookmarks::BookmarkNode* node = it->second;
      // Also after a discard, whose new contents start without it.
      const std::string uuid = node->uuid().AsLowercaseString();
      if (uuid != current) {
        WritePinnedItem(tab, uuid);
      }
      continue;
    }
    if (current.empty()) {
      continue;
    }
    // A tab whose item is outside the Space's folder is shown as an ordinary
    // tab but keeps its item: it may be in the middle of a move.
    const bookmarks::BookmarkNode* node = model_->GetNodeByUuid(
        base::Uuid::ParseLowercase(current),
        bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
    if (node && node->is_url() && !GetBoundTab(node->id())) {
      continue;
    }
    // Its item is gone, or has another tab now.
    WritePinnedItem(tab, std::string());
  }
}

bool AlerseSidebarView::HasBoundDescendant(
    const bookmarks::BookmarkNode* folder) const {
  for (const auto& child : folder->children()) {
    if (child->is_folder() ? HasBoundDescendant(child.get())
                           : GetBoundTab(child->id()) != nullptr) {
      return true;
    }
  }
  return false;
}

std::string AlerseSidebarView::SpeakerState(
    const bookmarks::BookmarkNode* folder) const {
  std::string state;
  for (const auto& child : folder->children()) {
    if (child->is_folder()) {
      state += SpeakerState(child.get());
    } else if (tabs::TabInterface* tab = GetBoundTab(child->id());
               tab && ShowsSpeaker(tab)) {
      state += base::NumberToString(child->id()) +
               (tab->GetContents()->IsAudioMuted() ? "m;" : "p;");
    }
  }
  return state;
}

ui::ImageModel AlerseSidebarView::GetBookmarkIcon(
    const bookmarks::BookmarkNode* node) {
  std::string glyph;
  if (node->GetMetaInfo(kGlyphMetaKey, &glyph) && !glyph.empty()) {
    const SkColor color = GetColorProvider()
                              ? alerse::SidebarInk(this, kGlyphColor)
                              : SK_ColorWHITE;
    return ui::ImageModel::FromImageSkia(
        gfx::CanvasImageSource::MakeImageSkia<GlyphImageSource>(glyph, color));
  }
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
  return DefaultPageIcon(this);
}

bool AlerseSidebarView::HasOwnIcon(const bookmarks::BookmarkNode* node) {
  std::string unused;
  return !model_->GetFavicon(node).IsEmpty() ||
         node->GetMetaInfo(kIconMetaKey, &unused) ||
         (node->GetMetaInfo(kGlyphMetaKey, &unused) && !unused.empty());
}

ui::ImageModel AlerseSidebarView::GetTabIcon(tabs::TabInterface* tab) const {
  ui::ImageModel icon = TabUIHelper::From(tab)->GetFavicon();
  // A page without a favicon shows Chromium's default one.
  if (icon.IsImage() &&
      icon.GetImage().AsImageSkia().BackedBySameObjectAs(
          favicon::GetDefaultFavicon().AsImageSkia())) {
    return DefaultPageIcon(this);
  }
  return icon.IsEmpty()
             ? DefaultPageIcon(this)
             : icon;
}

void AlerseSidebarView::ScheduleRebuild() {
  if (drag_source_) {
    // A rebuild would destroy the row being dragged.
    if (GetWidget() && GetWidget()->dragged_view()) {
      rebuild_after_drag_ = true;
      return;
    }
    // The drag ended without reaching the row's OnDragDone().
    EndRowDrag();
    HideSplitDropZones();
    rebuild_after_drag_ = false;
  }
  weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AlerseSidebarView::Rebuild,
                                weak_factory_.GetWeakPtr()));
}

void AlerseSidebarView::PrepareSpaceSlide(bool forward, int distance) {
  pending_slide_ = forward ? 1 : -1;
  pending_slide_distance_ = distance;
  pending_slide_from_ = GetActiveSpace();
  pending_slide_until_ = base::TimeTicks::Now() + base::Seconds(1);
}

void AlerseSidebarView::CancelSpaceSlide() {
  pending_slide_ = 0;
}

std::unique_ptr<views::View> AlerseSidebarView::BuildSpaceSnapshot(
    const AlerseSpaceRef& space) {
  // The Space bar names another Space by its profile's directory.
  constexpr std::string_view kPrefix = "profile:";
  ProfileManager* manager = g_browser_process->profile_manager();
  if (!manager || !space.space.starts_with(kPrefix) || !GetWidget()) {
    return nullptr;
  }
  Profile* profile = manager->GetProfileByPath(manager->user_data_dir().Append(
      base::FilePath::FromUTF8Unsafe(space.space.substr(kPrefix.size()))));
  alerse::WindowStacks& stacks = alerse::WindowStacks::Get();
  BrowserWindowInterface* member =
      profile ? stacks.FindMember(stacks.StackOf(browser_), profile) : nullptr;
  BrowserView* view =
      member ? BrowserView::GetBrowserViewForBrowser(member) : nullptr;
  AlerseSidebarView* other = view ? FindSidebarIn(view) : nullptr;
  if (!other || !other->rows_) {
    return nullptr;
  }
  // A hidden window lays out late.
  other->GetWidget()->LayoutRootViewIfNecessary();
  const gfx::Size size = other->rows_->size();
  if (size.IsEmpty()) {
    return nullptr;
  }
  const ui::Compositor* compositor = GetWidget()->GetCompositor();
  const float scale = compositor->device_scale_factor();
  SkBitmap bitmap;
  {
    ui::CanvasPainter painter(&bitmap, size, scale, SK_ColorTRANSPARENT,
                              compositor->is_pixel_canvas());
    other->rows_->Paint(
        views::PaintInfo::CreateRootPaintInfo(painter.context(), size));
  }
  auto image = std::make_unique<views::ImageView>(ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFromBitmap(bitmap, scale)));
  image->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  image->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  return image;
}

std::unique_ptr<views::View> AlerseSidebarView::BuildSpacePreview(
    const AlerseSpaceRef& space) {
  if (std::unique_ptr<views::View> snapshot = BuildSpaceSnapshot(space)) {
    return snapshot;
  }
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
  // BuildRows() points these into the rows it builds; the preview's are
  // deleted with it while the active Space's live on.
  views::View* const space_title = space_title_;
  auto folder_anim = folder_anim_;
  preview_space_ = space;
  BuildRows();
  preview_space_.reset();
  space_title_ = space_title;
  folder_anim_ = std::move(folder_anim);
  row_pitch_offset_ = pitch_offset;
  bound_ = std::move(bound);
  row_targets_ = std::move(targets);
  rows_ = rows;
  return container;
}

void AlerseSidebarView::HideSpacePreview() {
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

void AlerseSidebarView::TrackSpaceSwipe(
    float dx,
    bool forward,
    const std::optional<AlerseSpaceRef>& neighbor) {
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

void AlerseSidebarView::EndSpaceSwipe(bool commit,
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
      Motion(base::Milliseconds(std::clamp(260.0f * left, 70.0f, 220.0f)));
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
        [](base::WeakPtr<AlerseSidebarView> sidebar,
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
              base::BindOnce(&AlerseSidebarView::RestoreSlide, sidebar),
              base::Milliseconds(500));
        },
        slide_weak_factory_.GetWeakPtr(), std::move(switch_space));
  } else {
    finish = base::BindOnce(&AlerseSidebarView::HideSpacePreview,
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

void AlerseSidebarView::RestoreSlide() {
  if (drop_preview_after_rebuild_) {
    HideSpacePreview();
  }
}

void AlerseSidebarView::RunPendingSlide() {
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
      .SetDuration(Motion(kSlideDuration))
      .SetTransform(scroll_.get(), gfx::Transform(), gfx::Tween::EASE_OUT)
      .SetOpacity(scroll_.get(), 1.0f, gfx::Tween::EASE_OUT);
}

void AlerseSidebarView::Rebuild() {
  absl::Cleanup slide = [this] { RunPendingSlide(); };
  hover_timer_.Stop();
  FinishFolderAnimation();
  folder_anim_.reset();
  // A speaker pressed from the keyboard keeps the focus across the rebuild
  // its own press causes, so a second press unmutes.
  tabs::TabInterface::Handle focused_speaker;
  if (const views::FocusManager* focus = GetFocusManager()) {
    for (const auto& [row, target] : row_targets_) {
      const Row* as_row = views::AsViewClass<Row>(row);
      const views::View* speaker = as_row ? as_row->speaker() : nullptr;
      if (speaker && speaker == focus->GetFocusedView()) {
        focused_speaker = target.tab;
      }
    }
  }
  space_title_ = nullptr;
  rows_->RemoveAllChildViews();
  row_targets_.clear();
  row_pitch_offset_ = 0;
  // A tab dragged to another window goes with its binding: that window's
  // sidebar binds it again by uuid. An item moved out of the Space's folder
  // lets the tab go.
  std::erase_if(bound_, [this](const auto& entry) {
    tabs::TabInterface* tab = entry.second.Get();
    const bookmarks::BookmarkNode* node = GetNode(entry.first);
    return !tab || !node ||
           browser_->GetTabStripModel()->GetIndexOfTab(tab) ==
               TabStripModel::kNoTab ||
           (IsFavorite(node) ? !tab->IsPinned()
                             : GetSpaceOfNode(node) != kDefaultSpace);
  });
  BindTabsByUuid();

  BuildRows();
  BuildFavorites();
  SaveBindings();
  if (focused_speaker.Get()) {
    for (const auto& [row, target] : row_targets_) {
      Row* as_row = views::AsViewClass<Row>(row);
      views::View* speaker = as_row ? as_row->speaker() : nullptr;
      if (speaker && target.tab == focused_speaker) {
        speaker->RequestFocus();
        break;
      }
    }
  }
  TabStripModel* tabs = browser_->GetTabStripModel();
  // It goes once its Space has a tab again and it is not shown.
  for (int i = 0; i < tabs->count(); ++i) {
    tabs::TabInterface* hidden = tabs->GetTabAtIndex(i);
    if (hidden == tabs->GetActiveTab() ||
        !TabStripModel::IsAlerseEmptySpace(hidden->GetContents())) {
      continue;
    }
    for (int j = 0; j < tabs->count(); ++j) {
      tabs::TabInterface* other = tabs->GetTabAtIndex(j);
      if (other != hidden && other->GetGroup() == hidden->GetGroup() &&
          !TabStripModel::IsAlerseEmptySpace(other->GetContents())) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(&AlerseSidebarView::OnTabClosePressed,
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
      // Its rows with a live tab show that tab's speaker: refilled when one
      // changed. This runs as its own task, never inside a popup row's press.
      if (const bookmarks::BookmarkNode* folder = GetNode(folder_popup_id_);
          folder && SpeakerState(folder) != folder_popup_speakers_) {
        RefreshFolderPopup();
      }
    } else {
      CloseFolderPopup();
    }
  }

  PreferredSizeChanged();
}

void AlerseSidebarView::BuildRows() {
  const bookmarks::BookmarkNode* folder = GetSpaceFolder(false);

  // Space name.
  // The window's Space is its profile (alerse_space_registry.h): named by
  // its home folder, or as the registry names a Space without one yet.
  const alerse::SpaceInfo space_info =
      alerse::GetSpaceOfProfile(browser_->GetProfile());
  std::u16string title = GetActiveSpaceTitle();
  if (title.empty()) {
    title = folder && !folder->GetTitle().empty() ? folder->GetTitle()
                                                  : space_info.name;
  }
  const bool collapsed = IsPinnedCollapsed(folder);
  auto* space_title = rows_->AddChildView(std::make_unique<SpaceTitle>(
      title,
      alerse::GetSpaceIconName(
          folder, space_info.home ? GetActiveSpace() : std::string()),
      collapsed,
      base::BindRepeating(&AlerseSidebarView::ToggleCollapsedPinned,
                          base::Unretained(this)),
      base::BindRepeating(
          [](AlerseSidebarView* sidebar, const gfx::Point& point) {
            sidebar->ShowSpaceMenu(point, ui::mojom::MenuSourceType::kMouse);
          },
          base::Unretained(this))));
  space_title->set_context_menu_controller(this);
  space_title->SetHighlighted(space_title_highlighted_);
  space_title_ = space_title;

  if (folder) {
    // Collapsed, only the pinned tabs that are open show, as in a folder.
    if (collapsed) {
      AddBoundTabRows(folder, 0);
    } else {
      AddPinnedRows(folder, 0);
    }
  }

  TabStripModel* tabs = browser_->GetTabStripModel();
  // An empty Space's hidden tab has no row, nor does a pinned item's tab.
  auto has_row = [this](tabs::TabInterface* tab) {
    return IsInActiveSpace(tab) && !IsBound(tab) &&
           !TabStripModel::IsAlerseEmptySpace(tab->GetContents());
  };
  bool has_tabs = false;
  for (int i = 0; i < tabs->count() && !has_tabs; ++i) {
    has_tabs = has_row(tabs->GetTabAtIndex(i));
  }
  rows_->AddChildView(std::make_unique<Divider>(
      has_tabs && !preview_space_
          ? base::BindRepeating(&AlerseSidebarView::RunMenuBarCommand,
                                base::Unretained(this), IDC_ALERSE_CLEAR_TODAY)
          : base::RepeatingClosure(),
      l10n_util::GetStringUTF16(IDS_ALERSE_SIDEBAR_CLEAR)));

  // Arc's New Tab opens its command bar; what is picked there opens in a
  // new tab.
  auto plus = std::make_unique<PlusGlyph>();
  [[maybe_unused]] PlusGlyph* plus_glyph = plus.get();
  [[maybe_unused]] Row* new_tab = AddRow(std::make_unique<Row>(
      base::BindRepeating(
          [](AlerseAddressBar* address_bar) {
            address_bar->OpenCommandBar(/*new_tab=*/true);
          },
          base::Unretained(address_bar_.get())),
      std::move(plus),
      l10n_util::GetStringUTF16(IDS_ALERSE_NEW_TAB), 0, ItemFont(false),
      // Arc for macOS shows the command bar's new tab as the active row.
      command_bar_new_tab_ && !preview_space_ ? kItemColor : kNewTabColor,
      /*slash=*/false, /*active=*/command_bar_new_tab_ && !preview_space_));
  // Arc for macOS brightens New Tab's "+" and text under the pointer.
  plus_glyph->SetActive(command_bar_new_tab_ && !preview_space_);
  if (!command_bar_new_tab_ || preview_space_) {
    new_tab->SetHoverLabelColor(SkColorSetRGB(0x8B, 0x8B, 0x8B));
    new_tab->SetHoverCallback(base::BindRepeating(
        [](PlusGlyph* plus, bool hovered) { plus->SetHovered(hovered); },
        base::Unretained(plus_glyph)));
  }
  // Arc for macOS has no tooltip on New Tab.
  new_tab->SetTooltipText(std::u16string());
  alerse::SetArcTooltip(new_tab, std::u16string());

  for (int i = 0; i < tabs->count(); ++i) {
    tabs::TabInterface* tab = tabs->GetTabAtIndex(i);
    if (!has_row(tab)) {
      continue;
    }
    // As in Arc, a split's two pages share one row (a split's tabs are next
    // to each other in the strip).
    if (tab->IsSplit() && i + 1 < tabs->count()) {
      tabs::TabInterface* next = tabs->GetTabAtIndex(i + 1);
      if (next->GetSplit() == tab->GetSplit() && has_row(next)) {
        AddSplitRow(tab, next);
        ++i;
        continue;
      }
    }
    AddTabRow(tab);
  }
}

AlerseSidebarView::Row* AlerseSidebarView::AddRow(
    std::unique_ptr<Row> row,
    int64_t node,
    tabs::TabInterface::Handle tab) {
  // Spread the fractional pitch so rows never drift from Arc's grid.
  const int top = static_cast<int>(std::lround(row_pitch_offset_));
  row_pitch_offset_ += kRowPitch;
  row->set_height(static_cast<int>(std::lround(row_pitch_offset_)) - top);
  Row* added = rows_->AddChildView(std::move(row));
  RegisterRow(added, node, tab);
  return added;
}

void AlerseSidebarView::RegisterRow(Row* row,
                                      int64_t node,
                                      tabs::TabInterface::Handle tab) {
  row->set_context_menu_controller(this);
  row_targets_[row] = RowTarget{node, tab};
  if (node || tab.Get()) {
    row->set_drag_controller(this);
    row->SetDragDoneCallback(base::BindRepeating(
        &AlerseSidebarView::OnRowDragDone, base::Unretained(this)));
  }
}

void AlerseSidebarView::AddPinnedRows(const bookmarks::BookmarkNode* folder,
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
                   base::BindRepeating(&AlerseSidebarView::OnFolderPressed,
                                       base::Unretained(this), id),
                   std::make_unique<FolderGlyph>(
                       !open && HasBoundDescendant(node), FolderIcon(node),
                       open),
                   node->GetTitle(), depth, ItemFont(true), kFolderColor,
                   /*slash=*/false, /*active=*/false),
               id);
    // Arc for macOS shows the popup, not a tooltip, on a folder.
    row->SetTooltipText(std::u16string());
    alerse::SetArcTooltip(row, std::u16string());
    row->SetHoverCallback(
        base::BindRepeating(&AlerseSidebarView::OnFolderHovered,
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

void AlerseSidebarView::AddBoundTabRows(const bookmarks::BookmarkNode* folder,
                                          int depth) {
  for (const auto& child : folder->children()) {
    if (child->is_folder()) {
      AddBoundTabRows(child.get(), depth);
    } else if (GetBoundTab(child->id())) {
      AddBookmarkRow(child.get(), depth);
    }
  }
}

void AlerseSidebarView::AddBookmarkRow(const bookmarks::BookmarkNode* node,
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
                 base::BindRepeating(&AlerseSidebarView::OnBookmarkPressed,
                                     base::Unretained(this), node->id()),
                 MakeImageIcon(icon), node->GetTitle(), depth, ItemFont(false),
                 kItemColor, slash, tab && tab->IsActivated()),
             node->id(), tab ? tab->GetHandle() : tabs::TabInterface::Handle());
  std::string renamed;
  row->SetDoubleClickCallback(
      node->id(),
      base::BindRepeating(&AlerseSidebarView::RenameNode,
                          base::Unretained(this), node->id()),
      node->GetMetaInfo(kRenamedMetaKey, &renamed) && renamed == "1");
  if (tab) {
    if (slash) {
      row->SetIconCallback(
          base::BindRepeating(&AlerseSidebarView::ResetPinnedTab,
                              base::Unretained(this), node->id()));
    }
    AddSpeaker(row, tab);
    // As in Arc, "-" closes the tab and keeps the item.
    row->SetCloseCallback(
        base::BindRepeating(&AlerseSidebarView::OnTabClosePressed,
                            base::Unretained(this), tab->GetHandle()),
        Row::CloseKind::kClosePinnedTab);
  } else {
    // With no tab open, Arc's "x" removes the pinned item.
    row->SetCloseCallback(
        base::BindRepeating(&AlerseSidebarView::OnRemovePinnedPressed,
                            base::Unretained(this), node->id()),
        Row::CloseKind::kRemovePinned);
  }
}

void AlerseSidebarView::OnRemovePinnedPressed(int64_t id) {
  // Looked up again: the item may have gone since the rows were built.
  const bookmarks::BookmarkNode* node = GetNode(id);
  if (node && node->is_url() && !GetBoundTab(id)) {
    model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                   FROM_HERE);
  }
}

std::unique_ptr<AlerseSidebarView::Row> AlerseSidebarView::MakeTabRow(
    tabs::TabInterface* tab,
    ui::ColorId color,
    bool active) {
  return std::make_unique<Row>(
      base::BindRepeating(
          [](AlerseSidebarView* view, tabs::TabInterface::Handle handle,
             const ui::Event& event) {
            if (event.flags() & ui::EF_MIDDLE_MOUSE_BUTTON) {
              view->OnTabClosePressed(handle);
            } else {
              view->OnTabPressed(handle);
            }
          },
          base::Unretained(this), tab->GetHandle()),
      MakeImageIcon(GetTabIcon(tab)), TabUIHelper::From(tab)->GetTitle(), 0,
      ItemFont(false), color, /*slash=*/false, active);
}

void AlerseSidebarView::AddTabRow(tabs::TabInterface* tab) {
  const tabs::TabInterface::Handle handle = tab->GetHandle();
  Row* row =
      AddRow(MakeTabRow(tab, kItemColor, tab->IsActivated()), 0, handle);
  AddSpeaker(row, tab);
  row->SetCloseCallback(base::BindRepeating(
      &AlerseSidebarView::OnTabClosePressed, base::Unretained(this), handle));
}

void AlerseSidebarView::AddSplitRow(tabs::TabInterface* start,
                                      tabs::TabInterface* end) {
  const int top = static_cast<int>(std::lround(row_pitch_offset_));
  row_pitch_offset_ += kRowPitch;
  const int height = static_cast<int>(std::lround(row_pitch_offset_)) - top;
  const bool active = start->IsActivated() || end->IsActivated();
  views::View* pair =
      rows_->AddChildView(std::make_unique<SplitPair>(height, active));
  for (tabs::TabInterface* tab : {start, end}) {
    // The shown pane's title in the item white, the other's dimmed.
    std::unique_ptr<Row> half = MakeTabRow(
        tab, active && !tab->IsActivated() ? kSubtleColor : kItemColor,
        /*active=*/false);
    half->set_height(height);
    half->SetSplitHalf(tab == start);
    Row* row = pair->AddChildView(std::move(half));
    RegisterRow(row, 0, tab->GetHandle());
    // No close button: on a half it would cover most of the title, and a
    // click meant to show the pane would close it. A middle click or the
    // menu's Close closes it.
    AddSpeaker(row, tab);
  }
}

void AlerseSidebarView::AddSpeaker(Row* row, tabs::TabInterface* tab) {
  // Rows are rebuilt when a tab's sound starts, stops or is muted (the tab
  // strip reports it through OnTabChangedAt), so the state is read once here.
  if (ShowsSpeaker(tab)) {
    row->SetSpeaker(
        base::BindRepeating(&AlerseSidebarView::OnTabMutePressed,
                            base::Unretained(this), tab->GetHandle()),
        tab->GetContents()->IsAudioMuted());
  }
}

void AlerseSidebarView::OnBookmarkPressed(int64_t id) {
  if (const bookmarks::BookmarkNode* node = GetNode(id)) {
    OpenBookmark(node);
  }
}

void AlerseSidebarView::ResetPinnedTab(int64_t id) {
  const bookmarks::BookmarkNode* node = GetNode(id);
  tabs::TabInterface* tab = GetBoundTab(id);
  if (!node || !node->is_url() || !tab || !tab->GetContents()) {
    return;
  }
  content::NavigationController::LoadURLParams params(node->url());
  params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  tab->GetContents()->GetController().LoadURLWithParams(params);
  // The click on the icon shows the tab too, as a click on its row.
  OnTabPressed(tab->GetHandle());
}

void AlerseSidebarView::OnFolderPressed(int64_t id) {
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

void AlerseSidebarView::StartFolderCollapse(
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

void AlerseSidebarView::ApplyFolderAnimation(double progress) {
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

void AlerseSidebarView::ResetFolderAnimation() {
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

void AlerseSidebarView::FinishFolderAnimation() {
  folder_animation_.End();
  if (const int64_t id = std::exchange(collapsing_folder_, 0)) {
    if (const bookmarks::BookmarkNode* folder = GetNode(id)) {
      model_->SetNodeMetaInfo(folder, kOpenMetaKey, "0");
    }
  }
}

void AlerseSidebarView::AnimationProgressed(const gfx::Animation* animation) {
  if (folder_anim_) {
    ApplyFolderAnimation(folder_animation_.GetCurrentValue());
  }
}

void AlerseSidebarView::AnimationEnded(const gfx::Animation* animation) {
  if (collapsing_folder_) {
    // Closing the folder rebuilds the rows where the animation left them.
    FinishFolderAnimation();
  } else {
    ResetFolderAnimation();
  }
}

void AlerseSidebarView::OnTabPressed(tabs::TabInterface::Handle handle) {
  TabStripModel* tabs = browser_->GetTabStripModel();
  if (tabs::TabInterface* tab = handle.Get()) {
    const int index = tabs->GetIndexOfTab(tab);
    if (index != TabStripModel::kNoTab) {
      tabs->ActivateTabAt(index);
    }
  }
}

void AlerseSidebarView::OnTabClosePressed(tabs::TabInterface::Handle handle) {
  TabStripModel* tabs = browser_->GetTabStripModel();
  if (tabs::TabInterface* tab = handle.Get()) {
    const int index = tabs->GetIndexOfTab(tab);
    if (index != TabStripModel::kNoTab) {
      // In the closed tabs' history, as a tab of Chromium's strip: Reopen
      // Closed Tab brings it back, which is what undoes Clear.
      tabs->CloseWebContentsAt(index,
                               TabCloseTypes::CLOSE_USER_GESTURE |
                                   TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
    }
  }
}

void AlerseSidebarView::RunMenuBarCommand(int command_id) {
  TabStripModel* tabs = browser_->GetTabStripModel();
  if (command_id == IDC_ALERSE_PIN_TAB) {
    tabs::TabInterface* tab = tabs->GetActiveTab();
    if (!tab) {
      return;
    }
    // As Arc's Pin/Unpin Tab: a pinned tab is unpinned and stays open.
    for (const auto& [id, handle] : bound_) {
      if (handle.Get() != tab) {
        continue;
      }
      if (const bookmarks::BookmarkNode* node =
              model_ ? bookmarks::GetBookmarkNodeByID(model_, id) : nullptr) {
        model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                       FROM_HERE);
      }
      return;
    }
    if (IsInActiveSpace(tab) &&
        !TabStripModel::IsAlerseEmptySpace(tab->GetContents())) {
      PinTab(tab, nullptr);
    }
  } else if (command_id == IDC_ALERSE_TOGGLE_PINNED) {
    ToggleCollapsedPinned();
  } else if (command_id >= IDC_SELECT_TAB_0 &&
             command_id <= IDC_SELECT_LAST_TAB) {
    // Arc's Go to Sidebar Item 1 to 8 and Go to last Sidebar Item.
    const std::vector<RowTarget> items = GetSidebarItems();
    if (items.empty()) {
      return;
    }
    const size_t index = command_id == IDC_SELECT_LAST_TAB
                             ? items.size() - 1
                             : static_cast<size_t>(command_id -
                                                   IDC_SELECT_TAB_0);
    if (index >= items.size()) {
      return;
    }
    if (const bookmarks::BookmarkNode* node = GetNode(items[index].node)) {
      OpenBookmark(node);
    } else if (items[index].tab.Get()) {
      OnTabPressed(items[index].tab);
    }
  } else if (command_id == IDC_ALERSE_REVEAL_TAB) {
    if (views::View* row = FindActiveTabRow()) {
      row->ScrollViewToVisible();
    }
  } else if (command_id == IDC_ALERSE_COPY_URL_QUOTE) {
    if (const std::u16string quote = GetQuote(browser_); !quote.empty()) {
      ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
          .WriteText(quote);
    }
  } else if (command_id == IDC_ALERSE_NEW_FOLDER) {
    AddFolder(nullptr);
  } else if (command_id == IDC_ALERSE_CLEAR_TODAY) {
    // Arc's Clear Today: the Space's tabs below its pinned ones close.
    std::vector<tabs::TabInterface::Handle> today;
    for (int i = 0; i < tabs->count(); ++i) {
      tabs::TabInterface* tab = tabs->GetTabAtIndex(i);
      if (IsInActiveSpace(tab) && !IsBound(tab) && !tab->IsPinned() &&
          !TabStripModel::IsAlerseEmptySpace(tab->GetContents())) {
        today.push_back(tab->GetHandle());
      }
    }
    for (const tabs::TabInterface::Handle& handle : today) {
      OnTabClosePressed(handle);
    }
  }
}

alerse::SpaceCommandState AlerseSidebarView::GetMenuBarCommandState(
    int command_id) {
  using alerse::SpaceCommandState;
  TabStripModel* tabs = browser_->GetTabStripModel();
  tabs::TabInterface* active = tabs->GetActiveTab();
  if (!model_ || !model_->loaded() || !IsDrawn()) {
    return SpaceCommandState::kUnavailable;
  }
  switch (command_id) {
    case IDC_ALERSE_PIN_TAB:
      if (!active) {
        return SpaceCommandState::kUnavailable;
      }
      if (IsBound(active)) {
        return SpaceCommandState::kOn;
      }
      return IsInActiveSpace(active) &&
                     !TabStripModel::IsAlerseEmptySpace(
                         active->GetContents())
                 ? SpaceCommandState::kAvailable
                 : SpaceCommandState::kUnavailable;
    case IDC_ALERSE_NEW_FOLDER:
      return SpaceCommandState::kAvailable;
    case IDC_ALERSE_CLEAR_TODAY:
      for (int i = 0; i < tabs->count(); ++i) {
        tabs::TabInterface* tab = tabs->GetTabAtIndex(i);
        if (IsInActiveSpace(tab) && !IsBound(tab) && !tab->IsPinned() &&
            !TabStripModel::IsAlerseEmptySpace(tab->GetContents())) {
          return SpaceCommandState::kAvailable;
        }
      }
      return SpaceCommandState::kUnavailable;
    case IDC_ALERSE_TOGGLE_PINNED: {
      // Only a Space with pinned items has them to collapse.
      const bookmarks::BookmarkNode* folder = GetSpaceFolder(false);
      if (!folder || folder->children().empty()) {
        return SpaceCommandState::kUnavailable;
      }
      return IsPinnedCollapsed(folder) ? SpaceCommandState::kOn
                                       : SpaceCommandState::kAvailable;
    }
    case IDC_ALERSE_REVEAL_TAB:
      return FindActiveTabRow() ? SpaceCommandState::kAvailable
                                : SpaceCommandState::kUnavailable;
    case IDC_ALERSE_COPY_URL_QUOTE:
      // Only with text selected on the page, as Arc's.
      return GetQuote(browser_).empty() ? SpaceCommandState::kUnavailable
                                        : SpaceCommandState::kAvailable;
    default:
      return SpaceCommandState::kUnavailable;
  }
}

void AlerseSidebarView::OnTabMutePressed(tabs::TabInterface::Handle handle) {
  tabs::TabInterface* tab = handle.Get();
  if (content::WebContents* contents = tab ? tab->GetContents() : nullptr) {
    // As the tab strip's speaker does.
    SetTabAudioMuted(contents, !contents->IsAudioMuted(),
                     TabMutedReason::kAudioIndicator, std::string());
  }
}

void AlerseSidebarView::OpenBookmark(const bookmarks::BookmarkNode* node) {
  if (IsFavorite(node)) {
    OpenFavorite(node);
    return;
  }
  if (tabs::TabInterface* tab = GetBoundTab(node->id())) {
    OnTabPressed(tab->GetHandle());
    return;
  }
  NavigateParams params(browser_, node->url(),
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  if (params.navigated_or_inserted_contents) {
    if (tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(
            params.navigated_or_inserted_contents)) {
      bound_[node->id()] = tab->GetHandle();
    }
  }
  ScheduleRebuild();
}

void AlerseSidebarView::PinTab(tabs::TabInterface* tab,
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

void AlerseSidebarView::AddFolder(const bookmarks::BookmarkNode* parent) {
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

void AlerseSidebarView::OnFolderHovered(int64_t id,
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
                     base::BindOnce(&AlerseSidebarView::ShowFolderPopup,
                                    weak_factory_.GetWeakPtr(), id, row));
}

void AlerseSidebarView::ShowFolderPopup(int64_t id, views::View* row) {
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
  folder_popup_query_.clear();
  folder_popup_selected_ = 0;
  folder_popup_rows_height_ = 0;
  BuildFolderPopup(contents.get());
  FillFolderPopup(contents.get(), node);

  // A bubble right of the row, shown without taking focus. It must be able to
  // activate: Windows drops clicks on bubbles that cannot.
  auto delegate = std::make_unique<FolderPopupBubble>(row, kPopupArrow);
  delegate->set_margins(gfx::Insets());
  delegate->set_corner_radius(kPopupRadius);
  delegate->SetBackgroundColor(alerse::SidebarInk(this, kPopupBackground));
  delegate->SetInitiallyFocusedView(folder_popup_field_);
  contents->SetBorder(std::make_unique<PopupRimBorder>());
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetShowCloseButton(false);
  delegate->set_close_on_deactivate(false);
  delegate->SetContentsView(std::move(contents));
  folder_popup_ = views::BubbleDialogDelegate::CreateBubble(
      delegate.get(), base::BindOnce(&AlerseSidebarView::OnFolderPopupClosed,
                                     drop_weak_factory_.GetWeakPtr()));
  folder_popup_delegate_ = std::move(delegate);
#if BUILDFLAG(IS_MAC)
  alerse::RemoveWindowShadow(folder_popup_.get());
#endif
  ui::Layer* layer = folder_popup_->GetLayer();
  if (layer) {
    layer->SetOpacity(0);
  }
  // Arc's popup takes the keyboard: its search field has the caret.
  folder_popup_->Show();
  if (layer) {
    ui::ScopedLayerAnimationSettings fade(layer->GetAnimator());
    fade.SetTransitionDuration(Motion(kPopupFadeDuration));
    fade.SetTweenType(gfx::Tween::EASE_OUT);
    layer->SetOpacity(1);
  }
  folder_popup_id_ = id;
  folder_popup_outside_ticks_ = 0;
  folder_popup_timer_.Start(FROM_HERE, kPopupPollInterval, this,
                            &AlerseSidebarView::CheckFolderPopupHover);
}

void AlerseSidebarView::BuildFolderPopup(views::View* contents) {
  folder_popup_search_ = std::make_unique<PopupSearchController>(
      base::BindRepeating(&AlerseSidebarView::OnFolderPopupQuery,
                          base::Unretained(this)),
      base::BindRepeating(&AlerseSidebarView::OnFolderPopupKey,
                          base::Unretained(this)));
  auto field = std::make_unique<views::Textfield>();
  field->set_controller(folder_popup_search_.get());
  field->SetBorder(views::NullBorder());
  field->SetBackgroundEnabled(false);
  // 13 pt, its hint #989897 as the command bar's.
  field->SetFontList(ArcFont(13, gfx::Font::Weight::NORMAL));
  field->SetTextColorId(kColorOmniboxText);
  field->SetPlaceholderTextColorId(kColorOmniboxForegroundDisabled);
  folder_popup_field_ = field.get();
  contents->AddChildView(std::make_unique<PopupSearchRow>(std::move(field)));
  auto* scroll = contents->AddChildView(std::make_unique<views::ScrollView>());
  scroll->SetBackgroundColor(std::nullopt);
  scroll->SetDrawOverflowIndicator(false);
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll->ClipHeightTo(0, kPopupMaxHeight - kPopupSearchHeight);
  auto rows = std::make_unique<views::View>();
  rows->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(4, 0)));
  folder_popup_rows_ = scroll->SetContents(std::move(rows));
}

void AlerseSidebarView::OnFolderPopupQuery(const std::u16string& query) {
  folder_popup_query_ = base::i18n::ToLower(query);
  // Later: this runs inside the field's own event.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AlerseSidebarView::RefreshFolderPopup,
                                drop_weak_factory_.GetWeakPtr()));
}

bool AlerseSidebarView::OnFolderPopupKey(ui::KeyboardCode key) {
  const int count = static_cast<int>(folder_popup_items_.size());
  switch (key) {
    case ui::VKEY_DOWN:
    case ui::VKEY_UP:
      if (count) {
        SelectFolderPopupRow(
            (folder_popup_selected_ + (key == ui::VKEY_DOWN ? 1 : -1) +
             count) %
            count);
      }
      return true;
    case ui::VKEY_RETURN:
      if (folder_popup_selected_ >= 0 && folder_popup_selected_ < count) {
        const int64_t id = folder_popup_items_[folder_popup_selected_];
        const bookmarks::BookmarkNode* node = GetNode(id);
        if (node && node->is_folder()) {
          ToggleFolderPopupFolder(id);
        } else {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](base::WeakPtr<AlerseSidebarView> sidebar, int64_t id) {
                    if (sidebar) {
                      sidebar->OnBookmarkPressed(id);
                      sidebar->CloseFolderPopup();
                    }
                  },
                  drop_weak_factory_.GetWeakPtr(), id));
        }
      }
      return true;
    case ui::VKEY_ESCAPE:
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&AlerseSidebarView::CloseFolderPopup,
                                    drop_weak_factory_.GetWeakPtr()));
      return true;
    default:
      return false;
  }
}

void AlerseSidebarView::SelectFolderPopupRow(int index) {
  folder_popup_selected_ = index;
  if (!folder_popup_rows_) {
    return;
  }
  int i = 0;
  for (views::View* child : folder_popup_rows_->children()) {
    if (Row* row = views::AsViewClass<Row>(child)) {
      row->SetSelected(i == index);
      if (i == index) {
        row->ScrollViewToVisible();
      }
    }
    ++i;
  }
}

void AlerseSidebarView::AddFilteredPopupRows(
    views::View* container,
    const bookmarks::BookmarkNode* folder) {
  for (const auto& child : folder->children()) {
    const bookmarks::BookmarkNode* node = child.get();
    if (node->is_folder()) {
      AddFilteredPopupRows(container, node);
    } else if (base::i18n::ToLower(node->GetTitle())
                       .find(folder_popup_query_) != std::u16string::npos ||
               base::i18n::ToLower(base::UTF8ToUTF16(node->url().spec()))
                       .find(folder_popup_query_) != std::u16string::npos) {
      AddFolderPopupLink(container, node, 0);
    }
  }
}

void AlerseSidebarView::FillFolderPopup(
    views::View* contents,
    const bookmarks::BookmarkNode* folder) {
  // Arc's popup: the search row stays, the rows below follow the query.
  folder_popup_field_->SetPlaceholderText(l10n_util::GetStringFUTF16(
      IDS_ALERSE_SEARCH_FOLDER, folder->GetTitle()));
  folder_popup_items_.clear();
  folder_popup_rows_->RemoveAllChildViews();
  if (folder_popup_query_.empty()) {
    AddFolderPopupRows(folder_popup_rows_, folder, 0);
  } else {
    AddFilteredPopupRows(folder_popup_rows_, folder);
  }
  folder_popup_speakers_ = SpeakerState(folder);
  // Arc keeps the selected row's place as the query narrows the list.
  const int count = static_cast<int>(folder_popup_items_.size());
  SelectFolderPopupRow(
      count ? std::clamp(folder_popup_selected_, 0, count - 1) : -1);
  folder_popup_rows_->SizeToPreferredSize();
  int rows_height = folder_popup_rows_->GetPreferredSize().height();
  // And its height: the rows a query leaves sit at the top of it.
  if (folder_popup_query_.empty()) {
    folder_popup_rows_height_ = rows_height;
  } else {
    rows_height = std::max(rows_height, folder_popup_rows_height_);
  }
  contents->SetPreferredSize(gfx::Size(
      kPopupWidth,
      kPopupSearchHeight +
          std::min(rows_height, kPopupMaxHeight - kPopupSearchHeight)));
}

void AlerseSidebarView::ToggleFolderPopupFolder(int64_t id) {
  if (!folder_popup_open_.erase(id)) {
    folder_popup_open_.insert(id);
  }
  // Later: the pressed row is among those rebuilt.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AlerseSidebarView::RefreshFolderPopup,
                                drop_weak_factory_.GetWeakPtr()));
}

void AlerseSidebarView::RefreshFolderPopup() {
  const bookmarks::BookmarkNode* folder = GetNode(folder_popup_id_);
  if (!folder_popup_ || !folder) {
    return;
  }
  FillFolderPopup(folder_popup_delegate_->GetContentsView(), folder);
  folder_popup_delegate_->GetContentsView()->InvalidateLayout();
  folder_popup_delegate_->SizeToContents();
}

void AlerseSidebarView::AddFolderPopupRows(
    views::View* container,
    const bookmarks::BookmarkNode* folder,
    int depth) {
  for (const auto& child : folder->children()) {
    const bookmarks::BookmarkNode* node = child.get();
    if (node->is_folder()) {
      [[maybe_unused]] Row* folder_row =
          container->AddChildView(std::make_unique<Row>(
              base::BindRepeating(&AlerseSidebarView::ToggleFolderPopupFolder,
                                  base::Unretained(this), node->id()),
              std::make_unique<FolderGlyph>(false, FolderIcon(node)),
              node->GetTitle(), depth, ItemFont(true), kFolderColor,
              /*slash=*/false, /*active=*/false));
      folder_row->SetPopupStyle(std::u16string());
      folder_popup_items_.push_back(node->id());
      if (folder_popup_open_.contains(node->id())) {
        AddFolderPopupRows(container, node, depth + 1);
      }
      continue;
    }
    AddFolderPopupLink(container, node, depth);
  }
}

void AlerseSidebarView::AddFolderPopupLink(
    views::View* container,
    const bookmarks::BookmarkNode* node,
    int depth) {
  Row* row = container->AddChildView(std::make_unique<Row>(
      base::BindRepeating(
          [](AlerseSidebarView* sidebar, int64_t id) {
            sidebar->OnBookmarkPressed(id);
            sidebar->CloseFolderPopup();
          },
          base::Unretained(this), node->id()),
      MakeImageIcon(GetBookmarkIcon(node)), node->GetTitle(), depth,
      ItemFont(false), kItemColor, /*slash=*/false, /*active=*/false));
  row->SetPopupStyle(LastUsedText(node));
  folder_popup_items_.push_back(node->id());
  if (tabs::TabInterface* tab = GetBoundTab(node->id())) {
    AddSpeaker(row, tab);
  }
}

void AlerseSidebarView::CheckFolderPopupHover() {
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
  // The row under a moving pointer is the selected one, as in Arc.
  if (folder_popup_rows_ && point != folder_popup_last_point_) {
    int i = 0;
    for (views::View* child : folder_popup_rows_->children()) {
      if (i != folder_popup_selected_ &&
          child->GetBoundsInScreen().Contains(point)) {
        SelectFolderPopupRow(i);
        break;
      }
      ++i;
    }
  }
  folder_popup_last_point_ = point;
  // The pointer is on the folder's menu while that runs.
  if (menu_runner_ && menu_runner_->IsRunning()) {
    inside = true;
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

void AlerseSidebarView::CloseFolderPopup() {
  folder_popup_timer_.Stop();
  folder_popup_id_ = 0;
  if (!folder_popup_) {
    return;
  }
  if (folder_popup_field_) {
    folder_popup_field_->set_controller(nullptr);
  }
  folder_popup_field_ = nullptr;
  folder_popup_rows_ = nullptr;
  folder_popup_items_.clear();
  // Deleted later: this may run from inside the popup's own events.
  folder_popup_->Hide();
  DeleteBubbleSoon(std::move(folder_popup_),
                   std::move(folder_popup_delegate_));
}

void AlerseSidebarView::OnFolderPopupClosed(
    views::Widget::ClosedReason reason) {
  CloseFolderPopup();
}

void AlerseSidebarView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  hover_timer_.Stop();
  menu_node_ = 0;
  menu_point_ = point;
  menu_tab_ = tabs::TabInterface::Handle();
  // Arc keeps a folder's popup open behind the folder's own menu, and after
  // Escape closes the menu.
  if (const auto source_it = row_targets_.find(source);
      !folder_popup_ || source_it == row_targets_.end() ||
      source_it->second.node != folder_popup_id_) {
    CloseFolderPopup();
  }
  // The previous menu's runner refers to the models replaced below.
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  // A row's speaker opens the row's menu.
  auto it = row_targets_.find(source);
  if (it == row_targets_.end() && source->parent()) {
    it = row_targets_.find(source->parent());
  }
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
      menu_model_->AddSubMenu(kMoveTo, l10n_util::GetStringUTF16(IDS_ALERSE_MOVE_TO),
                               move_menu_model_.get());
    }
  };

  // A tab's own items: its sound when it plays or is muted, and a split
  // with the current tab.
  // As in Arc, Mute stays in the menu, dimmed while there is no sound (the
  // row's speaker's rule), and so does Split View for a pinned tab that is
  // not open.
  auto add_mute = [this](tabs::TabInterface* tab) {
    content::WebContents* contents = tab ? tab->GetContents() : nullptr;
    const bool speaker = contents && ShowsSpeaker(tab);
    menu_model_->AddItem(
        kToggleMute,
        l10n_util::GetStringUTF16(speaker && contents->IsAudioMuted()
                                      ? IDS_ALERSE_UNMUTE_TAB
                                      : IDS_ALERSE_MUTE_TAB));
    menu_model_->SetEnabledAt(menu_model_->GetItemCount() - 1, speaker);
  };
  auto add_split = [this, node](tabs::TabInterface* tab) {
    content::WebContents* contents = tab ? tab->GetContents() : nullptr;
    if (!contents || !tab->IsSplit()) {
      menu_model_->AddItem(
          kSplitView, l10n_util::GetStringUTF16(IDS_ALERSE_OPEN_IN_SPLIT_VIEW));
      // A pinned item that is not open opens into the split.
      menu_model_->SetEnabledAt(menu_model_->GetItemCount() - 1,
                                contents || (node && node->is_url()));
    }
  };
  // Arc's Change Icon...: a glyph for a pinned item or folder.
  auto add_change_icon = [this] {
    menu_model_->AddItem(kChangeIcon,
                         l10n_util::GetStringUTF16(IDS_ALERSE_CHANGE_ICON));
  };

  // The pinned page: what the tab shows now, or edited by hand.
  auto add_pinned_page = [this, node](tabs::TabInterface* tab) {
    pinned_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    if (tab && tab->GetContents() &&
        tab->GetContents()->GetLastCommittedURL().is_valid() &&
        tab->GetContents()->GetLastCommittedURL() != node->url()) {
      pinned_menu_model_->AddItem(
          kReplacePinnedUrl,
          l10n_util::GetStringUTF16(IDS_ALERSE_REPLACE_PINNED_URL));
    }
    pinned_menu_model_->AddItem(
        kEditPinnedItem, l10n_util::GetStringUTF16(IDS_ALERSE_EDIT_PINNED_ITEM));
    menu_model_->AddSubMenu(kEditPinnedPage,
                            l10n_util::GetStringUTF16(IDS_ALERSE_EDIT_PINNED_PAGE),
                            pinned_menu_model_.get());
  };
  // Arc's Reset Tab to Original URL: once the item's tab has left its saved
  // page (the row shows "/"), back to that page.
  auto add_reset = [this, node](tabs::TabInterface* tab) {
    if (tab && tab->GetContents() && node->is_url() &&
        tab->GetContents()->GetLastCommittedURL().is_valid() &&
        tab->GetContents()->GetLastCommittedURL() != node->url()) {
      menu_model_->AddItem(
          kResetPinnedTab,
          l10n_util::GetStringUTF16(IDS_ALERSE_RESET_PINNED_TAB));
    }
  };

#if BUILDFLAG(IS_MAC)
  // Arc for macOS' Share: the system's services for the item's link.
  auto add_share = [this] {
    share_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    const std::vector<alerse::ShareService> services =
        alerse::GetShareServices();
    for (size_t i = 0; i < services.size(); ++i) {
      share_menu_model_->AddItemWithIcon(kShareFirst + static_cast<int>(i),
                                         services[i].title, services[i].icon);
    }
    if (!services.empty()) {
      menu_model_->AddSubMenu(kShare,
                              l10n_util::GetStringUTF16(IDS_ALERSE_SHARE),
                              share_menu_model_.get());
      menu_model_->SetEnabledAt(menu_model_->GetItemCount() - 1,
                                GetMenuLink().first.is_valid());
    }
  };
#else
  auto add_share = [] {};
#endif

  // Arc's menus, in its order (on macOS with Change Icon and Share).
  if (node && node->is_folder()) {
#if BUILDFLAG(IS_MAC)
    // Arc's Share Folder...: the system's picker for the folder's links.
    menu_model_->AddItem(kShareFolder,
                         l10n_util::GetStringUTF16(IDS_ALERSE_SHARE_FOLDER));
    menu_model_->SetEnabledAt(menu_model_->GetItemCount() - 1,
                              !GetFolderLinks(node).empty());
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
#endif
    menu_model_->AddItem(kNewFolder,
                         l10n_util::GetStringUTF16(IDS_ALERSE_NEW_FOLDER_INSIDE));
    menu_model_->AddItem(kTurnIntoSpace,
                         l10n_util::GetStringFUTF16(IDS_ALERSE_TURN_INTO_SPACE,
                                                    node->GetTitle()));
    menu_model_->AddItem(kCopyAllLinks,
                         l10n_util::GetStringUTF16(IDS_ALERSE_COPY_ALL_LINKS));
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    add_change_icon();
    menu_model_->AddItem(kRename, l10n_util::GetStringUTF16(IDS_ALERSE_RENAME));
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kDuplicate, l10n_util::GetStringUTF16(IDS_ALERSE_DUPLICATE));
    add_move_to();
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kRemove, l10n_util::GetStringUTF16(IDS_ALERSE_DELETE_FOLDER));
  } else if (node && IsFavorite(node)) {
    menu_model_->AddItem(kCopyLink, l10n_util::GetStringUTF16(IDS_ALERSE_COPY_LINK));
    add_share();
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    add_change_icon();
    menu_model_->AddItem(kRename, l10n_util::GetStringUTF16(IDS_ALERSE_RENAME));
    add_mute(tab);
    add_pinned_page(tab);
    add_reset(tab);
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    add_split(tab);
    menu_model_->AddItem(kDuplicate, l10n_util::GetStringUTF16(IDS_ALERSE_DUPLICATE));
    add_move_to();
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kRemoveFavorite,
                         l10n_util::GetStringUTF16(IDS_ALERSE_REMOVE_FAVORITE));
  } else if (node) {
    menu_model_->AddItem(kCopyLink, l10n_util::GetStringUTF16(IDS_ALERSE_COPY_LINK));
    add_share();
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    add_change_icon();
    menu_model_->AddItem(kRename, l10n_util::GetStringUTF16(IDS_ALERSE_RENAME));
    add_mute(tab);
    add_pinned_page(tab);
    add_reset(tab);
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    add_split(tab);
    menu_model_->AddItem(kDuplicate, l10n_util::GetStringUTF16(IDS_ALERSE_DUPLICATE));
    add_move_to();
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kRemove, l10n_util::GetStringUTF16(IDS_ALERSE_UNPIN));
    if (tab) {
      menu_model_->AddItem(kCloseTab, l10n_util::GetStringUTF16(IDS_ALERSE_CLOSE_TAB));
    }
  } else if (tab) {
    menu_model_->AddItem(kCopyLink, l10n_util::GetStringUTF16(IDS_ALERSE_COPY_LINK));
    add_share();
    add_mute(tab);
    add_split(tab);
    menu_model_->AddItem(kDuplicate, l10n_util::GetStringUTF16(IDS_ALERSE_DUPLICATE));
    menu_model_->AddItem(kPinTab, l10n_util::GetStringUTF16(IDS_ALERSE_PIN_TO_SPACE));
    menu_model_->AddItem(kAddFavorite,
                         l10n_util::GetStringUTF16(IDS_ALERSE_ADD_TO_FAVORITES));
    add_move_to();
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kCloseTab, l10n_util::GetStringUTF16(IDS_ALERSE_CLOSE_TAB));
  } else {
    // Arc's Space menu: on the Space's name, New Tab, the address field and
    // the empty space below the rows.
    menu_model_.reset();
    ShowSpaceMenu(point, source_type);
    return;
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(source->GetWidget(), nullptr,
                          gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

std::vector<AlerseSidebarView::SpaceTarget>
AlerseSidebarView::GetOtherSpaces() const {
  std::vector<SpaceTarget> spaces;
  Profile* profile = browser_->GetProfile();
  if (profile->IsOffTheRecord()) {
    return spaces;
  }
  // In the Space bar's order.
  for (const alerse::SpaceInfo& space : alerse::ListSpaces()) {
    if (space.path != profile->GetPath()) {
      spaces.push_back({space.path, space.name});
    }
  }
  return spaces;
}

void AlerseSidebarView::MoveToSpace(const SpaceTarget& target) {
  const bookmarks::BookmarkNode* node = GetNode(menu_node_);
  tabs::TabInterface* tab = menu_tab_.Get();
  if (!node) {
    if (tab) {
      MoveTabToSpace(tab, target);
    }
    return;
  }
  // The other Space's bookmarks are another profile's: the item is copied
  // into its folder, then removed here.
  alerse::WithSpaceBookmarks(
      target.path,
      base::BindOnce(
          [](base::WeakPtr<AlerseSidebarView> sidebar, int64_t id,
             std::u16string title, Profile* profile,
             bookmarks::BookmarkModel* to) {
            const bookmarks::BookmarkNode* node =
                sidebar ? sidebar->GetNode(id) : nullptr;
            if (!node) {
              return;
            }
            alerse::CopyNodeTo(node, to,
                                 alerse::GetOrAddSpaceFolder(to, title));
            sidebar->RemoveMovedNode(node);
          },
          drop_weak_factory_.GetWeakPtr(), node->id(), target.title));
}

void AlerseSidebarView::RemoveMovedNode(const bookmarks::BookmarkNode* node) {
  // A pinned item's open tab stays behind in this Space's storage, so it
  // closes; the item opens again in the other Space.
  std::vector<tabs::TabInterface*> bound;
  std::vector<const bookmarks::BookmarkNode*> pending = {node};
  while (!pending.empty()) {
    const bookmarks::BookmarkNode* next = pending.back();
    pending.pop_back();
    for (const auto& child : next->children()) {
      pending.push_back(child.get());
    }
    if (tabs::TabInterface* tab = GetBoundTab(next->id())) {
      bound.push_back(tab);
      bound_.erase(next->id());
    }
  }
  model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                 FROM_HERE);
  for (tabs::TabInterface* tab : bound) {
    // At once: a close its page cancels leaves an ordinary tab behind.
    WritePinnedItem(tab, std::string());
    OnTabClosePressed(tab->GetHandle());
  }
}

void AlerseSidebarView::MoveTabToSpace(tabs::TabInterface* tab,
                                         const SpaceTarget& target) {
  content::WebContents* old_contents = tab->GetContents();
  GURL url = old_contents->GetLastCommittedURL();
  if (!url.is_valid()) {
    url = old_contents->GetVisibleURL();
  }
  // The Space it leaves stays, empty, as in Arc.
  {
    tabs::TabInterface* const leaving[] = {tab};
    browser_->GetTabStripModel()->AlerseKeepSpaces(leaving);
  }
  OnTabClosePressed(tab->GetHandle());
  // Each Space has its own storage, so the page opens anew in the target's
  // window, which takes this one's place.
  alerse::OpenUrlInSpace(browser_, target.path, url);
}

void AlerseSidebarView::DuplicateItem() {
  const bookmarks::BookmarkNode* node = GetNode(menu_node_);
  tabs::TabInterface* tab = menu_tab_.Get();
  TabStripModel* tabs = browser_->GetTabStripModel();
  if (tab) {
    // Keeps the page's history.
    if (const int index = tabs->GetIndexOfTab(tab);
        index != TabStripModel::kNoTab) {
      chrome::DuplicateTabAt(browser_, index);
    }
    return;
  }
  if (node && node->is_url()) {
    NavigateParams params(browser_, node->url(),
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }
}

std::pair<GURL, std::u16string> AlerseSidebarView::GetMenuLink() const {
  const bookmarks::BookmarkNode* node = GetNode(menu_node_);
  tabs::TabInterface* tab = menu_tab_.Get();
  if (node && node->is_url()) {
    return {node->url(), node->GetTitle()};
  }
  if (tab && tab->GetContents()) {
    return {tab->GetContents()->GetLastCommittedURL(),
            tab->GetContents()->GetTitle()};
  }
  return {};
}

void AlerseSidebarView::CopyLink() {
  const GURL url = GetMenuLink().first;
  if (url.is_valid()) {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(base::UTF8ToUTF16(url.spec()));
  }
}

void AlerseSidebarView::ShowIconPicker(const bookmarks::BookmarkNode* node) {
  const bool folder = node->is_folder();
  const char* const key = folder ? kFolderIconMetaKey : kGlyphMetaKey;
  std::string current;
  node->GetMetaInfo(key, &current);
  // As in Arc, under the item's row with the arrow at its icon; under a
  // favorite's tile, at its middle.
  gfx::Rect anchor(menu_point_, gfx::Size());
  int arrow_x = menu_point_.x();
  for (const auto& [view, target] : row_targets_) {
    if (target.node == node->id()) {
      anchor = view->GetBoundsInScreen();
      const Row* row = views::AsViewClass<Row>(view);
      arrow_x = row ? anchor.x() + row->GetIconCenterX()
                    : anchor.CenterPoint().x();
      break;
    }
  }
  // As in Arc for macOS the item's row (or tile) is not kept lit while its
  // picker is open: only the pointer lights it.
  alerse::ShowArcIconPicker(
      this, anchor, arrow_x,
      folder ? alerse::IconPickerKind::kFolder
             : alerse::IconPickerKind::kItem,
      current,
      base::BindOnce(&AlerseSidebarView::SetNodeIcon,
                     drop_weak_factory_.GetWeakPtr(), node->id(),
                     std::string(key)),
      base::DoNothing());
}

bool AlerseSidebarView::GetSpaceTitleAnchor(gfx::Rect* bounds,
                                              int* arrow_x) const {
  if (!space_title_ || !space_title_->GetWidget() ||
      !space_title_->IsDrawn()) {
    return false;
  }
  *bounds = space_title_->GetBoundsInScreen();
  *arrow_x = bounds->x() + kSpaceTitleIconX;
  return true;
}

void AlerseSidebarView::SetSpaceTitleHighlighted(bool highlighted) {
  space_title_highlighted_ = highlighted;
  if (auto* title = views::AsViewClass<SpaceTitle>(space_title_.get())) {
    title->SetHighlighted(highlighted);
  }
}

void AlerseSidebarView::SetNodeIcon(int64_t id,
                                      const std::string& key,
                                      const std::string& name) {
  const bookmarks::BookmarkNode* node = GetNode(id);
  if (!node) {
    return;
  }
  if (name.empty()) {
    model_->DeleteNodeMetaInfo(node, key);
  } else {
    model_->SetNodeMetaInfo(node, key, name);
  }
  ScheduleRebuild();
}

void AlerseSidebarView::ExecuteCommand(int command_id, int event_flags) {
  // A folder's popup kept behind its menu goes once an item is picked.
  CloseFolderPopup();
#if BUILDFLAG(IS_MAC)
  if (command_id >= kShareFirst) {
    const auto [url, title] = GetMenuLink();
    if (url.is_valid()) {
      alerse::ShareWithService(static_cast<size_t>(command_id - kShareFirst),
                                 {url}, title);
    }
    return;
  }
#endif
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
#if BUILDFLAG(IS_MAC)
    case kShareFolder:
      if (node && node->is_folder()) {
        // Once the menu is gone: the picker is a menu of its own.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&AlerseSidebarView::ShareFolder,
                                      drop_weak_factory_.GetWeakPtr(),
                                      node->id()));
      }
      break;
#endif
    case kDuplicate:
      if (node && node->is_folder()) {
        DuplicateFolder(node);
      } else {
        DuplicateItem();
      }
      break;
    case kPinTab:
      // The menu's tab may have closed while the menu was open.
      if (tab) {
        PinTab(tab, node && node->is_folder() ? node : nullptr);
      }
      break;
    case kNewFolder:
      AddFolder(node && node->is_folder() ? node : nullptr);
      break;
    case kRename:
      if (node) {
        RenameNode(node->id());
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
    case kChangeIcon:
      if (node) {
        ShowIconPicker(node);
      }
      break;
    case kResetPinnedTab:
      if (node) {
        ResetPinnedTab(node->id());
      }
      break;
    case kSplitView:
      if (!tab && node && node->is_url()) {
        // Opens it, then splits it with the tab that was showing.
        TabStripModel* model = browser_->GetTabStripModel();
        tabs::TabInterface* shown = model->GetActiveTab();
        if (!shown || shown->IsSplit()) {
          OpenBookmark(node);
          break;
        }
        OpenBookmark(node);
        tabs::TabInterface* opened = GetBoundTab(node->id());
        const int shown_index = model->GetIndexOfTab(shown);
        const int opened_index = opened ? model->GetIndexOfTab(opened)
                                        : TabStripModel::kNoTab;
        if (opened && opened != shown && shown_index != TabStripModel::kNoTab &&
            opened_index != TabStripModel::kNoTab) {
          model->ActivateTabAt(shown_index);
          model->ExecuteAddToNewSplitCommand(
              model->GetIndexOfTab(opened),
              split_tabs::SplitTabLayout::kSideBySide);
        }
        break;
      }
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
    case kAddFavorite:
      AddFavorite(tab, node && !node->is_folder() ? node : nullptr,
                  /*index=*/SIZE_MAX);
      break;
    case kRemoveFavorite:
      if (node && IsFavorite(node)) {
        RemoveFavorite(node);
      }
      break;
    case kCopyAllLinks:
      if (node && node->is_folder()) {
        CopyAllLinks(node);
      }
      break;
    case kTurnIntoSpace:
      if (node && node->is_folder()) {
        TurnIntoSpace(node);
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

void AlerseSidebarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  // Each step of a Space's color transition re-themes the window; the rows
  // recolor themselves, and they are rebuilt once it settles.
  if (rows_ && rows_->children().empty()) {
    ScheduleRebuild();
    return;
  }
  theme_rebuild_timer_.Start(
      FROM_HERE, base::Milliseconds(250),
      base::BindOnce(&AlerseSidebarView::ScheduleRebuild,
                     base::Unretained(this)));
}

void AlerseSidebarView::AddedToWidget() {
  views::View::AddedToWidget();
#if BUILDFLAG(IS_MAC)
  // Only macOS lightens an inactive frame so far (alerse::FrameColor);
  // elsewhere a rebuild would change nothing.
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &AlerseSidebarView::ScheduleRebuild, base::Unretained(this)));
#endif
}

void AlerseSidebarView::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
  views::View::RemovedFromWidget();
}

void AlerseSidebarView::EditSpaceColorAt(const gfx::Point& screen_point) {
  menu_point_ = screen_point;
  ShowColorPicker();
}

void AlerseSidebarView::ShowColorPicker() {
  CloseColorPicker();
  if (!GetWidget()) {
    return;
  }
  alerse::SpaceColor initial;
  std::string value;
  if (const bookmarks::BookmarkNode* folder = GetSpaceFolder(false);
      folder && folder->GetMetaInfo(alerse::kSpaceColorMetaKey, &value)) {
    initial = alerse::ParseSpaceColor(value).value_or(initial);
  }
  // Arc's picker opens where the menu was, and closes when it loses focus.
  auto delegate = std::make_unique<views::BubbleDialogDelegate>(
      nullptr, views::BubbleBorder::TOP_LEFT);
  delegate->set_parent_window(GetWidget()->GetNativeView());
  delegate->SetAnchorRect(gfx::Rect(menu_point_, gfx::Size()));
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetShowCloseButton(false);
  delegate->set_margins(gfx::Insets());
  delegate->SetBackgroundColor(alerse::kSpaceColorPickerBackground);
  delegate->SetContentsView(alerse::CreateSpaceColorPicker(
      initial, base::BindRepeating(&AlerseSidebarView::OnSpaceColorPicked,
                                   drop_weak_factory_.GetWeakPtr())));
  color_picker_ = views::BubbleDialogDelegate::CreateBubble(
      delegate.get(), base::BindOnce(&AlerseSidebarView::OnColorPickerClosed,
                                     drop_weak_factory_.GetWeakPtr()));
  color_picker_delegate_ = std::move(delegate);
  color_picker_->Show();
}

void AlerseSidebarView::CloseColorPicker() {
  if (!color_picker_) {
    return;
  }
  // Deleted later: this may run from inside the picker's own events.
  color_picker_->Hide();
  DeleteBubbleSoon(std::move(color_picker_),
                   std::move(color_picker_delegate_));
}

void AlerseSidebarView::OnColorPickerClosed(
    views::Widget::ClosedReason reason) {
  CloseColorPicker();
}

void AlerseSidebarView::OnSpaceColorPicked(const alerse::SpaceColor& color,
                                             bool commit) {
  // Shown at once; stored (and synced) once a drag ends.
  alerse::ApplySpaceColor(GetWidget(), color);
  if (commit) {
    if (const bookmarks::BookmarkNode* folder = GetSpaceFolder(true)) {
      model_->SetNodeMetaInfo(folder, alerse::kSpaceColorMetaKey,
                              alerse::SerializeSpaceColor(color));
    }
  }
}

void AlerseSidebarView::WriteDragDataForView(views::View* sender,
                                               const gfx::Point& press_pt,
                                               ui::OSExchangeData* data) {
  auto it = row_targets_.find(sender);
  if (it == row_targets_.end()) {
    return;
  }
  drag_source_ = it->second;
  alerse::SidebarRowDragging() = true;
  const Row* row = views::AsViewClass<Row>(sender);
  const bookmarks::BookmarkNode* node = GetNode(it->second.node);
  // A page's address lets Chromium's split drop zone on the page's edges
  // take the row (SplitDraggedRow), and other apps take it as a link; the
  // page itself takes no drop from it (alerse_sidebar_drag.h). A folder
  // carries only a text that marks the data as ours.
  tabs::TabInterface* const tab = node ? nullptr : it->second.tab.Get();
  const GURL url = node ? (node->is_url() ? node->url() : GURL())
                        : (tab && tab->GetContents()
                               ? tab->GetContents()->GetLastCommittedURL()
                               : GURL());
  if (url.is_valid()) {
    data->SetURL(url, node ? node->GetTitle()
                           : tab->GetContents()->GetTitle());
  } else {
    data->SetString(kRowDragData);
  }
  button_drag_utils::SetDragImage(
      GURL(), row ? row->title() : (node ? node->GetTitle() : std::u16string()),
      gfx::ImageSkia(), &press_pt, data);
}

int AlerseSidebarView::GetDragOperationsForView(views::View* sender,
                                                  const gfx::Point& p) {
  if (!row_targets_.contains(sender)) {
    return ui::DragDropTypes::DRAG_NONE;
  }
#if BUILDFLAG(IS_MAC)
  return ui::DragDropTypes::DRAG_MOVE;
#else
  // Chromium's split drop zone on the page's edges takes a drop as a link
  // (MultiContentsViewDropTargetController::OnDragUpdated); Windows' drag and
  // drop delivers no drop whose operation the source does not allow.
  return ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_LINK;
#endif
}

bool AlerseSidebarView::CanStartDragForView(views::View* sender,
                                              const gfx::Point& press_pt,
                                              const gfx::Point& p) {
  return views::View::ExceededDragThreshold(p - press_pt);
}

bool AlerseSidebarView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::STRING | ui::OSExchangeData::URL;
  return true;
}

bool AlerseSidebarView::CanDrop(const ui::OSExchangeData& data) {
  // Only this sidebar's own rows: the drag started here.
  return drag_source_.has_value();
}

int AlerseSidebarView::OnDragUpdated(const ui::DropTargetEvent& event) {
  const std::optional<DropTarget> target = GetDropTarget(event.location());
  ShowDropIndicator(target);
  return target ? ui::DragDropTypes::DRAG_MOVE : ui::DragDropTypes::DRAG_NONE;
}

void AlerseSidebarView::OnDragExited() {
  ShowDropIndicator(std::nullopt);
}

views::View::DropCallback AlerseSidebarView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  std::optional<DropTarget> target = GetDropTarget(event.location());
  ShowDropIndicator(std::nullopt);
  if (!target) {
    return base::NullCallback();
  }
  return base::BindOnce(&AlerseSidebarView::PerformDrop,
                        drop_weak_factory_.GetWeakPtr(), *target);
}

std::optional<AlerseSidebarView::DropTarget>
AlerseSidebarView::GetDropTarget(const gfx::Point& point) {
  if (!drag_source_ || !model_) {
    return std::nullopt;
  }
  const bookmarks::BookmarkNode* source_node = GetNode(drag_source_->node);
  tabs::TabInterface* source_tab =
      source_node ? nullptr : drag_source_->tab.Get();
  if (!source_node && !source_tab) {
    return std::nullopt;
  }
  if (const gfx::Rect area = favorites_->bounds();
      (source_tab || source_node->is_url()) &&
      (area.Contains(point) ||
       // With no favorites, the strip under the address field takes one.
       (favorites_->children().empty() && point.y() >= area.y() - 4 &&
        point.y() < area.y() + 12))) {
    DropTarget target;
    target.favorite = true;
    gfx::Point in_grid = point;
    views::View::ConvertPointToTarget(this, favorites_, &in_grid);
    target.index =
        static_cast<FavoritesGrid*>(favorites_.get())->GetDropIndex(in_grid);
    target.into_folder = true;
    target.indicator = favorites_->children().empty()
                           ? gfx::Rect(kRowBackgroundInsetX, area.y(),
                                       width() - 2 * kRowBackgroundInsetX, 8)
                           : area;
    return target;
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
  int after_last_tab = tabs->count();
  // A split row's tabs: the strip indices of its first and last.
  auto pair_range = [this, tabs](views::View* pair) {
    std::pair<int, int> range(tabs->count(), -1);
    for (views::View* half : pair->children()) {
      auto found = row_targets_.find(half);
      tabs::TabInterface* tab =
          found == row_targets_.end() ? nullptr : found->second.tab.Get();
      const int index = tab ? tabs->GetIndexOfTab(tab) : TabStripModel::kNoTab;
      if (index != TabStripModel::kNoTab) {
        range = {std::min(range.first, index), std::max(range.second, index)};
      }
    }
    return range;
  };
  for (views::View* child : rows_->children()) {
    const bool hit = p.y() >= child->y() && p.y() < child->bounds().bottom();
    if (views::IsViewClass<SplitPair>(child)) {
      // A split's row takes no third page: a drop goes above or below it.
      const auto [first, last] = pair_range(child);
      if (last < 0) {
        continue;
      }
      last_tab_row = child;
      after_last_tab = last + 1;
      if (!hit) {
        continue;
      }
      if (source_node && source_node->is_folder()) {
        return std::nullopt;
      }
      const bool lower = p.y() >= child->y() + child->height() / 2;
      DropTarget target;
      target.tab_index = lower ? last + 1 : first;
      target.indicator = line(child, lower);
      return target;
    }
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
      after_last_tab = tabs->GetIndexOfTab(row.tab.Get()) + 1;
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
        rect.Inset(gfx::Insets::VH(static_cast<int>(kRowBackgroundInsetY),
                                   kRowBackgroundInsetX));
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
      // As in Arc, a tab or a pinned item dropped onto the middle of a tab
      // makes a split of the two; the row's top and bottom quarters still
      // move it. Not onto a split (two panes at most) or onto itself.
      const int quarter = child->height() / 4;
      const bool can_split =
          (source_tab || (source_node->is_url() && !IsFavorite(source_node))) &&
          source_tab != tab && !tab->IsSplit();
      if (can_split && p.y() >= child->y() + quarter &&
          p.y() < child->bounds().bottom() - quarter) {
        target.split_with = tab->GetHandle();
        target.into_folder = true;
        gfx::Rect rect =
            views::View::ConvertRectToTarget(rows_, this, child->bounds());
        rect.Inset(gfx::Insets::VH(static_cast<int>(kRowBackgroundInsetY),
                                   kRowBackgroundInsetX));
        target.indicator = rect;
        return target;
      }
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
      if (after && views::IsViewClass<SplitPair>(next)) {
        if (const int first = pair_range(next).first; first < tabs->count()) {
          target.tab_index = first;
          break;
        }
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
      target.tab_index = after_last_tab;
      target.indicator = line(last_tab_row, true);
    }
    return target;
  }
  return std::nullopt;
}

void AlerseSidebarView::ShowDropIndicator(
    const std::optional<DropTarget>& target) {
  if (!target || target->indicator.IsEmpty()) {
    drop_indicator_->SetVisible(false);
    return;
  }
  drop_indicator_->SetBackground(
      target->into_folder
          ? views::CreateRoundedRectBackground(
                alerse::SidebarInk(this, kActiveColor),
                                               kRowBackgroundRadius)
          : views::CreateSolidBackground(alerse::SidebarInk(this, kItemColor)));
  // A split's target is outlined, so it differs from the shown tab's plate.
  drop_indicator_->SetBorder(
      target->split_with.Get()
          ? views::CreateRoundedRectBorder(
                2, kRowBackgroundRadius, alerse::SidebarInk(this, kItemColor))
          : nullptr);
  drop_indicator_->SetBoundsRect(target->indicator);
  drop_indicator_->SetVisible(true);
}

void AlerseSidebarView::PerformDrop(
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

  if (target.favorite) {
    AddFavorite(tab, node, target.index);
    output_drag_op = ui::mojom::DragOperation::kMove;
    return;
  }
  if (tabs::TabInterface* with = target.split_with.Get()) {
    // The dropped page goes to the right of the one it was dropped on.
    if (SplitTabs(with, GetDraggedRowTab(/*open=*/true), /*start=*/false,
                  /*stacked=*/false)) {
      output_drag_op = ui::mojom::DragOperation::kMove;
    }
    ScheduleRebuild();
    return;
  }
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
      UnpinBoundTab(node);
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
        // Its row goes to the pinned items, out of the split's row.
        if (tab->IsSplit()) {
          browser_->GetTabStripModel()->RemoveSplit(*tab->GetSplit());
        }
      }
    }
  } else if (node) {
    // Unpins the item; its tab stays open, or opens.
    if (node->is_folder()) {
      return;
    }
    tabs::TabInterface* bound = GetBoundTab(node->id());
    UnpinBoundTab(node);
    bound_.erase(node->id());
    if (bound) {
      MoveTabToSlot(bound, *target.tab_index);
    } else {
      NavigateParams params(browser_, node->url(),
                            ui::PAGE_TRANSITION_AUTO_BOOKMARK);
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      Navigate(&params);
    }
    model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                   FROM_HERE);
  } else if (tab) {
    // A pane dragged out of its split's row leaves the split, its other
    // pane becoming a plain tab, even when dropped back where it was.
    if (tab->IsSplit()) {
      browser_->GetTabStripModel()->RemoveSplit(*tab->GetSplit());
    }
    MoveTabToSlot(tab, *target.tab_index);
  }
  output_drag_op = ui::mojom::DragOperation::kMove;
  ScheduleRebuild();
}

void AlerseSidebarView::MoveTabToSlot(tabs::TabInterface* tab, int slot) {
  TabStripModel* tabs = browser_->GetTabStripModel();
  const int from = tabs->GetIndexOfTab(tab);
  if (from == TabStripModel::kNoTab) {
    return;
  }
  const int to =
      std::clamp(slot > from ? slot - 1 : slot, 0, tabs->count() - 1);
  if (to != from) {
    tabs->MoveWebContentsAt(from, to, /*select_after_move=*/false);
  }
}

void AlerseSidebarView::EndRowDrag() {
  drag_source_.reset();
  alerse::SidebarRowDragging() = false;
}

tabs::TabInterface* AlerseSidebarView::GetDraggedRowTab(bool open) {
  if (!drag_source_) {
    return nullptr;
  }
  const bookmarks::BookmarkNode* node = GetNode(drag_source_->node);
  if (!node) {
    return drag_source_->tab.Get();
  }
  // A favorite's tab is pinned outside the Spaces and stays so.
  if (!node->is_url() || IsFavorite(node)) {
    return nullptr;
  }
  if (tabs::TabInterface* bound = GetBoundTab(node->id())) {
    return bound;
  }
  if (!open) {
    return nullptr;
  }
  OpenBookmark(node);
  return GetBoundTab(node->id());
}

bool AlerseSidebarView::SplitTabs(tabs::TabInterface* with,
                                    tabs::TabInterface* tab,
                                    bool start,
                                    bool stacked) {
  TabStripModel* const model = browser_->GetTabStripModel();
  if (!with || !tab || with == tab || with->IsSplit() ||
      with->GetGroup() != tab->GetGroup() ||
      with->IsPinned() != tab->IsPinned() ||
      model->GetIndexOfTab(with) == TabStripModel::kNoTab ||
      model->GetIndexOfTab(tab) == TabStripModel::kNoTab) {
    return false;
  }
  if (tab->IsSplit()) {
    // Out of its split first, which leaves its other pane a plain tab.
    model->RemoveSplit(*tab->GetSplit());
  }
  // The first index is the split's pivot: `with` keeps its place.
  const split_tabs::SplitTabId split = model->AddToNewSplit(
      {model->GetIndexOfTab(with), model->GetIndexOfTab(tab)},
      split_tabs::SplitTabVisualData(
          stacked ? split_tabs::SplitTabLayout::kStacked
                  : split_tabs::SplitTabLayout::kSideBySide),
      split_tabs::SplitTabCreatedSource::kDragAndDropTab);
  // The panes follow the tabs' order, from the start side.
  if (split_tabs::SplitTabData* data = model->GetSplitData(split);
      data && !data->ListTabs().empty() &&
      (data->ListTabs()[0] == tab) != start) {
    model->ReverseTabsInSplit(split);
  }
  model->ActivateTabAt(model->GetIndexOfTab(tab));
  return true;
}

// static
bool AlerseSidebarView::SplitDraggedRow(BrowserWindowInterface* browser,
                                          bool start,
                                          bool stacked) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  AlerseSidebarView* sidebar =
      browser_view ? FindSidebarIn(browser_view) : nullptr;
  if (!sidebar || !sidebar->drag_source_) {
    return false;
  }
  // A favorite opens a new pane at its address, as any link would.
  if (const bookmarks::BookmarkNode* node =
          sidebar->GetNode(sidebar->drag_source_->node);
      node && (!node->is_url() || sidebar->IsFavorite(node))) {
    return false;
  }
  TabStripModel* const model = browser->GetTabStripModel();
  tabs::TabInterface* const shown = model->GetActiveTab();
  // The drop is the sidebar's even when the page cannot join the shown one
  // (it is the shown page, or of another Space): no second copy opens.
  sidebar->SplitTabs(shown, sidebar->GetDraggedRowTab(/*open=*/true), start,
                     stacked);
  sidebar->ScheduleRebuild();
  return true;
}

void AlerseSidebarView::OnRowDragDone() {
  // Done, cancelled (Escape) or refused alike.
  EndRowDrag();
  HideSplitDropZones();
  ShowDropIndicator(std::nullopt);
  if (std::exchange(rebuild_after_drag_, false)) {
    ScheduleRebuild();
  }
}

void AlerseSidebarView::RenameNode(int64_t id) {
  const bookmarks::BookmarkNode* node = GetNode(id);
  if (node && GetWidget()) {
    // A new title from the editor marks the item as named by the user.
    renaming_node_ = id;
    renaming_title_ = node->GetTitle();
    BookmarkEditor::Show(GetWidget()->GetNativeWindow(), browser_->GetProfile(),
                         BookmarkEditor::EditDetails::EditNode(node),
                         BookmarkEditor::NO_TREE);
  }
}

void AlerseSidebarView::OnCommandBarChanged(bool open, bool new_tab) {
  if (command_bar_new_tab_ != (open && new_tab)) {
    command_bar_new_tab_ = open && new_tab;
    ScheduleRebuild();
  }
}

const bookmarks::BookmarkNode* AlerseSidebarView::GetFavoritesFolder(
    bool create) {
  if (!model_ || !model_->loaded()) {
    return nullptr;
  }
  for (const auto& child : model_->other_node()->children()) {
    std::string value;
    if (child->is_folder() && child->GetMetaInfo(kFavoritesMetaKey, &value) &&
        value == "1") {
      return child.get();
    }
  }
  if (!create) {
    return nullptr;
  }
  bookmarks::BookmarkNode::MetaInfoMap meta;
  meta[kFavoritesMetaKey] = "1";
  return model_->AddFolder(model_->other_node(),
                           model_->other_node()->children().size(),
                           l10n_util::GetStringUTF16(IDS_ALERSE_FAVORITES),
                           &meta);
}

bool AlerseSidebarView::IsFavorite(
    const bookmarks::BookmarkNode* node) const {
  if (!node || !model_ || !node->parent()) {
    return false;
  }
  const bookmarks::BookmarkNode* folder = node->parent();
  std::string value;
  return folder->parent() == model_->other_node() &&
         folder->GetMetaInfo(kFavoritesMetaKey, &value) && value == "1";
}

void AlerseSidebarView::BindFavoritesByUrl(
    const bookmarks::BookmarkNode* folder) {
  TabStripModel* tabs = browser_->GetTabStripModel();
  for (const auto& child : folder->children()) {
    if (!child->is_url() || GetBoundTab(child->id())) {
      continue;
    }
    for (int i = 0; i < tabs->count(); ++i) {
      tabs::TabInterface* tab = tabs->GetTabAtIndex(i);
      if (tab->IsPinned() && !IsBound(tab) &&
          tab->GetContents()->GetLastCommittedURL() == child->url()) {
        bound_[child->id()] = tab->GetHandle();
        break;
      }
    }
  }
}

void AlerseSidebarView::BuildFavorites() {
  favorites_->RemoveAllChildViews();
  const bookmarks::BookmarkNode* folder = GetFavoritesFolder(false);
  if (folder) {
    BindFavoritesByUrl(folder);
    TabStripModel* tabs = browser_->GetTabStripModel();
    for (const auto& child : folder->children()) {
      if (!child->is_url()) {
        continue;
      }
      tabs::TabInterface* tab = GetBoundTab(child->id());
      ui::ImageModel icon = GetBookmarkIcon(child.get());
      if (tab && !HasOwnIcon(child.get())) {
        icon = GetTabIcon(tab);
      }
      auto* tile = favorites_->AddChildView(std::make_unique<FavoriteTile>(
          base::BindRepeating(&AlerseSidebarView::OnBookmarkPressed,
                              base::Unretained(this), child->id()),
          icon, child->GetTitle(), tab && tab == tabs->GetActiveTab()));
      tile->set_context_menu_controller(this);
      tile->set_drag_controller(this);
      tile->SetDragDoneCallback(base::BindRepeating(
          &AlerseSidebarView::OnRowDragDone, base::Unretained(this)));
      row_targets_[tile] =
          RowTarget{child->id(), tab ? tab->GetHandle()
                                     : tabs::TabInterface::Handle()};
    }
  }
  favorites_->InvalidateLayout();
}

void AlerseSidebarView::OpenFavorite(const bookmarks::BookmarkNode* node) {
  if (tabs::TabInterface* tab = GetBoundTab(node->id())) {
    OnTabPressed(tab->GetHandle());
    return;
  }
  // Outside the Spaces, in their default storage, as Arc's favorites are
  // the same in every Space.
  Profile* profile = browser_->GetProfile();
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          profile, tab_util::GetSiteInstanceForNewTab(profile, node->url())));
  contents->GetController().LoadURL(node->url(), content::Referrer(),
                                    ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                    std::string());
  content::WebContents* opened = contents.get();
  TabStripModel* tabs = browser_->GetTabStripModel();
  tabs->AddWebContents(std::move(contents), tabs->IndexOfFirstNonPinnedTab(),
                       ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                       AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_PINNED,
                       std::nullopt);
  if (tabs::TabInterface* tab =
          tabs::TabInterface::MaybeGetFromContents(opened)) {
    bound_[node->id()] = tab->GetHandle();
  }
  ScheduleRebuild();
}

tabs::TabInterface* AlerseSidebarView::PinAsFavoriteTab(
    tabs::TabInterface* tab) {
  TabStripModel* tabs = browser_->GetTabStripModel();
  int index = tabs->GetIndexOfTab(tab);
  if (index == TabStripModel::kNoTab) {
    return nullptr;
  }
  if (!tab->GetGroup()) {
    if (!tab->IsPinned()) {
      index = tabs->SetTabPinned(index, true);
    }
    return tabs->GetTabAtIndex(index);
  }
  // A Space's tab keeps the Space's storage: the favorite opens anew outside.
  content::WebContents* old_contents = tab->GetContents();
  GURL url = old_contents->GetLastCommittedURL();
  if (!url.is_valid()) {
    url = old_contents->GetVisibleURL();
  }
  Profile* profile = browser_->GetProfile();
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          profile, tab_util::GetSiteInstanceForNewTab(profile, url)));
  contents->GetController().LoadURL(url, content::Referrer(),
                                    ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                    std::string());
  content::WebContents* opened = contents.get();
  tabs->AddWebContents(
      std::move(contents), tabs->IndexOfFirstNonPinnedTab(),
      ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      AddTabTypes::ADD_PINNED |
          (tab->IsActivated() ? AddTabTypes::ADD_ACTIVE : AddTabTypes::ADD_NONE),
      std::nullopt);
  {
    tabs::TabInterface* const leaving[] = {tab};
    tabs->AlerseKeepSpaces(leaving);
  }
  OnTabClosePressed(tab->GetHandle());
  return tabs::TabInterface::MaybeGetFromContents(opened);
}

void AlerseSidebarView::AddFavorite(tabs::TabInterface* tab,
                                      const bookmarks::BookmarkNode* node,
                                      size_t index) {
  const bookmarks::BookmarkNode* folder = GetFavoritesFolder(true);
  if (!folder) {
    return;
  }
  index = std::min(index, folder->children().size());
  if (node && node->is_url()) {
    if (!tab) {
      tab = GetBoundTab(node->id());
    }
    bound_.erase(node->id());
    model_->Move(node, folder, index);
  } else if (tab) {
    content::WebContents* contents = tab->GetContents();
    if (!contents->GetLastCommittedURL().is_valid()) {
      return;
    }
    node = model_->AddNewURL(folder, index, contents->GetTitle(),
                             contents->GetLastCommittedURL());
  }
  if (!node) {
    return;
  }
  if (tab) {
    if (tabs::TabInterface* pinned = PinAsFavoriteTab(tab)) {
      bound_[node->id()] = pinned->GetHandle();
    }
  }
  ScheduleRebuild();
}

void AlerseSidebarView::RemoveFavorite(const bookmarks::BookmarkNode* node) {
  UnpinBoundTab(node);
  bound_.erase(node->id());
  model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                 FROM_HERE);
}

void AlerseSidebarView::UnpinBoundTab(const bookmarks::BookmarkNode* node) {
  tabs::TabInterface* tab = GetBoundTab(node->id());
  if (!tab || !tab->IsPinned()) {
    return;
  }
  TabStripModel* tabs = browser_->GetTabStripModel();
  const int index = tabs->GetIndexOfTab(tab);
  if (index != TabStripModel::kNoTab) {
    tabs->SetTabPinned(index, false);
  }
}

void AlerseSidebarView::TurnIntoSpace(
    const bookmarks::BookmarkNode* folder) {
  if (!model_ || !model_->loaded() || browser_->GetProfile()->IsOffTheRecord()) {
    return;
  }
  // The new Space's profile has its own bookmarks: what the folder holds is
  // copied into the Space's folder there, and the folder goes from here.
  AlerseSpaceSwitcher::CreateSpace(
      browser_, folder->GetTitle(),
      base::BindOnce(
          [](base::WeakPtr<AlerseSidebarView> sidebar, int64_t id,
             Profile* profile) {
            alerse::WithBookmarksOf(
                profile,
                base::BindOnce(
                    [](base::WeakPtr<AlerseSidebarView> sidebar, int64_t id,
                       Profile* profile, bookmarks::BookmarkModel* to) {
                      const bookmarks::BookmarkNode* folder =
                          sidebar ? sidebar->GetNode(id) : nullptr;
                      if (!folder) {
                        return;
                      }
                      const bookmarks::BookmarkNode* space =
                          alerse::GetOrAddSpaceFolder(to, folder->GetTitle());
                      for (const auto& child : folder->children()) {
                        alerse::CopyNodeTo(child.get(), to, space);
                      }
                      sidebar->RemoveMovedNode(folder);
                    },
                    sidebar, id));
          },
          drop_weak_factory_.GetWeakPtr(), folder->id()));
}

std::vector<GURL> AlerseSidebarView::GetFolderLinks(
    const bookmarks::BookmarkNode* folder) const {
  std::vector<GURL> links;
  std::vector<const bookmarks::BookmarkNode*> pending = {folder};
  while (!pending.empty()) {
    const bookmarks::BookmarkNode* node = pending.back();
    pending.pop_back();
    // Depth first, in the sidebar's order.
    for (auto it = node->children().rbegin(); it != node->children().rend();
         ++it) {
      pending.push_back(it->get());
    }
    if (node->is_url() && node->url().is_valid()) {
      links.push_back(node->url());
    }
  }
  return links;
}

#if BUILDFLAG(IS_MAC)
void AlerseSidebarView::ShareFolder(int64_t id) {
  const bookmarks::BookmarkNode* folder = GetNode(id);
  if (!folder || !folder->is_folder() || !GetWidget()) {
    return;
  }
  // Beside the folder's row, as Arc's picker opens.
  gfx::Rect anchor(menu_point_, gfx::Size());
  for (const auto& [view, target] : row_targets_) {
    if (target.node == id && view->IsDrawn()) {
      anchor = view->GetBoundsInScreen();
      break;
    }
  }
  alerse::ShowSharePicker(GetFolderLinks(folder), GetWidget(), anchor);
}
#endif

void AlerseSidebarView::CopyAllLinks(const bookmarks::BookmarkNode* folder) {
  std::u16string links;
  for (const GURL& url : GetFolderLinks(folder)) {
    if (!links.empty()) {
      links += u"\n";
    }
    links += base::UTF8ToUTF16(url.spec());
  }
  if (!links.empty()) {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(links);
  }
}

void AlerseSidebarView::DuplicateFolder(
    const bookmarks::BookmarkNode* folder) {
  // A copy of titles, addresses and icons; not of what binds items to tabs.
  auto meta_of = [](const bookmarks::BookmarkNode* node) {
    bookmarks::BookmarkNode::MetaInfoMap meta;
    for (const char* key : {kIconMetaKey, kGlyphMetaKey, kFolderIconMetaKey,
                            kOpenMetaKey, kRenamedMetaKey}) {
      std::string value;
      if (node->GetMetaInfo(key, &value)) {
        meta[key] = value;
      }
    }
    return meta;
  };
  std::vector<std::pair<const bookmarks::BookmarkNode*,
                        const bookmarks::BookmarkNode*>>
      pending;
  const bookmarks::BookmarkNode* parent = folder->parent();
  const bookmarks::BookmarkNode::MetaInfoMap folder_meta = meta_of(folder);
  const bookmarks::BookmarkNode* copy = model_->AddFolder(
      parent, parent->GetIndexOf(folder).value() + 1, folder->GetTitle(),
      &folder_meta);
  pending.emplace_back(folder, copy);
  while (!pending.empty()) {
    auto [from, to] = pending.back();
    pending.pop_back();
    for (const auto& child : from->children()) {
      const bookmarks::BookmarkNode::MetaInfoMap meta = meta_of(child.get());
      if (child->is_folder()) {
        pending.emplace_back(
            child.get(), model_->AddFolder(to, to->children().size(),
                                           child->GetTitle(), &meta));
      } else {
        model_->AddNewURL(to, to->children().size(), child->GetTitle(),
                          child->url(), &meta);
      }
    }
  }
}

void AlerseSidebarView::ShowSpaceMenu(const gfx::Point& screen_point,
                                        ui::mojom::MenuSourceType source_type) {
  if (!GetWidget()) {
    return;
  }
  // The Space bar of this window has the Space's menu.
  std::vector<views::View*> views = {GetWidget()->GetRootView()};
  while (!views.empty()) {
    views::View* view = views.back();
    views.pop_back();
    if (auto* switcher = views::AsViewClass<AlerseSpaceSwitcher>(view)) {
      switcher->ShowActiveSpaceMenu(screen_point, source_type);
      return;
    }
    for (views::View* child : view->children()) {
      views.push_back(child);
    }
  }
}

std::vector<AlerseSidebarView::RowTarget>
AlerseSidebarView::GetSidebarItems() const {
  // The rows and favorites that are pages, top to bottom as drawn: the view
  // tree's order, favorites first. Folders, the Space's name and New Tab
  // are not items, as in Arc.
  std::vector<RowTarget> items;
  std::vector<const views::View*> pending = {this};
  while (!pending.empty()) {
    const views::View* view = pending.back();
    pending.pop_back();
    if (!view->GetVisible()) {
      continue;
    }
    if (auto it = row_targets_.find(const_cast<views::View*>(view));
        it != row_targets_.end()) {
      const bookmarks::BookmarkNode* node = GetNode(it->second.node);
      if ((node && node->is_url()) || (!node && it->second.tab.Get())) {
        items.push_back(it->second);
      }
      continue;
    }
    const auto& children = view->children();
    for (auto child = children.rbegin(); child != children.rend(); ++child) {
      pending.push_back(*child);
    }
  }
  return items;
}

views::View* AlerseSidebarView::FindActiveTabRow() const {
  tabs::TabInterface* active = browser_->GetTabStripModel()->GetActiveTab();
  if (!active) {
    return nullptr;
  }
  for (const auto& [view, target] : row_targets_) {
    if (target.tab.Get() == active && view->IsDrawn() &&
        !views::IsViewClass<FavoriteTile>(view)) {
      return view;
    }
  }
  return nullptr;
}

bool AlerseSidebarView::IsPinnedCollapsed(
    const bookmarks::BookmarkNode* folder) const {
  std::string value;
  return folder && folder->GetMetaInfo(kCollapsedMetaKey, &value) &&
         value == "1";
}

void AlerseSidebarView::ToggleCollapsedPinned() {
  if (const bookmarks::BookmarkNode* folder = GetSpaceFolder(true)) {
    model_->SetNodeMetaInfo(folder, kCollapsedMetaKey,
                            IsPinnedCollapsed(folder) ? "0" : "1");
  }
}

BEGIN_METADATA(AlerseSidebarView)
END_METADATA
