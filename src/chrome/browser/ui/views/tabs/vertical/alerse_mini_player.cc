// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/alerse_mini_player.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_space_color.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/alerse_window_stack.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/web_contents.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "chrome/browser/ui/views/tabs/vertical/alerse_motion.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace {

// Arc's dark sidebar, as in alerse_sidebar_view.cc.
// The colors (chrome/browser/ui/color/alerse_color_mixer.cc).
constexpr ui::ColorId kCardColor = kColorAlerseMiniPlayerCard;
constexpr ui::ColorId kHoverCardColor = kColorAlerseMiniPlayerCardHover;
constexpr ui::ColorId kTitleColor = kColorAlerseSidebarFolder;
constexpr ui::ColorId kSubtitleColor = kColorAlerseSidebarSubtle;
constexpr ui::ColorId kGlyphColor = kColorAlerseSidebarGlyph;

constexpr int kArtworkSize = 32;
constexpr int kArtworkRadius = 6;
constexpr int kButtonIconSize = 18;
constexpr float kCardRadius = 10;
constexpr auto kCardMargin = gfx::Insets::TLBR(4, 8, 8, 8);
constexpr auto kCardPadding = gfx::Insets::VH(8, 10);

gfx::FontList PlayerFont(int size, gfx::Font::Weight weight) {
#if BUILDFLAG(IS_WIN)
  return gfx::FontList({"Segoe UI Variable Text", "Segoe UI"},
                       gfx::Font::NORMAL, size, weight);
#else
  // The system font elsewhere, as in the sidebar.
  const gfx::FontList system;
  return system.DeriveWithSizeDelta(size - system.GetFontSize())
      .DeriveWithWeight(weight);
#endif
}

}  // namespace

AlerseMiniPlayer::AlerseMiniPlayer(BrowserWindowInterface* browser)
    : views::Button(base::BindRepeating(&AlerseMiniPlayer::GoToMediaTab,
                                        base::Unretained(this))),
      browser_(browser) {
  SetProperty(views::kMarginsKey, kCardMargin);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kCardPadding, 10));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  artwork_ = AddChildView(std::make_unique<views::ImageView>());
  artwork_->SetImageSize(gfx::Size(kArtworkSize, kArtworkSize));
  artwork_->SetCanProcessEventsWithinSubtree(false);

  auto* labels = AddChildView(std::make_unique<views::BoxLayoutView>());
  labels->SetOrientation(views::BoxLayout::Orientation::kVertical);
  labels->SetCanProcessEventsWithinSubtree(false);
  layout->SetFlexForView(labels, 1);
  title_ = labels->AddChildView(std::make_unique<views::Label>());
  title_->SetFontList(PlayerFont(13, gfx::Font::Weight::SEMIBOLD));
  title_->SetEnabledColor(kTitleColor);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetElideBehavior(gfx::ELIDE_TAIL);
  title_->SetAutoColorReadabilityEnabled(false);
  subtitle_ = labels->AddChildView(std::make_unique<views::Label>());
  subtitle_->SetFontList(PlayerFont(12, gfx::Font::Weight::NORMAL));
  subtitle_->SetEnabledColor(kSubtitleColor);
  subtitle_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  subtitle_->SetElideBehavior(gfx::ELIDE_TAIL);
  subtitle_->SetAutoColorReadabilityEnabled(false);

  play_pause_ = AddChildView(views::CreateVectorImageButton(base::BindRepeating(
      &AlerseMiniPlayer::TogglePlayPause, base::Unretained(this))));
  UpdatePlayPauseButton();

  SetVisible(false);

  // Follow the media session that last took audio focus.
  mojo::Remote<media_session::mojom::MediaControllerManager> manager;
  content::GetMediaSessionService().BindMediaControllerManager(
      manager.BindNewPipeAndPassReceiver());
  manager->CreateActiveMediaController(controller_.BindNewPipeAndPassReceiver());
  controller_->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
  controller_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kArtwork, kArtworkSize,
      kArtworkSize * 2, image_receiver_.BindNewPipeAndPassRemote());

  browser_->GetTabStripModel()->AddObserver(this);
}

AlerseMiniPlayer::~AlerseMiniPlayer() = default;

void AlerseMiniPlayer::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr info) {
  playing_ = info && info->playback_state ==
                         media_session::mojom::MediaPlaybackState::kPlaying;
  UpdatePlayPauseButton();
  UpdateVisibility();
}

void AlerseMiniPlayer::MediaSessionMetadataChanged(
    const std::optional<media_session::MediaMetadata>& metadata) {
  title_->SetText(metadata ? metadata->title : std::u16string());
  // The artist, or the site when the page names none.
  std::u16string subtitle = metadata ? metadata->artist : std::u16string();
  if (subtitle.empty() && metadata) {
    subtitle = metadata->source_title;
  }
  subtitle_->SetText(subtitle);
  subtitle_->SetVisible(!subtitle.empty());
  GetViewAccessibility().SetName(std::u16string(title_->GetText()));
}

void AlerseMiniPlayer::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {}

void AlerseMiniPlayer::MediaSessionChanged(
    const std::optional<base::UnguessableToken>& request_id) {
  request_id_ = request_id;
  has_artwork_ = false;
  UpdateVisibility();
}

void AlerseMiniPlayer::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  has_artwork_ = !bitmap.drawsNothing();
  if (has_artwork_) {
    gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    image = gfx::ImageSkiaOperations::CreateResizedImage(
        image, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kArtworkSize, kArtworkSize));
    artwork_->SetImage(ui::ImageModel::FromImageSkia(
        gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(kArtworkRadius,
                                                               image)));
  }
  UpdateVisibility();
}

void AlerseMiniPlayer::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed() ||
      change.type() == TabStripModelChange::kRemoved) {
    UpdateVisibility();
  }
}

content::WebContents* AlerseMiniPlayer::GetMediaContents() const {
  if (!request_id_) {
    return nullptr;
  }
  content::WebContents* contents =
      content::MediaSession::GetWebContentsFromRequestId(*request_id_);
  if (!contents) {
    return nullptr;
  }
  if (contents->GetBrowserContext() == browser_->GetProfile()) {
    return contents;
  }
  // Another Space's media shows too, while that Space is one of this
  // window's (a member of its stack): sound from a hidden Space has its
  // source on screen. An incognito tab's media never shows in a regular
  // window, nor a regular tab's in an incognito one.
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (!profile || profile->IsOffTheRecord() ||
      browser_->GetProfile()->IsOffTheRecord()) {
    return nullptr;
  }
  alerse::WindowStacks& stacks = alerse::WindowStacks::Get();
  return stacks.FindMember(stacks.StackOf(browser_), profile) ? contents
                                                              : nullptr;
}

void AlerseMiniPlayer::UpdateVisibility() {
  content::WebContents* contents = GetMediaContents();
  // The tab you are looking at plays its own media.
  const bool visible =
      contents && !title_->GetText().empty() &&
      browser_->GetTabStripModel()->GetActiveWebContents() != contents;
  bool has_image = has_artwork_;
  if (visible && !has_artwork_) {
    // Without artwork, the site's icon.
    if (auto* favicon =
            favicon::ContentFaviconDriver::FromWebContents(contents)) {
      const gfx::Image icon = favicon->GetFavicon();
      has_image = !icon.IsEmpty();
      if (has_image) {
        artwork_->SetImage(ui::ImageModel::FromImage(icon));
      }
    }
  }
  artwork_->SetVisible(has_image);
  if (GetVisible() != visible) {
    SetVisible(visible);
    PreferredSizeChanged();
    if (visible) {
      // It rises into its place. Leaving is one jump: the rows take its
      // room at once. Not in a hidden Space's window, which draws no
      // frames: the card would wait at its start, unseen, and stay so when
      // the Space shows.
      if (GetWidget() && GetWidget()->IsVisible()) {
        SetPaintToLayer();
        layer()->SetFillsBoundsOpaquely(false);
        alerse::motion::Arrive(this, gfx::Vector2d(0, 12),
                               alerse::motion::kStandard);
      }
    } else if (layer()) {
      DestroyLayer();
    }
  }
}

void AlerseMiniPlayer::UpdatePlayPauseButton() {
  views::SetImageFromVectorIconWithColor(
      play_pause_,
      playing_ ? vector_icons::kPauseIcon : vector_icons::kPlayArrowIcon,
      kButtonIconSize,
      views::IconColors(alerse::SidebarInk(this, kGlyphColor),
                        alerse::SidebarInk(this, kSubtitleColor)));
  const std::u16string play_pause = l10n_util::GetStringUTF16(
      playing_ ? IDS_ALERSE_PAUSE : IDS_ALERSE_PLAY);
  play_pause_->GetViewAccessibility().SetName(play_pause);
  play_pause_->SetTooltipText(play_pause);
}

void AlerseMiniPlayer::TogglePlayPause() {
  if (playing_) {
    controller_->Suspend();
  } else {
    controller_->Resume();
  }
}

void AlerseMiniPlayer::GoToMediaTab() {
  content::WebContents* contents = GetMediaContents();
  tabs::TabInterface* tab =
      contents ? tabs::TabInterface::MaybeGetFromContents(contents) : nullptr;
  if (!tab || !tab->GetBrowserWindowInterface()) {
    return;
  }
  BrowserWindowInterface* window = tab->GetBrowserWindowInterface();
  TabStripModel* model = window->GetTabStripModel();
  const int index = model->GetIndexOfTab(tab);
  if (index != TabStripModel::kNoTab) {
    model->ActivateTabAt(index);
  }
  // The tab's Space takes this window's place when it is one of its own
  // (alerse_window_stack.h); another window comes forward.
  alerse::WindowStacks& stacks = alerse::WindowStacks::Get();
  if (window != browser_ && stacks.StackOf(window) == stacks.StackOf(browser_)) {
    alerse::ShowSpaceInStack(browser_, window->GetProfile()->GetPath());
    return;
  }
  window->GetWindow()->Activate();
}

void AlerseMiniPlayer::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(alerse::SidebarInk(
      this, GetState() == STATE_HOVERED ? kHoverCardColor : kCardColor));
  canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), kCardRadius, flags);
}

void AlerseMiniPlayer::OnThemeChanged() {
  views::Button::OnThemeChanged();
  title_->SetEnabledColor(alerse::SidebarInk(this, kTitleColor));
  subtitle_->SetEnabledColor(alerse::SidebarInk(this, kSubtitleColor));
  UpdatePlayPauseButton();
}

BEGIN_METADATA(AlerseMiniPlayer)
END_METADATA
