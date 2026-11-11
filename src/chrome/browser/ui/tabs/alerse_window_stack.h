// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERSE_WINDOW_STACK_H_
#define CHROME_BROWSER_UI_TABS_ALERSE_WINDOW_STACK_H_

// The windows of one Alerse window (docs/adr/0001-space-profiles.md). Each
// Space is a profile, and Chromium ties a window to one profile, so an
// Alerse window is a stack of windows, one for each Space opened in it. They
// share one place on screen and exactly one is visible: switching Spaces
// shows the next and hides the previous.

#include <map>
#include <set>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace alerse {

// Which stack each window is a member of, and which member each stack shows.
// A window opened by Chromium (New Window, a restored session) starts a stack
// of its own on first use.
class WindowStacks : public BrowserCollectionObserver {
 public:
  static WindowStacks& Get() {
    static base::NoDestructor<WindowStacks> stacks;
    return *stacks;
  }

  // The stack of `browser`, a new one if it has none.
  int StackOf(BrowserWindowInterface* browser) {
    auto it = stacks_.find(browser);
    if (it != stacks_.end()) {
      return it->second;
    }
    visible_[next_stack_] = browser;
    return stacks_[browser] = next_stack_++;
  }

  // The member of `stack` that shows the Space of `profile`, if any.
  BrowserWindowInterface* FindMember(int stack, Profile* profile) {
    for (const auto& [browser, id] : stacks_) {
      if (id == stack && browser->GetProfile() == profile) {
        return browser;
      }
    }
    return nullptr;
  }

  // The member `stack` shows, if it has one.
  BrowserWindowInterface* Shown(int stack) const {
    auto it = visible_.find(stack);
    return it == visible_.end() ? nullptr : it->second;
  }

  // Shows the Space of `profile` in the stack of `from`, in the place of the
  // member it shows now. The next member shows before the others hide, so
  // that no frame shows what is behind the window.
  void ShowSpace(BrowserWindowInterface* from, Profile* profile) {
    if (!profile) {
      return;
    }
    const int stack = StackOf(from);
    // Not always `from`: a hidden member asks too, when a link is routed
    // through it.
    BrowserWindowInterface* shown = Shown(stack) ? Shown(stack) : from;
    BrowserWindowInterface* target = FindMember(stack, profile);
    if (target == shown) {
      return;
    }
    if (!target) {
      target = chrome::OpenEmptyWindow(profile,
                                       /*should_trigger_session_restore=*/true);
      if (!target) {
        return;
      }
      stacks_[target] = stack;
    }
    ui::BaseWindow* shown_window = shown->GetWindow();
    ui::BaseWindow* window = target->GetWindow();
    if (shown_window->IsMaximized()) {
      window->Maximize();
    } else if (!shown_window->IsFullscreen()) {
      window->SetBounds(shown_window->GetBounds());
    }
    window->Show();
    window->Activate();
    visible_[stack] = target;
    for (const auto& [member, id] : stacks_) {
      if (id == stack && member != target) {
        member->GetWindow()->Hide();
      }
    }
  }

  // BrowserCollectionObserver:
  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    auto it = stacks_.find(browser);
    if (it == stacks_.end()) {
      return;
    }
    const int stack = it->second;
    stacks_.erase(it);
    if (visible_[stack] != browser) {
      return;
    }
    visible_.erase(stack);
    // The user closed the window they see: its hidden members go with it,
    // each saving its own session.
    std::set<BrowserWindowInterface*> members;
    for (const auto& [member, id] : stacks_) {
      if (id == stack) {
        members.insert(member);
      }
    }
    for (BrowserWindowInterface* member : members) {
      member->GetWindow()->Close();
    }
  }

 private:
  friend class base::NoDestructor<WindowStacks>;
  WindowStacks() { observation_.Observe(GlobalBrowserCollection::GetInstance()); }
  ~WindowStacks() override = default;

  std::map<BrowserWindowInterface*, int> stacks_;
  // The member each stack shows.
  std::map<int, BrowserWindowInterface*> visible_;
  int next_stack_ = 1;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation_{this};
};

// Runs with the window of the Space now shown; null if `from` closed while
// the profile loaded or the profile could not load.
using SpaceShownCallback = base::OnceCallback<void(BrowserWindowInterface*)>;

// Shows the Space whose profile lives in `path` in the place of `from`,
// loading the profile on its first use; `done` runs after the switch.
inline void ShowSpaceInStack(BrowserWindowInterface* from,
                             const base::FilePath& path,
                             SpaceShownCallback done = base::DoNothing()) {
  // Registers `from` before the profile loads: it may close meanwhile.
  const int stack = WindowStacks::Get().StackOf(from);
  g_browser_process->profile_manager()->CreateProfileAsync(
      path,
      base::BindOnce(
          [](int stack, base::FilePath from_path, SpaceShownCallback done,
             Profile* profile) {
            WindowStacks& stacks = WindowStacks::Get();
            Profile* from_profile =
                g_browser_process->profile_manager()->GetProfileByPath(
                    from_path);
            BrowserWindowInterface* from =
                from_profile ? stacks.FindMember(stack, from_profile) : nullptr;
            if (from && profile) {
              stacks.ShowSpace(from, profile);
            }
            std::move(done).Run(from && profile
                                    ? stacks.FindMember(stack, profile)
                                    : nullptr);
          },
          stack, from->GetProfile()->GetPath(), std::move(done)));
}

// Opens `url` in a new tab of the Space in `path` and shows that Space in
// the place of `from`.
inline void OpenUrlInSpace(BrowserWindowInterface* from,
                           const base::FilePath& path,
                           const GURL& url) {
  ShowSpaceInStack(from, path,
                   base::BindOnce(
                       [](GURL url, BrowserWindowInterface* member) {
                         if (!member) {
                           return;
                         }
                         // Not a link from another app: no route applies.
                         NavigateParams params(
                             member, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
                         params.disposition =
                             WindowOpenDisposition::NEW_FOREGROUND_TAB;
                         Navigate(&params);
                       },
                       url));
}

}  // namespace alerse

#endif  // CHROME_BROWSER_UI_TABS_ALERSE_WINDOW_STACK_H_
