// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_MOTION_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_MOTION_H_

// Alerse's motion system (docs/design.md, "Motion"): every duration, curve
// and reduced-motion decision of the sidebar and its popups comes from here.
// Animations run on compositor layers (transform, opacity) and retarget from
// where the layer is drawn when interrupted.

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace alerse::motion {

// Something appearing in place: opacity only.
inline constexpr base::TimeDelta kQuick = base::Milliseconds(90);
// Movement inside the sidebar.
inline constexpr base::TimeDelta kStandard = base::Milliseconds(160);
// A surface that moves over or beside the page. Chromium's files cannot
// include this header's users, so tab_strip_animations.cc repeats the
// numbers (patches/look/sidebar-motion.patch).
inline constexpr base::TimeDelta kEmphasized = base::Milliseconds(240);

// Arriving or answering a click (fastest in its first frame), leaving, and
// a large surface (the sidebar itself, sidebar-motion.patch).
inline constexpr gfx::Tween::Type kEnter = gfx::Tween::LINEAR_OUT_SLOW_IN;
inline constexpr gfx::Tween::Type kExit = gfx::Tween::FAST_OUT_LINEAR_IN;
inline constexpr gfx::Tween::Type kLarge = gfx::Tween::ACCEL_20_DECEL_100;

// A new target starts from where the layer is drawn now: nothing queues.
inline constexpr auto kRetarget =
    ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET;

// Movement stops where the system asks for reduced motion.
inline base::TimeDelta Move(base::TimeDelta duration) {
  return gfx::Animation::PrefersReducedMotion() ? base::TimeDelta() : duration;
}

// Fades stay under reduced motion, no longer than kQuick.
inline base::TimeDelta Fade(base::TimeDelta duration) {
  return gfx::Animation::PrefersReducedMotion() ? std::min(duration, kQuick)
                                                : duration;
}

// Leaving is shorter than arriving.
inline base::TimeDelta Exit(base::TimeDelta duration) {
  return duration * 0.7;
}

// A move takes longer the further it goes.
inline base::TimeDelta ForDistance(int px) {
  return Move(std::clamp(base::Milliseconds(110 + 0.18 * std::abs(px)),
                         base::Milliseconds(140), base::Milliseconds(260)));
}

// Shows `widget` fading in. Opacity only: macOS clips a root layer's
// transform to the window.
inline void FadeInWidget(views::Widget* widget, bool activate) {
  ui::Layer* layer = widget->GetLayer();
  if (layer) {
    layer->SetOpacity(0.0f);
  }
  if (activate) {
    widget->Show();
  } else {
    widget->ShowInactive();
  }
  if (!layer) {
    return;
  }
  views::AnimationBuilder()
      .SetPreemptionStrategy(kRetarget)
      .Once()
      .SetDuration(Fade(kQuick))
      .SetOpacity(layer, 1.0f, kEnter);
}

// Fades `widget` out, then runs `done` with it: once, in a task of its own,
// also when the fade is cut short. A widget that has the keyboard hides at
// once: hidden later, it would take the focus back from what the user's pick
// just opened.
inline void FadeOutWidget(
    std::unique_ptr<views::Widget> widget,
    base::OnceCallback<void(std::unique_ptr<views::Widget>)> done) {
  ui::Layer* layer = widget->IsClosed() ? nullptr : widget->GetLayer();
  const bool fade = layer && widget->IsVisible() && !widget->IsActive();
  auto [ended, aborted] = base::SplitOnceCallback(base::BindOnce(
      [](std::unique_ptr<views::Widget> widget,
         base::OnceCallback<void(std::unique_ptr<views::Widget>)> done) {
        if (!widget->IsClosed()) {
          widget->Hide();
        }
        // Not inside the animator's own callback.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(done), std::move(widget)));
      },
      std::move(widget), std::move(done)));
  if (!fade) {
    std::move(ended).Run();
    return;
  }
  views::AnimationBuilder()
      .SetPreemptionStrategy(kRetarget)
      .OnEnded(std::move(ended))
      .OnAborted(std::move(aborted))
      .Once()
      .SetDuration(Fade(Exit(kQuick)))
      .SetOpacity(layer, 0.0f, kExit);
}

// Fades out and deletes a widget its caller owns.
inline void FadeOutAndDelete(std::unique_ptr<views::Widget> widget) {
  FadeOutWidget(std::move(widget), base::DoNothing());
}

// Fades a view that paints to a layer in or out; it stays visible, and laid
// out, until the fade out ends.
inline void SetShown(views::View* view, bool shown) {
  ui::Layer* layer = view->layer();
  if (!layer) {
    view->SetVisible(shown);
    return;
  }
  if (shown && !view->GetVisible()) {
    layer->SetOpacity(0.0f);
    view->SetVisible(true);
  }
  if (!view->GetVisible() ||
      layer->GetTargetOpacity() == (shown ? 1.0f : 0.0f)) {
    return;
  }
  views::AnimationBuilder builder;
  builder.SetPreemptionStrategy(kRetarget);
  if (!shown) {
    // Not OnAborted: a fade in that takes over keeps the view visible.
    builder.OnEnded(base::BindOnce(
        [](std::unique_ptr<views::ViewTracker> tracker) {
          if (tracker->view()) {
            tracker->view()->SetVisible(false);
          }
        },
        std::make_unique<views::ViewTracker>(view)));
  }
  builder.Once()
      .SetDuration(Fade(shown ? kQuick : Exit(kQuick)))
      .SetOpacity(layer, shown ? 1.0f : 0.0f, shown ? kEnter : kExit);
}

// Brings a view that paints to a layer in from `offset` away, fading in.
inline void Arrive(views::View* view,
                   const gfx::Vector2d& offset,
                   base::TimeDelta duration) {
  ui::Layer* layer = view->layer();
  if (!layer) {
    return;
  }
  layer->GetAnimator()->AbortAllAnimations();
  layer->SetOpacity(0.0f);
  // Reduced motion: the fade alone.
  if (!Move(duration).is_zero()) {
    layer->SetTransform(
        gfx::Transform::MakeTranslation(offset.x(), offset.y()));
  }
  views::AnimationBuilder()
      .SetPreemptionStrategy(kRetarget)
      .Once()
      .SetDuration(Fade(duration))
      .SetTransform(layer, gfx::Transform(), kEnter)
      .SetOpacity(layer, 1.0f, kEnter);
}

}  // namespace alerse::motion

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_MOTION_H_
