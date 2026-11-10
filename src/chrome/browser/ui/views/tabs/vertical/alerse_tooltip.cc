// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/alerse_tooltip.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace alerse {
namespace {

// Arc for macOS' tooltips, measured on its sidebar (points).
constexpr base::TimeDelta kShowDelay = base::Seconds(1);
// A sidebar row's, with its hint below the name: Arc's shows within 1.05 s
// of the pointer reaching a row, also a folder's nested row.
constexpr base::TimeDelta kDetailShowDelay = base::Milliseconds(900);
// Right after one hides, the next shows as soon as the pointer stops (no
// move for this long), where it stops, as Arc's.
constexpr base::TimeDelta kWarmWindow = base::Milliseconds(500);
constexpr base::TimeDelta kWarmSettle = base::Milliseconds(400);
constexpr base::TimeDelta kSettlePoll = base::Milliseconds(50);
constexpr base::TimeDelta kHoverPoll = base::Milliseconds(100);
// From the pointer to the pill's corner; near the window's bottom, centred
// 3 pt above the view instead.
constexpr int kCursorOffset = 7;
constexpr int kGapAbove = 3;
constexpr int kWindowMargin = 4;
// One line: 29 pt, round ends, the name in 12 pt medium 11 pt in;
// keycaps 18 pt wide, 4 apart, 6 after the name and 11.5 before the end,
// 18.25 pt tall with a 2.5 pt darker lip at their bottom.
constexpr float kLineHeight = 29;
constexpr float kTwoLineHeight = 44.0f;
constexpr float kTwoLineRadius = 12;
constexpr float kTextInset = 11;
// A two-line hint ends 9.5 pt after its longer line, as Arc's (141.5 pt for
// "Double-click to rename"), where one-line pills keep 11.
constexpr float kTwoLineEndInset = 9.5f;
constexpr float kKeySize = 18;
constexpr float kKeyHeight = 18.25f;
constexpr float kKeyLip = 2.5f;
constexpr float kKeyGap = 4;
constexpr float kKeysInset = 6;
constexpr float kKeysEndInset = 11.5f;
constexpr float kKeyRadius = 4;
// Black inside a #1B1B1B and a #3B3B3C hairline.
constexpr SkColor kFill = SkColorSetRGB(0x02, 0x02, 0x02);
constexpr SkColor kInnerRim = SkColorSetRGB(0x1B, 0x1B, 0x1B);
constexpr SkColor kOuterRim = SkColorSetRGB(0x3B, 0x3B, 0x3C);
constexpr SkColor kTitleColor = SkColorSetRGB(0xDA, 0xDA, 0xDA);
constexpr SkColor kDetailColor = SkColorSetRGB(0x90, 0x90, 0x90);
constexpr SkColor kKeyColor = SkColorSetRGB(0xCD, 0xCD, 0xCD);
constexpr SkColor kKeyLipColor = SkColorSetRGB(0x77, 0x77, 0x77);
// The corner at the pointer is tighter than the round ends.
constexpr float kPointerCornerRadius = 9.5f;
constexpr SkColor kKeyTextColor = SkColorSetRGB(0x1A, 0x1A, 0x1A);

struct Content {
  std::u16string title;
  std::u16string detail;
  std::vector<std::u16string> keys;
};

// Two lines: the name 1.5 pt lower than centred in its 16 pt line, the
// second line in 11 pt, as in Arc.
constexpr float kTitleShift = 1.5f;

gfx::FontList Font(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.DeriveWithSizeDelta(size - base.GetFontSize())
      .DeriveWithWeight(weight);
}

// The keys of `shortcut` as Arc's keycaps: the modifiers, then the key.
std::vector<std::u16string> KeysOf(const ui::Accelerator& shortcut) {
  std::vector<std::u16string> keys;
  const std::u16string kModifiers = u"⌃⌥⇧⌘";
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
  std::u16string key = shortcut.GetShortcutText();
  while (!key.empty() && kModifiers.find(key[0]) != std::u16string::npos) {
    key.erase(0, 1);
  }
  if (!key.empty()) {
    keys.push_back(key);
  }
  return keys;
}

class Pill : public views::View {
  METADATA_HEADER(Pill, views::View)

 public:
  Pill(Content content, bool at_pointer)
      : content_(std::move(content)),
        at_pointer_(at_pointer),
        title_font_(Font(12, gfx::Font::Weight::MEDIUM)),
        detail_font_(Font(11, gfx::Font::Weight::NORMAL)),
        key_font_(Font(12, gfx::Font::Weight::SEMIBOLD)) {}

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    float width =
        kTextInset + gfx::GetStringWidthF(content_.title, title_font_);
    if (!content_.detail.empty()) {
      width = std::max(width, kTextInset + gfx::GetStringWidthF(
                                               content_.detail, detail_font_));
    }
    if (content_.keys.empty()) {
      width += content_.detail.empty() ? kTextInset : kTwoLineEndInset;
    } else {
      width += kKeysInset + content_.keys.size() * (kKeySize + kKeyGap) -
               kKeyGap + kKeysEndInset;
    }
    return gfx::Size(std::ceil(width),
                     std::ceil(content_.detail.empty() ? kLineHeight
                                                       : kTwoLineHeight));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const bool two_lines = !content_.detail.empty();
    gfx::RectF pill(0, 0, width(),
                    two_lines ? kTwoLineHeight : kLineHeight);
    float radius = two_lines ? kTwoLineRadius : pill.height() / 2;
    float corner = at_pointer_ ? kPointerCornerRadius : radius;
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    for (const SkColor color : {kOuterRim, kInnerRim, kFill}) {
      flags.setColor(color);
      const SkVector radii[4] = {{corner, corner},
                                 {radius, radius},
                                 {radius, radius},
                                 {radius, radius}};
      SkRRect shape;
      shape.setRectRadii(gfx::RectFToSkRect(pill), radii);
      canvas->sk_canvas()->drawRRect(shape, flags);
      pill.Inset(0.5f);
      radius -= 0.5f;
      corner -= 0.5f;
    }
    if (two_lines) {
      {
        // Arc's second line sits half a point left of and above the name's
        // inset.
        gfx::ScopedCanvas detail_canvas(canvas);
        canvas->sk_canvas()->translate(-0.5f, -0.5f);
        canvas->DrawStringRect(content_.detail, detail_font_, kDetailColor,
                               gfx::Rect(kTextInset, 22, width(), 16));
      }
      gfx::ScopedCanvas scoped_canvas(canvas);
      canvas->sk_canvas()->translate(0, kTitleShift);
      canvas->DrawStringRect(content_.title, title_font_, kTitleColor,
                             gfx::Rect(kTextInset, 6, width(), 16));
      return;
    }
    const int line = std::round(kLineHeight);
    canvas->DrawStringRect(content_.title, title_font_, kTitleColor,
                           gfx::Rect(kTextInset, 0, width(), line));
    float x = width() - kKeysEndInset -
              content_.keys.size() * (kKeySize + kKeyGap) + kKeyGap;
    const float y = (kLineHeight - kKeyHeight) / 2;
    for (const std::u16string& key : content_.keys) {
      flags.setColor(kKeyLipColor);
      canvas->DrawRoundRect(gfx::RectF(x, y, kKeySize, kKeyHeight), kKeyRadius,
                            flags);
      flags.setColor(kKeyColor);
      canvas->DrawRoundRect(gfx::RectF(x, y, kKeySize, kKeyHeight - kKeyLip),
                            kKeyRadius, flags);
      canvas->DrawStringRectWithFlags(
          key, key_font_, kKeyTextColor,
          gfx::Rect(std::round(x), std::round(y), std::round(kKeySize),
                    std::round(kKeyHeight - kKeyLip)),
          gfx::Canvas::TEXT_ALIGN_CENTER);
      x += kKeySize + kKeyGap;
    }
  }

 private:
  const Content content_;
  const bool at_pointer_;
  const gfx::FontList title_font_;
  const gfx::FontList detail_font_;
  const gfx::FontList key_font_;
};

BEGIN_METADATA(Pill)
END_METADATA

class TooltipHandler;

// The tooltip showing, if any, and when the last one hid.
TooltipHandler*& Showing() {
  static TooltipHandler* showing = nullptr;
  return showing;
}

bool& Suppressed() {
  static bool suppressed = false;
  return suppressed;
}

base::TimeTicks& LastHidden() {
  static base::TimeTicks last_hidden;
  return last_hidden;
}

std::map<views::View*, TooltipHandler*>& Handlers() {
  static base::NoDestructor<std::map<views::View*, TooltipHandler*>> handlers;
  return *handlers;
}

// Watches the pointer over one view and shows its pill.
class TooltipHandler : public ui::EventHandler, public views::ViewObserver {
 public:
  explicit TooltipHandler(views::View* view) : view_(view) {
    view_->AddPreTargetHandler(this);
    observation_.Observe(view_);
    Handlers()[view_] = this;
  }

  ~TooltipHandler() override {
    Hide();
    Handlers().erase(view_);
  }

  void SetContent(Content content) {
    Hide();
    content_ = std::move(content);
  }

  void SetBelowPointer() { below_pointer_ = true; }

  void Dismiss() { Hide(); }

  void SetDetail(const std::u16string& detail) {
    Hide();
    content_.detail = detail;
  }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    switch (event->type()) {
      case ui::EventType::kMouseEntered:
      case ui::EventType::kMouseMoved:
        if (OverChildWithTooltip(event)) {
          // A child with a pill of its own (a row's close button) has the
          // pointer: this view's pill gives way to it.
          Hide();
          break;
        }
        if (!pressed_ && !widget_ && !show_timer_.IsRunning() &&
            !settle_poll_.IsRunning() && !Suppressed() &&
            view_->IsMouseHovered()) {
          if (base::TimeTicks::Now() - LastHidden() < kWarmWindow) {
            // Polled: a disabled button (Back) sees only the pointer's entry,
            // not its moves.
            settle_point_ = display::Screen::Get()->GetCursorScreenPoint();
            settled_since_ = base::TimeTicks::Now();
            settle_poll_.Start(FROM_HERE, kSettlePoll,
                               base::BindRepeating(&TooltipHandler::CheckSettled,
                                                   base::Unretained(this)));
          } else {
            show_timer_.Start(FROM_HERE,
                              content_.detail.empty() ? kShowDelay
                                                      : kDetailShowDelay,
                              base::BindOnce(&TooltipHandler::Show,
                                             base::Unretained(this)));
          }
        }
        break;
      case ui::EventType::kMouseExited:
        if (!view_->IsMouseHovered()) {
          pressed_ = false;
          Hide();
        }
        break;
      case ui::EventType::kMousePressed:
      case ui::EventType::kMousewheel:
        pressed_ = true;
        Hide();
        break;
      default:
        break;
    }
  }

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    view_->RemovePreTargetHandler(this);
    observation_.Reset();
    delete this;
  }

 private:
  void Show() {
    if (Suppressed()) {
      return;
    }
    views::Widget* parent = view_->GetWidget();
    if (!parent || content_.title.empty() || !view_->IsMouseHovered()) {
      return;
    }
    if (Showing() && Showing() != this) {
      Showing()->Hide();
    }
    const gfx::Size size = Pill(content_, false).GetPreferredSize();
    const gfx::Point cursor =
        display::Screen::Get()->GetCursorScreenPoint();
    const gfx::Rect window =
        parent->GetTopLevelWidget()->GetWindowBoundsInScreen();
    gfx::Rect bounds(cursor.x() + kCursorOffset, cursor.y() + kCursorOffset,
                     size.width(), size.height());
    if (below_pointer_) {
      // As macOS places Arc's Library tooltip, a little further off.
      bounds.Offset(1, 3);
    } else if (!content_.detail.empty() &&
               bounds.bottom() <= window.bottom() - kWindowMargin) {
      // Arc's two-line row hint sits 11 pt right of and 5 pt below the
      // pointer, not 7 and 7 as its one-line pills.
      bounds.Offset(11 - kCursorOffset, 5 - kCursorOffset);
    } else if (bounds.bottom() > window.bottom() - kWindowMargin) {
      const gfx::Rect view_bounds = view_->GetBoundsInScreen();
      bounds.set_x(view_bounds.CenterPoint().x() - size.width() / 2);
      bounds.set_y(view_bounds.y() - kGapAbove - size.height());
    }
    bounds.set_x(std::max(
        window.x() + kWindowMargin,
        std::min(bounds.x(), window.right() - kWindowMargin - size.width())));
    const bool at_pointer = bounds.y() > cursor.y();
    auto pill = std::make_unique<Pill>(content_, at_pointer);

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_TOOLTIP);
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
    params.accept_events = false;
    params.parent = parent->GetNativeView();
    params.bounds = bounds;
    params.name = "AlerseTooltip";
    widget_->Init(std::move(params));
    widget_->SetContentsView(std::move(pill));
    widget_->ShowInactive();
    Showing() = this;
    // The pointer may leave without an exit event (rows are rebuilt).
    hover_poll_.Start(FROM_HERE, kHoverPoll,
                      base::BindRepeating(&TooltipHandler::CheckHover,
                                          base::Unretained(this)));
  }

  // Whether `event` goes to a view inside view_ with its own pill.
  bool OverChildWithTooltip(const ui::MouseEvent* event) const {
    for (views::View* target = static_cast<views::View*>(event->target());
         target && target != view_; target = target->parent()) {
      if (Handlers().contains(target)) {
        return true;
      }
    }
    return false;
  }

  // Shows the pill once the pointer has not moved for kWarmSettle.
  void CheckSettled() {
    const gfx::Point point = display::Screen::Get()->GetCursorScreenPoint();
    if (point != settle_point_) {
      settle_point_ = point;
      settled_since_ = base::TimeTicks::Now();
      return;
    }
    if (base::TimeTicks::Now() - settled_since_ >= kWarmSettle) {
      settle_poll_.Stop();
      Show();
    }
  }

  void CheckHover() {
    if (!view_->IsMouseHovered()) {
      pressed_ = false;
      Hide();
    }
  }

  void Hide() {
    show_timer_.Stop();
    settle_poll_.Stop();
    hover_poll_.Stop();
    if (widget_) {
      widget_.reset();
      LastHidden() = base::TimeTicks::Now();
    }
    if (Showing() == this) {
      Showing() = nullptr;
    }
  }

  const raw_ptr<views::View> view_;
  Content content_;
  bool below_pointer_ = false;
  bool pressed_ = false;
  // Right after another pill hid: where the pointer was last seen, and since
  // when it has stayed there.
  gfx::Point settle_point_;
  base::TimeTicks settled_since_;
  base::RepeatingTimer settle_poll_;
  std::unique_ptr<views::Widget> widget_;
  base::OneShotTimer show_timer_;
  base::RepeatingTimer hover_poll_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

}  // namespace

void SetArcTooltip(views::View* view,
                   const std::u16string& title,
                   const std::u16string& detail,
                   std::optional<ui::Accelerator> shortcut) {
  Content content{title, detail,
                  shortcut ? KeysOf(*shortcut) : std::vector<std::u16string>()};
  auto it = Handlers().find(view);
  TooltipHandler* handler =
      it != Handlers().end() ? it->second : new TooltipHandler(view);
  handler->SetContent(std::move(content));
}

void SetArcTooltipBelowPointer(views::View* view) {
  auto it = Handlers().find(view);
  if (it != Handlers().end()) {
    it->second->SetBelowPointer();
  }
}

void SetArcTooltipDetail(views::View* view, const std::u16string& detail) {
  auto it = Handlers().find(view);
  if (it != Handlers().end()) {
    it->second->SetDetail(detail);
  }
}

void SetArcTooltipsSuppressed(bool suppressed) {
  Suppressed() = suppressed;
  if (suppressed && Showing()) {
    Showing()->Dismiss();
  }
}

}  // namespace alerse
