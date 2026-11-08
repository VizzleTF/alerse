// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/arcomium_popover.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_space_color.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_space_switcher.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
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
constexpr int kShortcutEndInset = 21;
constexpr float kKeyGap = 3;
constexpr float kStroke = 1.6f;
constexpr SkColor kPanelColor = SkColorSetRGB(0x01, 0x01, 0x01);
constexpr SkColor kHighlightColor = SkColorSetRGB(0x23, 0x23, 0x23);
constexpr SkColor kTextColor = SkColorSetRGB(0xFF, 0xFF, 0xFF);
constexpr SkColor kDisabledColor = SkColorSetRGB(0x79, 0x78, 0x80);
constexpr SkColor kShortcutColor = SkColorSetRGB(0x78, 0x78, 0x78);
// A press on the anchor closes the popover before it reaches the anchor.
constexpr base::TimeDelta kReopenGuard = base::Milliseconds(400);

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
      // arrow.clockwise: three quarters of a circle, the head at its end.
      path.arcTo(SkRect::MakeLTRB(x - 7, y - 7, x + 7, y + 7), 49, 270, true);
      canvas->DrawPath(path.detach(), flags);
      SkPathBuilder head;
      head.moveTo(x + 2, y + 0.4f)
          .lineTo(x + 7.4f, y + 0.4f)
          .lineTo(x + 7.4f, y - 5);
      canvas->DrawPath(head.detach(), flags);
      return;
    }
    case PopoverIcon::kSidebar:
      // sidebar.left: 21 by 15 with a pane a third in.
      AddRoundRect(path, x - 9.7f, y - 6.7f, 19.4f, 13.4f, 3);
      canvas->DrawPath(path.detach(), flags);
      canvas->DrawLine(gfx::PointF(x - 3.3f, y - 6.7f),
                       gfx::PointF(x - 3.3f, y + 6.7f), flags);
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
      // plus.square.
      AddRoundRect(path, x - 8.2f, y - 7.7f, 16.4f, 15.4f, 3.5f);
      canvas->DrawPath(path.detach(), flags);
      canvas->DrawLine(gfx::PointF(x - 3.7f, y), gfx::PointF(x + 3.7f, y),
                       flags);
      canvas->DrawLine(gfx::PointF(x, y - 3.7f), gfx::PointF(x, y + 3.7f),
                       flags);
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
        label_font_(Font(13, gfx::Font::Weight::SEMIBOLD)),
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

  void OnMouseMoved(const ui::MouseEvent& event) override {
    Select(RowAt(event.location()));
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    Select(RowAt(event.location()));
  }

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
};

BEGIN_METADATA(PopoverView)
END_METADATA

// Arc's icon picker: a grid of glyphs, the current one on a darker plate
// and the one under the pointer on the highlight.
class IconGridView : public views::View {
  METADATA_HEADER(IconGridView, views::View)

 public:
  IconGridView(std::vector<std::string> names,
               const std::string& current,
               base::RepeatingCallback<void(size_t)> pick)
      : names_(std::move(names)), pick_(std::move(pick)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ARCOMIUM_CHANGE_ICON));
    for (size_t i = 0; i < names_.size(); ++i) {
      if (names_[i] == current) {
        current_ = i;
        selected_ = i;
      }
    }
  }

  static gfx::Size SizeFor(size_t count) {
    const int rows =
        (static_cast<int>(count) + kGridColumns - 1) / kGridColumns;
    return gfx::Size(2 * kPadding + kGridColumns * kGridCell,
                     2 * kPadding + rows * kGridCell);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SidebarInk(this, kPanelColor));
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), kRadius, flags);
    const SkColor ink = SidebarInk(this, kTextColor);
    for (size_t i = 0; i < names_.size(); ++i) {
      const gfx::RectF cell = CellBounds(i);
      if (i == selected_ || i == current_) {
        flags.setColor(SidebarInk(
            this, i == selected_ ? kHighlightColor : kCurrentColor));
        canvas->DrawRoundRect(cell, kHighlightRadius, flags);
      }
      if (names_[i].empty()) {
        // No icon of its own: a slashed ring.
        cc::PaintFlags ring;
        ring.setAntiAlias(true);
        ring.setColor(SidebarInk(this, kDisabledColor));
        ring.setStyle(cc::PaintFlags::kStroke_Style);
        ring.setStrokeWidth(kStroke);
        const gfx::PointF c = cell.CenterPoint();
        canvas->DrawCircle(c, 6.5f, ring);
        canvas->DrawLine(c + gfx::Vector2dF(-4.6f, 4.6f),
                         c + gfx::Vector2dF(4.6f, -4.6f), ring);
        continue;
      }
      PaintSpaceIcon(canvas, cell.CenterPoint(), names_[i], ink);
    }
  }

  void OnMouseMoved(const ui::MouseEvent& event) override {
    if (std::optional<size_t> cell = CellAt(event.location());
        cell && cell != selected_) {
      selected_ = cell;
      SchedulePaint();
    }
  }

  bool OnMousePressed(const ui::MouseEvent& event) override { return true; }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (std::optional<size_t> cell = CellAt(event.location())) {
      pick_.Run(*cell);
    }
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    const int count = static_cast<int>(names_.size());
    int index = selected_ ? static_cast<int>(*selected_) : -1;
    switch (event.key_code()) {
      case ui::VKEY_ESCAPE:
        pick_.Run(SIZE_MAX);
        return true;
      case ui::VKEY_RETURN:
      case ui::VKEY_SPACE:
        if (selected_) {
          pick_.Run(*selected_);
        }
        return true;
      case ui::VKEY_LEFT:
        index = std::max(0, index - 1);
        break;
      case ui::VKEY_RIGHT:
        index = std::min(count - 1, index + 1);
        break;
      case ui::VKEY_UP:
        index = index >= kGridColumns ? index - kGridColumns : index;
        break;
      case ui::VKEY_DOWN:
        index = index + kGridColumns < count ? index + kGridColumns
                                             : std::max(index, 0);
        break;
      default:
        return false;
    }
    selected_ = static_cast<size_t>(std::max(index, 0));
    SchedulePaint();
    return true;
  }

 private:
  static constexpr int kGridColumns = 7;
  static constexpr int kGridCell = 32;
  static constexpr SkColor kCurrentColor = SkColorSetRGB(0x16, 0x16, 0x16);

  gfx::RectF CellBounds(size_t index) const {
    const int column = static_cast<int>(index) % kGridColumns;
    const int row = static_cast<int>(index) / kGridColumns;
    return gfx::RectF(kPadding + column * kGridCell, kPadding + row * kGridCell,
                      kGridCell, kGridCell);
  }

  std::optional<size_t> CellAt(const gfx::Point& point) const {
    for (size_t i = 0; i < names_.size(); ++i) {
      if (CellBounds(i).Contains(gfx::PointF(point))) {
        return i;
      }
    }
    return std::nullopt;
  }

  const std::vector<std::string> names_;
  base::RepeatingCallback<void(size_t)> pick_;
  std::optional<size_t> current_;
  std::optional<size_t> selected_;
};

BEGIN_METADATA(IconGridView)
END_METADATA

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

  void Show(std::unique_ptr<views::View> contents, const gfx::Rect& bounds) {
    views::View* focus = contents.get();
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
    widget_->SetContentsView(std::move(contents));
    observation_.Observe(widget_.get());
    Current() = this;
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
  // Owns itself until it closes.
  popover->Show(std::move(view), gfx::Rect(x, y, width, height));
}

void ShowArcIconPicker(views::View* anchor,
                       const gfx::Point& screen_point,
                       std::vector<std::string> names,
                       const std::string& current,
                       base::OnceCallback<void(const std::string&)> picked) {
  if (Popover::Current()) {
    Popover::Current()->Close(SIZE_MAX);
  }
  if (!anchor || !anchor->GetWidget() || names.empty()) {
    return;
  }
  const gfx::Rect window =
      anchor->GetWidget()->GetTopLevelWidget()->GetWindowBoundsInScreen();
  gfx::Rect bounds(screen_point, IconGridView::SizeFor(names.size()));
  bounds.AdjustToFit(gfx::Rect(window.x() + kWindowMargin,
                               window.y() + kWindowMargin,
                               window.width() - 2 * kWindowMargin,
                               window.height() - 2 * kWindowMargin));
  auto* popover = new Popover(
      anchor,
      base::BindOnce(
          [](std::vector<std::string> names,
             base::OnceCallback<void(const std::string&)> picked,
             size_t index) {
            if (index < names.size()) {
              std::move(picked).Run(names[index]);
            }
          },
          names, std::move(picked)),
      base::OnceClosure());
  // Owns itself until it closes.
  popover->Show(std::make_unique<IconGridView>(std::move(names), current,
                                               popover->GetCloseCallback()),
                bounds);
}

bool ArcPopoverJustClosed(const views::View* anchor) {
  return anchor && LastAnchor() == anchor &&
         base::TimeTicks::Now() - LastClosed() < kReopenGuard;
}

}  // namespace arcomium
