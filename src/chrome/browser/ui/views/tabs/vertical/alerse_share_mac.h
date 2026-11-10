// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SHARE_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SHARE_MAC_H_

#include <cstddef>
#include <string>
#include <vector>

#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace views {
class Widget;
}  // namespace views

// Alerse: Arc for macOS' Share in the sidebar's menus, on the system's
// sharing services (as Chromium's File > Share).
namespace alerse {

struct ShareService {
  std::u16string title;
  ui::ImageModel icon;
};

// The services that take a link (AirDrop, Mail, Messages, Notes, ...), as
// Arc's Share submenu lists them. ShareWithService's index refers to the
// list of the last call.
std::vector<ShareService> GetShareServices();

// Shares `urls`, with `title` as the subject, through the service at
// `index` of the last GetShareServices().
void ShareWithService(size_t index,
                      const std::vector<GURL>& urls,
                      const std::u16string& title);

// Arc's Share Folder...: the system's share picker for `urls`, beside
// `anchor_in_screen` in `widget`'s window.
void ShowSharePicker(const std::vector<GURL>& urls,
                     views::Widget* widget,
                     const gfx::Rect& anchor_in_screen);

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_SHARE_MAC_H_
