// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/tabs/vertical/arcomium_share_mac.h"

#import <AppKit/AppKit.h>

#include <string>

#include "base/strings/escape.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/hang_watcher.h"
#include "base/time/time.h"
#include "chrome/browser/platform_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/widget/widget.h"

// Private, as Chromium's share menu uses it to tell services apart.
@interface NSSharingService (ArcomiumName)
@property(readonly) NSString* name;
@end

namespace arcomium {
namespace {

// The services GetShareServices() listed, and when: the system enumerates
// its sharing plugins from disk, so a menu opened again soon reuses them.
NSArray<NSSharingService*>* __strong& Services() {
  static NSArray<NSSharingService*>* __strong services;
  return services;
}
base::TimeTicks& ServicesTime() {
  static base::TimeTicks time;
  return time;
}

// Keeps the picker alive while it shows.
NSSharingServicePicker* __strong& Picker() {
  static NSSharingServicePicker* __strong picker;
  return picker;
}

NSArray* ToItems(const std::vector<GURL>& urls) {
  NSMutableArray* items = [NSMutableArray array];
  for (const GURL& url : urls) {
    if (!url.is_valid()) {
      continue;
    }
    if (NSURL* item = [NSURL URLWithString:base::SysUTF8ToNSString(url.spec())]) {
      [items addObject:item];
    }
  }
  return items;
}

}  // namespace

std::vector<ShareService> GetShareServices() {
  if (!Services() || base::TimeTicks::Now() - ServicesTime() > base::Minutes(1)) {
    // As Chromium's share menu: listing the services may block on the disk,
    // which is not a hang.
    base::HangWatcher::InvalidateActiveExpectations();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // Deprecated in macOS 13, but its replacement is a picker, not a menu
    // (Chromium's share menu keeps it for the same reason).
    NSArray<NSSharingService*>* all = [NSSharingService
        sharingServicesForItems:@[ [NSURL URLWithString:@"https://example.com/"] ]];
#pragma clang diagnostic pop
    NSMutableArray<NSSharingService*>* kept = [NSMutableArray array];
    for (NSSharingService* service in all) {
      // Not Safari's Reading List, as in Chromium's menu.
      if (![service.name
              isEqualToString:NSSharingServiceNameAddToSafariReadingList]) {
        [kept addObject:service];
      }
    }
    Services() = kept;
    ServicesTime() = base::TimeTicks::Now();
  }
  std::vector<ShareService> services;
  for (NSSharingService* service in Services()) {
    ShareService entry;
    entry.title = base::SysNSStringToUTF16(service.menuItemTitle);
    if (NSImage* image = [service.image copy]) {
      image.size = NSMakeSize(16, 16);
      entry.icon = ui::ImageModel::FromImage(gfx::Image(image));
    }
    services.push_back(std::move(entry));
  }
  return services;
}

void ShareWithService(size_t index,
                      const std::vector<GURL>& urls,
                      const std::u16string& title) {
  NSArray<NSSharingService*>* services = Services();
  NSArray* items = ToItems(urls);
  if (index >= services.count || !items.count) {
    return;
  }
  NSSharingService* service = services[index];
  if ([service.name isEqualToString:NSSharingServiceNameComposeEmail]) {
    // Mail by a mailto: link, as Chromium's share menu does: the service
    // crashes it (https://crbug.com/356643975).
    std::string body;
    for (const GURL& url : urls) {
      body += base::EscapeQueryParamValue(url.spec(), false) + "%0A";
    }
    platform_util::OpenExternal(GURL(
        "mailto:?subject=" +
        base::EscapeQueryParamValue(base::UTF16ToUTF8(title), false) +
        "&body=" + body));
    return;
  }
  service.subject = base::SysUTF16ToNSString(title);
  [service performWithItems:items];
}

void ShowSharePicker(const std::vector<GURL>& urls,
                     views::Widget* widget,
                     const gfx::Rect& anchor_in_screen) {
  NSArray* items = ToItems(urls);
  if (!items.count || !widget) {
    return;
  }
  NSWindow* window = widget->GetNativeWindow().GetNativeNSWindow();
  NSView* view = window.contentView;
  if (!view) {
    return;
  }
  const NSRect in_window =
      [window convertRectFromScreen:gfx::ScreenRectToNSRect(anchor_in_screen)];
  Picker() = [[NSSharingServicePicker alloc] initWithItems:items];
  [Picker() showRelativeToRect:[view convertRect:in_window fromView:nil]
                        ofView:view
                 preferredEdge:NSRectEdgeMaxX];
}

}  // namespace arcomium
