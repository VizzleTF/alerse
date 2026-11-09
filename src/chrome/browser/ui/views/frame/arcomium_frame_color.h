// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_ARCOMIUM_FRAME_COLOR_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_ARCOMIUM_FRAME_COLOR_H_

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace arcomium {

#if BUILDFLAG(IS_MAC)
// The hidden sidebar floating over the page is Arc's darker #0D0D0D panel:
// what it holds is shaded off that, not off the frame. The window property
// is set (to a non-null mark) while it floats, by VerticalTabStripRegionView.
inline constexpr char kFloatingSidebarProperty[] = "arcomium-floating-sidebar";
inline constexpr SkColor kFloatingSidebarColor = SkColorSetRGB(0x0D, 0x0D, 0x0D);

inline bool InFloatingSidebar(const views::View* view) {
  views::Widget* widget = const_cast<views::Widget*>(view->GetWidget());
  if (!widget || widget->parent() ||
      !widget->GetNativeWindowProperty(kFloatingSidebarProperty)) {
    return false;
  }
  for (const views::View* v = view; v; v = v->parent()) {
    if (v->GetClassName() == "VerticalTabStripRegionView") {
      return true;
    }
  }
  return false;
}
#endif

// The frame's color as the window paints it now. Arc for macOS lightens an
// inactive window (chrome_color_mixers.cc), and the shades drawn off the
// frame follow it; BrowserView repaints them on the change. Windows and
// Linux keep the active frame: Arc for Windows does not lighten, and there
// nothing repaints these views when the window turns inactive.
inline SkColor FrameColor(const views::View* view) {
#if BUILDFLAG(IS_MAC)
  const views::Widget* widget = view->GetWidget();
  const bool active = !widget || widget->ShouldPaintAsActive();
  if (active && InFloatingSidebar(view)) {
    return kFloatingSidebarColor;
  }
  return view->GetColorProvider()->GetColor(active ? ui::kColorFrameActive
                                                   : ui::kColorFrameInactive);
#else
  return view->GetColorProvider()->GetColor(ui::kColorFrameActive);
#endif
}

// `color`, a shade drawn for the active frame, as it shows over the frame the
// window paints now. Arc for macOS keeps each opaque shade's share of white
// over the frame it lightens (#808080 on #161616 is #929292 on #393939), and
// a shade darker than the frame its share of the way down to black. In the
// floating sidebar the same holds over its darker panel (#525252 on #161616
// is #4A4A4A on #0D0D0D, as Arc's dimmed arrows). Shades with alpha and those
// of child widgets (popups, tooltips: their own panels) stay as they are.
inline SkColor InkOverFrame(const views::View* view, SkColor color) {
#if BUILDFLAG(IS_MAC)
  const views::Widget* widget = view->GetWidget();
  const ui::ColorProvider* provider = view->GetColorProvider();
  if (!widget || widget->parent() || !provider ||
      SkColorGetA(color) != SK_AlphaOPAQUE) {
    return color;
  }
  const bool active = widget->ShouldPaintAsActive();
  const bool floating = active && InFloatingSidebar(view);
  if (active && !floating) {
    return color;
  }
  const SkColor from = provider->GetColor(ui::kColorFrameActive);
  const SkColor to = floating ? kFloatingSidebarColor
                              : provider->GetColor(ui::kColorFrameInactive);
  const auto channel = [](int c, int f, int t) {
    if (c >= f) {
      return f >= 255 ? c : t + (c - f) * (255 - t) / (255 - f);
    }
    return c * t / f;
  };
  return SkColorSetRGB(
      channel(SkColorGetR(color), SkColorGetR(from), SkColorGetR(to)),
      channel(SkColorGetG(color), SkColorGetG(from), SkColorGetG(to)),
      channel(SkColorGetB(color), SkColorGetB(from), SkColorGetB(to)));
#else
  return color;
#endif
}

// Whether `view` is in a browser window Arc for macOS would show lightened:
// another app or window has the keyboard. Always false elsewhere.
inline bool PaintsInactiveFrame(const views::View* view) {
#if BUILDFLAG(IS_MAC)
  const views::Widget* widget = view->GetWidget();
  return widget && !widget->parent() && !widget->ShouldPaintAsActive();
#else
  return false;
#endif
}

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_ARCOMIUM_FRAME_COLOR_H_
