// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_MINI_PLAYER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_MINI_PLAYER_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class ImageButton;
class ImageView;
class Label;
}  // namespace views

// Alerse's mini player, at the bottom of the sidebar: the media playing in
// a tab you are not looking at, in any Space or window of the profile, with
// its artwork, title and a play/pause button. Clicking it goes to the tab.
// It follows the active media session, the one that last took audio focus.
class AlerseMiniPlayer
    : public views::Button,
      public media_session::mojom::MediaControllerObserver,
      public media_session::mojom::MediaControllerImageObserver,
      public TabStripModelObserver {
  METADATA_HEADER(AlerseMiniPlayer, views::Button)

 public:
  explicit AlerseMiniPlayer(BrowserWindowInterface* browser);
  AlerseMiniPlayer(const AlerseMiniPlayer&) = delete;
  AlerseMiniPlayer& operator=(const AlerseMiniPlayer&) = delete;
  ~AlerseMiniPlayer() override;

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr info) override;
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionChanged(
      const std::optional<base::UnguessableToken>& request_id) override;
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override {}

  // media_session::mojom::MediaControllerImageObserver:
  void MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType type,
      const SkBitmap& bitmap) override;
  void MediaControllerChapterImageChanged(int chapter_index,
                                          const SkBitmap& bitmap) override {}

  // views::Button:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  // The tab of the active media session, if it belongs to this profile.
  content::WebContents* GetMediaContents() const;
  // Shows the player while its media plays outside the active tab.
  void UpdateVisibility();
  void UpdatePlayPauseButton();
  void GoToMediaTab();
  void TogglePlayPause();

  const raw_ptr<BrowserWindowInterface> browser_;

  mojo::Remote<media_session::mojom::MediaController> controller_;
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      observer_receiver_{this};
  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      image_receiver_{this};

  std::optional<base::UnguessableToken> request_id_;
  bool playing_ = false;
  bool has_artwork_ = false;

  raw_ptr<views::ImageView> artwork_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
  raw_ptr<views::ImageButton> play_pause_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ALERSE_MINI_PLAYER_H_
