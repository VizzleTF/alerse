// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ARCOMIUM_SPLIT_H_
#define CHROME_BROWSER_UI_TABS_ARCOMIUM_SPLIT_H_

// Arc's Split View commands on Chromium's split tabs, which hold two panes
// (Arc's hold up to four): Close this Split Pane, Separate Page from Split
// View, Expand Current Split and Switch to Split View 1 / 2. Add Split View
// is Chromium's IDC_NEW_SPLIT_TAB.

#include <vector>

#include "base/i18n/rtl.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace arcomium {

// The share of the window an expanded pane takes; the other keeps
// Chromium's minimum (MultiContentsView clamps it to 10%, at most 200 px).
inline constexpr double kExpandedSplitRatio = 0.9;

// The shown tab when it is one of a split's two panes, else nullptr.
inline tabs::TabInterface* GetActiveSplitTab(TabStripModel* model) {
  tabs::TabInterface* const tab = model ? model->GetActiveTab() : nullptr;
  if (!tab || !tab->IsSplit() || !model->GetSplitData(*tab->GetSplit()) ||
      model->GetSplitData(*tab->GetSplit())->ListTabs().size() != 2) {
    return nullptr;
  }
  return tab;
}

// Whether `contents` is the pane a new split waits to fill: Chromium's
// "Choose a tab" page, which Add Split View opens beside the page.
inline bool IsSplitChooser(content::WebContents* contents) {
  if (!contents) {
    return false;
  }
  const GURL& url = contents->GetVisibleURL();
  const GURL chooser(chrome::kChromeUISplitViewNewTabPageURL);
  return url.SchemeIs(chooser.scheme()) && url.host() == chooser.host() &&
         url.path() == chooser.path();
}

// Whether the shown pane is expanded, so Expand Current Split evens them.
inline bool IsActiveSplitExpanded(TabStripModel* model) {
  tabs::TabInterface* const tab = GetActiveSplitTab(model);
  if (!tab) {
    return false;
  }
  split_tabs::SplitTabData* const data = model->GetSplitData(*tab->GetSplit());
  const double ratio = data->visual_data()->split_ratio();
  return data->ListTabs()[0] == tab ? ratio > 0.55 : ratio < 0.45;
}

inline void RunSplitCommand(TabStripModel* model, int command_id) {
  tabs::TabInterface* const tab = GetActiveSplitTab(model);
  if (!tab) {
    return;
  }
  const split_tabs::SplitTabId split = *tab->GetSplit();
  const std::vector<tabs::TabInterface*> panes =
      model->GetSplitData(split)->ListTabs();
  switch (command_id) {
    case IDC_ARCOMIUM_CLOSE_SPLIT_PANE:
      // As the split's own Close: the other pane fills the window, and
      // Reopen Closed Tab brings the closed one back.
      model->CloseWebContentsAt(
          model->GetIndexOfTab(tab),
          TabCloseTypes::CLOSE_USER_GESTURE |
              TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
      break;
    case IDC_ARCOMIUM_SEPARATE_SPLIT:
      // With two panes, taking one out leaves two separate tabs.
      model->RemoveSplit(split);
      break;
    case IDC_ARCOMIUM_EXPAND_SPLIT: {
      const bool expanded = IsActiveSplitExpanded(model);
      const bool start = panes[0] == tab;
      model->UpdateSplitRatio(
          split, expanded ? 0.5
                          : (start ? kExpandedSplitRatio
                                   : 1.0 - kExpandedSplitRatio));
      break;
    }
    case IDC_ARCOMIUM_FOCUS_SPLIT_1:
    case IDC_ARCOMIUM_FOCUS_SPLIT_2: {
      // Panes count from the left, as Arc's; the split lists them from its
      // start, which is the right in a right-to-left layout.
      size_t pane = command_id == IDC_ARCOMIUM_FOCUS_SPLIT_1 ? 0 : 1;
      if (base::i18n::IsRTL()) {
        pane = 1 - pane;
      }
      if (panes[pane] != tab) {
        model->ActivateTabAt(model->GetIndexOfTab(panes[pane]));
      }
      break;
    }
  }
}

}  // namespace arcomium

#endif  // CHROME_BROWSER_UI_TABS_ARCOMIUM_SPLIT_H_
