// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/alerse_app_menu.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/alerse_update/alerse_update_checker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/alerse_menu_bar_hook.h"
#include "chrome/browser/ui/tabs/alerse_space_command_hook.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_popover.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_space_switcher.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
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

namespace alerse {

namespace {

// Commands of this menu that are not the browser's own.
enum Command {
  kCheckForUpdates = 1,
  kNewSpace,
  kNextSpace,
  kPreviousSpace,
  kMore,
  // The Spaces submenu's Spaces, as macOS' Spaces menu lists them.
  kSpaceFirst = 100,
  kSpaceLast = 199,
};

// The sidebar's commands that are no browser command: Arc's Copy URL as
// Quote, Reveal Tab in Sidebar and Collapse Pinned Tabs
// (alerse_space_command_hook.h).
bool IsSidebarCommand(int command_id) {
  return command_id == IDC_ALERSE_COPY_URL_QUOTE ||
         command_id == IDC_ALERSE_REVEAL_TAB ||
         command_id == IDC_ALERSE_TOGGLE_PINNED;
}

AlerseSpaceSwitcher* FindSpaceSwitcher(views::View* view) {
  if (!view) {
    return nullptr;
  }
  if (auto* switcher = views::AsViewClass<AlerseSpaceSwitcher>(view)) {
    return switcher;
  }
  for (views::View* child : view->children()) {
    if (AlerseSpaceSwitcher* switcher = FindSpaceSwitcher(child)) {
      return switcher;
    }
  }
  return nullptr;
}

// Arc for macOS' "..." popover is 200 pt wide.
constexpr int kHeaderMenuWidth = 200;

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
                    l10n_util::GetStringUTF16(IDS_ALERSE_MENU_CHECK_UPDATES));
    } else {
      root_.AddHighlightedItemWithIcon(
          IDC_ALERSE_UPDATE,
          l10n_util::GetStringFUTF16(IDS_ALERSE_MENU_UPDATE_TO,
                                     base::UTF8ToUTF16(update)),
          ui::ImageModel());
    }
    root_.AddItem(IDC_ALERSE_IMPORT,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_IMPORT));
    root_.AddSeparator(ui::NORMAL_SEPARATOR);
    root_.AddItem(IDC_NEW_TAB, l10n_util::GetStringUTF16(IDS_ALERSE_NEW_TAB));
    root_.AddItem(IDC_NEW_WINDOW,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_NEW_WINDOW));
    root_.AddItem(IDC_NEW_INCOGNITO_WINDOW,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_NEW_INCOGNITO));
    root_.AddSeparator(ui::NORMAL_SEPARATOR);

    spaces_.AddItem(kNewSpace, l10n_util::GetStringUTF16(IDS_ALERSE_NEW_SPACE));
    spaces_.AddItem(kNextSpace,
                    l10n_util::GetStringUTF16(IDS_ALERSE_MENU_NEXT_SPACE));
    spaces_.AddItem(kPreviousSpace,
                    l10n_util::GetStringUTF16(IDS_ALERSE_MENU_PREVIOUS_SPACE));
    // Then the window's Spaces, the shown one checked.
    const std::vector<MenuBarEntry> spaces =
        GetMenuBarList(browser_, MenuBarList::kSpaces);
    if (!spaces.empty()) {
      spaces_.AddSeparator(ui::NORMAL_SEPARATOR);
    }
    for (size_t i = 0;
         i < spaces.size() && kSpaceFirst + static_cast<int>(i) <= kSpaceLast;
         ++i) {
      const int id = kSpaceFirst + static_cast<int>(i);
      spaces_.AddCheckItem(id, spaces[i].title);
      if (spaces[i].checked) {
        checked_space_ = id;
      }
    }
    root_.AddSubMenu(0, l10n_util::GetStringUTF16(IDS_ALERSE_MENU_SPACES),
                     &spaces_);

    tabs_.AddItem(IDC_FOCUS_LOCATION,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_COMMAND_BAR));
    // Arc for macOS' Copy URL items (its Edit menu) and Tabs menu items.
    tabs_.AddItem(IDC_COPY_URL,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_COPY_URL));
    tabs_.AddItem(IDC_ALERSE_COPY_URL_MARKDOWN,
                  l10n_util::GetStringUTF16(IDS_ALERSE_COPY_URL_MARKDOWN));
    tabs_.AddItem(IDC_ALERSE_COPY_URL_QUOTE,
                  l10n_util::GetStringUTF16(IDS_ALERSE_COPY_URL_QUOTE));
    tabs_.AddItem(IDC_ALERSE_REVEAL_TAB,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_REVEAL_TAB));
    tabs_.AddItem(
        IDC_ALERSE_TOGGLE_PINNED,
        l10n_util::GetStringUTF16(
            GetSpaceCommandState(browser_, IDC_ALERSE_TOGGLE_PINNED) ==
                    SpaceCommandState::kOn
                ? IDS_ALERSE_EXPAND_PINNED
                : IDS_ALERSE_COLLAPSE_PINNED));
    tabs_.AddSeparator(ui::NORMAL_SEPARATOR);
    // Arc's Split View items, which its View menu holds on macOS.
    tabs_.AddItem(IDC_NEW_SPLIT_TAB,
                  l10n_util::GetStringUTF16(IDS_ALERSE_ADD_SPLIT_VIEW));
    tabs_.AddItem(IDC_ALERSE_CLOSE_SPLIT_PANE,
                  l10n_util::GetStringUTF16(IDS_ALERSE_CLOSE_SPLIT_PANE));
    tabs_.AddItem(IDC_ALERSE_SEPARATE_SPLIT,
                  l10n_util::GetStringUTF16(IDS_ALERSE_SEPARATE_SPLIT));
    tabs_.AddItem(IDC_ALERSE_EXPAND_SPLIT,
                  l10n_util::GetStringUTF16(IDS_ALERSE_EXPAND_SPLIT));
    tabs_.AddItem(IDC_SELECT_NEXT_TAB,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_NEXT_TAB));
    tabs_.AddItem(IDC_SELECT_PREVIOUS_TAB,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_PREVIOUS_TAB));
    tabs_.AddItem(IDC_RESTORE_TAB,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_REOPEN_TAB));
    root_.AddSubMenu(0, l10n_util::GetStringUTF16(IDS_ALERSE_MENU_TABS),
                     &tabs_);

    root_.AddItem(IDC_SHOW_DOWNLOADS,
                  l10n_util::GetStringUTF16(IDS_ALERSE_DOWNLOADS));
    root_.AddItem(IDC_SHOW_HISTORY,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_HISTORY));
    root_.AddItem(IDC_SHOW_BOOKMARK_MANAGER,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_BOOKMARKS));

    extensions_.AddItem(IDC_MANAGE_EXTENSIONS,
                        l10n_util::GetStringUTF16(IDS_ALERSE_MANAGE_EXTENSIONS));
    root_.AddSubMenu(0, l10n_util::GetStringUTF16(IDS_ALERSE_EXTENSIONS),
                     &extensions_);

    developer_.AddItem(IDC_VIEW_SOURCE,
                       l10n_util::GetStringUTF16(IDS_ALERSE_MENU_VIEW_SOURCE));
    developer_.AddItem(IDC_DEV_TOOLS,
                       l10n_util::GetStringUTF16(IDS_ALERSE_MENU_DEV_TOOLS));
    developer_.AddItem(IDC_DEV_TOOLS_INSPECT,
                       l10n_util::GetStringUTF16(IDS_ALERSE_MENU_INSPECT));
    developer_.AddItem(IDC_DEV_TOOLS_CONSOLE,
                       l10n_util::GetStringUTF16(IDS_ALERSE_MENU_CONSOLE));
    developer_.AddItem(IDC_TASK_MANAGER_MAIN_MENU,
                       l10n_util::GetStringUTF16(IDS_ALERSE_MENU_TASK_MANAGER));
    root_.AddSubMenu(0, l10n_util::GetStringUTF16(IDS_ALERSE_MENU_DEVELOPER),
                     &developer_);
    root_.AddSeparator(ui::NORMAL_SEPARATOR);

    root_.AddItem(IDC_SHOW_SYNC_SETTINGS,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_SYNC));
    root_.AddItem(IDC_EDIT_SEARCH_ENGINES,
                  l10n_util::GetStringUTF16(IDS_ALERSE_SEARCH_ENGINE));
    root_.AddItem(IDC_ALERSE_LINK_ROUTING,
                  l10n_util::GetStringUTF16(IDS_ALERSE_LINK_ROUTING));
    root_.AddItem(IDC_OPTIONS,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_SETTINGS));
    root_.AddItem(IDC_PRINT, l10n_util::GetStringUTF16(IDS_ALERSE_MENU_PRINT));
    root_.AddSeparator(ui::NORMAL_SEPARATOR);
    root_.AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ALERSE_MENU_ABOUT));
    root_.AddItem(kMore, l10n_util::GetStringUTF16(IDS_ALERSE_MORE));
    root_.AddItem(IDC_CLOSE_WINDOW,
                  l10n_util::GetStringUTF16(IDS_ALERSE_MENU_CLOSE_WINDOW));
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
  bool IsCommandIdChecked(int command_id) const override {
    return command_id == checked_space_;
  }

  bool IsCommandIdEnabled(int command_id) const override {
    if (command_id >= kSpaceFirst && command_id <= kSpaceLast) {
      return true;
    }
    if (IsSidebarCommand(command_id)) {
      return GetSpaceCommandState(browser_, command_id) !=
             SpaceCommandState::kUnavailable;
    }
    switch (command_id) {
      // The submenus (Spaces, Tabs, Extensions, Developer) have no command;
      // the browser's says 0 is disabled, which greyed them out.
      case 0:
      case kCheckForUpdates:
      case kNewSpace:
      case kMore:
        return true;
      case kNextSpace:
      case kPreviousSpace: {
        AlerseSpaceSwitcher* switcher = GetSwitcher();
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
    if (command_id >= kSpaceFirst && command_id <= kSpaceLast) {
      RunMenuBarEntry(browser.get(), MenuBarList::kSpaces,
                      static_cast<size_t>(command_id - kSpaceFirst));
      return;
    }
    if (IsSidebarCommand(command_id)) {
      RunSpaceCommand(browser.get(), command_id);
      return;
    }
    switch (command_id) {
      case kCheckForUpdates:
        browser->OpenGURL(GetLatestReleaseUrl(),
                          WindowOpenDisposition::NEW_FOREGROUND_TAB);
        return;
      case kNewSpace:
        AlerseSpaceSwitcher::CreateSpace(browser.get());
        return;
      case kNextSpace:
      case kPreviousSpace:
        if (BrowserView* view =
                BrowserView::GetBrowserViewForBrowser(browser.get())) {
          if (AlerseSpaceSwitcher* switcher = FindSpaceSwitcher(view)) {
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

  AlerseSpaceSwitcher* GetSwitcher() const {
    return FindSpaceSwitcher(BrowserView::GetBrowserViewForBrowser(browser_));
  }

  void OnMenuClosed() {
    // Deleted after the runner returns.
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
  }

  raw_ptr<BrowserWindowInterface> browser_;
  int checked_space_ = -1;
  ui::SimpleMenuModel root_;
  ui::SimpleMenuModel spaces_;
  ui::SimpleMenuModel tabs_;
  ui::SimpleMenuModel extensions_;
  ui::SimpleMenuModel developer_;
  std::unique_ptr<views::MenuRunner> runner_;
};

// Runs `command` in `browser`, if it is still there.
void RunBrowserCommand(base::WeakPtr<BrowserWindowInterface> browser,
                       int command) {
  if (browser) {
    chrome::ExecuteCommand(browser.get(), command);
  }
}

}  // namespace

void ShowHeaderMenu(BrowserWindowInterface* browser,
                    views::View* anchor,
                    base::RepeatingClosure toggle_sidebar,
                    base::OnceClosure on_closed) {
  if (!browser || !anchor || !anchor->GetWidget()) {
    if (on_closed) {
      std::move(on_closed).Run();
    }
    return;
  }
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
  auto shortcut = [view](int command) -> std::optional<ui::Accelerator> {
    ui::Accelerator accelerator;
    if (view && view->GetAccelerator(command, &accelerator)) {
      return accelerator;
    }
    return std::nullopt;
  };
  // As in Arc, Reload is dimmed on an empty Space.
  const tabs::TabInterface* tab = browser->GetTabStripModel()->GetActiveTab();
  const bool page = tab && tab->GetContents() &&
                    tab->GetContents()->GetVisibleURL().ref() !=
                        "arcomium-space";
  std::vector<PopoverItem> items;
  items.emplace_back(
      PopoverIcon::kReload, l10n_util::GetStringUTF16(IDS_ALERSE_RELOAD),
      shortcut(IDC_RELOAD),
      page && chrome::IsCommandEnabled(browser, IDC_RELOAD),
      base::BindOnce(&RunBrowserCommand, browser->GetWeakPtr(), IDC_RELOAD));
  items.emplace_back(
      PopoverIcon::kSidebar,
      l10n_util::GetStringUTF16(IDS_ALERSE_TOGGLE_SIDEBAR),
      shortcut(IDC_TOGGLE_VERTICAL_TABS_COLLAPSE), true,
      base::BindOnce(
          [](base::WeakPtr<BrowserWindowInterface> browser,
             base::RepeatingClosure toggle) {
            if (browser) {
              toggle.Run();
            }
          },
          browser->GetWeakPtr(), std::move(toggle_sidebar)));
  ShowArcPopover(anchor, std::move(items), /*above=*/false, kHeaderMenuWidth,
                 std::move(on_closed));
}

void ShowAppMenu(BrowserWindowInterface* browser, views::View* anchor) {
  if (!browser || !anchor || !anchor->GetWidget()) {
    return;
  }
  // Owns itself until its menu closes.
  (new AppMenu(browser))->RunAt(anchor);
}

void ShowSpaceMenu(BrowserWindowInterface* browser,
                   const gfx::Point& point,
                   ui::mojom::MenuSourceType source_type) {
  AlerseSpaceSwitcher::ShowActiveSpaceMenuIn(browser, point, source_type);
}

}  // namespace alerse
