// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_SPACES_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_SPACES_H_

// What a tab carries for the sidebar. A Space is a Chromium profile
// (docs/adr/0001-space-profiles.md): the list of Spaces is
// alerse_space_registry.h, their windows are alerse_window_stack.h.

#include <memory>
#include <string>
#include <utility>

#include "base/supports_user_data.h"
#include "content/public/browser/web_contents.h"

namespace alerse {

// Tab extra data: uuid of the sidebar's pinned item (a bookmark) the tab is
// bound to, "" once unbound. A uuid, unlike a node id, survives sync.
inline constexpr char kPinnedTabExtraDataKey[] = "arcomium.pinned";

// The pinned item uuid on a tab's contents, so a rewrite of the session file
// keeps it and a restored tab binds to its item again.
struct PinnedTabData : public base::SupportsUserData::Data {
  explicit PinnedTabData(std::string uuid) : uuid(std::move(uuid)) {}
  std::string uuid;
};

inline std::string GetPinnedTabUuid(const content::WebContents* contents) {
  const auto* data = static_cast<const PinnedTabData*>(
      contents->GetUserData(kPinnedTabExtraDataKey));
  return data ? data->uuid : std::string();
}

inline void SetPinnedTabUuid(content::WebContents* contents,
                             const std::string& uuid) {
  if (uuid.empty()) {
    contents->RemoveUserData(kPinnedTabExtraDataKey);
  } else {
    contents->SetUserData(kPinnedTabExtraDataKey,
                          std::make_unique<PinnedTabData>(uuid));
  }
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_SPACES_H_
