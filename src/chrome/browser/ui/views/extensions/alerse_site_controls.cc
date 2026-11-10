// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/alerse_site_controls.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/page_info/page_info_navigation_handler.h"
#include "chrome/browser/ui/views/page_info/permission_toggle_row_view.h"
#include "chrome/browser/ui/views/page_info/permission_toggle_row_view_observer.h"
#include "chrome/grit/generated_resources.h"
#include "components/page_info/page_info.h"
#include "components/page_info/page_info_ui.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace alerse_site_controls {

ExtensionMenu::ExtensionMenu(ToolbarActionViewModel* action,
                             base::RepeatingClosure on_closed)
    : action_(action), on_closed_(std::move(on_closed)) {}

ExtensionMenu::~ExtensionMenu() = default;

void ExtensionMenu::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  // The toolbar button's menu, which has Pin and Unpin.
  ui::MenuModel* model = action_->GetContextMenu(
      extensions::ExtensionContextMenuModel::ContextMenuSource::kToolbarAction);
  if (!model || !source->GetWidget()) {
    return;
  }
  adapter_ = std::make_unique<views::MenuModelAdapter>(model, on_closed_);
  runner_ = std::make_unique<views::MenuRunner>(
      adapter_->CreateMenu(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  runner_->RunMenuAt(source->GetWidget(), nullptr,
                     gfx::Rect(point, gfx::Size()),
                     views::MenuAnchorPosition::kTopLeft, source_type);
}

std::unique_ptr<ExtensionMenu> CreateExtensionMenu(
    ToolbarActionViewModel* action,
    base::RepeatingClosure on_closed) {
  return std::make_unique<ExtensionMenu>(action, std::move(on_closed));
}

namespace {

constexpr int kWidth = 300;
constexpr int kMargin = 12;
constexpr int kGap = 8;
constexpr int kColumns = 4;
constexpr int kTileWidth = (kWidth - 2 * kMargin - (kColumns - 1) * kGap) /
                           kColumns;
constexpr int kTileHeight = 46;
constexpr int kTileRadius = 10;
constexpr int kMaxPermissionRows = 6;

enum MenuCommand {
  kSiteSettings = 1,
  kExtensionSiteAccess,
  kManageExtensions,
};

// A rounded tile: a page action, an extension or "add". A pinned extension
// carries a pin in its corner, as in Arc.
class Tile : public views::ImageButton {
  METADATA_HEADER(Tile, views::ImageButton)

 public:
  Tile(PressedCallback callback, const std::u16string& name)
      : views::ImageButton(std::move(callback)) {
    SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    SetPreferredSize(gfx::Size(kTileWidth, kTileHeight));
    SetTooltipText(name);
    GetViewAccessibility().SetName(name);
    UpdateBackground();
  }

  void SetPinned(bool pinned) {
    if (pinned_ != pinned) {
      pinned_ = pinned;
      SchedulePaint();
    }
  }

  // views::ImageButton:
  void StateChanged(ButtonState old_state) override {
    views::ImageButton::StateChanged(old_state);
    UpdateBackground();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    views::ImageButton::PaintButtonContents(canvas);
    const ui::ColorProvider* colors = GetColorProvider();
    if (!pinned_ || !colors) {
      return;
    }
    constexpr int kBadge = 16;
    const gfx::Point center(width() / 2 + 14, height() / 2 + 12);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(colors->GetColor(ui::kColorSysOnSurface));
    canvas->DrawCircle(center, kBadge / 2, flags);
    const gfx::ImageSkia pin = gfx::CreateVectorIcon(
        kKeepIcon, 10, colors->GetColor(ui::kColorSysSurface));
    canvas->DrawImageInt(pin, center.x() - 5, center.y() - 5);
  }

 private:
  void UpdateBackground() {
    const bool hot =
        GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    SetBackground(views::CreateRoundedRectBackground(
        hot ? ui::kColorSysTonalContainer : ui::kColorSysNeutralContainer,
        kTileRadius));
  }

  bool pinned_ = false;
};

BEGIN_METADATA(Tile)
END_METADATA

std::unique_ptr<views::View> MakeRow() {
  auto row = std::make_unique<views::View>();
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), kGap));
  return row;
}

std::unique_ptr<views::Label> MakeHeading(const std::u16string& text) {
  auto label = std::make_unique<views::Label>(
      text, views::style::CONTEXT_LABEL, views::style::STYLE_HEADLINE_5);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

class SiteControlsView;
SiteControlsView* g_shown = nullptr;

// The bubble's contents; a views::BubbleDialogDelegate frames them.
class SiteControlsView : public views::View,
                         public PageInfoUI,
                         public PageInfoNavigationHandler,
                         public PermissionToggleRowViewObserver,
                         public ExtensionContextMenuController::Observer,
                         public content::WebContentsObserver,
                         public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(SiteControlsView, views::View)

 public:
  SiteControlsView(BrowserWindowInterface* browser,
                   ExtensionsToolbarDesktop* extensions)
      : browser_(browser->GetWeakPtr()), extensions_(extensions) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 10));

    content::WebContents* contents =
        browser->GetTabStripModel()->GetActiveWebContents();
    if (contents) {
      url_ = contents->GetLastCommittedURL();
    }

    AddActions();
    AddExtensions();
    settings_heading_ = AddChildView(
        MakeHeading(l10n_util::GetStringUTF16(IDS_ALERSE_SITE_PERMISSIONS)));
    permissions_ = AddChildView(std::make_unique<views::View>());
    permissions_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    settings_heading_->SetVisible(false);
    permissions_->SetVisible(false);
    AddChildView(std::make_unique<views::Separator>());
    AddFooter();

    // Closed when the tab goes or leaves the page: the page info presenter
    // holds the tab and would change another site's permissions.
    Observe(contents);
    if (contents && url_.SchemeIsHTTPOrHTTPS()) {
      ui_delegate_ =
          std::make_unique<ChromePageInfoUiDelegate>(contents, url_);
      presenter_ = std::make_unique<PageInfo>(
          std::make_unique<ChromePageInfoDelegate>(contents), contents, url_);
      presenter_->InitializeUiState(this, base::DoNothing());
    }
  }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    // The rows point at the delegate, the presenter at the tab: all go now,
    // the bubble with them once it closes.
    presenter_.reset();
    permission_rows_.clear();
    if (permissions_) {
      permissions_->RemoveAllChildViews();
    }
    ui_delegate_.reset();
    Observe(nullptr);
    CloseSoon();
  }
  void PrimaryPageChanged(content::Page& page) override { CloseSoon(); }

  void CloseSoon() {
    if (views::Widget* widget = GetWidget()) {
      widget->Close();
    }
  }

  ~SiteControlsView() override {
    if (g_shown == this) {
      g_shown = nullptr;
    }
    // The permission rows and the presenter point at the delegates and at
    // this view; they go first.
    presenter_.reset();
    permission_rows_.clear();
    extension_tiles_.clear();
    settings_heading_ = nullptr;
    permissions_ = nullptr;
    secure_button_ = nullptr;
    more_button_ = nullptr;
    // The tiles point at their context menus: they go before them.
    RemoveAllChildViews();
    context_menus_.clear();
    if (after_close_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(after_close_));
    }
  }

  // Closes, then runs `action` once this view is gone, so that what it opens
  // (a popup, a screenshot) neither anchors to it nor shows it.
  void CloseThen(base::OnceClosure action) {
    after_close_ = std::move(action);
    GetWidget()->Close();
  }

  // PageInfoUI:
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override {
    if (!permission_rows_.empty()) {
      for (const auto& permission : permission_info_list) {
        auto it = permission_rows_.find(permission.type);
        if (it != permission_rows_.end()) {
          it->second->UpdatePermission(permission);
        }
      }
      return;
    }
    int count = 0;
    for (const auto& permission : permission_info_list) {
      if (count++ == kMaxPermissionRows) {
        break;
      }
      auto* row = permissions_->AddChildView(
          std::make_unique<PermissionToggleRowView>(
              ui_delegate_.get(), this, permission,
              /*should_show_spacer_view=*/false));
      row->AddObserver(this);
      permission_rows_[permission.type] = row;
    }
    const bool any = !permission_rows_.empty();
    settings_heading_->SetVisible(any);
    permissions_->SetVisible(any);
    if (views::Widget* widget = GetWidget()) {
      if (views::DialogDelegate* dialog =
              widget->widget_delegate()->AsDialogDelegate()) {
        if (views::BubbleDialogDelegate* bubble =
                dialog->AsBubbleDialogDelegate()) {
          bubble->SizeToContents();
        }
      }
    }
  }

  void SetIdentityInfo(const IdentityInfo& identity_info) override {
    const std::unique_ptr<SecurityDescription> description =
        GetSecurityDescription(identity_info);
    const bool secure =
        description &&
        description->summary_style != SecuritySummaryColor::RED &&
        identity_info.connection_status ==
            PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
    secure_button_->SetText(l10n_util::GetStringUTF16(
        secure ? IDS_ALERSE_SECURE : IDS_ALERSE_NOT_SECURE));
    secure_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            secure ? vector_icons::kLockIcon
                   : vector_icons::kNotSecureWarningOldIcon,
            ui::kColorSysOnSurface, 16));
    secure_button_->SetVisible(true);
  }

  // PermissionToggleRowViewObserver:
  void OnPermissionChanged(
      const PageInfo::PermissionInfo& permission) override {
    if (presenter_) {
      presenter_->OnSitePermissionChanged(permission.type, permission.setting,
                                          permission.requesting_origin,
                                          permission.is_one_time);
    }
  }

  // PageInfoNavigationHandler: the details are Chrome's page info.
  void OpenMainPage(base::OnceClosure initialized_callback) override {
    OpenPageInfo(std::nullopt);
  }
  void OpenSecurityPage() override { OpenPageInfo(std::nullopt); }
  void OpenPermissionPage(ContentSettingsType type) override {
    OpenPageInfo(type);
  }
  void OpenCookiesPage() override { OpenPageInfo(std::nullopt); }
  void OpenMerchantTrustPage() override { OpenPageInfo(std::nullopt); }
  void CloseBubble() override { GetWidget()->Close(); }

  // ExtensionContextMenuController::Observer:
  void OnContextMenuShown() override {}
  void OnContextMenuClosed() override { UpdatePins(); }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    base::WeakPtr<BrowserWindowInterface> browser = browser_;
    const GURL url = url_;
    switch (command_id) {
      case kSiteSettings:
        CloseThen(base::BindOnce(
            [](base::WeakPtr<BrowserWindowInterface> browser, GURL url) {
              if (browser) {
                chrome::ShowSiteSettings(browser.get(), url);
              }
            },
            browser, url));
        break;
      case kExtensionSiteAccess:
        CloseThen(base::BindOnce(
            [](std::unique_ptr<views::ViewTracker> extensions) {
              if (auto* container = static_cast<ExtensionsToolbarDesktop*>(
                      extensions->view())) {
                if (ExtensionsToolbarButton* button =
                        container->GetExtensionsButton()) {
                  button->ToggleExtensionsMenu();
                }
              }
            },
            std::make_unique<views::ViewTracker>(extensions_)));
        break;
      case kManageExtensions:
        CloseThen(base::BindOnce(
            [](base::WeakPtr<BrowserWindowInterface> browser) {
              if (browser) {
                chrome::ShowExtensions(browser.get());
              }
            },
            browser));
        break;
    }
  }

 private:
  void AddActions() {
    struct Action {
      int command;
      raw_ref<const gfx::VectorIcon> icon;
      int name;
    };
    const Action actions[] = {
        {IDC_COPY_URL, raw_ref(vector_icons::kAlerseLinkIcon),
         IDS_ALERSE_COPY_LINK},
        {IDC_FIND, raw_ref(kFindInPageIcon), IDS_ALERSE_FIND_IN_PAGE},
        {IDC_SHARING_HUB_SCREENSHOT, raw_ref(kScreenshotRegionIcon),
         IDS_ALERSE_SCREENSHOT},
        {IDC_SHOW_READING_MODE, raw_ref(kMenuBookIcon),
         IDS_ALERSE_READING_MODE},
    };
    views::View* row = AddChildView(MakeRow());
    for (const Action& action : actions) {
      auto* tile = row->AddChildView(std::make_unique<Tile>(
          base::BindRepeating(&SiteControlsView::RunCommand,
                              base::Unretained(this), action.command),
          l10n_util::GetStringUTF16(action.name)));
      tile->SetImageModel(views::Button::STATE_NORMAL,
                          ui::ImageModel::FromVectorIcon(
                              *action.icon, ui::kColorSysOnSurface, 18));
      tile->SetImageModel(views::Button::STATE_DISABLED,
                          ui::ImageModel::FromVectorIcon(
                              *action.icon, ui::kColorSysStateDisabled, 18));
      tile->SetEnabled(browser_ &&
                       chrome::IsCommandEnabled(browser_.get(), action.command));
    }
  }

  void AddExtensions() {
    AddChildView(MakeHeading(l10n_util::GetStringUTF16(IDS_ALERSE_EXTENSIONS)));
    ExtensionsToolbarViewModel* model = extensions_->GetToolbarViewModel();
    content::WebContents* contents =
        browser_ ? browser_->GetTabStripModel()->GetActiveWebContents()
                 : nullptr;
    // Pinned ones first, in their order, then the rest by name.
    std::vector<ToolbarActionsModel::ActionId> ids;
    if (model && model->AreActionsInitialized()) {
      ids = model->GetPinnedActionIds();
      std::vector<ToolbarActionsModel::ActionId> rest;
      for (const auto& id : model->GetAllActionIds()) {
        if (!std::ranges::contains(ids, id)) {
          rest.push_back(id);
        }
      }
      std::ranges::sort(rest, [model](const auto& a, const auto& b) {
        ToolbarActionViewModel* action_a = model->GetActionModelForId(a);
        ToolbarActionViewModel* action_b = model->GetActionModelForId(b);
        return action_a && action_b &&
               action_a->GetActionName() < action_b->GetActionName();
      });
      ids.insert(ids.end(), rest.begin(), rest.end());
    }

    views::View* row = nullptr;
    auto next_tile_row = [&]() {
      if (!row || row->children().size() == kColumns) {
        row = AddChildView(MakeRow());
      }
      return row;
    };
    for (const auto& id : ids) {
      ToolbarActionViewModel* action = model->GetActionModelForId(id);
      if (!action) {
        continue;
      }
      auto* tile = next_tile_row()->AddChildView(std::make_unique<Tile>(
          base::BindRepeating(&SiteControlsView::RunExtension,
                              base::Unretained(this), id),
          action->GetActionName()));
      tile->SetImageModel(views::Button::STATE_NORMAL,
                          action->GetIcon(contents, gfx::Size(28, 28)));
      auto menu = CreateExtensionMenu(
          action, base::BindRepeating(&SiteControlsView::OnContextMenuClosed,
                                      base::Unretained(this)));
      tile->set_context_menu_controller(menu.get());
      context_menus_.push_back(std::move(menu));
      extension_tiles_.emplace_back(id, tile);
    }
    auto* add = next_tile_row()->AddChildView(std::make_unique<Tile>(
        base::BindRepeating(&SiteControlsView::ExecuteCommand,
                            base::Unretained(this), kManageExtensions, 0),
        l10n_util::GetStringUTF16(IDS_ALERSE_MANAGE_EXTENSIONS)));
    add->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kAddIcon, ui::kColorSysOnSurface, 18));
    UpdatePins();
  }

  void AddFooter() {
    views::View* footer = AddChildView(std::make_unique<views::View>());
    auto* layout = footer->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    secure_button_ = footer->AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&SiteControlsView::OpenPageInfo,
                            base::Unretained(this), std::nullopt),
        l10n_util::GetStringUTF16(IDS_ALERSE_SECURE)));
    secure_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(6, 10)));
    secure_button_->SetImageLabelSpacing(6);
    secure_button_->SetBackground(
        views::CreateRoundedRectBackground(ui::kColorSysNeutralContainer, 8));
    secure_button_->SetVisible(false);

    auto* spacer = footer->AddChildView(std::make_unique<views::View>());
    layout->SetFlexForView(spacer, 1);

    auto more = std::make_unique<views::ImageButton>(base::BindRepeating(
        &SiteControlsView::ShowMoreMenu, base::Unretained(this)));
    more->SetImageModel(views::Button::STATE_NORMAL,
                        ui::ImageModel::FromVectorIcon(
                            kMoreVertIcon, ui::kColorSysOnSurface, 18));
    more->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    more->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    more->SetPreferredSize(gfx::Size(34, 34));
    more->SetBackground(
        views::CreateRoundedRectBackground(ui::kColorSysNeutralContainer, 17));
    more->SetTooltipText(l10n_util::GetStringUTF16(IDS_ALERSE_MORE));
    more->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_ALERSE_MORE));
    more_button_ = footer->AddChildView(std::move(more));
  }

  void UpdatePins() {
    if (!browser_) {
      return;
    }
    ToolbarActionsModel* model =
        ToolbarActionsModel::Get(browser_->GetProfile());
    for (auto& [id, tile] : extension_tiles_) {
      tile->SetPinned(model && model->IsActionPinned(id));
    }
  }

  void RunCommand(int command) {
    // A screenshot waits for the bubble to leave the screen.
    const base::TimeDelta delay = command == IDC_SHARING_HUB_SCREENSHOT
                                      ? base::Milliseconds(250)
                                      : base::TimeDelta();
    CloseThen(base::BindOnce(
        [](base::WeakPtr<BrowserWindowInterface> browser, int command,
           base::TimeDelta delay) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE,
              base::BindOnce(
                  [](base::WeakPtr<BrowserWindowInterface> browser,
                     int command) {
                    if (browser) {
                      chrome::ExecuteCommand(browser.get(), command);
                    }
                  },
                  browser, command),
              delay);
        },
        browser_, command, delay));
  }

  void RunExtension(const ToolbarActionsModel::ActionId& id) {
    CloseThen(base::BindOnce(
        [](std::unique_ptr<views::ViewTracker> extensions,
           ToolbarActionsModel::ActionId id) {
          auto* container =
              static_cast<ExtensionsToolbarDesktop*>(extensions->view());
          if (!container || !container->GetToolbarViewModel()) {
            return;
          }
          container->GetToolbarViewModel()->ExecuteUserAction(
              id, ToolbarActionViewModel::InvocationSource::kMenuEntry);
        },
        std::make_unique<views::ViewTracker>(extensions_), id));
  }

  void OpenPageInfo(std::optional<ContentSettingsType> type) {
    CloseThen(base::BindOnce(
        [](base::WeakPtr<BrowserWindowInterface> browser,
           std::optional<ContentSettingsType> type) {
          if (!browser) {
            return;
          }
          if (content::WebContents* contents =
                  browser->GetTabStripModel()->GetActiveWebContents()) {
            ShowPageInfoDialog(contents, base::DoNothing(),
                               bubble_anchor_util::Anchor::kLocationBar, type);
          }
        },
        browser_, type));
  }

  void ShowMoreMenu() {
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    if (url_.SchemeIsHTTPOrHTTPS()) {
      menu_model_->AddItem(kSiteSettings, l10n_util::GetStringUTF16(IDS_ALERSE_SITE_SETTINGS));
    }
    menu_model_->AddItem(kExtensionSiteAccess,
                         l10n_util::GetStringUTF16(IDS_ALERSE_EXTENSION_SITE_ACCESS));
    menu_model_->AddItem(kManageExtensions,
                         l10n_util::GetStringUTF16(IDS_ALERSE_MANAGE_EXTENSIONS));
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
    menu_runner_->RunMenuAt(GetWidget(), nullptr,
                            more_button_->GetAnchorBoundsInScreen(),
                            views::MenuAnchorPosition::kTopRight,
                            ui::mojom::MenuSourceType::kMouse);
  }

  base::WeakPtr<BrowserWindowInterface> browser_;
  raw_ptr<ExtensionsToolbarDesktop> extensions_;
  GURL url_;
  base::OnceClosure after_close_;

  std::unique_ptr<ChromePageInfoUiDelegate> ui_delegate_;
  std::unique_ptr<PageInfo> presenter_;

  raw_ptr<views::View> settings_heading_ = nullptr;
  raw_ptr<views::View> permissions_ = nullptr;
  base::flat_map<ContentSettingsType, raw_ptr<PermissionToggleRowView>>
      permission_rows_;
  raw_ptr<views::LabelButton> secure_button_ = nullptr;
  raw_ptr<views::ImageButton> more_button_ = nullptr;
  std::vector<std::pair<ToolbarActionsModel::ActionId, raw_ptr<Tile>>>
      extension_tiles_;
  std::vector<std::unique_ptr<ExtensionMenu>> context_menus_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

BEGIN_METADATA(SiteControlsView)
END_METADATA

}  // namespace

views::Widget* Show(views::View* anchor,
                    BrowserWindowInterface* browser,
                    ExtensionsToolbarDesktop* extensions) {
  Hide();
  auto delegate = std::make_unique<views::BubbleDialogDelegate>(
      anchor, views::BubbleBorder::TOP_RIGHT);
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetShowCloseButton(false);
  delegate->SetShowTitle(false);
  delegate->SetTitle(l10n_util::GetStringUTF16(IDS_ALERSE_SITE_CONTROLS));
  delegate->set_margins(gfx::Insets(kMargin));
  delegate->set_fixed_width(kWidth);
  g_shown = delegate->SetContentsView(
      std::make_unique<SiteControlsView>(browser, extensions));
  views::Widget* widget = views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(delegate), views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  widget->Show();
  return widget;
}

bool IsShowing() {
  return g_shown != nullptr;
}

void Hide() {
  if (g_shown && g_shown->GetWidget()) {
    g_shown->GetWidget()->Close();
  }
  g_shown = nullptr;
}

}  // namespace alerse_site_controls
