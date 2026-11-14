// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Peek and the Little window (chrome/browser/ui/tabs/alerse_peek.h). Both
// show a page in a panel: an address field, Open as Tab (Peek) or Open in
// Alerse (Little window), and the page. Peek's panel floats over its
// window's page on a dimmed backdrop; the Little window is a small window of
// its own. Their pages are not tabs until they are opened as one: the tab
// strip adopts the page with its history and state.

#include "chrome/browser/ui/tabs/alerse_peek.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/tabs/alerse_spaces.h"
#include "chrome/browser/ui/tabs/alerse_window_stack.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_motion.h"
#include "chrome/grit/generated_resources.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace alerse {

namespace {

constexpr int kHeaderHeight = 40;
constexpr int kCornerRadius = 12;
constexpr int kButtonSize = 28;
constexpr gfx::Size kLittleWindowSize(560, 720);

// The window of `profile` to give a page to: its last active normal one.
BrowserWindowInterface* NormalWindowOf(Profile* profile) {
  BrowserWindowInterface* found = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetProfile() == profile &&
            browser->GetType() == BrowserWindowInterface::TYPE_NORMAL) {
          found = browser;
          return false;
        }
        return true;
      });
  return found;
}

// The page and its header. The panel owns the page until it becomes a tab.
class PeekPanel : public views::View,
                  public content::WebContentsDelegate,
                  public views::TextfieldController {
  METADATA_HEADER(PeekPanel, views::View)

 public:
  enum class Kind { kPeek, kLittleWindow };

  PeekPanel(Profile* profile,
            Kind kind,
            base::RepeatingClosure close,
            base::RepeatingClosure open,
            base::RepeatingClosure title_changed)
      : profile_(profile),
        peek_(kind == Kind::kPeek),
        close_(std::move(close)),
        title_changed_(std::move(title_changed)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    if (kind == Kind::kPeek) {
      layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kCornerRadius));
      layer()->SetIsFastRoundedCorner(true);
      layer()->SetMasksToBounds(true);
    }
    SetBackground(views::CreateSolidBackground(ui::kColorDialogBackground));

    header_ = AddChildView(std::make_unique<views::View>());
    auto* layout = header_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(6, 10),
        6));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    address_ = header_->AddChildView(std::make_unique<views::Textfield>());
    address_->set_controller(this);
    address_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ALERSE_PEEK_ADDRESS));
    layout->SetFlexForView(address_, 1);

    auto open_button = views::CreateVectorImageButtonWithNativeTheme(
        std::move(open), vector_icons::kOpenInNewFlippableIcon, 16);
    open_button->SetPreferredSize(gfx::Size(kButtonSize, kButtonSize));
    const std::u16string open_name = l10n_util::GetStringUTF16(
        kind == Kind::kPeek ? IDS_ALERSE_PEEK_OPEN_AS_TAB
                            : IDS_ALERSE_LITTLE_WINDOW_OPEN_IN_ALERSE);
    open_button->SetTooltipText(open_name);
    open_button->GetViewAccessibility().SetName(open_name);
    header_->AddChildView(std::move(open_button));

    // The Little window closes as any window does.
    if (kind == Kind::kPeek) {
      auto close_button = views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating([](base::RepeatingClosure close) { close.Run(); },
                              close_),
          vector_icons::kCloseIcon, 16);
      close_button->SetPreferredSize(gfx::Size(kButtonSize, kButtonSize));
      close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
      close_button->GetViewAccessibility().SetName(
          l10n_util::GetStringUTF16(IDS_APP_CLOSE));
      header_->AddChildView(std::move(close_button));
    }

    web_view_ = AddChildView(std::make_unique<views::WebView>(profile));
    contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile));
    contents_->SetDelegate(this);
    web_view_->SetWebContents(contents_.get());
  }

  PeekPanel(const PeekPanel&) = delete;
  PeekPanel& operator=(const PeekPanel&) = delete;

  ~PeekPanel() override {
    if (contents_) {
      contents_->SetDelegate(nullptr);
    }
  }

  void Load(const GURL& url, const content::Referrer& referrer) {
    if (!contents_) {
      return;
    }
    content::NavigationController::LoadURLParams params(url);
    params.referrer = referrer;
    params.transition_type = ui::PAGE_TRANSITION_LINK;
    contents_->GetController().LoadURLWithParams(params);
    address_->SetText(DisplayUrl(url));
    web_view_->RequestFocus();
  }

  void FocusAddress() {
    address_->RequestFocus();
    address_->SelectAll(false);
  }

  // The page, for a tab to adopt; the panel is empty afterwards.
  std::unique_ptr<content::WebContents> TakeContents() {
    if (!contents_) {
      return nullptr;
    }
    web_view_->SetWebContents(nullptr);
    contents_->SetDelegate(nullptr);
    return std::move(contents_);
  }

  bool HasPage() const {
    return contents_ && !contents_->GetLastCommittedURL().is_empty();
  }

  std::u16string Title() const {
    return contents_ ? std::u16string(contents_->GetTitle()) : std::u16string();
  }

  // views::View:
  void Layout(PassKey) override {
    const gfx::Rect bounds = GetContentsBounds();
    header_->SetBounds(bounds.x(), bounds.y(), bounds.width(), kHeaderHeight);
    web_view_->SetBounds(bounds.x(), bounds.y() + kHeaderHeight,
                         bounds.width(),
                         std::max(0, bounds.height() - kHeaderHeight));
  }

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    if (params.disposition == WindowOpenDisposition::CURRENT_TAB) {
      source->GetController().LoadURLWithParams(
          content::NavigationController::LoadURLParams(params));
      return source;
    }
    // A link the page opens elsewhere opens as a tab of the Space.
    BrowserWindowInterface* browser = NormalWindowOf(profile_);
    if (!browser) {
      return nullptr;
    }
    NavigateParams nav(browser, params.url, params.transition);
    nav.referrer = params.referrer;
    nav.disposition = params.disposition == WindowOpenDisposition::NEW_WINDOW
                          ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                          : params.disposition;
    Navigate(&nav);
    return nav.navigated_or_inserted_contents;
  }

  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override {
    BrowserWindowInterface* browser = NormalWindowOf(profile_);
    if (!browser || !user_gesture) {
      return nullptr;
    }
    content::WebContents* raw = new_contents.get();
    browser->GetTabStripModel()->AppendWebContents(std::move(new_contents),
                                                   /*foreground=*/true);
    return raw;
  }

  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed) override {
    if ((changed & content::INVALIDATE_TYPE_URL) && !address_->HasFocus()) {
      address_->SetText(DisplayUrl(source->GetVisibleURL()));
    }
    if (changed & content::INVALIDATE_TYPE_TITLE) {
      title_changed_.Run();
    }
  }

  void CloseContents(content::WebContents* source) override { close_.Run(); }

  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    // Esc the page leaves to the browser closes Peek on every platform.
    if (peek_ && event.windows_key_code == ui::VKEY_ESCAPE &&
        event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown) {
      close_.Run();
      return true;
    }
    return keyboard_.HandleKeyboardEvent(event, GetFocusManager());
  }

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& event) override {
    if (event.type() != ui::EventType::kKeyPressed ||
        event.key_code() != ui::VKEY_RETURN) {
      return false;
    }
    AutocompleteMatch match;
    AutocompleteClassifierFactory::GetForProfile(profile_)->Classify(
        std::u16string(sender->GetText()), false, false,
        metrics::OmniboxEventProto::OTHER, &match, nullptr);
    if (match.destination_url.is_valid()) {
      Load(match.destination_url, content::Referrer());
    }
    return true;
  }

 private:
  static std::u16string DisplayUrl(const GURL& url) {
    return url_formatter::FormatUrl(url);
  }

  const raw_ptr<Profile> profile_;
  const bool peek_;
  base::RepeatingClosure close_;
  base::RepeatingClosure title_changed_;
  raw_ptr<views::View> header_ = nullptr;
  raw_ptr<views::Textfield> address_ = nullptr;
  raw_ptr<views::WebView> web_view_ = nullptr;
  std::unique_ptr<content::WebContents> contents_;
  views::UnhandledKeyboardEventHandler keyboard_;
};

BEGIN_METADATA(PeekPanel)
END_METADATA

// Peek's widget: the window's page area, dimmed, with the panel inset in it.
// A click on the dimmed part closes Peek.
class PeekBackdrop : public views::View {
  METADATA_HEADER(PeekBackdrop, views::View)

 public:
  explicit PeekBackdrop(base::RepeatingClosure close)
      : close_(std::move(close)) {}

  void SetPanel(PeekPanel* panel) { panel_ = panel; }

  gfx::Rect PanelBounds() const {
    gfx::Rect bounds = GetLocalBounds();
    const int x = std::max(24, bounds.width() / 14);
    bounds.Inset(gfx::Insets::VH(20, x));
    return bounds;
  }

  // views::View:
  void Layout(PassKey) override {
    if (panel_) {
      panel_->SetBoundsRect(PanelBounds());
    }
  }

  void OnPaint(gfx::Canvas* canvas) override {
    canvas->DrawColor(SkColorSetA(SK_ColorBLACK, 0x40));
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (!PanelBounds().Contains(event.location())) {
      close_.Run();
    }
    return true;
  }

 private:
  base::RepeatingClosure close_;
  raw_ptr<PeekPanel> panel_ = nullptr;
};

BEGIN_METADATA(PeekBackdrop)
END_METADATA

// Owns a Peek or a Little window and deletes itself once its widget is gone.
class PeekHost : public views::WidgetDelegate, public views::WidgetObserver {
 public:
  // The Peek over `browser`, if one is open.
  static PeekHost* PeekOf(BrowserWindowInterface* browser) {
    for (PeekHost* host : All()) {
      if (host->browser_ == browser) {
        return host;
      }
    }
    return nullptr;
  }

  static void OpenPeek(BrowserWindowInterface* browser,
                       const GURL& url,
                       const content::Referrer& referrer) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (!browser_view || !browser_view->GetWidget()) {
      return;
    }
    PeekHost* host = PeekOf(browser);
    if (!host) {
      host = new PeekHost(browser->GetProfile(), browser);
      host->InitPeek(browser_view);
    }
    host->panel_->Load(url, referrer);
  }

  static void OpenLittleWindow(Profile* profile, const GURL& url) {
    PeekHost* host = new PeekHost(profile, nullptr);
    host->InitLittleWindow();
    if (url.is_empty()) {
      host->panel_->FocusAddress();
    } else {
      host->panel_->Load(url, content::Referrer());
    }
  }

  ~PeekHost() override { All().erase(this); }

  // views::WidgetDelegate:
  std::u16string GetWindowTitle() const override {
    const std::u16string title = panel_ ? panel_->Title() : std::u16string();
    return title.empty()
               ? l10n_util::GetStringUTF16(IDS_ALERSE_NEW_LITTLE_WINDOW)
               : title;
  }

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    if (widget != widget_.get()) {
      FollowWindow();
    }
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    if (widget == widget_.get()) {
      panel_ = nullptr;
      parent_observation_.Reset();
      own_observation_.Reset();
      base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                  this);
    } else {
      // The window under Peek is closing: Peek goes with it.
      Close();
    }
  }

 private:
  PeekHost(Profile* profile, BrowserWindowInterface* browser)
      : profile_(profile), browser_(browser) {
    All().insert(this);
  }

  static std::set<PeekHost*>& All() {
    static base::NoDestructor<std::set<PeekHost*>> hosts;
    return *hosts;
  }

  std::unique_ptr<PeekPanel> MakePanel(PeekPanel::Kind kind) {
    auto panel = std::make_unique<PeekPanel>(
        profile_, kind,
        base::BindRepeating(&PeekHost::Close, weak_factory_.GetWeakPtr()),
        base::BindRepeating(&PeekHost::OpenAsTab, weak_factory_.GetWeakPtr()),
        base::BindRepeating(&PeekHost::TitleChanged,
                            weak_factory_.GetWeakPtr()));
    panel_ = panel.get();
    return panel;
  }

  void InitPeek(BrowserView* browser_view) {
    parent_view_ = browser_view;
    auto backdrop = std::make_unique<PeekBackdrop>(
        base::BindRepeating(&PeekHost::Close, weak_factory_.GetWeakPtr()));
    PeekBackdrop* backdrop_view = backdrop.get();
    backdrop_view->SetPanel(
        backdrop_view->AddChildView(MakePanel(PeekPanel::Kind::kPeek)));
    SetContentsView(std::move(backdrop));
    SetModalType(ui::mojom::ModalType::kNone);

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
    params.delegate = this;
    params.parent = browser_view->GetWidget()->GetNativeView();
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.activatable = views::Widget::InitParams::Activatable::kYes;
    params.bounds = browser_view->contents_container()->GetBoundsInScreen();
    params.name = "AlersePeek";
    widget_->Init(std::move(params));
    own_observation_.Observe(widget_.get());
    parent_observation_.Observe(browser_view->GetWidget());

    // Escape closes Peek wherever the focus is.
    accelerator_target_ = std::make_unique<EscapeTarget>(
        base::BindRepeating(&PeekHost::Close, weak_factory_.GetWeakPtr()));
    widget_->GetFocusManager()->RegisterAccelerator(
        ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE),
        ui::AcceleratorManager::kNormalPriority, accelerator_target_.get());

    widget_->Show();
    PlayShow(panel_->layer());
  }

  void InitLittleWindow() {
    SetContentsView(MakePanel(PeekPanel::Kind::kLittleWindow));
    SetCanResize(true);
    SetCanMinimize(true);
    SetCanMaximize(true);
    SetShowTitle(true);

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.delegate = this;
    params.name = "AlerseLittleWindow";
    // Centered at the top of the screen the pointer is on, as Arc's.
    const display::Display display =
        display::Screen::Get()->GetDisplayNearestPoint(
            display::Screen::Get()->GetCursorScreenPoint());
    const gfx::Rect area = display.work_area();
    gfx::Size size = kLittleWindowSize;
    size.SetToMin(gfx::Size(area.width() - 40, area.height() - 40));
    params.bounds = gfx::Rect(area.x() + (area.width() - size.width()) / 2,
                              area.y() + std::min(80, area.height() / 10),
                              size.width(), size.height());
    widget_->Init(std::move(params));
    own_observation_.Observe(widget_.get());
    widget_->Show();
    widget_->Activate();
  }

  // A light rise and fade in, as the sidebar's popovers (alerse_motion.h).
  static void PlayShow(ui::Layer* layer) {
    if (gfx::Animation::PrefersReducedMotion()) {
      return;
    }
    gfx::Transform from;
    const gfx::Rect bounds = gfx::Rect(layer->bounds().size());
    from = gfx::GetScaleTransform(bounds.CenterPoint(), 0.97f);
    from.Translate(0, 8);
    layer->SetOpacity(0);
    layer->SetTransform(from);
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(motion::kStandard);
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    layer->SetOpacity(1);
    layer->SetTransform(gfx::Transform());
  }

  // Peek covers its window's page area, wherever the window goes.
  void FollowWindow() {
    if (widget_ && parent_view_) {
      widget_->SetBounds(parent_view_->contents_container()->GetBoundsInScreen());
    }
  }

  void Close() {
    if (widget_ && !widget_->IsClosed()) {
      widget_->Close();
    }
  }

  void TitleChanged() {
    if (widget_ && !browser_) {
      widget_->UpdateWindowTitle();
    }
  }

  // Peek's page becomes a tab of its window; the Little window's a tab of
  // its Space's window.
  void OpenAsTab() {
    if (!panel_ || !panel_->HasPage()) {
      Close();
      return;
    }
    BrowserWindowInterface* browser =
        browser_ ? browser_.get() : NormalWindowOf(profile_);
    if (!browser) {
      // No window of the Space is open: one opens on the page's address.
      const GURL url = panel_->TakeContents()->GetLastCommittedURL();
      if (BrowserWindowInterface* any =
              GetLastActiveBrowserWindowInterfaceWithAnyProfile()) {
        OpenUrlInSpace(any, profile_->GetPath(), url);
      }
      Close();
      return;
    }
    browser->GetTabStripModel()->AppendWebContents(panel_->TakeContents(),
                                                   /*foreground=*/true);
    if (!browser_) {
      browser->GetWindow()->Activate();
    }
    Close();
  }

  // Escape's handler in Peek's widget.
  class EscapeTarget : public ui::AcceleratorTarget {
   public:
    explicit EscapeTarget(base::RepeatingClosure close)
        : close_(std::move(close)) {}
    bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
      close_.Run();
      return true;
    }
    bool CanHandleAccelerators() const override { return true; }

   private:
    base::RepeatingClosure close_;
  };

  const raw_ptr<Profile> profile_;
  // Peek's window; null for a Little window.
  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<BrowserView> parent_view_ = nullptr;
  raw_ptr<PeekPanel> panel_ = nullptr;
  std::unique_ptr<EscapeTarget> accelerator_target_;
  std::unique_ptr<views::Widget> widget_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      own_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      parent_observation_{this};
  base::WeakPtrFactory<PeekHost> weak_factory_{this};
};

// Sends a click in a pinned tab on a link to another site to Peek.
class PeekThrottle : public content::NavigationThrottle {
 public:
  using content::NavigationThrottle::NavigationThrottle;

  ThrottleCheckResult WillStartRequest() override { return Check(); }
  // A link through the site's own redirect (google.com/url?q=...) leaves
  // the site only here.
  ThrottleCheckResult WillRedirectRequest() override { return Check(); }

  const char* GetNameForLogging() override { return "AlersePeekThrottle"; }

 private:
  ThrottleCheckResult Check() {
    content::NavigationHandle* handle = navigation_handle();
    content::WebContents* contents = handle->GetWebContents();
    const GURL& url = handle->GetURL();
    const GURL& from = contents->GetLastCommittedURL();
    if (!PeekEnabled() || !handle->IsRendererInitiated() ||
        !handle->HasUserGesture() || handle->IsPost() ||
        !ui::PageTransitionCoreTypeIs(handle->GetPageTransition(),
                                      ui::PAGE_TRANSITION_LINK) ||
        !url.SchemeIsHTTPOrHTTPS() || !from.SchemeIsHTTPOrHTTPS() ||
        !IsOtherSite(contents, url)) {
      return PROCEED;
    }
    tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(contents);
    if (!tab || !tab->GetBrowserWindowInterface() ||
        tab->GetBrowserWindowInterface()->GetType() !=
            BrowserWindowInterface::TYPE_NORMAL ||
        !IsPinnedTab(tab->GetBrowserWindowInterface(), contents)) {
      return PROCEED;
    }
    // Posted: a navigation cannot start another while it is being decided.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<content::WebContents> contents, GURL url,
               content::Referrer referrer) {
              if (!contents) {
                return;
              }
              tabs::TabInterface* tab =
                  tabs::TabInterface::MaybeGetFromContents(contents.get());
              if (tab && tab->GetBrowserWindowInterface()) {
                ShowPeek(tab->GetBrowserWindowInterface(), url, referrer);
              }
            },
            contents->GetWeakPtr(), url,
            content::Referrer(from, network::mojom::ReferrerPolicy::kDefault)));
    return CANCEL_AND_IGNORE;
  }
};

}  // namespace

void ShowPeek(BrowserWindowInterface* browser,
              const GURL& url,
              const content::Referrer& referrer) {
  PeekHost::OpenPeek(browser, url, referrer);
}

void ShowLittleWindow(Profile* profile, const GURL& url) {
  PeekHost::OpenLittleWindow(profile, url);
}

void MaybeAddPeekThrottle(content::NavigationThrottleRegistry& registry) {
  if (registry.GetNavigationHandle().IsInPrimaryMainFrame()) {
    registry.AddThrottle(std::make_unique<PeekThrottle>(registry));
  }
}

}  // namespace alerse
