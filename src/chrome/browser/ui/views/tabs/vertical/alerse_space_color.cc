// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/alerse_space_color.h"

#include "chrome/browser/ui/views/frame/alerse_frame_color.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace alerse {

// Defined by browser_widget.cc, which reads it in GetColorProviderKey(): the
// frame color, opaque, so never null. Native window properties are keyed by
// this very pointer.
extern const char kSpaceTintWindowProperty[];

namespace {

// Arc's picker: a square pad, a row with the light/dark switch and the
// intensity slider, a brightness slider, then a row of preset swatches.
constexpr int kMargin = 10;
constexpr int kPadSize = 290;
constexpr int kPadRadius = 8;
constexpr int kWidth = kMargin * 2 + kPadSize;
constexpr int kControlsY = kMargin + kPadSize + 27;
constexpr int kBrightnessY = kControlsY + 36;
constexpr int kSwatchY = kBrightnessY + 40;
constexpr int kHeight = kSwatchY + 26;
constexpr float kHandleRadius = 11;
constexpr float kSwatchRadius = 14;
constexpr float kSwitchX = 32;
constexpr float kSwitchWidth = 40;
constexpr float kSwitchHeight = 20;
constexpr float kSliderLeft = 104;
constexpr float kSliderRight = kWidth - 14;
// The brightness slider, between a dim and a bright disc.
constexpr float kBrightnessLeft = 34;
constexpr float kBrightnessRight = kWidth - 36;

constexpr SkColor kControlColor = SkColorSetRGB(0xE6, 0xE6, 0xE6);
constexpr SkColor kSwitchRimColor = SkColorSetRGB(0x8A, 0x8A, 0x8A);
constexpr SkColor kTrackColor = SkColorSetRGB(0x4A, 0x4A, 0x4A);
constexpr SkColor kSliderColor = SkColorSetRGB(0x4F, 0xB8, 0xF0);
constexpr SkColor kRingColor = SkColorSetRGB(0x3A, 0x3A, 0x3A);

struct Preset {
  double hue;
  double saturation;
};

// Arc's swatches: purple, magenta, red, orange, yellow, olive, green, teal and
// blue.
constexpr Preset kPresets[] = {{275, 0.7}, {318, 0.7}, {356, 0.7},
                               {24, 0.75}, {44, 0.8},  {62, 0.7},
                               {135, 0.6}, {172, 0.6}, {218, 0.7}};

// What a point of the pad looks like: brighter than the frame it makes, so the
// hues read.
SkColor PadColor(double hue, double saturation, bool light) {
  return color_utils::HSLToSkColor(
      {hue / 360.0, saturation * 0.8, light ? 0.78 : 0.30}, 255);
}

class ColorPickerView : public views::View {
  METADATA_HEADER(ColorPickerView, views::View)

 public:
  ColorPickerView(const SpaceColor& initial, SpaceColorCallback callback)
      : color_(initial), callback_(std::move(callback)) {
    SetPreferredSize(gfx::Size(kWidth, kHeight));
  }

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    const gfx::PointF point(event.location());
    drag_ = Drag::kNone;
    if (PadBounds().Contains(point)) {
      drag_ = Drag::kPad;
      SetFromPad(point);
      return true;
    }
    if (SwitchBounds().Contains(point)) {
      color_.light = !color_.light;
      Changed(/*commit=*/true);
      return true;
    }
    if (SliderBounds().Contains(point)) {
      drag_ = Drag::kSlider;
      SetFromSlider(point);
      return true;
    }
    if (BrightnessBounds().Contains(point)) {
      drag_ = Drag::kBrightness;
      SetFromBrightness(point);
      return true;
    }
    for (size_t i = 0; i < std::size(kPresets); ++i) {
      if ((point - SwatchCenter(i)).Length() <= kSwatchRadius + 2) {
        color_.hue = kPresets[i].hue;
        color_.saturation = kPresets[i].saturation;
        Changed(/*commit=*/true);
        return true;
      }
    }
    return false;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    const gfx::PointF point(event.location());
    if (drag_ == Drag::kPad) {
      SetFromPad(point);
    } else if (drag_ == Drag::kSlider) {
      SetFromSlider(point);
    } else if (drag_ == Drag::kBrightness) {
      SetFromBrightness(point);
    }
    return drag_ != Drag::kNone;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (drag_ != Drag::kNone) {
      drag_ = Drag::kNone;
      Changed(/*commit=*/true);
    }
  }

  void OnMouseCaptureLost() override {
    if (drag_ != Drag::kNone) {
      drag_ = Drag::kNone;
      Changed(/*commit=*/true);
    }
  }

  void OnPaint(gfx::Canvas* canvas) override {
    PaintPad(canvas);
    PaintControls(canvas);
    PaintSwatches(canvas);
  }

 private:
  enum class Drag { kNone, kPad, kSlider, kBrightness };

  static gfx::RectF PadBounds() {
    return gfx::RectF(kMargin, kMargin, kPadSize, kPadSize);
  }
  static gfx::RectF SwitchBounds() {
    return gfx::RectF(kSwitchX - 4, kControlsY - kSwitchHeight / 2 - 4,
                      kSwitchWidth + 8, kSwitchHeight + 8);
  }
  static gfx::RectF SliderBounds() {
    return gfx::RectF(kSliderLeft - 10, kControlsY - 12,
                      kSliderRight - kSliderLeft + 20, 24);
  }
  static gfx::RectF BrightnessBounds() {
    return gfx::RectF(kBrightnessLeft - 10, kBrightnessY - 12,
                      kBrightnessRight - kBrightnessLeft + 20, 24);
  }
  static gfx::PointF SwatchCenter(size_t index) {
    const float first = kMargin + kSwatchRadius;
    const float pitch = (kPadSize - 2 * kSwatchRadius) /
                        static_cast<float>(std::size(kPresets) - 1);
    return gfx::PointF(first + pitch * static_cast<float>(index), kSwatchY);
  }

  gfx::PointF HandleCenter() const {
    const gfx::PointF center = PadBounds().CenterPoint();
    const double angle = color_.hue * std::numbers::pi / 180;
    const double radius = color_.saturation * (kPadSize / 2.0 - kHandleRadius);
    return gfx::PointF(center.x() + std::cos(angle) * radius,
                       center.y() - std::sin(angle) * radius);
  }

  void SetFromPad(const gfx::PointF& point) {
    const gfx::PointF center = PadBounds().CenterPoint();
    const double dx = point.x() - center.x();
    const double dy = center.y() - point.y();
    double hue = std::atan2(dy, dx) * 180 / std::numbers::pi;
    if (hue < 0) {
      hue += 360;
    }
    color_.hue = hue;
    color_.saturation = std::clamp(
        std::hypot(dx, dy) / (kPadSize / 2.0 - kHandleRadius), 0.0, 1.0);
    Changed(/*commit=*/false);
  }

  void SetFromSlider(const gfx::PointF& point) {
    color_.intensity = std::clamp(
        (point.x() - kSliderLeft) / (kSliderRight - kSliderLeft), 0.0f, 1.0f);
    Changed(/*commit=*/false);
  }

  void SetFromBrightness(const gfx::PointF& point) {
    color_.brightness = std::clamp(
        (point.x() - kBrightnessLeft) / (kBrightnessRight - kBrightnessLeft),
        0.0f, 1.0f);
    Changed(/*commit=*/false);
  }

  static void PaintSlider(gfx::Canvas* canvas,
                          float left,
                          float right,
                          float y,
                          double value) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const float knob = left + (right - left) * static_cast<float>(value);
    flags.setColor(kTrackColor);
    canvas->DrawRoundRect(gfx::RectF(left, y - 2, right - left, 4), 2, flags);
    flags.setColor(kSliderColor);
    canvas->DrawRoundRect(gfx::RectF(left, y - 2, knob - left, 4), 2, flags);
    flags.setColor(SkColorSetRGB(0x55, 0x55, 0x55));
    canvas->DrawCircle(gfx::PointF(knob, y), 9, flags);
    flags.setColor(kSliderColor);
    canvas->DrawCircle(gfx::PointF(knob, y), 5, flags);
  }

  void Changed(bool commit) {
    SchedulePaint();
    callback_.Run(color_, commit);
  }

  const gfx::ImageSkia& PadImage() {
    if (pad_image_.isNull() || pad_image_light_ != color_.light) {
      SkBitmap bitmap;
      bitmap.allocN32Pixels(kPadSize, kPadSize);
      const double half = kPadSize / 2.0;
      for (int y = 0; y < kPadSize; ++y) {
        for (int x = 0; x < kPadSize; ++x) {
          const double dx = x + 0.5 - half;
          const double dy = half - y - 0.5;
          double hue = std::atan2(dy, dx) * 180 / std::numbers::pi;
          if (hue < 0) {
            hue += 360;
          }
          const double saturation =
              std::min(1.0, std::hypot(dx, dy) / (half - kHandleRadius));
          *bitmap.getAddr32(x, y) =
              SkPreMultiplyColor(PadColor(hue, saturation, color_.light));
        }
      }
      bitmap.setImmutable();
      pad_image_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
      pad_image_light_ = color_.light;
    }
    return pad_image_;
  }

  void PaintPad(gfx::Canvas* canvas) {
    const gfx::RectF pad = PadBounds();
    canvas->Save();
    canvas->ClipPath(
        SkPathBuilder()
            .addRRect(SkRRect::MakeRectXY(gfx::RectFToSkRect(pad), kPadRadius,
                                          kPadRadius))
            .detach(),
        /*do_anti_alias=*/true);
    canvas->DrawImageInt(PadImage(), kMargin, kMargin);
    canvas->Restore();

    // Arc's handle: a dark dot in a light ring.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const gfx::PointF handle = HandleCenter();
    flags.setColor(SkColorSetARGB(0x99, 0xFF, 0xFF, 0xFF));
    canvas->DrawCircle(handle, kHandleRadius, flags);
    flags.setColor(SkColorSetRGB(0x0C, 0x0C, 0x0C));
    canvas->DrawCircle(handle, kHandleRadius - 2.5f, flags);
  }

  void PaintControls(gfx::Canvas* canvas) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kControlColor);

    // Sun.
    const gfx::PointF sun(17, kControlsY);
    canvas->DrawCircle(sun, 3.2f, flags);
    cc::PaintFlags rays = flags;
    rays.setStyle(cc::PaintFlags::kStroke_Style);
    rays.setStrokeWidth(1.4f);
    rays.setStrokeCap(cc::PaintFlags::kRound_Cap);
    for (int i = 0; i < 8; ++i) {
      const double angle = i * std::numbers::pi / 4;
      const gfx::Vector2dF dir(std::cos(angle), std::sin(angle));
      canvas->DrawLine(sun + gfx::ScaleVector2d(dir, 5.2f),
                       sun + gfx::ScaleVector2d(dir, 7.2f), rays);
    }

    // The light/dark switch: the knob sits right in dark mode.
    const gfx::RectF track(kSwitchX, kControlsY - kSwitchHeight / 2,
                           kSwitchWidth, kSwitchHeight);
    cc::PaintFlags rim;
    rim.setAntiAlias(true);
    rim.setStyle(cc::PaintFlags::kStroke_Style);
    rim.setStrokeWidth(1.5f);
    rim.setColor(kSwitchRimColor);
    gfx::RectF rim_bounds = track;
    rim_bounds.Inset(0.75f);
    canvas->DrawRoundRect(rim_bounds, kSwitchHeight / 2, rim);
    const float knob_x = color_.light ? track.x() + kSwitchHeight / 2
                                      : track.right() - kSwitchHeight / 2;
    flags.setColor(SkColorSetRGB(0xCF, 0xCF, 0xCF));
    canvas->DrawCircle(gfx::PointF(knob_x, kControlsY), 6.5f, flags);

    // Moon: a disc with a bite of the panel's color.
    const gfx::PointF moon(kSwitchX + kSwitchWidth + 13, kControlsY);
    flags.setColor(kControlColor);
    canvas->DrawCircle(moon, 6, flags);
    flags.setColor(kSpaceColorPickerBackground);
    canvas->DrawCircle(moon + gfx::Vector2dF(-3.5f, -2.5f), 5, flags);

    // The intensity slider.
    PaintSlider(canvas, kSliderLeft, kSliderRight, kControlsY,
                color_.intensity);

    // Brightness: darker at the dim disc, lighter at the bright one.
    cc::PaintFlags dim;
    dim.setAntiAlias(true);
    dim.setStyle(cc::PaintFlags::kStroke_Style);
    dim.setStrokeWidth(1.4f);
    dim.setColor(kControlColor);
    canvas->DrawCircle(gfx::PointF(17, kBrightnessY), 5.5f, dim);
    flags.setColor(kControlColor);
    canvas->DrawCircle(gfx::PointF(kWidth - 18, kBrightnessY), 6.5f, flags);
    PaintSlider(canvas, kBrightnessLeft, kBrightnessRight, kBrightnessY,
                color_.brightness);
  }

  void PaintSwatches(gfx::Canvas* canvas) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    for (size_t i = 0; i < std::size(kPresets); ++i) {
      const Preset& preset = kPresets[i];
      const bool selected =
          std::abs(preset.hue - color_.hue) < 0.5 &&
          std::abs(preset.saturation - color_.saturation) < 0.005;
      const gfx::PointF center = SwatchCenter(i);
      flags.setColor(selected ? SK_ColorWHITE : kRingColor);
      canvas->DrawCircle(center, kSwatchRadius, flags);
      flags.setColor(
          PadColor(preset.hue, preset.saturation, /*light=*/false));
      canvas->DrawCircle(center, kSwatchRadius - 2.5f, flags);
    }
  }

  SpaceColor color_;
  SpaceColorCallback callback_;
  Drag drag_ = Drag::kNone;
  gfx::ImageSkia pad_image_;
  bool pad_image_light_ = false;
};

BEGIN_METADATA(ColorPickerView)
END_METADATA

}  // namespace

std::optional<SpaceColor> ParseSpaceColor(const std::string& value) {
  const std::vector<std::string> parts = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  SpaceColor color;
  if ((parts.size() != 4 && parts.size() != 5) ||
      !base::StringToDouble(parts[0], &color.hue) ||
      !base::StringToDouble(parts[1], &color.saturation) ||
      !base::StringToDouble(parts[2], &color.intensity) ||
      (parts[3] != "light" && parts[3] != "dark") ||
      (parts.size() == 5 &&
       !base::StringToDouble(parts[4], &color.brightness))) {
    return std::nullopt;
  }
  color.brightness = std::clamp(color.brightness, 0.0, 1.0);
  color.hue = std::fmod(std::fmod(color.hue, 360) + 360, 360);
  color.saturation = std::clamp(color.saturation, 0.0, 1.0);
  color.intensity = std::clamp(color.intensity, 0.0, 1.0);
  color.light = parts[3] == "light";
  return color;
}

std::string SerializeSpaceColor(const SpaceColor& color) {
  return base::StringPrintf("%.1f,%.3f,%.3f,%s,%.3f", color.hue,
                            color.saturation, color.intensity,
                            color.light ? "light" : "dark", color.brightness);
}

SkColor GetSpaceFrameColor(const SpaceColor& color) {
  const double saturation = std::clamp(color.saturation, 0.0, 1.0);
  const double intensity = std::clamp(color.intensity, 0.0, 1.0);
  color_utils::HSL hsl;
  hsl.h = color.hue / 360.0;
  if (color.light) {
    hsl.s = saturation * (0.35 + 0.55 * intensity);
    hsl.l = 0.93 - 0.12 * intensity * saturation;
  } else {
    // Without color, Arc's own dark frame (#1B1B1B).
    hsl.s = saturation * (0.30 + 0.50 * intensity);
    hsl.l = 0.106 + 0.12 * intensity * saturation;
  }
  // Brightness moves it darker or lighter, staying a light or a dark frame.
  const double shift = (std::clamp(color.brightness, 0.0, 1.0) - 0.5) * 0.3;
  hsl.l = color.light ? std::clamp(hsl.l + shift, 0.62, 0.98)
                      : std::clamp(hsl.l + shift, 0.02, 0.40);
  return color_utils::HSLToSkColor(hsl, 255);
}

void ApplySpaceColor(views::Widget* widget,
                     const std::optional<SpaceColor>& color) {
  views::Widget* top = widget ? widget->GetTopLevelWidget() : nullptr;
  if (!top) {
    return;
  }
  void* tint = color ? reinterpret_cast<void*>(static_cast<uintptr_t>(
                           GetSpaceFrameColor(*color)))
                     : nullptr;
  std::optional<ui::ColorProviderKey::ColorMode> mode;
  if (color) {
    mode = color->light ? ui::ColorProviderKey::ColorMode::kLight
                        : ui::ColorProviderKey::ColorMode::kDark;
  }
  if (top->GetNativeWindowProperty(kSpaceTintWindowProperty) == tint &&
      top->color_mode_override() == mode) {
    return;
  }
  top->SetNativeWindowProperty(kSpaceTintWindowProperty, tint);
  top->SetColorModeOverride(mode);
  top->ThemeChanged();
}

namespace {

constexpr base::TimeDelta kSpaceColorTransitionDuration =
    base::Milliseconds(200);

// Sets the window's tint and theme mode, as ApplySpaceColor() does.
void SetSpaceTint(views::Widget* top,
                  std::optional<SkColor> tint,
                  std::optional<ui::ColorProviderKey::ColorMode> mode) {
  void* value = tint ? reinterpret_cast<void*>(static_cast<uintptr_t>(*tint))
                     : nullptr;
  if (top->GetNativeWindowProperty(kSpaceTintWindowProperty) == value &&
      top->color_mode_override() == mode) {
    return;
  }
  top->SetNativeWindowProperty(kSpaceTintWindowProperty, value);
  top->SetColorModeOverride(mode);
  top->ThemeChanged();
}

}  // namespace

SpaceColorTransition::SpaceColorTransition()
    : animation_(kSpaceColorTransitionDuration,
                 gfx::LinearAnimation::kDefaultFrameRate,
                 this) {}

SpaceColorTransition::~SpaceColorTransition() = default;

void SpaceColorTransition::AnimateTo(views::Widget* widget,
                                     const std::optional<SpaceColor>& color,
                                     bool animate) {
  views::Widget* top = widget ? widget->GetTopLevelWidget() : nullptr;
  if (!top || !top->GetColorProvider()) {
    return;
  }
  if (has_target_ && target_ == color && widget_ == top) {
    return;
  }
  const bool tinted = !!top->GetNativeWindowProperty(kSpaceTintWindowProperty);
  const SkColor current =
      top->GetColorProvider()->GetColor(ui::kColorFrameActive);
  if (!tinted && !animation_.is_animating()) {
    default_frame_ = current;
  }
  widget_ = top;
  target_ = color;
  has_target_ = true;
  const std::optional<SkColor> to =
      color ? std::optional<SkColor>(GetSpaceFrameColor(*color))
            : default_frame_;
  if (!animate || !to || !top->IsVisible() ||
      !gfx::Animation::ShouldRenderRichAnimation()) {
    animation_.Stop();
    ApplySpaceColor(top, color);
    return;
  }
  from_ = current;
  to_ = *to;
  last_step_ = -1;
  animation_.Start();
}

void SpaceColorTransition::AnimationProgressed(
    const gfx::Animation* animation) {
  if (!widget_) {
    return;
  }
  // Each step re-themes the whole window, so a few fixed ones: the same
  // colors every time between two Spaces, which the color providers keep.
  constexpr int kSteps = 6;
  const int step = static_cast<int>(
      std::lround(gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                             animation->GetCurrentValue()) *
                  kSteps));
  if (step == last_step_ || step >= kSteps) {
    return;
  }
  last_step_ = step;
  SetSpaceTint(widget_,
               gfx::Tween::ColorValueBetween(double(step) / kSteps, from_, to_),
               widget_->color_mode_override());
}

void SpaceColorTransition::AnimationEnded(const gfx::Animation* animation) {
  if (widget_) {
    ApplySpaceColor(widget_, target_);
  }
}

SkColor SidebarInk(const views::View* view, SkColor dark_color) {
  const ui::ColorProvider* provider = view ? view->GetColorProvider() : nullptr;
  if (!provider) {
    return dark_color;
  }
  if (color_utils::IsDark(provider->GetColor(ui::kColorFrameActive))) {
    return InkOverFrame(view, dark_color);
  }
  return InkOverFrame(
      view, SkColorSetARGB(SkColorGetA(dark_color), 255 - SkColorGetR(dark_color),
                           255 - SkColorGetG(dark_color),
                           255 - SkColorGetB(dark_color)));
}

SkColor SidebarInk(const views::View* view, ui::ColorId id) {
  const ui::ColorProvider* provider = view ? view->GetColorProvider() : nullptr;
  // An inactive window's frame is lighter on macOS; the shade follows it.
  return provider ? InkOverFrame(view, provider->GetColor(id))
                  : gfx::kPlaceholderColor;
}

std::unique_ptr<views::View> CreateSpaceColorPicker(
    const SpaceColor& initial,
    SpaceColorCallback callback) {
  return std::make_unique<ColorPickerView>(initial, std::move(callback));
}

}  // namespace alerse
