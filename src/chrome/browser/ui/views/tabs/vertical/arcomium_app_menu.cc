// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/arcomium_app_menu.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/arcomium_update/arcomium_update_checker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_space_switcher.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace arcomium {

namespace {

// Commands of this menu that are not the browser's own.
enum Command {
  kCheckForUpdates = 1,
  kNewSpace,
  kNextSpace,
  kPreviousSpace,
  kMore,
};

ArcomiumSpaceSwitcher* FindSpaceSwitcher(views::View* view) {
  if (!view) {
    return nullptr;
  }
  if (auto* switcher = views::AsViewClass<ArcomiumSpaceSwitcher>(view)) {
    return switcher;
  }
  for (views::View* child : view->children()) {
    if (ArcomiumSpaceSwitcher* switcher = FindSpaceSwitcher(child)) {
      return switcher;
    }
  }
  return nullptr;
}

class AppMenu : public ui::SimpleMenuModel::Delegate {
 public:
  explicit AppMenu(BrowserWindowInterface* browser)
      : browser_(browser),
        root_(this),
        spaces_(this),
        tabs_(this),
        extensions_(this),
        developer_(this) {
    // A newer release, when the update check found one, takes the place of
    // "Check for Updates", highlighted.
    const std::string update =
        GetAvailableUpdate(g_browser_process->local_state());
    if (update.empty()) {
      root_.AddItem(kCheckForUpdates,
                    l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_CHECK_UPDATES));
    } else {
      root_.AddHighlightedItemWithIcon(
          IDC_ARCOMIUM_UPDATE,
          l10n_util::GetStringFUTF16(IDS_ARCOMIUM_MENU_UPDATE_TO,
                                     base::UTF8ToUTF16(update)),
          ui::ImageModel());
    }
    root_.AddItem(IDC_ARCOMIUM_IMPORT,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_IMPORT));
    root_.AddSeparator(ui::NORMAL_SEPARATOR);
    root_.AddItem(IDC_NEW_TAB, l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_TAB));
    root_.AddItem(IDC_NEW_WINDOW,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_NEW_WINDOW));
    root_.AddItem(IDC_NEW_INCOGNITO_WINDOW,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_NEW_INCOGNITO));
    root_.AddSeparator(ui::NORMAL_SEPARATOR);

    spaces_.AddItem(kNewSpace, l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_SPACE));
    spaces_.AddItem(kNextSpace,
                    l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_NEXT_SPACE));
    spaces_.AddItem(kPreviousSpace,
                    l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_PREVIOUS_SPACE));
    root_.AddSubMenu(0, l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_SPACES),
                     &spaces_);

    tabs_.AddItem(IDC_FOCUS_LOCATION,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_COMMAND_BAR));
    tabs_.AddItem(IDC_NEW_SPLIT_TAB,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_SPLIT_VIEW));
    tabs_.AddItem(IDC_SELECT_NEXT_TAB,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_NEXT_TAB));
    tabs_.AddItem(IDC_SELECT_PREVIOUS_TAB,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_PREVIOUS_TAB));
    tabs_.AddItem(IDC_RESTORE_TAB,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_REOPEN_TAB));
    root_.AddSubMenu(0, l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_TABS),
                     &tabs_);

    root_.AddItem(IDC_SHOW_DOWNLOADS,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_DOWNLOADS));
    root_.AddItem(IDC_SHOW_HISTORY,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_HISTORY));
    root_.AddItem(IDC_SHOW_BOOKMARK_MANAGER,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_BOOKMARKS));

    extensions_.AddItem(IDC_MANAGE_EXTENSIONS,
                        l10n_util::GetStringUTF16(IDS_ARCOMIUM_MANAGE_EXTENSIONS));
    root_.AddSubMenu(0, l10n_util::GetStringUTF16(IDS_ARCOMIUM_EXTENSIONS),
                     &extensions_);

    developer_.AddItem(IDC_VIEW_SOURCE,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_VIEW_SOURCE));
    developer_.AddItem(IDC_DEV_TOOLS,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_DEV_TOOLS));
    developer_.AddItem(IDC_DEV_TOOLS_INSPECT,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_INSPECT));
    developer_.AddItem(IDC_DEV_TOOLS_CONSOLE,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_CONSOLE));
    developer_.AddItem(IDC_TASK_MANAGER_MAIN_MENU,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_TASK_MANAGER));
    root_.AddSubMenu(0, l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_DEVELOPER),
                     &developer_);
    root_.AddSeparator(ui::NORMAL_SEPARATOR);

    root_.AddItem(IDC_SHOW_SYNC_SETTINGS,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_SYNC));
    root_.AddItem(IDC_EDIT_SEARCH_ENGINES,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_SEARCH_ENGINE));
    root_.AddItem(IDC_ARCOMIUM_LINK_ROUTING,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_LINK_ROUTING));
    root_.AddItem(IDC_OPTIONS,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_SETTINGS));
    root_.AddItem(IDC_PRINT, l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_PRINT));
    root_.AddSeparator(ui::NORMAL_SEPARATOR);
    root_.AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_ABOUT));
    root_.AddItem(kMore, l10n_util::GetStringUTF16(IDS_ARCOMIUM_MORE));
    root_.AddItem(IDC_CLOSE_WINDOW,
                  l10n_util::GetStringUTF16(IDS_ARCOMIUM_MENU_CLOSE_WINDOW));
  }

  AppMenu(const AppMenu&) = delete;
  AppMenu& operator=(const AppMenu&) = delete;
  ~AppMenu() override = default;

  void RunAt(views::View* anchor) {
    runner_ = std::make_unique<views::MenuRunner>(
        &root_, views::MenuRunner::HAS_MNEMONICS,
        base::BindRepeating(&AppMenu::OnMenuClosed, base::Unretained(this)));
    runner_->RunMenuAt(anchor->GetWidget(), nullptr, anchor->GetBoundsInScreen(),
                       views::MenuAnchorPosition::kTopLeft,
                       ui::mojom::MenuSourceType::kMouse);
  }

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override {
    switch (command_id) {
      case kCheckForUpdates:
      case kNewSpace:
      case kMore:
        return true;
      case kNextSpace:
      case kPreviousSpace: {
        ArcomiumSpaceSwitcher* switcher = GetSwitcher();
        return switcher && switcher->CanRotateSpace(command_id == kNextSpace);
      }
      default:
        return chrome::IsCommandEnabled(browser_, command_id);
    }
  }

  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser_);
    return view && view->GetAccelerator(command_id, accelerator);
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    // Run once the menu is gone: a command may close this window.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AppMenu::Run, browser_->GetWeakPtr(),
                                  command_id));
  }

 private:
  static void Run(base::WeakPtr<BrowserWindowInterface> browser,
                  int command_id) {
    if (!browser) {
      return;
    }
    switch (command_id) {
      case kCheckForUpdates:
        browser->OpenGURL(GetLatestReleaseUrl(),
                          WindowOpenDisposition::NEW_FOREGROUND_TAB);
        return;
      case kNewSpace:
        ArcomiumSpaceSwitcher::CreateSpace(browser.get());
        return;
      case kNextSpace:
      case kPreviousSpace:
        if (BrowserView* view =
                BrowserView::GetBrowserViewForBrowser(browser.get())) {
          if (ArcomiumSpaceSwitcher* switcher = FindSpaceSwitcher(view)) {
            switcher->RotateSpace(command_id == kNextSpace);
          }
        }
        return;
      case kMore:
        chrome::ShowAppMenu(browser.get());
        return;
      default:
        chrome::ExecuteCommand(browser.get(), command_id);
    }
  }

  ArcomiumSpaceSwitcher* GetSwitcher() const {
    return FindSpaceSwitcher(BrowserView::GetBrowserViewForBrowser(browser_));
  }

  void OnMenuClosed() {
    // Deleted after the runner returns.
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
  }

  raw_ptr<BrowserWindowInterface> browser_;
  ui::SimpleMenuModel root_;
  ui::SimpleMenuModel spaces_;
  ui::SimpleMenuModel tabs_;
  ui::SimpleMenuModel extensions_;
  ui::SimpleMenuModel developer_;
  std::unique_ptr<views::MenuRunner> runner_;
};

}  // namespace

void ShowAppMenu(BrowserWindowInterface* browser, views::View* anchor) {
  if (!browser || !anchor || !anchor->GetWidget()) {
    return;
  }
  // Owns itself until its menu closes.
  (new AppMenu(browser))->RunAt(anchor);
}

}  // namespace arcomium
