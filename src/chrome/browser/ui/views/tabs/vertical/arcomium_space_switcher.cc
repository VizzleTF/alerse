// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/arcomium_space_switcher.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_space_color.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "base/token.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_commands.h"
#include "ui/events/event.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/arcomium_spaces.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/vertical/arcomium_sidebar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

// Arc's Space bar: the archive at the left, the Spaces' icons in the middle
// (the active one bright), "+" at the right edge.
constexpr int kButtonSize = 28;
constexpr int kDotDiameter = 6;
constexpr int kAddIconSize = 20;
constexpr int kSpaceIconSize = 16;
// The colors (chrome/browser/ui/color/arcomium_color_mixer.cc).
constexpr ui::ColorId kActiveIconColor = kColorArcomiumSpaceIconActive;
constexpr ui::ColorId kHoveredIconColor = kColorArcomiumSpaceIconHovered;
constexpr ui::ColorId kIconColor = kColorArcomiumSpaceIcon;
constexpr ui::ColorId kArchiveSlotColor = kColorArcomiumArchiveSlot;
// Space icons a folder records; home is a sun until chosen otherwise.
constexpr char kSpaceIconMetaKey[] = "arcomium.space_icon";
constexpr char kDefaultSpace[] = "default";
// Context menu commands: kIconCommandFirst + index into kSpaceIcons.
constexpr int kIconCommandFirst = 100;
constexpr int kEditColorCommand = 1;
constexpr int kDeleteSpaceCommand = 2;
// The plus button's menu.
constexpr int kNewSpaceCommand = 3;
constexpr int kNewFolderCommand = 4;
constexpr int kNewTabCommand = 5;

// Meta info of a Space's bookmark folder: its Space id, and the saved tab
// group it syncs as.
constexpr char kSpaceMetaKey[] = "arcomium.space";
constexpr char kSavedGroupMetaKey[] = "arcomium.group";

constexpr tab_groups::TabGroupColorId kSpaceColors[] = {
    tab_groups::TabGroupColorId::kBlue,   tab_groups::TabGroupColorId::kPurple,
    tab_groups::TabGroupColorId::kGreen,  tab_groups::TabGroupColorId::kPink,
    tab_groups::TabGroupColorId::kOrange, tab_groups::TabGroupColorId::kCyan,
    tab_groups::TabGroupColorId::kYellow, tab_groups::TabGroupColorId::kRed,
};

// The icons a Space can have, under the names Arc gives them.
struct SpaceIconChoice {
  const char* name;
  int label;
  // Null for the icons drawn here.
  const gfx::VectorIcon* icon;
};

const SpaceIconChoice kSpaceIcons[] = {
    {"sunny", IDS_ARCOMIUM_SPACE_ICON_SUN, nullptr},
    {"book", IDS_ARCOMIUM_SPACE_ICON_BOOK, &kMenuBookIcon},
    {"home", IDS_ARCOMIUM_SPACE_ICON_HOME, &vector_icons::kHomeIcon},
    {"code", IDS_ARCOMIUM_SPACE_ICON_CODE, &vector_icons::kCodeIcon},
    {"globe", IDS_ARCOMIUM_SPACE_ICON_GLOBE, &vector_icons::kGlobeIcon},
    {"bolt", IDS_ARCOMIUM_SPACE_ICON_BOLT, &kBoltIcon},
    {"star", IDS_ARCOMIUM_SPACE_ICON_STAR, &kStarIcon},
    {"chat", IDS_ARCOMIUM_SPACE_ICON_CHAT, &vector_icons::kChatIcon},
    {"mail", IDS_ARCOMIUM_SPACE_ICON_MAIL, &vector_icons::kMailIcon},
    {"lightbulb", IDS_ARCOMIUM_SPACE_ICON_IDEA, &vector_icons::kLightbulbIcon},
    {"rocket", IDS_ARCOMIUM_SPACE_ICON_ROCKET, &kRocketLaunchIcon},
    {"pets", IDS_ARCOMIUM_SPACE_ICON_PETS, &vector_icons::kPetsIcon},
    {"flight", IDS_ARCOMIUM_SPACE_ICON_TRAVEL, &vector_icons::kFlightIcon},
    {"dot", IDS_ARCOMIUM_SPACE_ICON_DOT, nullptr},
};

const SpaceIconChoice* FindSpaceIcon(const std::string& name) {
  for (const SpaceIconChoice& choice : kSpaceIcons) {
    if (name == choice.name) {
      return &choice;
    }
  }
  return nullptr;
}

// Draws the Space icon `name` centered on `center`.
void PaintSpaceIcon(gfx::Canvas* canvas,
                    const gfx::PointF& center,
                    const std::string& name,
                    SkColor color) {
  const SpaceIconChoice* choice = FindSpaceIcon(name);
  if (choice && choice->icon) {
    const gfx::ImageSkia image =
        gfx::CreateVectorIcon(*choice->icon, kSpaceIconSize, color);
    canvas->DrawImageInt(image, std::round(center.x() - kSpaceIconSize / 2.0f),
                         std::round(center.y() - kSpaceIconSize / 2.0f));
    return;
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  if (name == "sunny") {
    // Arc's sun: a disc with eight short rays.
    canvas->DrawCircle(center, 3.5f, flags);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.6f);
    flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
    for (int i = 0; i < 8; ++i) {
      const float angle = i * std::numbers::pi_v<float> / 4;
      const gfx::Vector2dF ray(std::cos(angle), std::sin(angle));
      canvas->DrawLine(center + gfx::ScaleVector2d(ray, 5.8f),
                       center + gfx::ScaleVector2d(ray, 7.6f), flags);
    }
    return;
  }
  canvas->DrawCircle(center, kDotDiameter / 2.0f, flags);
}

// A Space: its icon, bright while it is the active one and dimmed otherwise.
class SpaceIcon : public views::Button {
  METADATA_HEADER(SpaceIcon, views::Button)

 public:
  SpaceIcon(PressedCallback callback,
            std::string icon,
            bool active,
            const std::u16string& title,
            std::string space,
            std::string sync_id)
      : views::Button(std::move(callback)),
        icon_(std::move(icon)),
        active_(active),
        space_(std::move(space)),
        sync_id_(std::move(sync_id)) {
    SetTooltipText(title);
    GetViewAccessibility().SetName(title);
    SetPreferredSize(gfx::Size(kButtonSize, kButtonSize));
  }

  // The Space's id, or "" for a saved Space without a tab group here.
  const std::string& space() const { return space_; }
  // The saved tab group of a Space without a tab group here.
  const std::string& sync_id() const { return sync_id_; }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const ui::ColorId color = active_            ? kActiveIconColor
                              : IsMouseHovered() ? kHoveredIconColor
                                                 : kIconColor;
    PaintSpaceIcon(canvas, gfx::RectF(GetContentsBounds()).CenterPoint(),
                   icon_, arcomium::SidebarInk(this, color));
  }

  void StateChanged(ButtonState old_state) override { SchedulePaint(); }

 private:
  const std::string icon_;
  const bool active_;
  const std::string space_;
  const std::string sync_id_;
};

BEGIN_METADATA(SpaceIcon)
END_METADATA

// Arc's archive button: a box with a lid. Opens the history.
class ArchiveButton : public views::Button {
  METADATA_HEADER(ArchiveButton, views::Button)

 public:
  explicit ArchiveButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetTooltipText(l10n_util::GetStringUTF16(IDS_ARCOMIUM_ARCHIVE));
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_ARCOMIUM_ARCHIVE));
    SetPreferredSize(gfx::Size(kButtonSize, kButtonSize));
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::PointF c = gfx::RectF(GetContentsBounds()).CenterPoint();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(arcomium::SidebarInk(
        this, IsMouseHovered() ? kHoveredIconColor : kIconColor));
    canvas->DrawRoundRect(gfx::RectF(c.x() - 7, c.y() - 6, 14, 4), 1.2f,
                          flags);
    canvas->DrawRoundRect(gfx::RectF(c.x() - 6, c.y() - 1, 12, 8), 1.5f,
                          flags);
    flags.setColor(arcomium::SidebarInk(this, kArchiveSlotColor));
    canvas->DrawRoundRect(gfx::RectF(c.x() - 2.5f, c.y() + 1, 5, 1.6f), 0.8f,
                          flags);
  }

  void StateChanged(ButtonState old_state) override { SchedulePaint(); }
};

BEGIN_METADATA(ArchiveButton)
END_METADATA

}  // namespace

ArcomiumSpaceSwitcher::ArcomiumSpaceSwitcher(BrowserWindowInterface* browser)
    : browser_(browser), model_(browser->GetTabStripModel()) {
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(4, 6)));
  model_->AddObserver(this);
  if (!browser->GetProfile()->IsOffTheRecord()) {
    sync_service_ = tab_groups::TabGroupSyncServiceFactory::GetForProfile(
        browser->GetProfile());
  }
  if (sync_service_) {
    sync_observation_.Observe(sync_service_);
  }
  if (bookmarks::BookmarkModel* bookmarks =
          BookmarkModelFactory::GetForBrowserContext(browser->GetProfile())) {
    bookmark_observation_.Observe(bookmarks);
  }
  Rebuild();
}

ArcomiumSpaceSwitcher::~ArcomiumSpaceSwitcher() = default;

// static
void ArcomiumSpaceSwitcher::CreateSpace(BrowserWindowInterface* browser) {
  Profile* profile = browser->GetProfile();
  TabStripModel* model = browser->GetTabStripModel();
  if (profile->IsOffTheRecord() || !model->SupportsTabGroups()) {
    return;
  }
  const std::string space =
      base::ToLowerASCII(base::Token::CreateRandom().ToString());

  const size_t count = model->group_model()->ListTabGroups().size();
  // Name the Space after a Space imported from Arc that has no Arcomium Space
  // yet; the sidebar then adopts its pinned folder.
  std::u16string title = u"Space " + base::NumberToString16(count + 1);
  if (bookmarks::BookmarkModel* bookmarks =
          BookmarkModelFactory::GetForBrowserContext(profile);
      bookmarks && bookmarks->loaded()) {
    for (const auto& folder : bookmarks->other_node()->children()) {
      std::string value;
      if (folder->is_folder() &&
          folder->GetMetaInfo("arcomium.space", &value) &&
          value.starts_with("arc:")) {
        title = folder->GetTitle();
        break;
      }
    }
  }
  OpenSpace(browser, space, title);
  chrome::FocusLocationBar(browser);
}

// static
void ArcomiumSpaceSwitcher::OpenSpace(BrowserWindowInterface* browser,
                                      const std::string& space,
                                      const std::u16string& title) {
  Profile* profile = browser->GetProfile();
  TabStripModel* model = browser->GetTabStripModel();
  if (profile->IsOffTheRecord() || !model->SupportsTabGroups()) {
    return;
  }
  // A blank tab with no history: the Space's hidden one, so it opens empty
  // (TabStripModel::IsArcomiumEmptySpace()).
  const GURL url("about:blank#arcomium-space");
  content::WebContents::CreateParams create_params(
      profile, arcomium::CreateSpaceSiteInstance(profile, url, space));
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(create_params);
  contents->GetController().LoadURL(url, content::Referrer(),
                                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                    std::string());
  // Marked before it shows, or it takes the address bar's focus: an empty
  // Space waits for a bookmark or for typing.
  TabStripModel::MarkArcomiumEmptySpace(contents.get());
  model->AddWebContents(std::move(contents), -1,
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                        AddTabTypes::ADD_ACTIVE);

  const size_t count = model->group_model()->ListTabGroups().size();
  const tab_groups::TabGroupId group =
      model->AddToNewGroup({model->active_index()});
  model->ChangeTabGroupVisuals(
      group, tab_groups::TabGroupVisualData(
                 title, kSpaceColors[count % std::size(kSpaceColors)]));
  model->EnterFocusMode(group);
}

void ArcomiumSpaceSwitcher::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }
  // A tab still in the strip, and the Space it is in.
  auto space_of = [this](tabs::TabInterface* tab)
      -> std::optional<std::optional<tab_groups::TabGroupId>> {
    if (!tab || model_->GetIndexOfTab(tab) == TabStripModel::kNoTab) {
      return std::nullopt;
    }
    return tab->GetGroup();
  };
  const auto old_space = space_of(selection.old_tab);
  const auto new_space = space_of(selection.new_tab);
  // Switching Spaces activates a tab of the new Space that the user did not
  // pick, so the new tab only counts while moving within one Space; the one
  // left behind counts when leaving it.
  if (new_space && (!old_space || *old_space == *new_space)) {
    last_active_tabs_[*new_space] = selection.new_tab->GetHandle();
  } else if (old_space && new_space) {
    last_active_tabs_[*old_space] = selection.old_tab->GetHandle();
  }
}

void ArcomiumSpaceSwitcher::RestoreActiveTab(
    std::optional<tab_groups::TabGroupId> space) {
  if (model_->GetFocusedGroup() != space) {
    return;
  }
  auto it = last_active_tabs_.find(space);
  tabs::TabInterface* tab = it != last_active_tabs_.end() ? it->second.Get()
                                                          : nullptr;
  int index = tab && tab->GetGroup() == space ? model_->GetIndexOfTab(tab)
                                              : TabStripModel::kNoTab;
  // Home never visited yet: its first tab, not a tab of the Space just left.
  if (index == TabStripModel::kNoTab && !space) {
    for (int i = 0; i < model_->count(); ++i) {
      if (!model_->GetTabAtIndex(i)->GetGroup()) {
        index = i;
        break;
      }
    }
  }
  if (index != TabStripModel::kNoTab && index != model_->active_index()) {
    model_->ActivateTabAt(index);
  }
}

void ArcomiumSpaceSwitcher::OnTabGroupChanged(const TabGroupChange& change) {
  if (change.type == TabGroupChange::kCreated ||
      change.type == TabGroupChange::kVisualsChanged ||
      change.type == TabGroupChange::kMoved ||
      change.type == TabGroupChange::kClosed) {
    ScheduleRebuild();
  }
}

void ArcomiumSpaceSwitcher::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  ScheduleRebuild();
  // Posted: the tab strip does not take changes from its observers.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcomiumSpaceSwitcher::RestoreActiveTab,
                                weak_factory_.GetWeakPtr(),
                                new_focused_group_id));
}

void ArcomiumSpaceSwitcher::OnInitialized() {
  sync_initialized_ = true;
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  // A saved group changes on every navigation in it; the icons only show its
  // title, color, whether it is open here and whether it is empty.
  std::string signature = base::StrCat(
      {base::UTF16ToUTF8(group.title()), "|",
       base::NumberToString(static_cast<int>(group.color())), "|",
       group.local_group_id() ? "open" : "closed", "|",
       group.saved_tabs().empty() ? "empty" : "tabs"});
  std::string& known = group_signatures_[group.saved_guid()];
  if (known == signature) {
    return;
  }
  known = std::move(signature);
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::OnTabGroupRemoved(
    const base::Uuid& sync_id,
    tab_groups::TriggerSource source) {
  group_signatures_.erase(sync_id);
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::BookmarkModelChanged() {
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::BookmarkMetaInfoChanged(
    const bookmarks::BookmarkNode* node) {
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::BookmarkModelBeingDeleted() {
  bookmark_observation_.Reset();
}

void ArcomiumSpaceSwitcher::RetryRebuildLater() {
  // One retry at a time, however many rebuilds ask for it.
  if (!retry_timer_.IsRunning()) {
    retry_timer_.Start(FROM_HERE, base::Seconds(2),
                       base::BindOnce(&ArcomiumSpaceSwitcher::ScheduleRebuild,
                                      base::Unretained(this)));
  }
}

void ArcomiumSpaceSwitcher::ScheduleRebuild() {
  // Coalesced into one pending rebuild. Its own factory: invalidating the one
  // the delete dialog and RestoreActiveTab() hold would drop them.
  rebuild_weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcomiumSpaceSwitcher::Rebuild,
                                rebuild_weak_factory_.GetWeakPtr()));
}

bool ArcomiumSpaceSwitcher::RotateSpace(bool forward) {
  const int next = active_space_ + (forward ? 1 : -1);
  if (active_space_ < 0 || next < 0 ||
      next >= static_cast<int>(space_actions_.size())) {
    return false;
  }
  // A copy: switching rebuilds the row.
  base::RepeatingClosure action = space_actions_[next];
  action.Run();
  return true;
}

bool ArcomiumSpaceSwitcher::CanRotateSpace(bool forward) const {
  const int next = active_space_ + (forward ? 1 : -1);
  return active_space_ >= 0 && next >= 0 &&
         next < static_cast<int>(space_actions_.size());
}

std::optional<ArcomiumSpaceRef> ArcomiumSpaceSwitcher::GetNeighborSpace(
    bool forward) const {
  if (!CanRotateSpace(forward)) {
    return std::nullopt;
  }
  return space_refs_[active_space_ + (forward ? 1 : -1)];
}

void ArcomiumSpaceSwitcher::Rebuild() {
  // Opening one changes the tab strip, which schedules another rebuild.
  if (OpenSavedSpacesHere() || MergeDuplicateSpaces()) {
    return;
  }
  add_button_ = nullptr;
  RemoveAllChildViews();
  TabGroupModel* groups = model_->group_model();
  if (!groups) {
    return;
  }

  std::vector<tab_groups::TabGroupId> spaces = groups->ListTabGroups();
  std::ranges::sort(spaces, {}, [groups](const tab_groups::TabGroupId& id) {
    return groups->GetTabGroup(id)->ListTabs().start();
  });
  const std::optional<tab_groups::TabGroupId> focused =
      model_->GetFocusedGroup();

  AddChildView(std::make_unique<ArchiveButton>(base::BindRepeating(
      [](BrowserWindowInterface* browser) { chrome::ShowHistory(browser); },
      base::Unretained(browser_.get()))));
  views::View* icons = AddChildView(std::make_unique<views::View>());
  icons->SetLayoutManager(std::make_unique<views::BoxLayout>(
                              views::BoxLayout::Orientation::kHorizontal))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout_->SetFlexForView(icons, 1);

  space_actions_.clear();
  space_refs_.clear();
  active_space_ = -1;
  auto add_icon = [&](base::RepeatingClosure callback,
                      std::string icon, bool active,
                      const std::u16string& title, std::string space,
                      std::string sync_id) {
    if (active) {
      active_space_ = static_cast<int>(space_actions_.size());
    }
    space_actions_.push_back(callback);
    space_refs_.push_back(ArcomiumSpaceRef{
        space, space == kDefaultSpace ? std::u16string() : title, std::nullopt});
    auto* view = icons->AddChildView(std::make_unique<SpaceIcon>(
        views::Button::PressedCallback(std::move(callback)), std::move(icon),
        active, title, std::move(space),
        std::move(sync_id)));
    view->set_context_menu_controller(this);
  };

  // The tabs outside of any Space form the first, default Space.
  add_icon(base::BindRepeating(
               [](TabStripModel* model) {
                 model->ExitFocusMode(
                     TabGroupFocusExitReason::kActiveTabGroupOperation);
               },
               base::Unretained(model_.get())),
           GetSpaceIcon(kDefaultSpace, std::string()), !focused.has_value(),
           u"home", kDefaultSpace, std::string());
  for (const tab_groups::TabGroupId& id : spaces) {
    const tab_groups::TabGroupVisualData* visuals =
        groups->GetTabGroup(id)->visual_data();
    const std::string space =
        arcomium::GetSpaceOfGroupOrFolder(browser_->GetProfile(), model_, id)
            .value_or(std::string());
    add_icon(base::BindRepeating(&ArcomiumSpaceSwitcher::SwitchTo,
                                 base::Unretained(this), id),
             GetSpaceIcon(space, std::string()), focused == id,
             visuals->title().empty() ? u"Space" : visuals->title(), space,
             std::string());
    space_refs_.back().group = id;
    space_refs_.back().title = visuals->title();
  }
  // Saved Spaces without a tab group here: from other devices, or closed.
  std::set<std::string> shown_saved;
  if (sync_service_) {
    // Only once its storage partition is known: opened before, its tabs would
    // stay out of the Space's partition.
    const std::set<std::string> linked = GetLinkedSavedGroups();
    for (const tab_groups::SavedTabGroup& saved :
         sync_service_->GetAllGroups()) {
      const std::string sync_id = saved.saved_guid().AsLowercaseString();
      if (saved.local_group_id() || saved.is_shared_tab_group() ||
          saved.saved_tabs().empty() || !linked.contains(sync_id)) {
        continue;
      }
      shown_saved.insert(sync_id);
      add_icon(base::BindRepeating(&ArcomiumSpaceSwitcher::OpenSavedSpace,
                                   base::Unretained(this), saved.saved_guid()),
               GetSpaceIcon(std::string(), sync_id), /*active=*/false,
               saved.title().empty() ? u"Space" : saved.title(), std::string(),
               sync_id);
    }
  }
  // Spaces whose tabs all closed, and with them their tab group: their
  // bookmark folder is left, and they open from it again, as in Arc, where a
  // Space stays until it is deleted.
  if (const bookmarks::BookmarkModel* bookmarks =
          BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
      bookmarks && bookmarks->loaded()) {
    std::set<std::string> open_spaces;
    for (const tab_groups::TabGroupId& id : spaces) {
      if (std::optional<std::string> space =
              arcomium::GetSpaceOfGroupOrFolder(browser_->GetProfile(),
                                                model_, id)) {
        open_spaces.insert(*space);
      }
    }
    for (const auto& folder : bookmarks->other_node()->children()) {
      std::string space;
      std::string saved;
      if (!folder->is_folder() ||
          !folder->GetMetaInfo(kSpaceMetaKey, &space) || space.empty() ||
          space == kDefaultSpace || space.starts_with("arc:") ||
          open_spaces.contains(space) ||
          shown_saved.contains(
              arcomium::SavedGroupGuidForSpace(space).AsLowercaseString()) ||
          (folder->GetMetaInfo(kSavedGroupMetaKey, &saved) &&
           shown_saved.contains(saved))) {
        continue;
      }
      add_icon(base::BindRepeating(&ArcomiumSpaceSwitcher::OpenSpace,
                                   base::Unretained(browser_.get()), space,
                                   folder->GetTitle()),
               GetSpaceIcon(space, std::string()), /*active=*/false,
               folder->GetTitle().empty() ? u"Space" : folder->GetTitle(),
               space, std::string());
    }
  }
  LinkSpaceFolders();
  ApplyActiveSpaceColor();

  auto add = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&ArcomiumSpaceSwitcher::ShowAddMenu,
                          base::Unretained(this)),
      vector_icons::kAdd2Icon, kAddIconSize);
  add->SetPreferredSize(gfx::Size(kButtonSize, kButtonSize));
  add->SetTooltipText(l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_SPACE));
  add->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_SPACE));
  add_button_ = AddChildView(std::move(add));

  PreferredSizeChanged();
}

const bookmarks::BookmarkNode* ArcomiumSpaceSwitcher::FindSpaceFolder(
    const std::string& space,
    const std::string& sync_id) const {
  const bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (!bookmarks || !bookmarks->loaded()) {
    return nullptr;
  }
  for (const auto& folder : bookmarks->other_node()->children()) {
    std::string value;
    if (!folder->is_folder()) {
      continue;
    }
    if (!folder->GetMetaInfo(kSpaceMetaKey, &value)) {
      continue;
    }
    if ((!space.empty() && value == space) ||
        (!sync_id.empty() && arcomium::IsGroupSpace(value) &&
         arcomium::SavedGroupGuidForSpace(value).AsLowercaseString() ==
             sync_id)) {
      return folder.get();
    }
    if (!sync_id.empty() && folder->GetMetaInfo(kSavedGroupMetaKey, &value) &&
        value == sync_id) {
      return folder.get();
    }
  }
  return nullptr;
}

std::string ArcomiumSpaceSwitcher::GetSpaceIcon(
    const std::string& space,
    const std::string& sync_id) const {
  std::string icon;
  if (const bookmarks::BookmarkNode* folder = FindSpaceFolder(space, sync_id);
      folder && folder->GetMetaInfo(kSpaceIconMetaKey, &icon) &&
      FindSpaceIcon(icon)) {
    return icon;
  }
  return space == kDefaultSpace ? "sunny" : "dot";
}

void ArcomiumSpaceSwitcher::AddedToWidget() {
  ApplyActiveSpaceColor();
#if !BUILDFLAG(IS_MAC)
  // Alt+1 to Alt+9 show the Spaces in the row's order, home first, as in Arc
  // for Windows. Not on macOS: Option with a digit types a character there.
  for (int i = 0; i < 9; ++i) {
    AddAccelerator(ui::Accelerator(
        static_cast<ui::KeyboardCode>(ui::VKEY_1 + i), ui::EF_ALT_DOWN));
  }
#endif
}

bool ArcomiumSpaceSwitcher::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  const int index = accelerator.key_code() - ui::VKEY_1;
  if (accelerator.modifiers() != ui::EF_ALT_DOWN || index < 0 || index > 8 ||
      index >= static_cast<int>(space_actions_.size())) {
    return false;
  }
  if (index != active_space_) {
    // A copy: switching rebuilds the row and its callbacks.
    base::RepeatingClosure action = space_actions_[index];
    action.Run();
  }
  return true;
}

void ArcomiumSpaceSwitcher::ApplyActiveSpaceColor() {
  if (!GetWidget()) {
    return;
  }
  std::string space = kDefaultSpace;
  if (std::optional<tab_groups::TabGroupId> focused =
          model_->GetFocusedGroup()) {
    space = arcomium::GetSpaceOfGroupOrFolder(browser_->GetProfile(), model_,
                                              *focused)
                .value_or(std::string());
  }
  std::optional<arcomium::SpaceColor> color;
  std::string value;
  if (const bookmarks::BookmarkNode* folder =
          space.empty() ? nullptr : FindSpaceFolder(space, std::string());
      folder && folder->GetMetaInfo(arcomium::kSpaceColorMetaKey, &value)) {
    color = arcomium::ParseSpaceColor(value);
  }
  // The first color a window gets is set at once; a switch of Space after it
  // blends into the next Space's color.
  const bool animate = !!color_transition_;
  if (!color_transition_) {
    color_transition_ = std::make_unique<arcomium::SpaceColorTransition>();
  }
  color_transition_->AnimateTo(GetWidget(), color, animate);
}

void ArcomiumSpaceSwitcher::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  const SpaceIcon* icon = views::AsViewClass<SpaceIcon>(source);
  if (!icon) {
    return;
  }
  // The previous menu's runner refers to the models replaced below.
  menu_runner_.reset();
  menu_space_ = icon->space();
  menu_sync_id_ = icon->sync_id();
  icon_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  for (size_t i = 0; i < std::size(kSpaceIcons); ++i) {
    const SpaceIconChoice& choice = kSpaceIcons[i];
    const int command = kIconCommandFirst + static_cast<int>(i);
    if (choice.icon) {
      icon_menu_model_->AddItemWithIcon(
          command, l10n_util::GetStringUTF16(choice.label),
          ui::ImageModel::FromVectorIcon(*choice.icon, ui::kColorMenuIcon,
                                         kSpaceIconSize));
    } else {
      icon_menu_model_->AddItem(command,
                                l10n_util::GetStringUTF16(choice.label));
    }
  }
  menu_point_ = point;
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddSubMenu(0, l10n_util::GetStringUTF16(IDS_ARCOMIUM_SPACE_ICON),
                           icon_menu_model_.get());
  menu_model_->AddItem(kEditColorCommand,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_EDIT_THEME_COLOR));
  // home is where tabs outside any Space go; it cannot be deleted.
  if (menu_space_ != kDefaultSpace) {
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kDeleteSpaceCommand,
                         l10n_util::GetStringUTF16(IDS_ARCOMIUM_DELETE_SPACE));
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kBubbleTopRight,
                          source_type);
}

void ArcomiumSpaceSwitcher::ShowAddMenu(const ui::Event& event) {
  views::View* button = add_button_;
  if (!button || !button->GetWidget()) {
    return;
  }
  // Arc's plus button: a new Space, folder or tab.
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItem(kNewSpaceCommand,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_SPACE));
  menu_model_->AddItem(kNewFolderCommand,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_FOLDER));
  menu_model_->AddItem(kNewTabCommand,
                       l10n_util::GetStringUTF16(IDS_ARCOMIUM_NEW_TAB));
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(button->GetWidget(), nullptr,
                          button->GetBoundsInScreen(),
                          views::MenuAnchorPosition::kBubbleTopLeft,
                          event.IsKeyEvent() ? ui::mojom::MenuSourceType::kKeyboard
                                             : ui::mojom::MenuSourceType::kMouse);
}

void ArcomiumSpaceSwitcher::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == kNewSpaceCommand) {
    CreateSpace(browser_);
    return;
  }
  if (command_id == kNewTabCommand) {
    chrome::ExecuteCommand(browser_, IDC_NEW_TAB);
    return;
  }
  if (command_id == kNewFolderCommand) {
    // The sidebar beside this row, in the same window.
    views::View* root = GetWidget() ? GetWidget()->GetRootView() : nullptr;
    std::vector<views::View*> views;
    if (root) {
      views.push_back(root);
    }
    while (!views.empty()) {
      views::View* view = views.back();
      views.pop_back();
      if (auto* sidebar = views::AsViewClass<ArcomiumSidebarView>(view)) {
        sidebar->NewFolder();
        return;
      }
      for (views::View* child : view->children()) {
        views.push_back(child);
      }
    }
    return;
  }
  if (command_id == kEditColorCommand) {
    EditMenuSpaceColor();
    return;
  }
  if (command_id == kDeleteSpaceCommand) {
    ConfirmDeleteMenuSpace();
    return;
  }
  const int index = command_id - kIconCommandFirst;
  if (index < 0 || index >= static_cast<int>(std::size(kSpaceIcons))) {
    return;
  }
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (!bookmarks || !bookmarks->loaded()) {
    return;
  }
  const bookmarks::BookmarkNode* folder =
      FindSpaceFolder(menu_space_, menu_sync_id_);
  if (!folder && !menu_space_.empty()) {
    // The Space has no folder yet: home before anything was pinned to it.
    bookmarks::BookmarkNode::MetaInfoMap meta;
    meta[kSpaceMetaKey] = menu_space_;
    folder = bookmarks->AddFolder(
        bookmarks->other_node(), bookmarks->other_node()->children().size(),
        menu_space_ == kDefaultSpace ? u"home" : u"Space", &meta);
  }
  if (folder) {
    bookmarks->SetNodeMetaInfo(folder, kSpaceIconMetaKey,
                               kSpaceIcons[index].name);
  }
}

std::optional<tab_groups::TabGroupId> ArcomiumSpaceSwitcher::FindGroupOfSpace(
    const std::string& space) const {
  if (space.empty() || !model_->group_model()) {
    return std::nullopt;
  }
  for (const tab_groups::TabGroupId& id :
       model_->group_model()->ListTabGroups()) {
    if (arcomium::GetSpaceOfGroupOrFolder(browser_->GetProfile(), model_, id) ==
        space) {
      return id;
    }
  }
  return std::nullopt;
}

void ArcomiumSpaceSwitcher::ActivateMenuSpace() {
  if (menu_space_ == kDefaultSpace) {
    model_->ExitFocusMode(TabGroupFocusExitReason::kActiveTabGroupOperation);
    return;
  }
  if (std::optional<tab_groups::TabGroupId> group =
          FindGroupOfSpace(menu_space_)) {
    SwitchTo(*group);
    return;
  }
  if (!menu_sync_id_.empty()) {
    OpenSavedSpace(base::Uuid::ParseLowercase(menu_sync_id_));
    return;
  }
  if (const bookmarks::BookmarkNode* folder =
          FindSpaceFolder(menu_space_, std::string())) {
    OpenSpace(browser_, menu_space_, folder->GetTitle());
  }
}

void ArcomiumSpaceSwitcher::EditMenuSpaceColor() {
  ActivateMenuSpace();
  // Once the sidebar shows the Space: the picker edits the active one.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<ArcomiumSpaceSwitcher> switcher) {
                       if (!switcher || !switcher->GetWidget()) {
                         return;
                       }
                       // The sidebar is in the same window.
                       std::vector<views::View*> views = {
                           switcher->GetWidget()->GetRootView()};
                       while (!views.empty()) {
                         views::View* view = views.back();
                         views.pop_back();
                         if (auto* sidebar =
                                 views::AsViewClass<ArcomiumSidebarView>(
                                     view)) {
                           sidebar->EditSpaceColorAt(switcher->menu_point_);
                           return;
                         }
                         for (views::View* child : view->children()) {
                           views.push_back(child);
                         }
                       }
                     },
                     weak_factory_.GetWeakPtr()));
}

void ArcomiumSpaceSwitcher::ConfirmDeleteMenuSpace() {
  if (menu_space_ == kDefaultSpace || !GetWidget()) {
    return;
  }
  std::u16string body =
      l10n_util::GetStringUTF16(IDS_ARCOMIUM_DELETE_UNNAMED_SPACE_BODY);
  if (const bookmarks::BookmarkNode* folder =
          FindSpaceFolder(menu_space_, menu_sync_id_);
      folder && !folder->GetTitle().empty()) {
    body = l10n_util::GetStringFUTF16(IDS_ARCOMIUM_DELETE_SPACE_BODY,
                                      folder->GetTitle());
  }
  chrome::ShowQuestionMessageBoxAsync(
      GetWidget()->GetNativeWindow(),
      l10n_util::GetStringUTF16(IDS_ARCOMIUM_DELETE_SPACE_TITLE), body,
      base::BindOnce(&ArcomiumSpaceSwitcher::DeleteSpace,
                     weak_factory_.GetWeakPtr(), menu_space_, menu_sync_id_));
}

void ArcomiumSpaceSwitcher::DeleteSpace(const std::string& space,
                                        const std::string& sync_id,
                                        chrome::MessageBoxResult result) {
  if (result != chrome::MESSAGE_BOX_RESULT_YES || space == kDefaultSpace) {
    return;
  }
  // Found before the folder goes: an open Space is known by its folder too.
  const bookmarks::BookmarkNode* folder = FindSpaceFolder(space, sync_id);
  const int64_t folder_id = folder ? folder->id() : 0;
  std::optional<base::Uuid> saved;
  if (!sync_id.empty()) {
    saved = base::Uuid::ParseLowercase(sync_id);
  }
  if (std::optional<tab_groups::TabGroupId> group = FindGroupOfSpace(space)) {
    if (sync_service_) {
      if (std::optional<tab_groups::SavedTabGroup> saved_group =
              sync_service_->GetGroup(*group)) {
        saved = saved_group->saved_guid();
      }
    }
    model_->CloseAllTabsInGroup(*group);
  }
  if (saved && saved->is_valid() && sync_service_ &&
      sync_service_->GetGroup(*saved)) {
    sync_service_->RemoveGroup(*saved);
  }
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (bookmarks && bookmarks->loaded() && folder_id) {
    if (const bookmarks::BookmarkNode* node =
            bookmarks::GetBookmarkNodeByID(bookmarks, folder_id)) {
      bookmarks->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                        FROM_HERE);
    }
  }
  ScheduleRebuild();
}

void ArcomiumSpaceSwitcher::SwitchTo(const tab_groups::TabGroupId& group) {
  if (model_->group_model() && model_->group_model()->ContainsTabGroup(group)) {
    model_->EnterFocusMode(group);
  }
}

void ArcomiumSpaceSwitcher::OpenSavedSpace(const base::Uuid& sync_id) {
  if (!sync_service_) {
    return;
  }
  std::optional<tab_groups::TabGroupId> group = sync_service_->OpenTabGroup(
      sync_id, std::make_unique<tab_groups::TabGroupActionContextDesktop>(
                   browser_, tab_groups::OpeningSource::kOpenedFromRevisitUi));
  if (group) {
    SwitchTo(*group);
  }
}

bool ArcomiumSpaceSwitcher::OpenSavedSpacesHere() {
  if (!sync_service_ || !sync_initialized_ || !model_->group_model() ||
      model_->closing_all()) {
    return false;
  }
  const bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (SessionRestore::IsRestoring(browser_->GetProfile()) || !bookmarks ||
      !bookmarks->loaded()) {
    // Restore brings back the Spaces open before, and the Spaces' folders
    // tell which saved groups are Spaces: asked again after both. Not left to
    // the bookmark model's load notification, which may come first.
    RetryRebuildLater();
    return false;
  }
  // Each saved Space is looked at once: one open here already, or one this
  // window opened, is not opened again when its group closes, as all do
  // when the window closes. Only once its storage partition is known, as for
  // the switcher's icons.
  const std::set<std::string> linked = GetLinkedSavedGroups();
  std::vector<base::Uuid> to_open;
  for (const tab_groups::SavedTabGroup& saved : sync_service_->GetAllGroups()) {
    const std::string sync_id = saved.saved_guid().AsLowercaseString();
    if (saved.is_shared_tab_group() || saved.saved_tabs().empty() ||
        !linked.contains(sync_id) || !opened_saved_.insert(sync_id).second ||
        saved.local_group_id()) {
      continue;
    }
    to_open.push_back(saved.saved_guid());
  }
  if (to_open.empty()) {
    return false;
  }
  // Opening a saved group leaves the focused one and activates its tabs; the
  // Space and tab shown stay as they were.
  const std::optional<tab_groups::TabGroupId> focused =
      model_->GetFocusedGroup();
  tabs::TabInterface* const active = model_->GetActiveTab();
  for (const base::Uuid& sync_id : to_open) {
    sync_service_->OpenTabGroup(
        sync_id, std::make_unique<tab_groups::TabGroupActionContextDesktop>(
                     browser_, tab_groups::OpeningSource::kOpenedFromRevisitUi));
  }
  if (focused && model_->group_model()->ContainsTabGroup(*focused)) {
    model_->EnterFocusMode(*focused);
  } else if (!focused && model_->GetFocusedGroup()) {
    model_->ExitFocusMode(TabGroupFocusExitReason::kActiveTabGroupOperation);
  }
  if (active) {
    if (const int index = model_->GetIndexOfTab(active);
        index != TabStripModel::kNoTab && index != model_->active_index()) {
      model_->ActivateTabAt(index);
    }
  }
  return true;
}

bool ArcomiumSpaceSwitcher::MergeDuplicateSpaces() {
  // Once per profile and run, once session restore has opened every window:
  // before that, groups still missing would look like nothing to merge, and
  // deleting a group deletes it on every device.
  static base::NoDestructor<std::set<const Profile*>> merged;
  Profile* const profile = browser_->GetProfile();
  TabGroupModel* const groups = model_->group_model();
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(profile);
  if (!sync_service_ || !sync_initialized_ || !groups || !bookmarks ||
      merged->contains(profile)) {
    return false;
  }
  if (SessionRestore::IsRestoring(profile) || !bookmarks->loaded()) {
    // Asked again once restore is over and the folders are known: nothing
    // else may rebuild the row.
    RetryRebuildLater();
    return false;
  }
  merged->insert(profile);

  bool changed = false;
  for (const auto& folder : bookmarks->other_node()->children()) {
    std::string space;
    if (!folder->is_folder() || !folder->GetMetaInfo(kSpaceMetaKey, &space) ||
        !arcomium::IsGroupSpace(space)) {
      continue;
    }
    const base::Uuid canonical = arcomium::SavedGroupGuidForSpace(space);
    // The Space's groups here: by their saved group's id, or by the
    // partition of their tabs for a group no folder names.
    std::vector<tab_groups::TabGroupId> mine;
    for (const tab_groups::TabGroupId& id : groups->ListTabGroups()) {
      const std::optional<tab_groups::SavedTabGroup> saved =
          sync_service_->GetGroup(id);
      if (!saved) {
        continue;
      }
      std::optional<std::string> owner =
          arcomium::GetSpaceOfSavedGroup(profile, saved->saved_guid());
      if (!owner) {
        owner = arcomium::GetSpaceOfGroup(model_, id);
      }
      if (owner == space) {
        mine.push_back(id);
      }
    }
    if (mine.empty() ||
        (mine.size() == 1 &&
         sync_service_->GetGroup(mine[0])->saved_guid() == canonical)) {
      continue;
    }
    LOG(WARNING) << "Arcomium: merging " << mine.size()
                 << " tab groups of Space " << space;

    // The group that stays: the one with the derived id, or else the largest,
    // saved again under the derived id.
    std::optional<tab_groups::TabGroupId> kept;
    for (const tab_groups::TabGroupId& id : mine) {
      if (sync_service_->GetGroup(id)->saved_guid() == canonical) {
        kept = id;
      }
    }
    if (!kept) {
      if (const std::optional<tab_groups::SavedTabGroup> derived =
              sync_service_->GetGroup(canonical);
          derived && !derived->saved_tabs().empty()) {
        // Saved elsewhere with tabs but not open here: OpenSavedSpacesHere()
        // opens such a group before this runs, unless it was closed here.
        continue;
      }
      // Saved again under the derived id. If an empty group has that id
      // already, the tabs join it (TabGroupSyncServiceImpl::AddGroup()).
      kept = *std::ranges::max_element(
          mine, {}, [groups](const tab_groups::TabGroupId& id) {
            return groups->GetTabGroup(id)->tab_count();
          });
      sync_service_->UnsaveGroup(*kept);
      sync_service_->SaveGroup(
          tab_groups::SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
              *kept));
    }

    // Tabs of the other groups move into it, but for pages it has already;
    // their saved groups go.
    std::set<GURL> urls;
    const gfx::Range kept_tabs = groups->GetTabGroup(*kept)->ListTabs();
    for (size_t i = kept_tabs.start(); i < kept_tabs.end(); ++i) {
      urls.insert(
          model_->GetWebContentsAt(static_cast<int>(i))->GetVisibleURL());
    }
    for (const tab_groups::TabGroupId& id : mine) {
      if (id == *kept) {
        continue;
      }
      const base::Uuid old_guid = sync_service_->GetGroup(id)->saved_guid();
      std::vector<tabs::TabInterface*> tabs;
      const gfx::Range range = groups->GetTabGroup(id)->ListTabs();
      for (size_t i = range.start(); i < range.end(); ++i) {
        tabs.push_back(model_->GetTabAtIndex(static_cast<int>(i)));
      }
      for (tabs::TabInterface* tab : tabs) {
        const int index = model_->GetIndexOfTab(tab);
        if (index == TabStripModel::kNoTab) {
          continue;
        }
        content::WebContents* contents = tab->GetContents();
        if (TabStripModel::IsArcomiumEmptySpace(contents) ||
            !urls.insert(contents->GetVisibleURL()).second) {
          model_->CloseWebContentsAt(index, TabCloseTypes::CLOSE_NONE);
        } else {
          model_->AddToExistingGroup({index}, *kept, /*add_to_end=*/true);
        }
      }
      if (sync_service_->GetGroup(old_guid)) {
        sync_service_->RemoveGroup(old_guid);
      }
    }
    changed = true;
  }
  return changed;
}

std::set<std::string> ArcomiumSpaceSwitcher::GetLinkedSavedGroups() const {
  std::set<std::string> linked;
  const bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  if (!bookmarks || !bookmarks->loaded()) {
    return linked;
  }
  // A Space's group has the id derived from it; a folder may still name an
  // older one, until MergeDuplicateSpaces() folds it in.
  for (const auto& folder : bookmarks->other_node()->children()) {
    std::string space;
    if (!folder->is_folder() || !folder->GetMetaInfo(kSpaceMetaKey, &space) ||
        !arcomium::IsGroupSpace(space)) {
      continue;
    }
    linked.insert(arcomium::SavedGroupGuidForSpace(space).AsLowercaseString());
    std::string group;
    if (folder->GetMetaInfo(kSavedGroupMetaKey, &group)) {
      linked.insert(group);
    }
  }
  return linked;
}

void ArcomiumSpaceSwitcher::LinkSpaceFolders() {
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  TabGroupModel* groups = model_->group_model();
  if (!sync_service_ || !bookmarks || !bookmarks->loaded() || !groups) {
    return;
  }
  for (const tab_groups::TabGroupId& id : groups->ListTabGroups()) {
    const std::optional<std::string> space =
        arcomium::GetSpaceOfGroupOrFolder(browser_->GetProfile(), model_, id);
    const std::optional<tab_groups::SavedTabGroup> saved =
        sync_service_->GetGroup(id);
    if (!space || !saved) {
      continue;
    }
    const bookmarks::BookmarkNode* folder = nullptr;
    for (const auto& child : bookmarks->other_node()->children()) {
      std::string value;
      if (child->is_folder() && child->GetMetaInfo(kSpaceMetaKey, &value) &&
          value == *space) {
        folder = child.get();
        break;
      }
    }
    const std::u16string& title =
        groups->GetTabGroup(id)->visual_data()->title();
    if (!folder) {
      // Adopt a folder imported from Arc for a Space of the same name, as the
      // sidebar does.
      for (const auto& child : bookmarks->other_node()->children()) {
        std::string value;
        if (child->is_folder() && child->GetTitle() == title &&
            child->GetMetaInfo(kSpaceMetaKey, &value) &&
            value.starts_with("arc:")) {
          folder = child.get();
          bookmarks->SetNodeMetaInfo(folder, kSpaceMetaKey, *space);
          break;
        }
      }
    }
    if (!folder) {
      bookmarks::BookmarkNode::MetaInfoMap meta;
      meta[kSpaceMetaKey] = *space;
      bookmarks->AddFolder(bookmarks->other_node(),
                           bookmarks->other_node()->children().size(),
                           title.empty() ? u"Space" : title, &meta);
      continue;
    }
    // The folder names no saved group: the group's id derives from the Space
    // (arcomium::SavedGroupGuidForSpace()). Devices writing their own group
    // here took the link from one another and made duplicate groups.
  }
}

BEGIN_METADATA(ArcomiumSpaceSwitcher)
END_METADATA
