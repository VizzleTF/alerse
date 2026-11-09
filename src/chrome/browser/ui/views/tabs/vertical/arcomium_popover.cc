// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/arcomium_popover.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_glyphs.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_space_color.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_space_switcher.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_tooltip.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/tabs/vertical/arcomium_window_mac.h"
#endif
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/color/color_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace arcomium {

PopoverItem::PopoverItem() = default;
PopoverItem::PopoverItem(PopoverIcon icon,
                         std::u16string label,
                         std::optional<ui::Accelerator> shortcut,
                         bool enabled,
                         base::OnceClosure action)
    : icon(icon),
      label(std::move(label)),
      shortcut(std::move(shortcut)),
      enabled(enabled),
      action(std::move(action)) {}
PopoverItem::PopoverItem(PopoverItem&&) = default;
PopoverItem& PopoverItem::operator=(PopoverItem&&) = default;
PopoverItem::~PopoverItem() = default;

namespace {

// Arc for macOS' popovers, measured on its "..." and "+" (points).
constexpr int kPadding = 8;
constexpr int kRowHeight = 40;
// A separator: 8 pt, a 1 pt line, 8 pt.
constexpr int kSeparatorHeight = 17;
constexpr int kSeparatorInset = 12;
constexpr int kHighlightInset = 8;
constexpr float kHighlightRadius = 8;
constexpr float kRadius = 12;
// The arrow: 6 pt out of the panel, 13 pt wide at its base, its tip 10 pt
// off the button.
constexpr int kArrowHeight = 6;
constexpr float kArrowHalfWidth = 6.5f;
constexpr int kAnchorGap = 10;
constexpr int kWindowMargin = 6;
constexpr float kIconCenterX = 27.5f;
constexpr int kTextX = 50;
constexpr float kShortcutEndInset = 20.5f;
// Arc sets its keys 3.5 pt of ink apart ("⌘" then "R"), not 5.
constexpr float kKeyGap = 1.5f;
constexpr float kStroke = 1.6f;
constexpr SkColor kPanelColor = SkColorSetRGB(0x01, 0x01, 0x01);
constexpr SkColor kHighlightColor = SkColorSetRGB(0x23, 0x23, 0x23);
constexpr SkColor kTextColor = SkColorSetRGB(0xFF, 0xFF, 0xFF);
constexpr SkColor kDisabledColor = SkColorSetRGB(0x79, 0x78, 0x80);
constexpr SkColor kShortcutColor = SkColorSetRGB(0x78, 0x78, 0x78);
// A press on the anchor closes the popover before it reaches the anchor.
constexpr base::TimeDelta kReopenGuard = base::Milliseconds(400);

// Arc for macOS' icon picker (points), measured whole (it reaches past the
// window's left edge): a 273 pt panel, 8 columns of 31 pt cells from 12.5 pt
// in, an arrow 11.5 pt out to its dark rim (Arc's rim runs round it) with
// straight sides at 45 degrees, whose tip is 94 pt from the panel's left
// edge; a light, vibrant grey with a dark rim around a faint light one. Its
// panel starts 24 pt below the middle of the row (or the Space's name).
constexpr int kPickerWidth = 273;
constexpr int kPickerColumns = 8;
constexpr float kPickerCell = 31;
constexpr float kPickerGridX = 13;
constexpr int kPickerArrowHeight = 12;
constexpr float kPickerArrowTip = 11.5f;
constexpr float kPickerArrowHalfWidth = 11;
constexpr int kPickerArrowOffset = 94;
constexpr float kItemPickerDrop = 24;
// A folder's panel is a point shorter.
constexpr float kFolderPickerDrop = 24;
constexpr float kSpacePickerDrop = 24;
constexpr float kPickerRadius = 14;
// Arc's vibrant panel shows #3F3F3F to #424242 over its dark sidebar, with a
// sheen under its top edge that fades within 4 pt (#646464 at the rim, then
// #545454, #484848, #454545 every half point). On macOS the panel lets a
// blur of what is behind it through, as Arc's vibrancy does: Arc's shows
// half of that blur under half of #565656 (#2B over black, #36 over its
// sidebar, #42 over its page).
#if BUILDFLAG(IS_MAC)
constexpr SkColor kPickerPanel = SkColorSetARGB(0x80, 0x56, 0x56, 0x56);
#else
constexpr SkColor kPickerPanel = SkColorSetARGB(0xF7, 0x42, 0x42, 0x42);
#endif
constexpr SkColor kPickerSheen = SkColorSetARGB(0x30, 0xFF, 0xFF, 0xFF);
constexpr float kPickerSheenHeight = 4;
constexpr SkColor kPickerRimDark = SkColorSetARGB(0xB0, 0x00, 0x00, 0x00);
constexpr SkColor kPickerRimLight = SkColorSetARGB(0x30, 0xFF, 0xFF, 0xFF);
constexpr SkColor kPickerText = SkColorSetRGB(0xFF, 0xFF, 0xFF);
// The folder picker's "Icon" title is a shade off white, as Arc's.
constexpr SkColor kPickerTitle = SkColorSetRGB(0xEC, 0xEC, 0xEC);
constexpr SkColor kPickerDimText = SkColorSetRGB(0xE2, 0xE2, 0xE2);
constexpr SkColor kPickerSegment = SkColorSetRGB(0x3D, 0x81, 0xF9);
constexpr SkColor kPickerCurrent = SkColorSetARGB(0x3D, 0xFF, 0xFF, 0xFF);
constexpr SkColor kPickerCurrentHover = SkColorSetARGB(0x4D, 0xFF, 0xFF, 0xFF);
constexpr SkColor kPickerHover = SkColorSetARGB(0x1F, 0xFF, 0xFF, 0xFF);
constexpr SkColor kPickerTrack = SkColorSetARGB(0x1C, 0xFF, 0xFF, 0xFF);

gfx::FontList Font(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.DeriveWithSizeDelta(size - base.GetFontSize())
      .DeriveWithWeight(weight);
}

// The keys of `shortcut` as Arc writes them: the modifiers, then the key.
std::vector<std::u16string> KeysOf(const ui::Accelerator& shortcut) {
  std::vector<std::u16string> keys;
  if (shortcut.IsCtrlDown()) {
    keys.push_back(u"⌃");
  }
  if (shortcut.IsAltDown()) {
    keys.push_back(u"⌥");
  }
  if (shortcut.IsShiftDown()) {
    keys.push_back(u"⇧");
  }
  if (shortcut.IsCmdDown()) {
    keys.push_back(u"⌘");
  }
  const std::u16string modifiers = u"⌃⌥⇧⌘";
  std::u16string key = shortcut.GetShortcutText();
  while (!key.empty() && modifiers.find(key[0]) != std::u16string::npos) {
    key.erase(0, 1);
  }
  if (!key.empty()) {
    keys.push_back(key);
  }
  return keys;
}

// A rounded rect's outline from (x, y), `w` by `h`.
void AddRoundRect(SkPathBuilder& path, float x, float y, float w, float h,
                  float r) {
  path.addRRect(SkRRect::MakeRectXY(SkRect::MakeXYWH(x, y, w, h), r, r));
}

// Arc's SF Symbols, in outline, centred on `c`.
void PaintIcon(gfx::Canvas* canvas,
               PopoverIcon icon,
               const gfx::PointF& c,
               SkColor color,
               SkColor background) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kStroke);
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
  const float x = c.x();
  const float y = c.y();
  SkPathBuilder path;
  switch (icon) {
    case PopoverIcon::kNone:
      return;
    case PopoverIcon::kReload: {
      // arrow.clockwise: a 12 pt ring in a 2 pt stroke, open at the upper
      // right, and a filled head there, as Arc's (14 pt across in all).
      flags.setStrokeWidth(2);
      path.arcTo(SkRect::MakeLTRB(x - 6, y - 6, x + 6, y + 6), 30, 274, true);
      canvas->DrawPath(path.detach(), flags);
      SkPathBuilder head;
      head.moveTo(x + 2, y + 0.8f)
          .lineTo(x + 8, y + 0.8f)
          .lineTo(x + 8, y - 5.2f)
          .close();
      flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->DrawPath(head.detach(), flags);
      return;
    }
    case PopoverIcon::kSidebar:
      // sidebar.left: 21 by 15 with a pane a third in, as bold as Arc's.
      flags.setStrokeWidth(1.9f);
      AddRoundRect(path, x - 9.55f, y - 6.55f, 19.1f, 13.1f, 3);
      canvas->DrawPath(path.detach(), flags);
      canvas->DrawLine(gfx::PointF(x - 3.3f, y - 6.55f),
                       gfx::PointF(x - 3.3f, y + 6.55f), flags);
      return;
    case PopoverIcon::kNewSpace: {
      // plus.square.on.square, the back card tilted.
      {
        gfx::ScopedCanvas scoped(canvas);
        canvas->sk_canvas()->translate(x - 2.8f, y - 1.5f);
        canvas->sk_canvas()->rotate(-9);
        SkPathBuilder back;
        AddRoundRect(back, -5.9f, -7.5f, 11.8f, 15, 2.5f);
        canvas->DrawPath(back.detach(), flags);
      }
      cc::PaintFlags fill;
      fill.setAntiAlias(true);
      fill.setColor(background);
      const gfx::RectF front(x - 3.8f, y - 5.5f, 11.8f, 15);
      canvas->DrawRoundRect(front, 2.5f, fill);
      canvas->DrawRoundRect(front, 2.5f, flags);
      const gfx::PointF p = front.CenterPoint();
      canvas->DrawLine(gfx::PointF(p.x() - 3.6f, p.y()),
                       gfx::PointF(p.x() + 3.6f, p.y()), flags);
      canvas->DrawLine(gfx::PointF(p.x(), p.y() - 3.6f),
                       gfx::PointF(p.x(), p.y() + 3.6f), flags);
      return;
    }
    case PopoverIcon::kNewFolder: {
      // folder: a tab at the left of its top, a line under the lid.
      const float l = x - 9.2f;
      const float r = x + 9.2f;
      const float t = y - 7.2f;
      const float b = y + 7.2f;
      const float k = 2.5f;
      path.moveTo(l, b - k)
          .lineTo(l, t + k)
          .quadTo(l, t, l + k, t)
          .lineTo(l + 4.6f, t)
          .quadTo(l + 5.6f, t, l + 6.4f, t + 0.9f)
          .lineTo(l + 7, t + 1.6f)
          .lineTo(r - k, t + 1.6f)
          .quadTo(r, t + 1.6f, r, t + 1.6f + k)
          .lineTo(r, b - k)
          .quadTo(r, b, r - k, b)
          .lineTo(l + k, b)
          .quadTo(l, b, l, b - k)
          .close();
      canvas->DrawPath(path.detach(), flags);
      flags.setStrokeWidth(1);
      canvas->DrawLine(gfx::PointF(l + 2.6f, t + 5),
                       gfx::PointF(r - 2.6f, t + 5), flags);
      return;
    }
    case PopoverIcon::kNewSplit: {
      // rectangle.split.2x1 open at the right for a plus.
      const float l = x - 8.7f;
      const float r = x + 7.6f;
      const float t = y - 7.2f;
      const float b = y + 7.2f;
      const float k = 3;
      path.moveTo(r, t + k + 1.2f)
          .lineTo(r, t + k)
          .quadTo(r, t, r - k, t)
          .lineTo(l + k, t)
          .quadTo(l, t, l, t + k)
          .lineTo(l, b - k)
          .quadTo(l, b, l + k, b)
          .lineTo(r - k, b)
          .quadTo(r, b, r, b - k)
          .lineTo(r, b - k - 1.2f);
      canvas->DrawPath(path.detach(), flags);
      canvas->DrawLine(gfx::PointF(x - 0.4f, t), gfx::PointF(x - 0.4f, b),
                       flags);
      canvas->DrawLine(gfx::PointF(x + 2.1f, y), gfx::PointF(x + 9.9f, y),
                       flags);
      canvas->DrawLine(gfx::PointF(x + 6, y - 3.9f),
                       gfx::PointF(x + 6, y + 3.9f), flags);
      return;
    }
    case PopoverIcon::kNewTab:
      // plus.square: Arc's is drawn bolder (1.9 pt), with tighter corners
      // and a shorter cross.
      flags.setStrokeWidth(1.9f);
      AddRoundRect(path, x - 8.1f, y - 7.55f, 16.2f, 15.1f, 2.8f);
      canvas->DrawPath(path.detach(), flags);
      canvas->DrawLine(gfx::PointF(x - 3, y), gfx::PointF(x + 3, y), flags);
      canvas->DrawLine(gfx::PointF(x, y - 3), gfx::PointF(x, y + 3), flags);
      return;
  }
}

const views::View*& LastAnchor() {
  static const views::View* anchor = nullptr;
  return anchor;
}

base::TimeTicks& LastClosed() {
  static base::TimeTicks closed;
  return closed;
}

// The panel, its arrow and its rows.
class PopoverView : public views::View {
  METADATA_HEADER(PopoverView, views::View)

 public:
  PopoverView(std::vector<PopoverItem> items,
              bool above,
              base::RepeatingCallback<void(size_t)> activate)
      : items_(std::move(items)),
        above_(above),
        activate_(std::move(activate)),
        // Arc's labels are medium weight: "Toggle Sidebar" is 92.5 pt of ink,
        // a point narrower than semibold.
        label_font_(Font(13, gfx::Font::Weight::MEDIUM)),
        key_font_(Font(13, gfx::Font::Weight::NORMAL)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ARCOMIUM_MORE));
    // The row nearest the arrow that can be used.
    for (size_t i = 0; i < items_.size(); ++i) {
      const size_t index = above_ ? items_.size() - 1 - i : i;
      if (IsUsable(index)) {
        selected_ = index;
        break;
      }
    }
  }

  static int PanelHeight(const std::vector<PopoverItem>& items) {
    int height = 2 * kPadding;
    for (const PopoverItem& item : items) {
      height += item.label.empty() ? kSeparatorHeight : kRowHeight;
    }
    return height;
  }

  void set_arrow_x(float arrow_x) { arrow_x_ = arrow_x; }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    const gfx::RectF panel = PanelBounds();
    SkPathBuilder shape;
    AddRoundRect(shape, panel.x(), panel.y(), panel.width(), panel.height(),
                 kRadius);
    const float base = above_ ? panel.bottom() - 1 : panel.y() + 1;
    const float tip = above_ ? panel.bottom() + kArrowHeight
                             : panel.y() - kArrowHeight;
    shape.moveTo(arrow_x_ - kArrowHalfWidth - 1, base)
        .lineTo(arrow_x_ - 0.8f, tip + (above_ ? -0.6f : 0.6f))
        .quadTo(arrow_x_, tip, arrow_x_ + 0.8f, tip + (above_ ? -0.6f : 0.6f))
        .lineTo(arrow_x_ + kArrowHalfWidth + 1, base)
        .close();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const SkColor panel_color = SidebarInk(this, kPanelColor);
    flags.setColor(panel_color);
    canvas->DrawPath(shape.detach(), flags);

    for (size_t i = 0; i < items_.size(); ++i) {
      const PopoverItem& item = items_[i];
      const gfx::RectF row = RowBounds(i);
      if (item.label.empty()) {
        canvas->FillRect(
            gfx::ToEnclosingRect(gfx::RectF(
                kSeparatorInset, row.y() + (kSeparatorHeight - 1) / 2,
                width() - 2 * kSeparatorInset, 1)),
            SidebarInk(this, kHighlightColor));
        continue;
      }
      const bool selected = selected_ == i;
      if (selected) {
        flags.setColor(SidebarInk(this, kHighlightColor));
        canvas->DrawRoundRect(
            gfx::RectF(kHighlightInset, row.y(), width() - 2 * kHighlightInset,
                       row.height()),
            kHighlightRadius, flags);
      }
      const SkColor ink =
          SidebarInk(this, item.enabled ? kTextColor : kDisabledColor);
      PaintIcon(canvas, item.icon,
                gfx::PointF(kIconCenterX, row.CenterPoint().y()), ink,
                selected ? SidebarInk(this, kHighlightColor) : panel_color);
      const gfx::Rect text_row = gfx::ToEnclosingRect(row);
      canvas->DrawStringRect(
          item.label, label_font_, ink,
          gfx::Rect(kTextX, text_row.y(), width() - kTextX, text_row.height()));
      if (!item.shortcut) {
        continue;
      }
      const std::vector<std::u16string> keys = KeysOf(*item.shortcut);
      float x = width() - kShortcutEndInset;
      for (auto key = keys.rbegin(); key != keys.rend(); ++key) {
        const float key_width = gfx::GetStringWidthF(*key, key_font_);
        x -= key_width;
        canvas->DrawStringRect(
            *key, key_font_,
            SidebarInk(this, item.enabled ? kShortcutColor : kDisabledColor),
            gfx::Rect(std::round(x), text_row.y(), std::ceil(key_width) + 1,
                      text_row.height()));
        x -= kKeyGap;
      }
    }
  }

  // The highlight follows the pointer; as Arc's, it goes while the pointer
  // is on a dimmed row or a separator, or out of the popover. Up and Down
  // bring it back.
  void OnMouseMoved(const ui::MouseEvent& event) override {
    const std::optional<size_t> row = RowAt(event.location());
    if (row && IsUsable(*row)) {
      Select(row);
    } else {
      Deselect();
    }
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    OnMouseMoved(event);
  }

  void OnMouseExited(const ui::MouseEvent& event) override { Deselect(); }

  // macOS does not always send the popover the pointer's moves (a "+"
  // popover opened from the Space bar got none): the highlight also follows
  // the pointer's polled position whenever it moves.
  void AddedToWidget() override {
    pointer_poll_.Start(FROM_HERE, base::Milliseconds(50),
                        base::BindRepeating(&PopoverView::PollPointer,
                                            base::Unretained(this)));
  }

  void RemovedFromWidget() override { pointer_poll_.Stop(); }

  bool OnMousePressed(const ui::MouseEvent& event) override { return true; }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    const std::optional<size_t> row = RowAt(event.location());
    if (row && IsUsable(*row)) {
      activate_.Run(*row);
    }
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    switch (event.key_code()) {
      case ui::VKEY_ESCAPE:
        activate_.Run(SIZE_MAX);
        return true;
      case ui::VKEY_UP:
      case ui::VKEY_DOWN:
        Step(event.key_code() == ui::VKEY_DOWN ? 1 : -1);
        return true;
      case ui::VKEY_RETURN:
      case ui::VKEY_SPACE:
        if (selected_ && IsUsable(*selected_)) {
          activate_.Run(*selected_);
        }
        return true;
      default:
        return false;
    }
  }

 private:
  gfx::RectF PanelBounds() const {
    return gfx::RectF(0, above_ ? 0 : kArrowHeight, width(),
                      height() - kArrowHeight);
  }

  gfx::RectF RowBounds(size_t index) const {
    float y = PanelBounds().y() + kPadding;
    for (size_t i = 0; i < index; ++i) {
      y += items_[i].label.empty() ? kSeparatorHeight : kRowHeight;
    }
    return gfx::RectF(
        0, y, width(),
        items_[index].label.empty() ? kSeparatorHeight : kRowHeight);
  }

  std::optional<size_t> RowAt(const gfx::Point& point) const {
    for (size_t i = 0; i < items_.size(); ++i) {
      if (RowBounds(i).Contains(gfx::PointF(point))) {
        return i;
      }
    }
    return std::nullopt;
  }

  bool IsUsable(size_t index) const {
    return index < items_.size() && !items_[index].label.empty() &&
           items_[index].enabled;
  }

  void Select(std::optional<size_t> row) {
    if (row && IsUsable(*row) && selected_ != row) {
      selected_ = row;
      SchedulePaint();
    }
  }

  void PollPointer() {
    const gfx::Point screen = display::Screen::Get()->GetCursorScreenPoint();
    if (last_pointer_ == screen) {
      return;
    }
    const bool first = !last_pointer_;
    last_pointer_ = screen;
    if (first) {
      // Where the pointer was when the popover opened (its button).
      return;
    }
    gfx::Point point = screen;
    views::View::ConvertPointFromScreen(this, &point);
    if (!GetLocalBounds().Contains(point)) {
      Deselect();
      return;
    }
    const std::optional<size_t> row = RowAt(point);
    if (row && IsUsable(*row)) {
      Select(row);
    } else {
      Deselect();
    }
  }

  void Deselect() {
    if (selected_) {
      selected_.reset();
      SchedulePaint();
    }
  }

  void Step(int delta) {
    const int count = static_cast<int>(items_.size());
    int index = selected_ ? static_cast<int>(*selected_)
                          : (delta > 0 ? -1 : count);
    for (int i = 0; i < count; ++i) {
      index += delta;
      if (index < 0 || index >= count) {
        return;
      }
      if (IsUsable(index)) {
        Select(static_cast<size_t>(index));
        return;
      }
    }
  }

  const std::vector<PopoverItem> items_;
  const bool above_;
  base::RepeatingCallback<void(size_t)> activate_;
  const gfx::FontList label_font_;
  const gfx::FontList key_font_;
  float arrow_x_ = 0;
  std::optional<size_t> selected_;
  std::optional<gfx::Point> last_pointer_;
  base::RepeatingTimer pointer_poll_;
};

BEGIN_METADATA(PopoverView)
END_METADATA

// Arc for macOS' icon picker: a light, vibrant panel under the row with an
// arrow at the row's icon. For a folder an "Icon" title over 24 glyphs; for
// a pinned item Emoji and Icon pages, opening on Emoji; for a Space its
// pages and a trash button. The Emoji page is Arc's: a search field over a
// scrolling grid of the whole set and a bar of its eight categories. The
// current icon sits on a plate, the pointer's on a fainter one.
class IconPickerView : public views::View, public views::TextfieldController {
  METADATA_HEADER(IconPickerView, views::View)

 public:
  // `choices`: the glyphs, then the emoji (not a folder's), then "" for
  // none; `pick` gets an index into them, SIZE_MAX for nothing.
  IconPickerView(IconPickerKind kind,
                 std::vector<std::string> choices,
                 size_t glyph_count,
                 const std::string& current,
                 bool above,
                 float panel_top,
                 base::RepeatingCallback<void(size_t)> pick)
      : kind_(kind),
        choices_(std::move(choices)),
        glyph_count_(glyph_count),
        above_(above),
        panel_top_(panel_top),
        pick_(std::move(pick)),
        title_font_(Font(13, gfx::Font::Weight::SEMIBOLD)),
        segment_font_(Font(13, gfx::Font::Weight::NORMAL)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        kind == IconPickerKind::kSpace ? IDS_ARCOMIUM_CHANGE_SPACE_ICON
                                       : IDS_ARCOMIUM_CHANGE_ICON));
    // Arc's pinned item picker opens on Emoji unless a glyph is picked.
    // An emoji is stored with its prefix, a glyph maybe under an old name.
    const std::string wanted =
        IsEmojiIcon(current)
            ? current.substr(std::size(kEmojiIconPrefix) - 1)
            : CanonicalGlyphName(current);
    for (size_t i = 0; i + 1 < choices_.size(); ++i) {
      if (choices_[i] == wanted && (i >= glyph_count_) == IsEmojiIcon(current)) {
        current_ = i;
      }
    }
    emoji_page_ = current_ ? *current_ >= glyph_count_
                           : kind == IconPickerKind::kItem;
    if (HasPages(kind_)) {
      auto search = std::make_unique<views::Textfield>();
      search->SetBorder(views::NullBorder());
      search->SetBackgroundEnabled(false);
      search->SetFontList(segment_font_);
      search->SetTextColorId(ui::kColorRefNeutral100);
      search->SetPlaceholderTextColorId(ui::kColorRefNeutral60);
      search->SetPlaceholderText(
          l10n_util::GetStringUTF16(IDS_ARCOMIUM_ICON_PICKER_SEARCH));
      search->GetViewAccessibility().SetName(
          l10n_util::GetStringUTF16(IDS_ARCOMIUM_ICON_PICKER_SEARCH));
      search->set_controller(this);
      // The field's ring is painted here, Arc's.
      views::FocusRing::Remove(search.get());
      search_ = AddChildView(std::move(search));
      search_->SetBoundsRect(gfx::ToEnclosingRect(gfx::RectF(
          kSearchTextX, PanelTop() + kSearchTop, kSearchRight - kSearchTextX,
          kSearchHeight)));
    }
    if (search_) {
      search_->SetVisible(emoji_page_);
    }
    BuildPage();
    if (current_ && emoji_page_) {
      ScrollToShow(*current_ - glyph_count_);
    }
  }

  static int PanelHeight(IconPickerKind kind) {
    return HasPages(kind) ? 273 : 145;
  }

  void set_arrow_x(float arrow_x) { arrow_x_ = arrow_x; }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const float panel_top = PanelTop();
    const float panel_bottom = panel_top + PanelHeight(kind_);
    const SkPath outline = Outline();
    {
      flags.setColor(kPickerPanel);
      canvas->DrawPath(outline, flags);
      // The sheen along the panel's top edge (its bottom edge above).
      const float edge = above_ ? panel_bottom : panel_top;
      const float dir = above_ ? -1 : 1;
      const SkPoint sheen_points[] = {
          SkPoint::Make(0, edge),
          SkPoint::Make(0, edge + dir * kPickerSheenHeight)};
      const SkColor4f sheen_colors[] = {
          SkColor4f::FromColor(kPickerSheen),
          SkColor4f::FromColor(SkColorSetA(kPickerSheen, 0))};
      flags.setShader(cc::PaintShader::MakeLinearGradient(
          sheen_points, sheen_colors, nullptr, 2, SkTileMode::kClamp));
      canvas->DrawPath(Shape(1), flags);
      flags.setShader(nullptr);
    }
    // Arc's rim: a dark line outside a faint light one, round the arrow too:
    // strokes along the outline, clipped to its inside.
    {
      gfx::ScopedCanvas clip(canvas);
      canvas->ClipPath(outline, true);
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
      // A half-point dark line, then a point of light, as Arc's pixels show.
      flags.setStrokeWidth(3);
      flags.setColor(kPickerRimLight);
      canvas->DrawPath(outline, flags);
      flags.setStrokeWidth(1);
      flags.setColor(kPickerRimDark);
      canvas->DrawPath(outline, flags);
      flags.setStyle(cc::PaintFlags::kFill_Style);
    }

    const float top = PanelTop();
    if (!HasPages(kind_)) {
      const std::u16string title =
          l10n_util::GetStringUTF16(IDS_ARCOMIUM_ICON_PICKER_TITLE);
      const int title_width =
          std::ceil(gfx::GetStringWidthF(title, title_font_));
      canvas->DrawStringRect(
          title, title_font_, kPickerTitle,
          gfx::Rect(std::round(kTitleCenterX - title_width / 2.0f),
                    std::round(top + kTitleCenterY - 9), title_width + 1, 18));
    } else {
      // Arc's segmented control: a faint track under both segments.
      const gfx::RectF emoji_segment = SegmentBounds(true);
      flags.setColor(kPickerTrack);
      canvas->DrawRoundRect(
          gfx::RectF(emoji_segment.x() + 1, emoji_segment.y(),
                     2 * emoji_segment.width() - 1, emoji_segment.height()),
          kSegmentRadius, flags);
      for (const bool emoji : {true, false}) {
        const gfx::RectF segment = SegmentBounds(emoji);
        const bool selected = emoji == emoji_page_;
        if (selected) {
          flags.setColor(kPickerSegment);
          canvas->DrawRoundRect(segment, kSegmentRadius, flags);
        }
        canvas->DrawStringRectWithFlags(
            l10n_util::GetStringUTF16(emoji ? IDS_ARCOMIUM_ICON_PICKER_EMOJI
                                            : IDS_ARCOMIUM_ICON_PICKER_TITLE),
            segment_font_, selected ? kPickerText : kPickerDimText,
            gfx::ToEnclosingRect(segment), gfx::Canvas::TEXT_ALIGN_CENTER);
      }
      if (kind_ == IconPickerKind::kSpace) {
        const gfx::RectF trash = TrashBounds();
        if (hovered_ == kTrash) {
          flags.setColor(kPickerHover);
          canvas->DrawRoundRect(trash, kSegmentRadius, flags);
        }
        PaintTrash(canvas, trash.CenterPoint());
      }
    }
    if (emoji_page_) {
      PaintSearchField(canvas);
      PaintCategoryBar(canvas);
    }

    gfx::ScopedCanvas clip(canvas);
    if (emoji_page_) {
      canvas->ClipRect(GridViewport());
    }
    for (size_t i = 0; i < page_.size(); ++i) {
      const gfx::RectF cell = CellBounds(i);
      if (emoji_page_ && !cell.Intersects(GridViewport())) {
        continue;
      }
      const size_t index = page_[i];
      const bool current = current_ == index;
      const bool hovered = hovered_ == static_cast<int>(i) ||
                           (keyboard_ && selected_ == i);
      if (current || hovered) {
        flags.setColor(current ? (hovered ? kPickerCurrentHover
                                          : kPickerCurrent)
                               : kPickerHover);
        gfx::RectF plate = cell;
        plate.Inset(0.5f);
        canvas->DrawRoundRect(plate, kCellRadius, flags);
      }
      const gfx::PointF c = cell.CenterPoint();
      if (index >= glyph_count_) {
        // Centred in the cell as Arc's: the emoji font's box sits a little
        // left of and above its picture.
        PaintEmojiIcon(canvas, c + gfx::Vector2dF(0.75f, 1.25f),
                       std::string(kEmojiIconPrefix) + choices_[index],
                       255, kEmojiFontSize);
        continue;
      }
      gfx::ScopedCanvas scoped(canvas);
      canvas->sk_canvas()->translate(c.x(), c.y());
      canvas->sk_canvas()->scale(kGlyphScale, kGlyphScale);
      PaintSpaceIcon(canvas, gfx::PointF(), choices_[index], kPickerText);
    }
  }

  void OnMouseMoved(const ui::MouseEvent& event) override {
    keyboard_ = false;
    SetHovered(HitAt(event.location()));
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    OnMouseMoved(event);
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    SetHovered(kNothing);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override { return true; }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    const int hit = HitAt(event.location());
    if (hit == kEmojiSegment || hit == kIconSegment) {
      ShowPage(hit == kEmojiSegment);
      return;
    }
    if (hit == kTrash) {
      pick_.Run(choices_.size() - 1);
      return;
    }
    if (hit <= kCategoryFirst) {
      ShowCategory(kCategoryFirst - hit);
      return;
    }
    if (hit >= 0) {
      PickCell(static_cast<size_t>(hit));
    }
  }

  bool OnMouseWheel(const ui::MouseWheelEvent& event) override {
    if (!emoji_page_) {
      return false;
    }
    SetScroll(scroll_ - event.y_offset());
    SetHovered(HitAt(event.location()));
    return true;
  }

  void OnFocus() override {
    // The Emoji page types into its search field, as Arc's.
    if (emoji_page_ && search_) {
      search_->RequestFocus();
    }
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    return HandleKey(event, /*from_search=*/false);
  }

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    BuildPage();
    SetScroll(0);
  }

  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& event) override {
    return event.type() == ui::EventType::kKeyPressed &&
           HandleKey(event, /*from_search=*/true);
  }

 private:
  static constexpr int kNothing = -1;
  static constexpr int kEmojiSegment = -2;
  static constexpr int kIconSegment = -3;
  static constexpr int kTrash = -4;
  // -10 to -17: the Emoji page's category bar.
  static constexpr int kCategoryFirst = -10;
  // Measured on Arc for macOS' pickers (points from the panel's corner).
  static constexpr float kTitleCenterX = 136.5f;
  static constexpr float kTitleCenterY = 22.5f;
  static constexpr float kSegmentRadius = 6;
  static constexpr float kCellRadius = 6;
  // Arc's glyphs are about 11 pt across.
  static constexpr float kGlyphScale = 0.88f;
  // Arc's emoji are 18.5 pt across, in rows 33 pt apart.
  static constexpr int kEmojiFontSize = 21;
  static constexpr float kEmojiRow = 33;
  // The Emoji page: a 23 pt search field from 12 to 260.5 pt across, 49 pt
  // down, in a 3.25 pt blue ring; the grid's first row centred 91.5 pt down;
  // a line at 228.5 pt over the categories, centred 247.5 pt down.
  static constexpr float kSearchLeft = 12;
  static constexpr float kSearchRight = 260.5f;
  static constexpr float kSearchTop = 49;
  static constexpr float kSearchHeight = 23;
  static constexpr float kSearchTextX = 37.5f;
  static constexpr float kEmojiGridTop = 75;
  static constexpr float kEmojiGridBottom = 228.5f;
  static constexpr float kCategoryCenterY = 247.5f;
  static constexpr SkColor kSearchFill = SkColorSetRGB(0x1C, 0x1C, 0x1C);
  static constexpr SkColor kSearchRing = SkColorSetRGB(0x4A, 0x76, 0x9E);
  static constexpr SkColor kSearchGlass = SkColorSetRGB(0x98, 0x98, 0x98);
  static constexpr SkColor kCategoryInk = SkColorSetRGB(0x92, 0x92, 0x92);
  static constexpr SkColor kCategoryLine = SkColorSetARGB(0x60, 0xFF, 0xFF,
                                                          0xFF);
  static constexpr SkColor kScrollTrack = SkColorSetARGB(0x0A, 0xFF, 0xFF,
                                                         0xFF);

  // Pinned items and Spaces have Emoji and Icon pages, folders glyphs only.
  static bool HasPages(IconPickerKind kind) {
    return kind != IconPickerKind::kFolder;
  }

  static float GridTop(IconPickerKind kind) {
    return HasPages(kind) ? 47.75f : 42;
  }

  // Below the anchor, the panel may start between pixels, as Arc's does.
  float PanelTop() const { return above_ ? 0 : panel_top_; }

  // The panel and its arrow as one outline.
  SkPath Outline() const {
    SkPath outline;
    if (!Op(Shape(0),
            SkPath::RRect(SkRect::MakeXYWH(0, PanelTop(), width(),
                                           PanelHeight(kind_)),
                          kPickerRadius, kPickerRadius),
            kUnion_SkPathOp, &outline)) {
      return Shape(0);
    }
    return outline;
  }

  // The panel and its arrow, `inset` in from the outline.
  SkPath Shape(float inset) const {
    const float top = PanelTop();
    const float panel_height = PanelHeight(kind_);
    SkPathBuilder shape;
    shape.addRRect(SkRRect::MakeRectXY(
        SkRect::MakeXYWH(inset, top + inset, width() - 2 * inset,
                         panel_height - 2 * inset),
        kPickerRadius - inset, kPickerRadius - inset));
    if (inset == 0) {
      // Arc's arrow: straight sides at 45 degrees from a rounded tip, flaring
      // into the panel's edge (27.5 pt across where it meets it).
      const float dir = above_ ? 1 : -1;
      const float edge = above_ ? top + panel_height : top;
      auto at = [&](float dx, float out) {
        return SkPoint::Make(arrow_x_ + dx, edge + dir * out);
      };
      constexpr float kTip = kPickerArrowTip;
      // Arc's tip is blunt: 5.5 pt across a point from its end.
      constexpr float kSide = kPickerArrowHalfWidth;
      constexpr float kNearTip = kTip - 1.6f;
      constexpr float kTipHalf = kSide - (kNearTip - 1.5f);
      shape.moveTo(at(-kSide - 3.5f, -1))
          .quadTo(at(-kSide - 1.5f, 0), at(-kSide, 1.5f))
          .lineTo(at(-kTipHalf, kNearTip))
          .quadTo(at(0, kTip + 0.9f), at(kTipHalf, kNearTip))
          .lineTo(at(kSide, 1.5f))
          .quadTo(at(kSide + 1.5f, 0), at(kSide + 3.5f, -1))
          .close();
    }
    return shape.detach();
  }

  // Arc centres a pinned item's segments; a Space's leave room for its trash.
  gfx::RectF SegmentBounds(bool emoji) const {
    const float x = kind_ == IconPickerKind::kSpace ? 64.5f : 80;
    return gfx::RectF(emoji ? x : x + 57, PanelTop() + 14, 57, 24);
  }

  gfx::RectF TrashBounds() const {
    return gfx::RectF(184.5f, PanelTop() + 14, 26, 24);
  }

  void PaintTrash(gfx::Canvas* canvas, const gfx::PointF& c) const {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kPickerDimText);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.3f);
    flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
    flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
    canvas->DrawLine(c + gfx::Vector2dF(-5.6f, -4.6f),
                     c + gfx::Vector2dF(5.6f, -4.6f), flags);
    SkPathBuilder handle;
    handle.moveTo(c.x() - 2, c.y() - 4.6f)
        .lineTo(c.x() - 2, c.y() - 6.4f)
        .lineTo(c.x() + 2, c.y() - 6.4f)
        .lineTo(c.x() + 2, c.y() - 4.6f);
    canvas->DrawPath(handle.detach(), flags);
    SkPathBuilder can;
    can.moveTo(c.x() - 4.4f, c.y() - 4.6f)
        .lineTo(c.x() - 3.7f, c.y() + 5.4f)
        .quadTo(c.x() - 3.6f, c.y() + 6.6f, c.x() - 2.4f, c.y() + 6.6f)
        .lineTo(c.x() + 2.4f, c.y() + 6.6f)
        .quadTo(c.x() + 3.6f, c.y() + 6.6f, c.x() + 3.7f, c.y() + 5.4f)
        .lineTo(c.x() + 4.4f, c.y() - 4.6f);
    canvas->DrawPath(can.detach(), flags);
    for (const float x : {-1.4f, 1.4f}) {
      canvas->DrawLine(c + gfx::Vector2dF(x, -2.2f),
                       c + gfx::Vector2dF(x, 4.2f), flags);
    }
  }

  // Arc's search field: a dark capsule in a blue ring, a magnifier at its
  // start; the text field itself draws only the text.
  void PaintSearchField(gfx::Canvas* canvas) const {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const gfx::RectF field(kSearchLeft, PanelTop() + kSearchTop,
                           kSearchRight - kSearchLeft, kSearchHeight);
    gfx::RectF ring = field;
    ring.Outset(3.25f);
    flags.setColor(kSearchRing);
    canvas->DrawRoundRect(ring, ring.height() / 2, flags);
    flags.setColor(kSearchFill);
    canvas->DrawRoundRect(field, field.height() / 2, flags);
    const gfx::PointF glass(25, field.CenterPoint().y() - 0.6f);
    flags.setColor(kSearchGlass);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.4f);
    flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
    canvas->DrawCircle(glass, 4.4f, flags);
    canvas->DrawLine(glass + gfx::Vector2dF(3.2f, 3.2f),
                     glass + gfx::Vector2dF(6.2f, 6.2f), flags);
  }

  // The line over Arc's category bar and its eight outlined icons: smileys,
  // nature, food, activities, travel, objects, symbols, flags.
  void PaintCategoryBar(gfx::Canvas* canvas) const {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kCategoryLine);
    canvas->DrawRect(gfx::RectF(kPickerGridX - 1, PanelTop() + kEmojiGridBottom,
                                kPickerColumns * kPickerCell + 0.5f, 0.5f),
                     flags);
    // A faint scroll track beside the grid, as macOS shows it.
    flags.setColor(kScrollTrack);
    canvas->DrawRect(
        gfx::RectF(kPickerGridX + kPickerColumns * kPickerCell - 2.5f,
                   PanelTop() + kEmojiGridTop + 2, 6,
                   kEmojiGridBottom - kEmojiGridTop - 4),
        flags);
    for (int category = 0; category < kPickerEmojiCategories; ++category) {
      PaintCategory(canvas, category, CategoryBounds(category).CenterPoint());
    }
  }

  static void PaintCategory(gfx::Canvas* canvas,
                            int category,
                            const gfx::PointF& c) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kCategoryInk);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.2f);
    flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
    flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
    const float x = c.x();
    const float y = c.y();
    switch (category) {
      case 0: {  // A smiling face.
        canvas->DrawCircle(c, 6, flags);
        SkPathBuilder smile;
        smile.moveTo(x - 3, y + 1.4f).quadTo(x, y + 4.4f, x + 3, y + 1.4f);
        canvas->DrawPath(smile.detach(), flags);
        flags.setStyle(cc::PaintFlags::kFill_Style);
        canvas->DrawCircle(gfx::PointF(x - 2.1f, y - 1.8f), 0.9f, flags);
        canvas->DrawCircle(gfx::PointF(x + 2.1f, y - 1.8f), 0.9f, flags);
        break;
      }
      case 1:  // A sun.
        canvas->DrawCircle(c, 3, flags);
        for (int i = 0; i < 8; ++i) {
          const float angle = i * std::numbers::pi_v<float> / 4;
          const gfx::Vector2dF ray(std::cos(angle), std::sin(angle));
          canvas->DrawLine(c + gfx::ScaleVector2d(ray, 5),
                           c + gfx::ScaleVector2d(ray, 6.6f), flags);
        }
        break;
      case 2: {  // A cup with steam.
        SkPathBuilder cup;
        cup.moveTo(x - 6, y - 1.6f)
            .lineTo(x + 3.4f, y - 1.6f)
            .lineTo(x + 3.4f, y + 3.4f)
            .quadTo(x + 3.4f, y + 6, x + 0.8f, y + 6)
            .lineTo(x - 3.4f, y + 6)
            .quadTo(x - 6, y + 6, x - 6, y + 3.4f)
            .close();
        canvas->DrawPath(cup.detach(), flags);
        canvas->DrawRoundRect(gfx::RectF(x + 3.4f, y - 0.2f, 3, 3.4f), 1.5f,
                              flags);
        for (const float sx : {-3.6f, -1.3f, 1.0f}) {
          canvas->DrawLine(gfx::PointF(x + sx, y - 6),
                           gfx::PointF(x + sx, y - 3.8f), flags);
        }
        break;
      }
      case 3: {  // An award ribbon.
        canvas->DrawCircle(gfx::PointF(x, y - 2.2f), 4.2f, flags);
        SkPathBuilder ribbon;
        ribbon.moveTo(x - 2.4f, y + 1.4f)
            .lineTo(x - 3.4f, y + 6.6f)
            .lineTo(x, y + 5)
            .lineTo(x + 3.4f, y + 6.6f)
            .lineTo(x + 2.4f, y + 1.4f);
        canvas->DrawPath(ribbon.detach(), flags);
        break;
      }
      case 4: {  // A bus.
        canvas->DrawRoundRect(gfx::RectF(x - 6.4f, y - 4.6f, 10.4f, 8.4f),
                              1.6f, flags);
        SkPathBuilder front;
        front.moveTo(x + 4, y - 1.6f)
            .lineTo(x + 6.6f, y - 1.6f)
            .lineTo(x + 6.6f, y + 3.8f)
            .lineTo(x + 4, y + 3.8f);
        canvas->DrawPath(front.detach(), flags);
        canvas->DrawCircle(gfx::PointF(x - 3.6f, y + 4.6f), 1.4f, flags);
        canvas->DrawCircle(gfx::PointF(x + 3.6f, y + 4.6f), 1.4f, flags);
        break;
      }
      case 5:  // A gift.
        canvas->DrawRoundRect(gfx::RectF(x - 6, y - 2.4f, 12, 3.2f), 0.8f,
                              flags);
        canvas->DrawRoundRect(gfx::RectF(x - 5, y + 0.8f, 10, 5.6f), 0.8f,
                              flags);
        canvas->DrawLine(gfx::PointF(x, y - 2.4f), gfx::PointF(x, y + 6.4f),
                         flags);
        canvas->DrawCircle(gfx::PointF(x - 1.9f, y - 4.3f), 1.9f, flags);
        canvas->DrawCircle(gfx::PointF(x + 1.9f, y - 4.3f), 1.9f, flags);
        break;
      case 6: {  // A heart.
        SkPathBuilder heart;
        heart.moveTo(x, y + 5.6f)
            .cubicTo(x - 4.4f, y + 2.6f, x - 6.4f, y + 0.2f, x - 6.4f, y - 2.2f)
            .cubicTo(x - 6.4f, y - 4.4f, x - 4.8f, y - 5.8f, x - 3, y - 5.8f)
            .cubicTo(x - 1.6f, y - 5.8f, x - 0.6f, y - 5, x, y - 3.8f)
            .cubicTo(x + 0.6f, y - 5, x + 1.6f, y - 5.8f, x + 3, y - 5.8f)
            .cubicTo(x + 4.8f, y - 5.8f, x + 6.4f, y - 4.4f, x + 6.4f, y - 2.2f)
            .cubicTo(x + 6.4f, y + 0.2f, x + 4.4f, y + 2.6f, x, y + 5.6f);
        canvas->DrawPath(heart.detach(), flags);
        break;
      }
      default: {  // A flag.
        canvas->DrawLine(gfx::PointF(x - 4.6f, y - 6.2f),
                         gfx::PointF(x - 4.6f, y + 6.4f), flags);
        SkPathBuilder cloth;
        cloth.moveTo(x - 4.6f, y - 5.6f)
            .cubicTo(x - 1.6f, y - 7, x + 1.4f, y - 4, x + 5.4f, y - 5.4f)
            .lineTo(x + 5.4f, y + 1)
            .cubicTo(x + 1.4f, y + 2.4f, x - 1.6f, y - 0.6f, x - 4.6f, y + 0.8f);
        canvas->DrawPath(cloth.detach(), flags);
        break;
      }
    }
  }

  gfx::RectF CategoryBounds(int category) const {
    return gfx::RectF(kPickerGridX + category * kPickerCell,
                      PanelTop() + kCategoryCenterY - kPickerCell / 2,
                      kPickerCell, kPickerCell);
  }

  gfx::RectF GridViewport() const {
    return gfx::RectF(0, PanelTop() + kEmojiGridTop + 1, width(),
                      kEmojiGridBottom - kEmojiGridTop - 1);
  }

  float MaxScroll() const {
    const size_t rows = (page_.size() + kPickerColumns - 1) / kPickerColumns;
    return std::max(0.0f, rows * kEmojiRow + 2 -
                              (kEmojiGridBottom - kEmojiGridTop));
  }

  void SetScroll(float scroll) {
    scroll = std::clamp(scroll, 0.0f, MaxScroll());
    if (scroll != scroll_) {
      scroll_ = scroll;
      SchedulePaint();
    }
  }

  // Scrolls the Emoji page so its `i`th cell shows.
  void ScrollToShow(size_t i) {
    const float row = (i / kPickerColumns) * kEmojiRow;
    const float viewport = kEmojiGridBottom - kEmojiGridTop;
    if (row < scroll_) {
      SetScroll(row);
    } else if (row + kEmojiRow > scroll_ + viewport) {
      SetScroll(row + kEmojiRow - viewport);
    }
  }

  // The category bar scrolls to the category's first row; any search ends.
  void ShowCategory(int category) {
    if (search_ && !search_->GetText().empty()) {
      search_->SetText(std::u16string());
      BuildPage();
    }
    const size_t first = GetPickerEmojiCategoryStart(category);
    // The grid flows on through the categories: show the row holding it.
    SetScroll((first / kPickerColumns) * kEmojiRow);
    keyboard_ = false;
  }

  // The indices into `choices_` that the shown page offers: every glyph, or
  // the emoji whose names hold every word of the search.
  void BuildPage() {
    page_.clear();
    if (!emoji_page_) {
      for (size_t i = 0; i < glyph_count_; ++i) {
        page_.push_back(i);
      }
    } else {
      const std::vector<std::string> words = base::SplitString(
          base::UTF16ToUTF8(base::ToLowerASCII(
              search_ ? search_->GetText() : std::u16string_view())),
          " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      for (size_t i = glyph_count_; i + 1 < choices_.size(); ++i) {
        const std::string_view name = GetPickerEmojiName(i - glyph_count_);
        if (std::ranges::all_of(words, [&name](const std::string& word) {
              return name.find(word) != std::string_view::npos;
            })) {
          page_.push_back(i);
        }
      }
    }
    selected_ = 0;
    hovered_ = kNothing;
    SchedulePaint();
  }

  gfx::RectF CellBounds(size_t i) const {
    const int column = static_cast<int>(i) % kPickerColumns;
    const int row = static_cast<int>(i) / kPickerColumns;
    if (emoji_page_) {
      return gfx::RectF(kPickerGridX + column * kPickerCell,
                        PanelTop() + kEmojiGridTop + row * kEmojiRow - scroll_,
                        kPickerCell, kEmojiRow);
    }
    return gfx::RectF(kPickerGridX + column * kPickerCell,
                      PanelTop() + GridTop(kind_) + row * kPickerCell,
                      kPickerCell, kPickerCell);
  }

  int HitAt(const gfx::Point& point) const {
    const gfx::PointF p(point);
    if (HasPages(kind_)) {
      if (SegmentBounds(true).Contains(p)) {
        return kEmojiSegment;
      }
      if (SegmentBounds(false).Contains(p)) {
        return kIconSegment;
      }
      if (kind_ == IconPickerKind::kSpace && TrashBounds().Contains(p)) {
        return kTrash;
      }
    }
    if (emoji_page_) {
      for (int category = 0; category < kPickerEmojiCategories; ++category) {
        if (CategoryBounds(category).Contains(p)) {
          return kCategoryFirst - category;
        }
      }
      if (!GridViewport().Contains(p)) {
        return kNothing;
      }
    }
    for (size_t i = 0; i < page_.size(); ++i) {
      if (CellBounds(i).Contains(p)) {
        return static_cast<int>(i);
      }
    }
    return kNothing;
  }

  void SetHovered(int hovered) {
    if (hovered_ != hovered) {
      hovered_ = hovered;
      SchedulePaint();
    }
  }

  void ShowPage(bool emoji) {
    if (emoji_page_ != emoji) {
      emoji_page_ = emoji;
      keyboard_ = false;
      scroll_ = 0;
      BuildPage();
      if (search_) {
        search_->SetVisible(emoji_page_);
      }
      if (emoji_page_ && search_) {
        search_->RequestFocus();
      } else {
        RequestFocus();
      }
    }
  }

  // Arrows move a selection over the grid (left and right only once there
  // is one, so they still move the search field's caret), Return picks it,
  // Tab switches pages, Escape picks nothing.
  bool HandleKey(const ui::KeyEvent& event, bool from_search) {
    const int count = static_cast<int>(page_.size());
    int index = keyboard_ ? static_cast<int>(selected_) : -1;
    switch (event.key_code()) {
      case ui::VKEY_ESCAPE:
        pick_.Run(SIZE_MAX);
        return true;
      case ui::VKEY_TAB:
        if (HasPages(kind_)) {
          ShowPage(!emoji_page_);
        }
        return true;
      case ui::VKEY_RETURN:
        if (keyboard_) {
          PickCell(selected_);
        } else if (from_search && count > 0) {
          PickCell(0);
        }
        return true;
      case ui::VKEY_SPACE:
        if (from_search) {
          return false;
        }
        if (keyboard_) {
          PickCell(selected_);
        }
        return true;
      case ui::VKEY_LEFT:
        if (from_search && !keyboard_) {
          return false;
        }
        index = std::max(0, index - 1);
        break;
      case ui::VKEY_RIGHT:
        if (from_search && !keyboard_) {
          return false;
        }
        index = std::min(count - 1, index + 1);
        break;
      case ui::VKEY_UP:
        index = index >= kPickerColumns ? index - kPickerColumns
                                        : std::max(index, 0);
        break;
      case ui::VKEY_DOWN:
        index = index < 0                        ? 0
                : index + kPickerColumns < count ? index + kPickerColumns
                                                 : index;
        break;
      default:
        return false;
    }
    if (count == 0) {
      return true;
    }
    keyboard_ = true;
    selected_ = static_cast<size_t>(std::clamp(index, 0, count - 1));
    if (emoji_page_) {
      ScrollToShow(selected_);
    }
    SchedulePaint();
    return true;
  }

  void PickCell(size_t i) {
    if (i >= page_.size()) {
      return;
    }
    const size_t index = page_[i];
    // A pinned item or folder has no trash: its icon picked again goes back
    // to its own icon.
    if (kind_ != IconPickerKind::kSpace && current_ == index) {
      pick_.Run(choices_.size() - 1);
      return;
    }
    pick_.Run(index);
  }

  const IconPickerKind kind_;
  const std::vector<std::string> choices_;
  const size_t glyph_count_;
  const bool above_;
  const float panel_top_;
  base::RepeatingCallback<void(size_t)> pick_;
  const gfx::FontList title_font_;
  const gfx::FontList segment_font_;
  raw_ptr<views::Textfield> search_ = nullptr;
  float arrow_x_ = 0;
  std::optional<size_t> current_;
  bool emoji_page_ = false;
  // The shown page's cells, as indices into `choices_`.
  std::vector<size_t> page_;
  float scroll_ = 0;
  int hovered_ = kNothing;
  bool keyboard_ = false;
  size_t selected_ = 0;
};

BEGIN_METADATA(IconPickerView)
END_METADATA

#if BUILDFLAG(IS_MAC)
// Arc for macOS' "..." and "+" popovers cast a faint shadow: what is under
// them darkens by 3 to 5 levels within about 4 pt of the panel's edge.
constexpr int kMenuShadowMargin = 6;

gfx::ShadowValues MenuShadows() {
  return {gfx::ShadowValue(gfx::Vector2d(0, 1), 8,
                           SkColorSetA(SK_ColorBLACK, 0x48))};
}

// Holds a popover's panel `kMenuShadowMargin` in and paints its shadow.
class MenuShadowHost : public views::View {
  METADATA_HEADER(MenuShadowHost, views::View)

 public:
  MenuShadowHost(std::unique_ptr<views::View> contents, const gfx::Rect& panel)
      : panel_(panel) {
    contents_ = AddChildView(std::move(contents));
  }

  void Layout(PassKey) override {
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(kMenuShadowMargin);
    contents_->SetBoundsRect(bounds);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const SkRRect rrect = SkRRect::MakeRectXY(gfx::RectToSkRect(panel_),
                                              kRadius, kRadius);
    gfx::ScopedCanvas scoped(canvas);
    // Only the shadow: the panel paints itself over it.
    canvas->sk_canvas()->clipRRect(rrect, SkClipOp::kDifference, true);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SK_ColorBLACK);
    flags.setLooper(gfx::CreateShadowDrawLooper(MenuShadows()));
    canvas->sk_canvas()->drawRRect(rrect, flags);
  }

 private:
  const gfx::Rect panel_;
  raw_ptr<views::View> contents_ = nullptr;
};

BEGIN_METADATA(MenuShadowHost)
END_METADATA
#endif

// Owns a popover's widget; deletes itself once it closes.
class Popover : public views::WidgetObserver {
 public:
  Popover(views::View* anchor,
          base::OnceCallback<void(size_t)> on_pick,
          base::OnceClosure on_closed)
      : anchor_(anchor),
        on_pick_(std::move(on_pick)),
        on_closed_(std::move(on_closed)) {}

  Popover(const Popover&) = delete;
  Popover& operator=(const Popover&) = delete;

  ~Popover() override {
    if (Current() == this) {
      Current() = nullptr;
      SetArcTooltipsSuppressed(false);
    }
  }

  // The popover showing, if any.
  static Popover*& Current() {
    static Popover* current = nullptr;
    return current;
  }

  // Closes, then picks the row or cell at its index; SIZE_MAX picks none.
  base::RepeatingCallback<void(size_t)> GetCloseCallback() {
    return base::BindRepeating(&Popover::Close, weak_factory_.GetWeakPtr());
  }

  views::Widget* widget() { return widget_.get(); }

  // `menu`: the "..." or "+" menu, not an icon picker. `focus` takes the
  // keyboard, `contents` when null.
  void Show(std::unique_ptr<views::View> contents,
            const gfx::Rect& bounds,
            bool menu,
            views::View* focus = nullptr) {
    if (!focus) {
      focus = contents.get();
    }
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
    // It takes the keyboard, and closes once something else is clicked.
    params.activatable = views::Widget::InitParams::Activatable::kYes;
    params.parent = anchor_.view()->GetWidget()->GetNativeView();
    params.bounds = bounds;
    params.name = "ArcomiumPopover";
    widget_->Init(std::move(params));
#if BUILDFLAG(IS_MAC)
    // Arc greys the window controls' rings while its menu is open, not while
    // an icon picker is.
    if (menu) {
      MarkPopoverWindow(widget_.get());
    }
#endif
    widget_->SetContentsView(std::move(contents));
    observation_.Observe(widget_.get());
    Current() = this;
    SetArcTooltipsSuppressed(true);
    widget_->Show();
    widget_->Activate();
    focus->RequestFocus();
  }

  void Close(size_t index) {
    if (closing_) {
      return;
    }
    closing_ = true;
    LastAnchor() = anchor_.view();
    LastClosed() = base::TimeTicks::Now();
    observation_.Reset();
    if (widget_) {
      widget_->Hide();
    }
    // Deleted later: this may run from inside the popover's own events.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<Popover> popover, size_t index) {
              base::OnceClosure on_closed = std::move(popover->on_closed_);
              base::OnceCallback<void(size_t)> on_pick =
                  std::move(popover->on_pick_);
              popover.reset();
              if (on_closed) {
                std::move(on_closed).Run();
              }
              if (on_pick && index != SIZE_MAX) {
                std::move(on_pick).Run(index);
              }
            },
            base::WrapUnique(this), index));
  }

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {
    if (!active) {
      Close(SIZE_MAX);
    }
  }

  void OnWidgetDestroying(views::Widget* widget) override { Close(SIZE_MAX); }

 private:
  views::ViewTracker anchor_;
  base::OnceCallback<void(size_t)> on_pick_;
  base::OnceClosure on_closed_;
  std::unique_ptr<views::Widget> widget_;
  bool closing_ = false;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
  base::WeakPtrFactory<Popover> weak_factory_{this};
};

}  // namespace

void ShowArcPopover(views::View* anchor,
                    std::vector<PopoverItem> items,
                    bool above,
                    int width,
                    base::OnceClosure on_closed) {
  if (Popover::Current()) {
    Popover::Current()->Close(SIZE_MAX);
  }
  if (!anchor || !anchor->GetWidget() || items.empty()) {
    if (on_closed) {
      std::move(on_closed).Run();
    }
    return;
  }
  // The rows' actions run once the popover is gone.
  std::vector<base::OnceClosure> actions;
  for (PopoverItem& item : items) {
    actions.push_back(std::move(item.action));
  }
  auto* popover = new Popover(
      anchor,
      base::BindOnce(
          [](std::vector<base::OnceClosure> actions, size_t index) {
            if (index < actions.size() && actions[index]) {
              std::move(actions[index]).Run();
            }
          },
          std::move(actions)),
      std::move(on_closed));

  const gfx::Rect anchor_bounds = anchor->GetBoundsInScreen();
  const gfx::Rect window =
      anchor->GetWidget()->GetTopLevelWidget()->GetWindowBoundsInScreen();
  const int height = PopoverView::PanelHeight(items) + kArrowHeight;
  const float center = anchor_bounds.x() + anchor_bounds.width() / 2.0f;
  // Arc's Space bar popover starts at the window's margin; others centre.
  int x = above ? window.x() : std::round(center - width / 2.0f);
  x = std::max(window.x() + kWindowMargin,
               std::min(x, window.right() - kWindowMargin - width));
  const int y = above ? anchor_bounds.y() - kAnchorGap - height
                      : anchor_bounds.bottom() + kAnchorGap;
  auto view = std::make_unique<PopoverView>(std::move(items), above,
                                            popover->GetCloseCallback());
  view->set_arrow_x(center - x);
#if BUILDFLAG(IS_MAC)
  // Room round the panel for its shadow.
  views::View* focus = view.get();
  auto host = std::make_unique<MenuShadowHost>(
      std::move(view),
      gfx::Rect(kMenuShadowMargin,
                kMenuShadowMargin + (above ? 0 : kArrowHeight), width,
                height - kArrowHeight));
  // Owns itself until it closes.
  popover->Show(std::move(host),
                gfx::Rect(x - kMenuShadowMargin, y - kMenuShadowMargin,
                          width + 2 * kMenuShadowMargin,
                          height + 2 * kMenuShadowMargin),
                /*menu=*/true, focus);
#else
  // Owns itself until it closes.
  popover->Show(std::move(view), gfx::Rect(x, y, width, height),
                /*menu=*/true);
#endif
}

void ShowArcIconPicker(views::View* anchor,
                       const gfx::Rect& anchor_bounds,
                       int arrow_x,
                       IconPickerKind kind,
                       const std::string& current,
                       base::OnceCallback<void(const std::string&)> picked,
                       base::OnceClosure on_closed) {
  if (Popover::Current()) {
    Popover::Current()->Close(SIZE_MAX);
  }
  if (!anchor || !anchor->GetWidget()) {
    if (on_closed) {
      std::move(on_closed).Run();
    }
    return;
  }
  // The glyphs, the emoji (not a folder's), then "" for none.
  // A pinned item's Icon page holds the Space picker's 56, as Arc's; a
  // folder's picker its 24.
  std::vector<std::string> choices = kind == IconPickerKind::kFolder
                                         ? GetFolderPickerGlyphs()
                                         : GetSpacePickerGlyphs();
  const size_t glyph_count = choices.size();
  if (kind != IconPickerKind::kFolder) {
    for (const std::string& emoji : GetPickerEmoji()) {
      choices.push_back(emoji);
    }
  }
  choices.emplace_back();

  int height = IconPickerView::PanelHeight(kind) + kPickerArrowHeight;
  const gfx::Rect work =
      display::Screen::Get()
          ->GetDisplayNearestPoint(anchor_bounds.CenterPoint())
          .work_area();
  // Under the row as Arc's, its arrow's tip over the row's highlight; above
  // the anchor when there is no room below (a Space's button in the bar).
  const float drop =
      kind == IconPickerKind::kSpace    ? kSpacePickerDrop
      : kind == IconPickerKind::kFolder ? kFolderPickerDrop
                                        : kItemPickerDrop;
  // Arc's panel may start between pixels: the widget starts on a point and
  // the panel inside it, a fraction lower.
  const float panel_y =
      anchor_bounds.y() + anchor_bounds.height() / 2.0f + drop;
  const int below_y = std::floor(panel_y) - kPickerArrowHeight;
  const bool above = below_y + height + 1 > work.bottom() &&
                     anchor_bounds.y() - height >= work.y();
  const float panel_top = above ? 0 : panel_y - below_y;
  if (!above) {
    height = std::ceil(panel_top + IconPickerView::PanelHeight(kind));
  }
  int x = arrow_x - kPickerArrowOffset;
  x = std::max(work.x() + kWindowMargin,
               std::min(x, work.right() - kWindowMargin - kPickerWidth));
  // Above, the tip stops 2 pt into the anchor.
  const int y = above ? std::round(anchor_bounds.y() + 2 - kPickerArrowTip) -
                            (height - kPickerArrowHeight)
                      : below_y;

  auto* popover = new Popover(
      anchor,
      base::BindOnce(
          [](std::vector<std::string> choices, size_t glyph_count,
             base::OnceCallback<void(const std::string&)> picked,
             size_t index) {
            if (index >= choices.size()) {
              return;
            }
            std::move(picked).Run(
                index >= glyph_count && index + 1 < choices.size()
                    ? std::string(kEmojiIconPrefix) + choices[index]
                    : choices[index]);
          },
          choices, glyph_count, std::move(picked)),
      std::move(on_closed));
  auto view = std::make_unique<IconPickerView>(
      kind, std::move(choices), glyph_count, current, above, panel_top,
      popover->GetCloseCallback());
  view->set_arrow_x(arrow_x - x);
  // Owns itself until it closes.
  popover->Show(std::move(view), gfx::Rect(x, y, kPickerWidth, height),
                /*menu=*/false);
#if BUILDFLAG(IS_MAC)
  // Arc's vibrancy: the panel blurs what is behind it.
  BlurBehindPopover(popover->widget());
#endif
}

bool ArcPopoverJustClosed(const views::View* anchor) {
  return anchor && LastAnchor() == anchor &&
         base::TimeTicks::Now() - LastClosed() < kReopenGuard;
}

}  // namespace arcomium
