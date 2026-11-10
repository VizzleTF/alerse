// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/alerse_window_mac.h"

#import <AppKit/AppKit.h>

#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

// The window server's background blur, as apps with vibrant windows use it.
using AlerseCGSConnectionID = int;
extern "C" AlerseCGSConnectionID CGSMainConnectionID(void);
extern "C" CGError CGSSetWindowBackgroundBlurRadius(AlerseCGSConnectionID cid,
                                                    NSInteger window_number,
                                                    int radius);

namespace alerse {

namespace {
// Arc's picker blurs strongly: its sidebar's icons show as soft blots.
constexpr int kPopoverBlurRadius = 20;
}  // namespace

void MarkPopoverWindow(views::Widget* popover) {
  NSWindow* window = popover->GetNativeWindow().GetNativeNSWindow();
  window.identifier = @"AlersePopover";
}

void RemoveWindowShadow(views::Widget* widget) {
  if (widget) {
    widget->GetNativeWindow().GetNativeNSWindow().hasShadow = NO;
  }
}

void BlurBehindPopover(views::Widget* popover) {
  if (!popover) {
    return;
  }
  // The window server's blur follows the window's painted shape, so the
  // rounded corners and the arrow stay clear; an NSVisualEffectView's material
  // tints far more than Arc's panel does (its #565656 at half strength over
  // the blur: #2B over black, #42 over the page).
  CGSSetWindowBackgroundBlurRadius(
      CGSMainConnectionID(),
      popover->GetNativeWindow().GetNativeNSWindow().windowNumber,
      kPopoverBlurRadius);
}

}  // namespace alerse
