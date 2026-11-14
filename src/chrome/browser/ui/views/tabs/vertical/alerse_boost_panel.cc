// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The Boost panel: chrome://alerse-boost/?tab=<handle> in a bubble that
// floats over the right side of the page, where Arc's boost editor stands.
// The page stays live under it, so each change shows at once.

#include <algorithm>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/alerse_boost.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_motion.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace alerse {

namespace {

constexpr int kWidth = 340;
constexpr int kMaxHeight = 640;
constexpr int kInset = 12;

struct Shown {
  base::WeakPtr<views::Widget> widget;
  int tab = 0;
};

Shown& CurrentPanel() {
  static base::NoDestructor<Shown> shown;
  return *shown;
}

class BoostBubble : public views::BubbleDialogDelegate {
 public:
  explicit BoostBubble(views::View* contents_area)
      : views::BubbleDialogDelegate(contents_area,
                                    views::BubbleBorder::TOP_RIGHT) {}

  // views::BubbleDialogDelegate: the panel's top right corner sits inside
  // the page's, as Arc's editor does.
  gfx::Rect GetAnchorRect() const override {
    const gfx::Rect area = views::BubbleDialogDelegate::GetAnchorRect();
    return gfx::Rect(area.right() - kInset, area.y() + kInset, 0, 0);
  }
};

}  // namespace

void ShowBoostPanel(BrowserWindowInterface* browser) {
  if (!browser) {
    return;
  }
  tabs::TabInterface* tab = browser->GetActiveTabInterface();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!tab || !browser_view || !tab->GetContents() ||
      !tab->GetContents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }
  const int tab_id = tab->GetHandle().raw_value();
  Shown& shown = CurrentPanel();
  if (shown.widget) {
    const bool same = shown.tab == tab_id;
    shown.widget->Close();
    shown = Shown();
    if (same) {
      return;  // The command toggles the panel.
    }
  }

  views::View* area = browser_view->contents_container();
  const int height =
      std::clamp(area->height() - 2 * kInset, 240, kMaxHeight);
  auto web_view = std::make_unique<views::WebView>(browser->GetProfile());
  web_view->SetPreferredSize(gfx::Size(kWidth, height));
  web_view->LoadInitialURL(GURL("chrome://alerse-boost/?tab=" +
                                base::NumberToString(tab_id)));

  auto delegate = std::make_unique<BoostBubble>(area);
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetShowCloseButton(false);
  delegate->SetShowTitle(false);
  delegate->set_margins(gfx::Insets());
  delegate->set_close_on_deactivate(false);
  delegate->set_corner_radius(12);
  delegate->SetInitiallyFocusedView(web_view.get());
  delegate->SetContentsView(std::move(web_view));
  views::Widget* widget = views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(delegate), views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  shown.widget = widget->GetWeakPtr();
  shown.tab = tab_id;
  motion::FadeInWidget(widget, /*activate=*/true);
}

}  // namespace alerse
