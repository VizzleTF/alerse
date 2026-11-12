// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ALERSE_FIND_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_ALERSE_FIND_BAR_H_

// The find bar as Arc for macOS draws it (docs/design.md, "Find bar"): a
// 300x33 pt pill inside the page card with a magnifier and "Find in
// page...". find_bar_view.cc and find_bar_host.cc call in here
// (patches/look/find-bar-pill.patch); every function of the bar stays
// Chromium's.

#include <memory>

#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_motion.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace alerse {

// Arc's pill, measured at 1440x900: x 1120 to 1420, y 20 to 53 in a card
// whose top right corner is 1430,10.
inline constexpr int kFindPillWidth = 300;
inline constexpr int kFindPillHeight = 33;
inline constexpr float kFindPillRadius = 6.0f;
inline constexpr int kFindPillCardGap = 10;
// Room round the pill for its shadow, which falls mostly below it.
inline constexpr gfx::Insets kFindPillShadowInsets =
    gfx::Insets::TLBR(4, 8, 14, 8);
inline constexpr int kFindPillFontSize = 13;

// The pill and its shadow, inside the view's border.
class FindPillBackground : public views::Background {
 public:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(view->GetColorProvider()->GetColor(kColorFindBarBackground));
    flags.setLooper(gfx::CreateShadowDrawLooper({gfx::ShadowValue(
        gfx::Vector2d(0, 3), 14, SkColorSetA(SK_ColorBLACK, 0x40))}));
    canvas->DrawRoundRect(gfx::RectF(view->GetContentsBounds()),
                          kFindPillRadius, flags);
  }
};

inline gfx::Size FindPillSize() {
  return gfx::Size(kFindPillWidth + kFindPillShadowInsets.width(),
                   kFindPillHeight + kFindPillShadowInsets.height());
}

// The match counter and the arrows show once there is text to find.
inline void UpdateFindPill(const views::Textfield* text,
                           views::View* count,
                           views::View* previous,
                           views::View* next) {
  const bool has_text = !text->GetText().empty();
  count->SetVisible(has_text);
  previous->SetVisible(has_text);
  next->SetVisible(has_text);
}

// Restyles Chromium's find bar, built just before: `bar` holds `row`, which
// holds the other views in this order.
inline void StyleFindPill(views::BoxLayoutView* bar,
                          views::BoxLayoutView* row,
                          views::Textfield* text,
                          views::View* count,
                          views::View* separator,
                          views::ImageButton* previous,
                          views::ImageButton* next,
                          views::ImageButton* close) {
  bar->SetBorder(views::CreateEmptyBorder(kFindPillShadowInsets));
  bar->SetBackground(std::make_unique<FindPillBackground>());

  row->SetInsideBorderInsets(gfx::Insets::TLBR(0, 8, 0, 4));
  row->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  row->SetPreferredSize(gfx::Size(kFindPillWidth, kFindPillHeight));

  // Arc's magnifier, 13 pt of ink, in the placeholder's grey.
  auto* glass = row->AddChildViewAt(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kAlerseSearchIcon, kColorFindBarMatchCount, 18)),
      0);
  glass->SetCanProcessEventsWithinSubtree(false);
  glass->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 5));

  const gfx::FontList font;
  text->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  text->SetProperty(views::kMarginsKey, gfx::Insets());
  text->SetBackgroundEnabled(false);
  views::FocusRing::Remove(text);
  text->SetFontList(
      font.DeriveWithSizeDelta(kFindPillFontSize - font.GetFontSize()));
  text->SetTextColorId(kColorFindBarForeground);
  text->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_ALERSE_FIND_PLACEHOLDER));
  text->SetPlaceholderTextColorId(kColorFindBarMatchCount);

  count->SetProperty(views::kMarginsKey, gfx::Insets::VH(0, 6));
  // Arc's pill has no divider.
  separator->SetVisible(false);
  for (views::ImageButton* button : {previous, next, close}) {
    button->SetBorder(views::CreateEmptyBorder(gfx::Insets(4)));
    button->SetProperty(views::kMarginsKey, gfx::Insets());
  }
  UpdateFindPill(text, count, previous, next);
}

// How long the bar takes to show or hide. FindBarHost's own animation only
// times the hide; what is seen is the fade below.
inline base::TimeDelta FindPillFadeDuration() {
  return motion::Fade(motion::kQuick);
}

// Fades the bar's layer in from 4 pt above its place, or out where it is.
// `animate` false puts it in its shown state at once.
inline void FadeFindPill(views::View* clip, bool shown, bool animate) {
  ui::Layer* layer = clip ? clip->layer() : nullptr;
  if (!layer) {
    return;
  }
  if (!animate) {
    layer->GetAnimator()->AbortAllAnimations();
    layer->SetOpacity(1.0f);
    layer->SetTransform(gfx::Transform());
    return;
  }
  if (!shown) {
    views::AnimationBuilder()
        .SetPreemptionStrategy(motion::kRetarget)
        .Once()
        .SetDuration(motion::Fade(motion::Exit(motion::kQuick)))
        .SetOpacity(layer, 0.0f, motion::kExit);
    return;
  }
  layer->GetAnimator()->AbortAllAnimations();
  layer->SetOpacity(0.0f);
  if (!motion::Move(motion::kQuick).is_zero()) {
    layer->SetTransform(gfx::Transform::MakeTranslation(0, -4));
  }
  views::AnimationBuilder()
      .SetPreemptionStrategy(motion::kRetarget)
      .Once()
      .SetDuration(FindPillFadeDuration())
      .SetTransform(layer, gfx::Transform(), motion::kEnter)
      .SetOpacity(layer, 1.0f, motion::kEnter);
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_VIEWS_ALERSE_FIND_BAR_H_
