// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/arcomium_glyphs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/scoped_canvas.h"

namespace arcomium {

namespace {

// Draws in a 16 pt box around (0, 0): solid shapes, and "cuts" that clear
// what is under them, all within one layer so the cuts show the background.
class GlyphPainter {
 public:
  GlyphPainter(gfx::Canvas* canvas, SkColor color)
      : canvas_(canvas), color_(color) {}

  cc::PaintFlags Fill() const {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);
    return flags;
  }

  cc::PaintFlags Stroke(float width) const {
    cc::PaintFlags flags = Fill();
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(width);
    flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
    flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
    return flags;
  }

  static cc::PaintFlags Clear(cc::PaintFlags flags) {
    flags.setBlendMode(SkBlendMode::kClear);
    return flags;
  }

  // A filled polygon with corners rounded by a thin stroke around it.
  void Solid(SkPathBuilder& path, float round = 0.9f) {
    path.close();
    const SkPath shape = path.detach();
    canvas_->DrawPath(shape, Fill());
    if (round > 0) {
      canvas_->DrawPath(shape, Stroke(round));
    }
  }

  void Circle(float x, float y, float r, bool cut = false) {
    canvas_->DrawCircle(gfx::PointF(x, y), r, cut ? Clear(Fill()) : Fill());
  }

  void Oval(float x, float y, float rx, float ry, bool cut = false) {
    canvas_->sk_canvas()->drawOval(SkRect::MakeLTRB(x - rx, y - ry, x + rx,
                                                    y + ry),
                                   cut ? Clear(Fill()) : Fill());
  }

  void Rect(float x, float y, float w, float h, float r, bool cut = false) {
    canvas_->DrawRoundRect(gfx::RectF(x, y, w, h), r,
                           cut ? Clear(Fill()) : Fill());
  }

  void Line(float x1, float y1, float x2, float y2, float w, bool cut = false) {
    canvas_->DrawLine(gfx::PointF(x1, y1), gfx::PointF(x2, y2),
                      cut ? Clear(Stroke(w)) : Stroke(w));
  }

  void Path(SkPathBuilder& path, float w, bool cut = false) {
    canvas_->DrawPath(path.detach(), cut ? Clear(Stroke(w)) : Stroke(w));
  }

  // Runs `draw` rotated by `degrees` around (0, 0).
  template <typename Draw>
  void Rotated(float degrees, Draw draw) {
    canvas_->sk_canvas()->save();
    canvas_->sk_canvas()->rotate(degrees);
    draw();
    canvas_->sk_canvas()->restore();
  }

 private:
  const raw_ptr<gfx::Canvas> canvas_;
  const SkColor color_;
};

using Paint = void (*)(GlyphPainter&);

struct Glyph {
  const char* name;
  Paint paint;
  // Measured against Arc's picker cell by cell: some drawings are a little
  // smaller than Arc's.
  float scale = 1;
};

void Heart(GlyphPainter& p) {
  SkPathBuilder path;
  path.moveTo(0, 6.2f)
      .cubicTo(-4.6f, 3.2f, -6.9f, 0.6f, -6.9f, -2.2f)
      .cubicTo(-6.9f, -4.6f, -5.1f, -6.2f, -3.2f, -6.2f)
      .cubicTo(-1.7f, -6.2f, -0.6f, -5.4f, 0, -4.1f)
      .cubicTo(0.6f, -5.4f, 1.7f, -6.2f, 3.2f, -6.2f)
      .cubicTo(5.1f, -6.2f, 6.9f, -4.6f, 6.9f, -2.2f)
      .cubicTo(6.9f, 0.6f, 4.6f, 3.2f, 0, 6.2f);
  p.Solid(path, 0);
}

void Flag(GlyphPainter& p) {
  p.Rect(-5.9f, -6.8f, 1.7f, 13.6f, 0.85f);
  SkPathBuilder cloth;
  cloth.moveTo(-4.4f, -6.2f)
      .cubicTo(-1.4f, -7.6f, 1.6f, -4.6f, 5.9f, -5.9f)
      .lineTo(5.9f, 1.4f)
      .cubicTo(1.6f, 2.7f, -1.4f, -0.3f, -4.4f, 1.1f);
  p.Solid(cloth);
}

void Flash(GlyphPainter& p) {
  SkPathBuilder path;
  path.moveTo(1.6f, -7)
      .lineTo(-4.6f, 1.1f)
      .lineTo(-0.4f, 1.1f)
      .lineTo(-1.6f, 7)
      .lineTo(4.6f, -1.1f)
      .lineTo(0.4f, -1.1f);
  p.Solid(path, 1.2f);
}

void Triangle(GlyphPainter& p) {
  SkPathBuilder path;
  path.moveTo(0, -5.6f).lineTo(6.2f, 5.2f).lineTo(-6.2f, 5.2f);
  p.Solid(path, 1.8f);
}

void Medical(GlyphPainter& p) {
  for (const float angle : {0.0f, 60.0f, 120.0f}) {
    p.Rotated(angle, [&p] { p.Rect(-1.6f, -6.4f, 3.2f, 12.8f, 1.6f); });
  }
}

void Notifications(GlyphPainter& p) {
  SkPathBuilder bell;
  bell.moveTo(-4.9f, 2.6f)
      .lineTo(-4.9f, -1.2f)
      .cubicTo(-4.9f, -4.4f, -2.7f, -6.4f, 0, -6.4f)
      .cubicTo(2.7f, -6.4f, 4.9f, -4.4f, 4.9f, -1.2f)
      .lineTo(4.9f, 2.6f)
      .lineTo(6.3f, 4.3f)
      .lineTo(-6.3f, 4.3f);
  p.Solid(bell);
  p.Oval(0, 5.7f, 1.9f, 1.4f);
}

void Bulb(GlyphPainter& p) {
  SkPathBuilder bulb;
  bulb.moveTo(-2.4f, 3.4f)
      .cubicTo(-2.4f, 1.6f, -5.2f, 0.4f, -5.2f, -2.4f)
      .cubicTo(-5.2f, -5.3f, -2.9f, -7, 0, -7)
      .cubicTo(2.9f, -7, 5.2f, -5.3f, 5.2f, -2.4f)
      .cubicTo(5.2f, 0.4f, 2.4f, 1.6f, 2.4f, 3.4f);
  p.Solid(bulb);
  p.Rect(-2.4f, 4.4f, 4.8f, 1.3f, 0.6f);
  p.Rect(-1.7f, 6.1f, 3.4f, 1.2f, 0.6f);
}

void Grid(GlyphPainter& p) {
  for (const float x : {-6.3f, 0.7f}) {
    for (const float y : {-6.3f, 0.7f}) {
      p.Rect(x, y, 5.6f, 5.6f, 1.4f);
    }
  }
}

void Apps(GlyphPainter& p) {
  for (const float x : {-4.6f, 0.0f, 4.6f}) {
    for (const float y : {-4.6f, 0.0f, 4.6f}) {
      p.Circle(x, y, 1.6f);
    }
  }
}

void Layers(GlyphPainter& p) {
  SkPathBuilder top;
  top.moveTo(0, -6.6f).lineTo(6.8f, -3.2f).lineTo(0, 0.2f).lineTo(-6.8f, -3.2f);
  p.Solid(top);
  for (const float y : {0.2f, 3.4f}) {
    SkPathBuilder layer;
    layer.moveTo(-6.4f, y).lineTo(0, y + 3.2f).lineTo(6.4f, y);
    p.Path(layer, 1.5f);
  }
}

void Server(GlyphPainter& p) {
  for (const float y : {-6.4f, -1.8f, 2.8f}) {
    p.Rect(-6.2f, y, 12.4f, 3.6f, 1.8f);
  }
}

void FileTray(GlyphPainter& p) {
  SkPathBuilder lid;
  lid.moveTo(-4.6f, -5.6f).lineTo(4.6f, -5.6f).lineTo(6.6f, -0.6f).lineTo(-6.6f,
                                                                         -0.6f);
  p.Solid(lid);
  p.Rect(-6.6f, -0.6f, 13.2f, 6.4f, 1.6f);
  p.Rect(-2.2f, 1.6f, 4.4f, 1.4f, 0.7f, /*cut=*/true);
}

void Copy(GlyphPainter& p) {
  p.Rect(-6.2f, -6.2f, 9.2f, 9.2f, 2);
  p.Rect(-3.8f, -3.8f, 11.2f, 11.2f, 3, /*cut=*/true);
  p.Rect(-2.8f, -2.8f, 9.2f, 9.2f, 2);
}

void Calendar(GlyphPainter& p) {
  p.Rect(-6.6f, -5.2f, 13.2f, 11.8f, 2);
  p.Rect(-5.2f, -1.6f, 10.4f, 6.8f, 0.8f, /*cut=*/true);
  for (const float x : {-3.6f, -0.9f, 1.8f}) {
    for (const float y : {-0.4f, 2.4f}) {
      p.Rect(x, y, 1.8f, 1.6f, 0.4f);
    }
  }
  p.Rect(-4.2f, -7, 1.5f, 3, 0.75f);
  p.Rect(2.7f, -7, 1.5f, 3, 0.75f);
}

void Mail(GlyphPainter& p) {
  p.Rect(-7, -5.2f, 14, 10.4f, 2);
  SkPathBuilder flap;
  flap.moveTo(-5.4f, -3.4f).lineTo(0, 0.6f).lineTo(5.4f, -3.4f);
  p.Path(flap, 1.3f, /*cut=*/true);
}

void Checkbox(GlyphPainter& p) {
  p.Rect(-6.2f, -6.2f, 12.4f, 12.4f, 2.6f);
  SkPathBuilder check;
  check.moveTo(-3.3f, 0.1f).lineTo(-0.9f, 2.6f).lineTo(3.4f, -2.5f);
  p.Path(check, 1.6f, /*cut=*/true);
}

void Document(GlyphPainter& p) {
  SkPathBuilder page;
  page.moveTo(-4.9f, -6.6f)
      .lineTo(1.4f, -6.6f)
      .lineTo(5.3f, -2.7f)
      .lineTo(5.3f, 6.6f)
      .lineTo(-4.9f, 6.6f);
  p.Solid(page, 1.2f);
  SkPathBuilder fold;
  fold.moveTo(1.2f, -6.4f).lineTo(1.2f, -2.5f).lineTo(5.1f, -2.5f);
  p.Path(fold, 0.9f, /*cut=*/true);
}

void ChatBubble(GlyphPainter& p) {
  p.Oval(0.3f, -0.6f, 6.6f, 5.6f);
  SkPathBuilder tail;
  tail.moveTo(-4.6f, 2).lineTo(-6.6f, 6.4f).lineTo(-1.2f, 4.4f);
  p.Solid(tail, 0.6f);
  for (const float x : {-2.7f, 0.3f, 3.3f}) {
    p.Circle(x, -0.6f, 1.05f, /*cut=*/true);
  }
}

void Construction(GlyphPainter& p) {
  // A hammer across a wrench.
  p.Rotated(45, [&p] {
    p.Rect(-1.15f, -3.6f, 2.3f, 10.6f, 1.15f);
    p.Circle(0, -4.6f, 2.9f);
    p.Rect(-1.1f, -8.2f, 2.2f, 3.8f, 0.3f, /*cut=*/true);
  });
  p.Rotated(-45, [&p] {
    p.Rect(-1.15f, -3.2f, 2.3f, 10.2f, 1.15f);
    p.Rect(-4.2f, -6.8f, 8.4f, 3.4f, 1);
  });
}

void Square(GlyphPainter& p) {
  p.Rect(-5.6f, -5.6f, 11.2f, 11.2f, 2.2f);
}

void Egg(GlyphPainter& p) {
  SkPathBuilder egg;
  egg.moveTo(0, -6.6f)
      .cubicTo(3.4f, -6.6f, 5.2f, -1.2f, 5.2f, 1.6f)
      .cubicTo(5.2f, 4.8f, 3, 6.6f, 0, 6.6f)
      .cubicTo(-3, 6.6f, -5.2f, 4.8f, -5.2f, 1.6f)
      .cubicTo(-5.2f, -1.2f, -3.4f, -6.6f, 0, -6.6f);
  p.Solid(egg, 0);
}

void Ellipse(GlyphPainter& p) {
  p.Circle(0, 0, 5.9f);
}

void Moon(GlyphPainter& p) {
  p.Circle(-0.4f, 0.4f, 6.2f);
  p.Circle(3.2f, -2.9f, 5.1f, /*cut=*/true);
}

void Cloud(GlyphPainter& p) {
  p.Rect(-6.6f, -0.2f, 13.4f, 5.2f, 2.6f);
  p.Circle(-2.6f, 0.2f, 3.3f);
  p.Circle(1.4f, -1.2f, 4.1f);
  p.Circle(4.1f, 1.8f, 2.9f);
}

void Paw(GlyphPainter& p) {
  p.Oval(0, 3, 3.8f, 3.2f);
  p.Oval(-5.2f, -0.6f, 1.6f, 2);
  p.Oval(-2.1f, -4.4f, 1.7f, 2.2f);
  p.Oval(2.1f, -4.4f, 1.7f, 2.2f);
  p.Oval(5.2f, -0.6f, 1.6f, 2);
}

void Basket(GlyphPainter& p) {
  SkPathBuilder handle;
  handle.moveTo(-4, -0.8f).cubicTo(-4, -6.8f, 4, -6.8f, 4, -0.8f);
  p.Path(handle, 1.6f);
  SkPathBuilder body;
  body.moveTo(-6.8f, -1.2f).lineTo(6.8f, -1.2f).lineTo(5, 6).lineTo(-5, 6);
  p.Solid(body);
  for (const float x : {-2.2f, 2.2f}) {
    p.Line(x, 1.2f, x * 0.9f, 3.8f, 1.1f, /*cut=*/true);
  }
}

void Gift(GlyphPainter& p) {
  p.Rect(-6.8f, -3.8f, 13.6f, 3.2f, 1);
  p.Rect(-5.9f, 0.2f, 11.8f, 6.4f, 1.4f);
  p.Line(0, -3.8f, 0, 6.6f, 1.3f, /*cut=*/true);
  SkPathBuilder bow;
  bow.moveTo(0, -3.8f)
      .cubicTo(-1.2f, -7.6f, -5, -6.8f, -3.2f, -4.4f)
      .moveTo(0, -3.8f)
      .cubicTo(1.2f, -7.6f, 5, -6.8f, 3.2f, -4.4f);
  p.Path(bow, 1.4f);
}

void Bed(GlyphPainter& p) {
  p.Rect(-5.4f, -4.6f, 10.8f, 4.4f, 1.6f);
  p.Rect(-7, 0.4f, 14, 3.8f, 1.2f);
  p.Rect(-6.6f, 3.6f, 1.7f, 2.6f, 0.6f);
  p.Rect(4.9f, 3.6f, 1.7f, 2.6f, 0.6f);
}

void Restaurant(GlyphPainter& p) {
  // A fork across a knife.
  p.Rotated(45, [&p] {
    p.Rect(-1.7f, -7, 3.4f, 6.4f, 1.7f);
    p.Rect(-1, -2, 2, 9, 1);
  });
  p.Rotated(-45, [&p] {
    for (const float x : {-1.7f, 0.0f, 1.7f}) {
      p.Rect(x - 0.5f, -7, 1, 3.6f, 0.5f);
    }
    p.Rect(-2.2f, -4.2f, 4.4f, 2.4f, 1.1f);
    p.Rect(-1, -2.6f, 2, 9.6f, 1);
  });
}

void Airplane(GlyphPainter& p) {
  p.Rect(-6.8f, -1.15f, 14, 2.3f, 1.15f);
  for (const float side : {-1.0f, 1.0f}) {
    SkPathBuilder wing;
    wing.moveTo(-1.8f, -0.6f * side)
        .lineTo(2.2f, -0.6f * side)
        .lineTo(-0.8f, -6.8f * side)
        .lineTo(-2.8f, -6.8f * side);
    p.Solid(wing, 0.8f);
    SkPathBuilder tail;
    tail.moveTo(-6.6f, -0.6f * side)
        .lineTo(-4.6f, -0.6f * side)
        .lineTo(-5.6f, -3.8f * side)
        .lineTo(-6.8f, -3.8f * side);
    p.Solid(tail, 0.8f);
  }
}

void MusicalNote(GlyphPainter& p) {
  p.Rotated(-20, [&p] { p.Oval(-1.8f, 4.6f, 2.9f, 2.3f); });
  p.Rect(-0.2f, -6.6f, 1.7f, 11, 0.6f);
  SkPathBuilder flag;
  flag.moveTo(1.4f, -6.6f)
      .cubicTo(3.6f, -5.6f, 6.2f, -4.8f, 5.6f, -1.4f)
      .cubicTo(5, -3.2f, 3.2f, -3.6f, 1.4f, -3.6f);
  p.Solid(flag, 0.6f);
}

void ColorPalette(GlyphPainter& p) {
  SkPathBuilder palette;
  palette.moveTo(0, -6.4f)
      .cubicTo(3.8f, -6.4f, 6.6f, -3.8f, 6.6f, -0.6f)
      .cubicTo(6.6f, 2, 4.8f, 2.6f, 3.4f, 2.4f)
      .cubicTo(2, 2.2f, 1.2f, 3.4f, 2, 4.6f)
      .cubicTo(2.8f, 5.8f, 1.6f, 6.6f, 0, 6.6f)
      .cubicTo(-3.8f, 6.6f, -6.6f, 3.6f, -6.6f, 0)
      .cubicTo(-6.6f, -3.6f, -3.8f, -6.4f, 0, -6.4f);
  p.Solid(palette, 0);
  p.Circle(-3.4f, 1.6f, 1.15f, /*cut=*/true);
  p.Circle(-3.2f, -2.2f, 1.15f, /*cut=*/true);
  p.Circle(0, -3.9f, 1.15f, /*cut=*/true);
  p.Circle(3.3f, -2.2f, 1.15f, /*cut=*/true);
}

void Videocam(GlyphPainter& p) {
  p.Rect(-7, -4.6f, 9.8f, 9.2f, 2);
  SkPathBuilder lens;
  lens.moveTo(3.8f, -1.4f).lineTo(7, -4).lineTo(7, 4).lineTo(3.8f, 1.4f);
  p.Solid(lens);
}

void Bandage(GlyphPainter& p) {
  p.Rotated(-45, [&p] {
    p.Rect(-7.4f, -2.9f, 14.8f, 5.8f, 2.9f);
    for (const float x : {-0.9f, 0.9f}) {
      for (const float y : {-0.9f, 0.9f}) {
        p.Circle(x, y, 0.55f, /*cut=*/true);
      }
    }
  });
}

void Code(GlyphPainter& p) {
  SkPathBuilder left;
  left.moveTo(-2.4f, -4.6f).lineTo(-6.6f, 0).lineTo(-2.4f, 4.6f);
  p.Path(left, 1.7f);
  SkPathBuilder right;
  right.moveTo(2.4f, -4.6f).lineTo(6.6f, 0).lineTo(2.4f, 4.6f);
  p.Path(right, 1.7f);
}

void Basketball(GlyphPainter& p) {
  p.Circle(0, 0, 6.4f);
  p.Line(0, -6.6f, 0, 6.6f, 0.9f, /*cut=*/true);
  p.Line(-6.6f, 0, 6.6f, 0, 0.9f, /*cut=*/true);
  for (const float side : {-1.0f, 1.0f}) {
    SkPathBuilder seam;
    seam.moveTo(4.6f * side, -4.6f)
        .cubicTo(1.8f * side, -2, 1.8f * side, 2, 4.6f * side, 4.6f);
    p.Path(seam, 0.9f, /*cut=*/true);
  }
}

void Bug(GlyphPainter& p) {
  for (const float side : {-1.0f, 1.0f}) {
    p.Line(3.4f * side, -1.2f, 6.6f * side, -3, 1.2f);
    p.Line(3.8f * side, 1.8f, 6.8f * side, 1.8f, 1.2f);
    p.Line(3.4f * side, 4.4f, 6.2f * side, 6.4f, 1.2f);
  }
  p.Oval(0, 1.6f, 4.3f, 5.1f);
  p.Circle(0, -4.6f, 2.5f);
  p.Line(0, -2.2f, 0, 6.8f, 0.8f, /*cut=*/true);
}

void Pizza(GlyphPainter& p) {
  SkPathBuilder slice;
  slice.moveTo(-6.2f, -4.6f)
      .cubicTo(-2.2f, -7, 2.2f, -7, 6.2f, -4.6f)
      .lineTo(0, 7);
  p.Solid(slice);
  SkPathBuilder crust;
  crust.moveTo(-5.2f, -2.8f).cubicTo(-1.8f, -4.8f, 1.8f, -4.8f, 5.2f, -2.8f);
  p.Path(crust, 0.9f, /*cut=*/true);
  p.Circle(-1.6f, -0.9f, 1.05f, /*cut=*/true);
  p.Circle(1.8f, 0.4f, 1.05f, /*cut=*/true);
  p.Circle(0, 3.4f, 0.9f, /*cut=*/true);
}

void Skull(GlyphPainter& p) {
  p.Circle(0, -1.4f, 6);
  p.Rect(-3.6f, 2, 7.2f, 4.8f, 1.4f);
  p.Circle(-2.4f, -1.2f, 1.75f, /*cut=*/true);
  p.Circle(2.4f, -1.2f, 1.75f, /*cut=*/true);
  SkPathBuilder nose;
  nose.moveTo(0, 0.9f).lineTo(0.9f, 2.4f).lineTo(-0.9f, 2.4f);
  p.Path(nose, 0.6f, /*cut=*/true);
  for (const float x : {-1.2f, 1.2f}) {
    p.Line(x, 4.8f, x, 7, 0.7f, /*cut=*/true);
  }
}

void Newspaper(GlyphPainter& p) {
  p.Rect(-6.6f, -5.6f, 13.2f, 11.2f, 1.6f);
  p.Rect(-4.6f, -3.6f, 3.8f, 3.2f, 0.4f, /*cut=*/true);
  for (const float y : {-3.1f, -1.0f}) {
    p.Line(1.2f, y, 4.6f, y, 0.9f, /*cut=*/true);
  }
  for (const float y : {1.4f, 3.5f}) {
    p.Line(-4.6f, y, 4.6f, y, 0.9f, /*cut=*/true);
  }
}

void ThumbsUp(GlyphPainter& p) {
  p.Rect(-6.8f, -0.6f, 3.2f, 7.2f, 1);
  SkPathBuilder hand;
  hand.moveTo(-2.6f, -0.6f)
      .lineTo(0, -6)
      .cubicTo(1.2f, -7.4f, 3.4f, -6.4f, 2.8f, -4.4f)
      .lineTo(2.2f, -1.4f)
      .lineTo(5.4f, -1.4f)
      .cubicTo(6.6f, -1.4f, 7.2f, -0.4f, 6.8f, 0.8f)
      .lineTo(5.4f, 5.6f)
      .cubicTo(5.1f, 6.3f, 4.6f, 6.6f, 3.8f, 6.6f)
      .lineTo(-2.6f, 6.6f);
  p.Solid(hand, 0.8f);
}

void Train(GlyphPainter& p) {
  p.Rect(-5.2f, -6.8f, 10.4f, 11.6f, 2.8f);
  p.Rect(-3.6f, -4.8f, 7.2f, 4, 1, /*cut=*/true);
  p.Circle(-2.4f, 2, 1.05f, /*cut=*/true);
  p.Circle(2.4f, 2, 1.05f, /*cut=*/true);
  p.Line(-3, 5.4f, -4.8f, 7.4f, 1.4f);
  p.Line(3, 5.4f, 4.8f, 7.4f, 1.4f);
}

void Planet(GlyphPainter& p) {
  p.Circle(0, 0, 4.4f);
  p.Rotated(-25, [&p] {
    SkPathBuilder front;
    front.moveTo(-7.4f, 0).cubicTo(-7.4f, 3.4f, 7.4f, 3.4f, 7.4f, 0);
    p.Path(front, 2.8f, /*cut=*/true);
    SkPathBuilder ring;
    ring.moveTo(-3.6f, -2.4f)
        .cubicTo(-9.2f, -1.2f, -7.4f, 2.6f, 0, 2.6f)
        .cubicTo(7.4f, 2.6f, 9.2f, -1.2f, 3.6f, -2.4f);
    p.Path(ring, 1.3f);
  });
}

void Leaf(GlyphPainter& p) {
  SkPathBuilder leaf;
  leaf.moveTo(-6, 6.2f)
      .cubicTo(-6.6f, -2, -1.2f, -6.6f, 6.6f, -6.6f)
      .cubicTo(6.6f, 1.2f, 2, 6.6f, -6, 6.2f);
  p.Solid(leaf, 0);
  p.Line(-4.6f, 4.8f, 3, -3, 0.9f, /*cut=*/true);
}

void Star(GlyphPainter& p) {
  SkPathBuilder star;
  for (int i = 0; i < 10; ++i) {
    const float angle = (i * 36 - 90) * std::numbers::pi_v<float> / 180;
    const float r = i % 2 ? 3.1f : 7.2f;
    const SkPoint point = SkPoint::Make(r * std::cos(angle),
                                        0.6f + r * std::sin(angle));
    if (i == 0) {
      star.moveTo(point);
    } else {
      star.lineTo(point);
    }
  }
  p.Solid(star, 1);
}

void Bookmark(GlyphPainter& p) {
  SkPathBuilder mark;
  mark.moveTo(-4.8f, -6.8f)
      .lineTo(4.8f, -6.8f)
      .lineTo(4.8f, 6.8f)
      .lineTo(0, 2.6f)
      .lineTo(-4.8f, 6.8f);
  p.Solid(mark, 1.2f);
}

void Shapes(GlyphPainter& p) {
  p.Circle(2.6f, 2.6f, 4.4f);
  SkPathBuilder gap;
  gap.moveTo(-1.6f, -6.6f).lineTo(3.6f, 2).lineTo(-6.8f, 2).close();
  p.Path(gap, 2.6f, /*cut=*/true);
  SkPathBuilder triangle;
  triangle.moveTo(-1.6f, -6.6f).lineTo(3.6f, 2).lineTo(-6.8f, 2);
  p.Solid(triangle, 1.2f);
}

void Folder(GlyphPainter& p) {
  SkPathBuilder back;
  back.moveTo(-7, -4.4f)
      .quadTo(-7, -6, -5.4f, -6)
      .lineTo(-2.2f, -6)
      .lineTo(-0.6f, -4.6f)
      .lineTo(5.4f, -4.6f)
      .quadTo(7, -4.6f, 7, -3)
      .lineTo(7, -2.4f)
      .lineTo(-7, -2.4f);
  p.Solid(back, 0);
  p.Rect(-7, -1.4f, 14, 7.4f, 1.6f);
}

void Archive(GlyphPainter& p) {
  p.Rect(-7, -6.4f, 14, 12.8f, 2.2f);
  p.Rect(-5.4f, -4.8f, 10.8f, 5.4f, 0.8f, /*cut=*/true);
  p.Rect(-3.8f, -3.4f, 7.6f, 1.5f, 0.75f);
  p.Rect(-5.4f, -1.2f, 10.8f, 1.2f, 0.4f);
}

void People(GlyphPainter& p) {
  p.Circle(3, -3.4f, 2.7f);
  p.Oval(3, 4.6f, 4.8f, 3.8f);
  p.Rect(-2.2f, 4.4f, 11, 3, 0, /*cut=*/true);
  p.Circle(-3.4f, -3, 3.4f, /*cut=*/true);
  p.Oval(-3.2f, 4.6f, 5, 4.2f, /*cut=*/true);
  p.Circle(-3.4f, -3, 2.4f);
  p.Oval(-3.2f, 4.6f, 3.9f, 3.4f);
  p.Rect(-8, 4.4f, 16, 4, 0, /*cut=*/true);
}

void Terminal(GlyphPainter& p) {
  p.Rect(-7, -6, 14, 12, 2.2f);
  SkPathBuilder prompt;
  prompt.moveTo(-4.4f, -3.4f).lineTo(-1.8f, -1.2f).lineTo(-4.4f, 1);
  p.Path(prompt, 1.2f, /*cut=*/true);
  p.Line(-0.6f, 1, 2.4f, 1, 1.2f, /*cut=*/true);
}

void Barbell(GlyphPainter& p) {
  p.Rect(-4, -0.8f, 8, 1.6f, 0.4f);
  for (const float side : {-1.0f, 1.0f}) {
    p.Rect(side * 4.6f - 1.4f, -4.2f, 2.8f, 8.4f, 1.4f);
    p.Rect(side * 6.9f - 0.9f, -2.6f, 1.8f, 5.2f, 0.9f);
  }
}

void CloudOutline(GlyphPainter& p) {
  p.Rect(-7, -0.2f, 14.2f, 5.6f, 2.8f);
  p.Circle(-2.6f, 0.2f, 3.5f);
  p.Circle(1.4f, -1.4f, 4.4f);
  p.Circle(4.2f, 1.8f, 3.4f);
  p.Rect(-5.7f, 1.1f, 11.6f, 3, 1.5f, /*cut=*/true);
  p.Circle(-2.6f, 0.2f, 2.2f, /*cut=*/true);
  p.Circle(1.4f, -1.4f, 3.1f, /*cut=*/true);
  p.Circle(4.2f, 1.8f, 2.1f, /*cut=*/true);
}

void Map(GlyphPainter& p) {
  for (const float x : {-6.9f, 2.5f}) {
    p.Rect(x, -5.6f, 4.4f, 12.4f, 1);
  }
  p.Rect(-1.9f, -6.8f, 3.8f, 12.4f, 0.6f);
}

constexpr auto kGlyphs = std::to_array<Glyph>({
    {"star", &Star},
    {"bookmark", &Bookmark},
    {"shapes", &Shapes},
    {"folder", &Folder, 1.08f},
    {"archive", &Archive},
    {"people", &People},
    {"terminal", &Terminal, 1.1f},
    {"barbell", &Barbell},
    {"cloudOutline", &CloudOutline, 1.1f},
    {"map", &Map},
    {"heart", &Heart},
    {"flag", &Flag},
    {"flash", &Flash},
    {"triangle", &Triangle},
    {"medical", &Medical},
    {"notifications", &Notifications},
    {"bulb", &Bulb},
    {"grid", &Grid, 1.08f},
    {"apps", &Apps, 1.08f},
    {"layers", &Layers},
    {"server", &Server, 1.08f},
    {"fileTray", &FileTray},
    {"copy", &Copy},
    {"calendar", &Calendar},
    {"mail", &Mail},
    {"checkbox", &Checkbox, 1.08f},
    {"document", &Document},
    {"chatBubbleEllipses", &ChatBubble},
    {"construction", &Construction, 1.15f},
    {"square", &Square, 1.2f},
    {"egg", &Egg, 1.08f},
    {"ellipse", &Ellipse, 1.15f},
    {"moon", &Moon, 1.1f},
    {"cloud", &Cloud, 1.15f},
    {"paw", &Paw, 1.08f},
    {"basket", &Basket},
    {"gift", &Gift},
    {"bed", &Bed},
    {"restaurant", &Restaurant},
    {"airplane", &Airplane},
    {"musicalNote", &MusicalNote},
    {"colorPalette", &ColorPalette},
    {"videocam", &Videocam},
    {"bandage", &Bandage, 1.2f},
    {"code", &Code},
    {"basketball", &Basketball},
    {"bug", &Bug},
    {"pizza", &Pizza},
    {"skull", &Skull},
    {"newspaper", &Newspaper, 1.1f},
    {"thumbsUp", &ThumbsUp},
    {"train", &Train},
    {"planet", &Planet, 1.17f},
    {"leaf", &Leaf},
});

const Glyph* FindGlyph(std::string_view name) {
  for (const Glyph& glyph : kGlyphs) {
    if (name == glyph.name) {
      return &glyph;
    }
  }
  return nullptr;
}

}  // namespace

bool IsArcGlyph(std::string_view name) {
  return FindGlyph(name) != nullptr;
}

bool PaintArcGlyph(gfx::Canvas* canvas,
                   const gfx::PointF& center,
                   std::string_view name,
                   SkColor color) {
  const Glyph* glyph = FindGlyph(name);
  if (!glyph) {
    return false;
  }
  gfx::ScopedCanvas scoped(canvas);
  canvas->sk_canvas()->translate(center.x(), center.y());
  // The cuts clear only this layer, so they show what is under the glyph.
  canvas->SaveLayerAlpha(0xFF, gfx::Rect(-11, -11, 22, 22));
  canvas->sk_canvas()->scale(glyph->scale, glyph->scale);
  GlyphPainter painter(canvas, color);
  glyph->paint(painter);
  canvas->Restore();
  return true;
}

const std::vector<std::string>& GetSpacePickerGlyphs() {
  static const base::NoDestructor<std::vector<std::string>> names({
      "star",     "bookmark",     "heart",        "flag",
      "flash",    "triangle",     "medical",      "notifications",
      "bulb",     "shapes",       "grid",         "apps",
      "layers",   "server",       "fileTray",     "copy",
      "folder",   "archive",      "calendar",     "mail",
      "checkbox", "document",     "book",         "chatBubbleEllipses",
      "people",   "terminal",     "construction", "square",
      "egg",      "ellipse",      "moon",         "sunny",
      "planet",   "leaf",         "cloud",        "paw",
      "basket",   "gift",         "bed",          "restaurant",
      "barbell",  "airplane",     "musicalNote",  "colorPalette",
      "videocam", "bandage",      "code",         "basketball",
      "cloudOutline", "map",      "bug",          "pizza",
      "skull",    "newspaper",    "thumbsUp",     "train",
  });
  return *names;
}

const std::vector<std::string>& GetFolderPickerGlyphs() {
  static const base::NoDestructor<std::vector<std::string>> names({
      "star",  "bookmark", "heart",  "flash",  "triangle", "medical",
      "notifications", "bulb", "chatBubbleEllipses", "people",
      "construction", "egg", "ellipse", "moon", "planet", "leaf",
      "cloud", "paw", "restaurant", "airplane", "musicalNote", "videocam",
      "code", "skull",
  });
  return *names;
}

bool IsEmojiIcon(std::string_view name) {
  return name.size() > std::size(kEmojiIconPrefix) - 1 &&
         name.starts_with(kEmojiIconPrefix);
}

namespace {

struct PickerEmojiEntry {
  const char* text;
  const char* name;
};

#include "chrome/browser/ui/views/tabs/vertical/arcomium_emoji_data.inc"

static_assert(std::size(kPickerEmojiCategoryStarts) == kPickerEmojiCategories);

}  // namespace

const std::vector<std::string>& GetPickerEmoji() {
  static const base::NoDestructor<std::vector<std::string>> emoji([] {
    std::vector<std::string> texts;
    texts.reserve(std::size(kPickerEmojiTable));
    for (const PickerEmojiEntry& entry : kPickerEmojiTable) {
      texts.emplace_back(entry.text);
    }
    return texts;
  }());
  return *emoji;
}

std::string_view GetPickerEmojiName(size_t index) {
  return index < std::size(kPickerEmojiTable) ? kPickerEmojiTable[index].name
                                              : std::string_view();
}

size_t GetPickerEmojiCategoryStart(int category) {
  return category >= 0 && category < kPickerEmojiCategories
             ? kPickerEmojiCategoryStarts[category]
             : std::size(kPickerEmojiTable);
}

void PaintEmojiIcon(gfx::Canvas* canvas,
                    const gfx::PointF& center,
                    std::string_view name,
                    uint8_t alpha,
                    int font_size) {
  if (!IsEmojiIcon(name)) {
    return;
  }
  const std::u16string text =
      base::UTF8ToUTF16(name.substr(std::size(kEmojiIconPrefix) - 1));
#if BUILDFLAG(IS_WIN)
  // Name the color emoji face: the UI font's fallback may pick a mono one.
  const gfx::FontList font({"Segoe UI Emoji"}, gfx::Font::NORMAL, font_size,
                           gfx::Font::Weight::NORMAL);
#else
  const gfx::FontList base_font;
  const gfx::FontList font =
      base_font.DeriveWithSizeDelta(font_size - base_font.GetFontSize());
#endif
  const int box = font_size * 2;
  const gfx::Rect rect(std::round(center.x() - box / 2.0f),
                       std::round(center.y() - box / 2.0f), box, box);
  canvas->SaveLayerAlpha(alpha, rect);
  canvas->DrawStringRectWithFlags(text, font, SK_ColorWHITE, rect,
                                  gfx::Canvas::TEXT_ALIGN_CENTER |
                                      gfx::Canvas::NO_SUBPIXEL_RENDERING);
  canvas->Restore();
}

std::string CanonicalGlyphName(std::string_view name) {
  // Arcomium's first icons, under the names Arc gives the same pictures.
  static constexpr std::pair<std::string_view, std::string_view> kAliases[] = {
      {"bolt", "flash"},          {"lightbulb", "bulb"},
      {"chat", "chatBubbleEllipses"}, {"pets", "paw"},
      {"flight", "airplane"},
  };
  for (const auto& [old_name, new_name] : kAliases) {
    if (name == old_name) {
      return std::string(new_name);
    }
  }
  return std::string(name);
}

}  // namespace arcomium
