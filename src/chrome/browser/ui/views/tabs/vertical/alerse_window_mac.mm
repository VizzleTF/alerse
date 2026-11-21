// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/alerse_window_mac.h"

#import <AppKit/AppKit.h>
#include <unistd.h>

#include <vector>

#include "base/apple/bridging.h"
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

struct ScreenWindow {
  NSInteger number;
  bool own;
};

// The windows on screen in the normal layer, front to back.
std::vector<ScreenWindow> NormalWindowsOnScreen() {
  std::vector<ScreenWindow> windows;
  NSArray* list = base::apple::CFToNSOwnershipCast(CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID));
  const pid_t pid = getpid();
  for (NSDictionary* info in list) {
    if ([info[(__bridge id)kCGWindowLayer] intValue] != 0) {
      continue;
    }
    windows.push_back({[info[(__bridge id)kCGWindowNumber] integerValue],
                       [info[(__bridge id)kCGWindowOwnerPID] intValue] == pid});
  }
  return windows;
}

// Puts each of the app's windows back right below the window it was below
// in `before`; those in front of every other app's window stay in front.
void RestoreOrder(const std::vector<ScreenWindow>& before) {
  NSInteger above = 0;
  for (const ScreenWindow& entry : before) {
    if (!entry.own) {
      above = entry.number;
      continue;
    }
    NSWindow* window = [NSApp windowWithWindowNumber:entry.number];
    // A child window moves with its parent.
    if (!window || window.parentWindow || !above) {
      continue;
    }
    [window orderWindow:NSWindowBelow relativeTo:above];
    above = entry.number;
  }
}

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

void ShowAndActivateAlone(views::Widget* window) {
  const std::vector<ScreenWindow> before = NormalWindowsOnScreen();
  window->Show();
  window->Activate();
  RestoreOrder(before);
  // The window server may grant the activation later; it raises the main
  // window again then.
  NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
  __block id observer = [center
      addObserverForName:NSApplicationDidBecomeActiveNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                [center removeObserver:observer];
                observer = nil;
                RestoreOrder(before);
              }];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC),
                 dispatch_get_main_queue(), ^{
                   if (observer) {
                     [center removeObserver:observer];
                     observer = nil;
                   }
                 });
}

}  // namespace alerse
