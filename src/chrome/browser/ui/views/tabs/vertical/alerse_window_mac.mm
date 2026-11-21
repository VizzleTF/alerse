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

// When the app last became active (WatchAppActivation()), in seconds of
// uptime.
NSTimeInterval g_became_active_at = -1;

// Launch Services activates the app before the link's Little window opens.
constexpr NSTimeInterval kLinkActivationSeconds = 2;

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

// Puts the app's windows in front of `front`, another app's window, right
// below it in their order, all but `keep`.
void SendBehind(NSWindow* keep, NSInteger front) {
  std::vector<NSWindow*> raised;
  for (const ScreenWindow& entry : NormalWindowsOnScreen()) {
    if (entry.number == front) {
      for (auto it = raised.rbegin(); it != raised.rend(); ++it) {
        [*it orderWindow:NSWindowBelow relativeTo:front];
      }
      return;
    }
    NSWindow* window =
        entry.own ? [NSApp windowWithWindowNumber:entry.number] : nil;
    // A child window moves with its parent.
    if (window && window != keep && !window.parentWindow) {
      raised.push_back(window);
    }
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

void WatchAppActivation() {
  static bool watching = false;
  if (watching) {
    return;
  }
  watching = true;
  [NSNotificationCenter.defaultCenter
      addObserverForName:NSApplicationDidBecomeActiveNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                g_became_active_at = NSProcessInfo.processInfo.systemUptime;
              }];
}

void ShowAndActivateAlone(views::Widget* window) {
  NSWindow* little = window->GetNativeWindow().GetNativeNSWindow();
  const bool active = NSApp.active;
  NSInteger front = 0;
  if (!active || NSProcessInfo.processInfo.systemUptime - g_became_active_at <
                     kLinkActivationSeconds) {
    for (const ScreenWindow& entry : NormalWindowsOnScreen()) {
      if (!entry.own) {
        front = entry.number;
        break;
      }
    }
  }
  window->Show();
  window->Activate();
  if (!front) {
    return;
  }
  SendBehind(little, front);
  // The activation's raise may land after the notification, so again on the
  // next turn.
  void (^send_behind)(void) = ^{
    SendBehind(little, front);
    dispatch_async(dispatch_get_main_queue(), ^{
      SendBehind(little, front);
    });
  };
  if (active) {
    dispatch_async(dispatch_get_main_queue(), ^{
      SendBehind(little, front);
    });
    return;
  }
  NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
  __block id observer = [center
      addObserverForName:NSApplicationDidBecomeActiveNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                [center removeObserver:observer];
                observer = nil;
                send_behind();
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
