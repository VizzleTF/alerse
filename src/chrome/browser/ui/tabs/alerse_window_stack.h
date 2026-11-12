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
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "components/viz/common/frame_timing_details.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace alerse {

// Runs as a stack is about to show `to` in the place of `from`, before `to`
// draws: the sidebar (the views layer) puts the arriving rows where their
// entrance starts (AlerseSidebarView::OnSpaceShown).
using SpaceShownFunction = void (*)(BrowserWindowInterface* from,
                                    BrowserWindowInterface* to);

inline SpaceShownFunction& SpaceShownHook() {
  static SpaceShownFunction hook = nullptr;
  return hook;
}

// Runs once `to` has presented that first frame and is alone on screen: the
// entrance plays from here, so none of it is spent before it can be seen.
using SpaceDrawnFunction = void (*)(BrowserWindowInterface* to);

inline SpaceDrawnFunction& SpaceDrawnHook() {
  static SpaceDrawnFunction hook = nullptr;
  return hook;
}

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
  // member it shows now. The others hide once the next member has drawn, so
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
      if (GetBrowserWindowCreationStatusForProfile(*profile) !=
          BrowserWindowInterface::CreationStatus::kOk) {
        return;
      }
      // Not chrome::OpenEmptyWindow(), which opens the command bar: a Space
      // shows what it had, and an empty one shows nothing, as in Arc.
      BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_NORMAL,
                                       profile, /*from_user_gesture=*/true);
      params.should_trigger_session_restore = true;
      target = CreateBrowserWindow(std::move(params));
      if (target->GetTabStripModel()->empty()) {
        // The Space's session, if it has one, takes this tab's place
        // (session_restore.cc).
        chrome::AddTabAt(target, GURL("about:blank#arcomium-space"), -1,
                         /*foreground=*/true);
      }
      stacks_[target] = stack;
    }
    // The sidebar is the stack's: as wide, and as hidden, in every Space.
    auto* sidebar = tabs::VerticalTabStripStateController::From(shown);
    auto* next_sidebar = tabs::VerticalTabStripStateController::From(target);
    if (sidebar && next_sidebar) {
      if (next_sidebar->GetUncollapsedWidth() != sidebar->GetUncollapsedWidth()) {
        next_sidebar->SetUncollapsedWidth(sidebar->GetUncollapsedWidth());
      }
      if (next_sidebar->IsCollapsed() != sidebar->IsCollapsed()) {
        next_sidebar->RequestCollapse(sidebar->IsCollapsed());
      }
    }
    ui::BaseWindow* shown_window = shown->GetWindow();
    ui::BaseWindow* window = target->GetWindow();
    // A member keeps its own full screen state, so the next one takes the
    // shown one's. Before it shows: macOS would put a window shown beside a
    // full screen one into full screen itself, having first sized it to the
    // screen, and that size is what it would return to.
    if (window->IsFullscreen() != shown_window->IsFullscreen()) {
      chrome::ToggleFullscreenMode(target, /*user_initiated=*/false);
    }
    if (shown_window->IsMaximized()) {
      window->Maximize();
    } else if (!shown_window->IsFullscreen()) {
      window->SetBounds(shown_window->GetBounds());
    }
    if (SpaceShownHook()) {
      SpaceShownHook()(shown, target);
    }
    views::Widget* shown_widget = views::Widget::GetWidgetForNativeWindow(
        shown_window->GetNativeWindow());
    views::Widget* widget =
        views::Widget::GetWidgetForNativeWindow(window->GetNativeWindow());
    std::vector<std::unique_ptr<views::Widget::PaintAsActiveLock>> locks;
#if BUILDFLAG(IS_MAC)
    // macOS shows a window that was hidden with the last frame it drew: the
    // rows as they were, in an inactive window's lighter colors. The next
    // member comes in transparent and shows with its first new frame
    // (HideOthersOnceDrawn), and until then the member it replaces keeps an
    // active window's colors though the keyboard has left it. Not in full
    // screen, where each member is a desktop of its own.
    if (widget && shown_widget && !shown_window->IsFullscreen()) {
      locks.push_back(shown_widget->LockPaintAsActive());
      locks.push_back(widget->LockPaintAsActive());
      widget->SetOpacity(0.0f);
    }
#endif
    window->Show();
    window->Activate();
    // A dialog of the tab shows again with its window, without the keys:
    // the window has just taken them.
    if (widget) {
      for (views::Widget* child :
           views::Widget::GetAllChildWidgets(widget->GetNativeView())) {
        if (child != widget && child->IsModal() && child->IsVisible()) {
          child->Activate();
        }
      }
    }
    visible_[stack] = target;
    HideOthersOnceDrawn(stack, target, std::move(locks));
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
    hide_generation_.erase(stack);
    paint_locks_.erase(stack);
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

  // A window shown again has nothing new to present until its first frame
  // (Windows and Linux drop a hidden window's buffers), so the members of
  // `stack` other than `target` stay until `target` has presented one.
  // `locks` are held until then.
  void HideOthersOnceDrawn(
      int stack,
      BrowserWindowInterface* target,
      std::vector<std::unique_ptr<views::Widget::PaintAsActiveLock>> locks) {
    const int generation = ++hide_generation_[stack];
    paint_locks_[stack] = std::move(locks);
    base::RepeatingClosure hide = base::BindRepeating(
        [](int stack, int generation) {
          WindowStacks& stacks = WindowStacks::Get();
          auto it = stacks.hide_generation_.find(stack);
          // A later switch owns the hiding now.
          if (it != stacks.hide_generation_.end() &&
              it->second == generation) {
            stacks.HideAllBut(stack);
          }
        },
        stack, generation);
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
        target->GetWindow()->GetNativeWindow());
    if (ui::Compositor* compositor = widget ? widget->GetCompositor()
                                            : nullptr) {
      compositor->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
          [](base::RepeatingClosure hide, const viz::FrameTimingDetails&) {
            hide.Run();
          },
          hide));
      // A window with nothing new to draw presents no frame by itself.
      compositor->ScheduleFullRedraw();
    }
    // A window that presents nothing (occluded, minimized) must not keep two
    // members on screen. Hiding twice is harmless.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, hide, base::Milliseconds(250));
  }

  void HideAllBut(int stack) {
    BrowserWindowInterface* shown = Shown(stack);
    if (!shown) {
      return;
    }
    auto opaque = [](BrowserWindowInterface* member) {
#if BUILDFLAG(IS_MAC)
      if (views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
              member->GetWindow()->GetNativeWindow())) {
        widget->SetOpacity(1.0f);
      }
#endif
    };
    opaque(shown);
    if (SpaceDrawnHook()) {
      SpaceDrawnHook()(shown);
    }
    for (const auto& [member, id] : stacks_) {
      if (id == stack && member != shown) {
        member->GetWindow()->Hide();
        // One a later switch overtook while it was still transparent.
        opaque(member);
      }
    }
    paint_locks_.erase(stack);
  }

  std::map<BrowserWindowInterface*, int> stacks_;
  // The member each stack shows.
  std::map<int, BrowserWindowInterface*> visible_;
  // Counts each stack's switches: a pending hide of an earlier one does
  // nothing.
  std::map<int, int> hide_generation_;
  // Each stack's windows kept in an active window's colors over a switch.
  std::map<int, std::vector<std::unique_ptr<views::Widget::PaintAsActiveLock>>>
      paint_locks_;
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
