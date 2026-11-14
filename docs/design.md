# How Alerse works

How each Alerse feature maps onto Chromium, and why it is built that way.

Goals: Spaces with separate accounts in one window, a sidebar for people moving from Arc,
a command bar, Split View, real-time sync across Windows, macOS, Linux and iPhone, link routing to
Spaces, media controls and picture-in-picture. Left out by the user's choice: Arc's tab
archive (see [Left out on purpose](#left-out-on-purpose)). Peek and Little Arc are
built, each with a switch (see Peek and the Little window).

Alerse follows how Arc behaves, not how it looks in detail: its own names, icons, logo
and texts, and none of Arc's code or artwork.

## Name

The product is **Alerse** from 0.1.8 on, a free spelling of alerce, the Chilean tree
(the user's decision, 2026-11-09). Releases 0.1.0 to 0.1.7 went out under an earlier
name, Arcomium. Names that profiles, sync or the wire format already store keep
`arcomium` (prefs `arcomium.*`, bookmark meta info, the sync protocol's labels); new
code says Alerse. The icon is
Alerse's own: an "A" whose left stroke is a brown trunk with a cut branch and whose
right stroke is green, on a white rounded tile (`artwork/`, see
[patching](patching.md#branding-and-the-product-name)).

Everything carries the new name: the product name in Chromium's strings and their
translations (`tools/rename_product.py`), BRANDING (Alerse.app, "Alerse Installer"),
source files (`alerse_*`), resource, command and color ids (`IDS_ALERSE_*`,
`IDC_ALERSE_*`, `kColorAlerse*`), class names, the `// Alerse:` tag on patch hunks,
the `chrome://alerse-*` pages, the `ALERSE_VERSION` key, the platform repositories'
`alerse` submodule, the docs.

The app does not call itself Chromium, Chrome or ungoogled-chromium either.
`tools/rename_product.py` renames "Chromium" and "Chrome" where they stand for the
browser, in the `*chromium_strings.grd` files and in the unbranded strings that still
name it (`generated_resources.grd`, `components_strings.grd`,
`privacy_sandbox_strings.grd`, and the parts they include, down to the omnibox
actions: autofill, passwords, page info, the search engine choice, security warnings,
management, "Restart"), moving their translations; messages that then read the same
keep one translation each. It drops ", ungoogled-chromium" from the version line
("Official Build") and calls ungoogled-chromium's entries in `chrome://flags` "Alerse
flag". Not renamed, as they are other products: "Google Chrome" (a browser to import
from), the Chrome Web Store, Chrome OS and Chrome Enterprise.
`defaults/no-ungoogled-first-run.patch` removes `chrome://ungoogled-first-run`
(ungoogled-chromium's page about itself), which a new profile opened and
`chrome://about` listed. `look/alerse-product-icons.patch` draws Alerse's "A" for the
Chrome logo in the settings and extensions headers (dark mode) and for the product
icon of "About Alerse"; `look/alerse-name-in-pages.patch` names the URL list "Alerse
URLs" and hides `chrome://flags`' promotion of Chrome's release channels. Kept on
purpose, as notices of the code Alerse is built from: the About page's "made possible
by the Chromium open source project", the copyright line "The Chromium Authors", and
the licenses in `chrome://credits`. Sites still see Chrome in the User-Agent, so they
treat Alerse as the browser it is.

Names that keep the earlier one, because they are stored in every profile, in sync, or
find the earlier installation; renaming them would lose data:

| Name | Where it is kept | What renaming it would break |
|---|---|---|
| `arcomium.*` preferences and bookmark meta info (`arcomium.space`, `arcomium.favorites`, `arcomium.update_check.*`...) | every profile; bookmark meta info also in sync | every Space, pinned item and setting of existing profiles |
| the `"arcomium"` Preferences member a profile is recognised by | the profile takeover below | taking over a profile of 0.1.0 to 0.1.7 |
| the empty Space's page `about:blank#arcomium-space` | sessions | restoring empty Spaces |
| the sync key derivation salt `arcomium-sync` and the protocol's labels (`arcomium-sync-token-v1`, `arcomium-sync-login-*`, `arcomium-pair-*`) | every sync chain's keys and the server | decrypting every chain's data |
| the sync server's Go module, binary, image, environment variables and tables (`alerse-sync` repository) | its deployment | the running server, until it is redeployed with a renamed configuration |
| `Arcomium` directories, registry keys and uninstall entry in the takeover code (`alerse_legacy_install_win.cc`, `defaults/own-install-identity.patch`, `defaults/mac-own-profile-dir.patch`) | the earlier installation | taking it over and removing it |

Taking over the earlier profile. Alerse is another app (its own bundle ID, install
directory, registry keys and ProgIDs), so on its first start it finds no profile of its
own and takes over Arcomium's, as the earlier move out of Chromium's directory did
(`defaults/own-install-identity.patch` for Windows and Linux,
`defaults/mac-own-profile-dir.patch` for macOS):

- Windows: `%LOCALAPPDATA%\Arcomium\User Data` moves to `%LOCALAPPDATA%\Alerse\User
  Data` (`install_static::GetDefaultUserDataDirectory`, in chrome_elf, before anything
  opens the profile). macOS: `~/Library/Application Support/Arcomium` to `.../Alerse`.
  Linux: `~/.config/arcomium` to `~/.config/alerse`. A profile still in Chromium's
  directory (before the browser had its own identity) is taken too, only if its
  Preferences have the `arcomium` member; the earlier name's own directory needs no such
  check.
- Only when Alerse has no profile of its own (no `Local State`): a directory without one,
  such as the crash reports a development run leaves in `.../Alerse`, is kept aside as
  `Alerse.old` (`User Data.old` on Windows) and the move goes ahead.
- Never while the earlier build runs with that profile: on Windows its `lockfile` is
  held without sharing, on macOS and Linux its `SingletonLock` names a live process.
  Alerse then uses the old directory where it is (Chromium's process singleton hands the
  start over to the running browser, which opens a window) and moves it at a later
  start. If a move fails for any other reason, Alerse also uses the old directory where
  it is: nothing is lost.
- Only for the default directory: never with `--user-data-dir`, and on macOS and Linux
  only in the browser process (no `--type`), because there Chromium also asks for the
  default directory with `--user-data-dir` (crash reports live under it,
  `chrome::DIR_CRASH_DUMPS`) and in helper processes. Test runs with their own profile
  therefore never move the profile of an installed browser.
- Plain system calls (POSIX, Win32), as this runs before blocking calls are checked.

Removing the earlier installation on Windows. Alerse installs beside it, so without more
the user would have two browsers, one of them empty. A minute after startup Alerse runs
the earlier build's own uninstaller silently, `setup.exe --uninstall --force-uninstall`,
read from its uninstall entry (`HKCU\...\Uninstall\Arcomium`,
`chrome/browser/alerse_update/alerse_legacy_install_win.cc`, started from the update
checker's `PostBrowserStart`). It removes that build's shortcuts, taskbar pins, uninstall
entry, ProgIDs, registration and files, and keeps user data (`--force-uninstall` asks
nothing and deletes the profile only with `--delete-profile`). It runs only when this
Alerse is installed (`BASE\Alerse\Application\chrome.exe` with its default profile
`BASE\Alerse\User Data`), the entry's uninstaller is
`BASE\Arcomium\Application\VERSION\Installer\setup.exe --uninstall` beside it (not a
system-level one), and the old `User Data` has no profile left, that is, Alerse took it
over. A development build, a run with `--user-data-dir`, an earlier build whose profile
is still in use, or one installed elsewhere is left alone (and logged). Windows then
needs Alerse picked as the default browser (Chromium cannot set that itself on Windows
10 and 11). `--alerse-legacy-uninstall-key=KEY` reads the entry from another HKCU key,
for tests only. On macOS and Linux nothing removes the earlier build: an app bundle or
an AppImage is the user's to delete.

Updates and links. Builds of 0.1.7 and before ask GitHub for the latest release of the
platform repository under its old name, which GitHub redirects to
`VizzleTF/alerse-<platform>` (the API answers 301, which `SimpleURLLoader` follows;
checked with curl on 2026-11-09). Alerse registers `alerse://` as its direct-launch
scheme (Chromium's `chromium://URL` launch, Windows only) and the pairing QR code carries
`alerse://pair?...`; the earlier scheme is not accepted.

Repositories. They are `VizzleTF/alerse`, `alerse-win`, `alerse-mac`, `alerse-linux`,
`alerse-ios` and `alerse-sync` on GitHub, which redirects the old names; the platform
repositories hold the core as the `alerse` submodule.

## Spaces

A Space is a named set of tabs and pinned items with its own color and its own
logins, history, passwords, extensions and settings.

Each Space is a Chromium profile with a window of its own.
[ADR-0001](adr/0001-space-profiles.md) gives the reasons, the design and what each
feature does on it. In short:

- The windows of one Alerse window form a stack: one `Browser` for each Space opened
  in it, all in one place on screen, one visible. Switching Spaces shows the next and
  hides the previous (`alerse_window_stack.h`).
- The list of Spaces is Chromium's profile registry in `Local State`. Home is the
  first profile, `Default` (`alerse_space_registry.h`).
- A Space's pinned items are the folder of its profile's Other Bookmarks marked with
  the meta info `arcomium.space = default`. The folder's title is the Space's name
  (`alerse_space_folder.h`).
- At startup the Space shown last opens (`spaces/start-in-last-space.patch`). A
  second later the other Spaces' windows open hidden in its stack and restore their
  sessions (`alerse::PreloadSpaces`), so the first switch to a Space shows a window
  that is already drawn instead of one that builds itself on screen. A hidden window
  takes only its session's first window and is not activated by the restore, which
  would show it beside the Space shown (`spaces/preload-spaces-session.patch`). Startup
  continues the last session by default (`defaults/continue-session.patch`).
- An empty Space stays, as in Arc: closing its last tab leaves a hidden blank tab
  (`about:blank#arcomium-space`, `spaces/empty-space.patch`), and the window stays
  open. `--close-window-with-last-tab=always` turns this off. The empty page takes no
  focus, opens no command bar until asked, shows the page card's blank color and
  opens no context menu.
- Tabs of the Spaces not shown are background tabs of hidden windows and keep
  running, so music or a messenger in another Space goes on. Memory Saver, on by
  default, unloads idle tabs.
- A release before the profiles (0.1.11 and earlier) kept every Space in one profile,
  each in a storage partition. Such a profile opens as home, and each other Space's
  folder becomes a Space of its own with its pinned items (below); its tabs, cookies and
  logins are not carried over.
- UI: `AlerseSpaceSwitcher`, the Space bar at the bottom of the sidebar: Downloads
  at the left (where Arc has its archive; shows recent downloads in a popover above
  it and closes it on a second click, as Arc does, without a tab; with nothing from
  the last 24 hours the popover says "No recent downloads" and offers "All
  Downloads", the only way from it to `chrome://downloads` besides Ctrl+J and
  Shift+Cmd+J), an icon per Space (the active one bright), "+" at the
  right. A Space's icon is picked from its menu and stored in its bookmark folder as
  `arcomium.space_icon`, so it syncs; Home is a sun until another is picked. The same
  menu has Edit Theme Color and Delete Space (after a question; it deletes the
  Space's profile with its tabs, pinned items, logins and history; dimmed for Home,
  which cannot go, as Arc's menu always has it). A Space's own window edits its name,
  icon and color; in another Space's window those items are dimmed. Alt+1 to Alt+9 show the Spaces in the bar's order, Home first;
  on macOS, where Option+digit types characters, Control+1 to Control+9, as in Arc
  for macOS, whose tooltips name them ("home ^1"). A horizontal
  trackpad swipe walks the bar: the sidebar follows the fingers with a picture of the
  next Space's rows beside the current ones, and a lift past 60 pt switches.
  Each Space returns to the tab that was active in it.
- Color: Edit Theme Color (in the Space's menu, or a right click in the sidebar's empty
  space) picks hue and saturation, light or dark, intensity and brightness, or a preset.
  The color tints the window frame and the sidebar and is stored in the Space's folder
  as `arcomium.space_color`, so it syncs; on a light frame the sidebar inverts its
  colors. Each Space's window has its own tint, so a switch changes it at once, with the
  windows; picking a color in the picker applies it at once too (see Motion). Every tint is a new color provider key, so
  `look/bounded-tint-color-providers.patch` keeps at most 32 tinted providers, oldest out
  first, instead of all of them for the session.

What a Space that is not shown has to tell is on its icon in the Space bar
(`AlerseSpaceSwitcher::NoticeOfTabs`), the most pressing first: a dot while one of its
tabs waits for the user (a page dialog, a page that asked for attention, a crashed page,
a permission request waiting for its answer); the alert's glyph, in its own color, while
one records or shares (camera, microphone, screen, a device); a small speaker while one
plays sound. A window tells the other Space bars only when its own notice changed, on
its tabs' changes and on its tabs' permission requests (`PermissionWatch`: a request
changes nothing the tab strip reports). The mini player shows the media of every Space
of the window (a member of its stack; never an incognito window's in a regular one, nor
the reverse), and a click on it shows that Space with the tab. Verified on macOS on
2026-11-12 with sound, a microphone request and a video-less audio page in a second
Space; geolocation cannot test it, since Blink holds a hidden page's request back.

A Space of 0.1 (a tab group, whose pinned items are a folder of home's bookmarks
marked with its id) becomes a Space of its own the first time home's sidebar sees its
folder (`AlerseSidebarView::AdoptLegacySpace()`): a profile is made with the folder's
name, the folder's items, icon and color move into it, the Space of Arc an import bound
the folder to is recorded so that a later import fills this Space instead of making a
second one, and the folder goes from home. The window stays on the Space it shows. Only
pinned items move: the Space's tabs and its sites' cookies and logins stayed in 0.1's
storage partition and are not carried over.
An Arc Space that `tools/arc_import.py` left in home's bookmarks (a folder marked
`arc:<id>`) becomes a Space the same way, in home's first pass after start; the import
page makes its Spaces itself, so later in the session such a folder is left to it.

Known gaps:

- on macOS in full screen, each Space's window is a full screen desktop of its own,
  so switching Spaces slides between desktops (about 0.7 s) instead of switching at
  once;
- a prompt in a hidden Space shows only when the Space is opened (it then has the
  keyboard: `ShowSpace` activates the window's visible modal dialog after the
  window);
- moving a tab to another Space reopens it there, losing its history and form state;
- favorites are a set for each Space;
- a Space's folder, made with its first pinned item or theme color, was named "home"
  whatever the Space (fixed 2026-11-12: it takes the Space's name);
- Linux is not built with the profiles yet: the series targets Chromium 157, and
  Linux waits for ungoogled-chromium-portablelinux 157 (Windows has them from 0.1.12).

## Sidebar look

The expanded sidebar is Arc's. Its rows, Space name, folders, folder popup, Space bar,
popovers and tooltips follow Arc for macOS' measurements on every platform (see
[On macOS](#on-macos); a point is a DIP on Windows and Linux), because Windows is to look
like the Mac everywhere but the top of the window. The list below is what Arc for
Windows' measurements still set (see `AlerseSidebarView`):

### Header row

- Header row, 32 px: Alerse menu, sidebar toggle, downloads at x = 23, 58, 93.

### Favorites

- Favorites: Arc's tiles above the Space's name. Each Space has its own, as each Space
  is a profile (ADR-0001). They are the bookmarks of a folder beside the Space's folder
  (meta info `arcomium.favorites = 1`, created by the first Add to Favorites), so they
  sync like pinned tabs. Tiles are 47
  high, 8 apart, up to four in a row filling the sidebar's width (two are 70 pt wide on
  macOS, as in Arc), a shade off the frame (#2F2F2F on #161616), lighter on hover
  (#383839); the site's icon sits in the middle. The tile whose tab is shown is Arc's
  (measured 2026-11-12 on a 70x47 pt tile): a light fill, about #92 grey, tinted a
  little by its rim, inside a 2 pt rim whose pastel changes round the tile: #C5CFEF at
  the right, #BBD7CA at the bottom, #E9D8B3 at the left, #EAC4C4 at the top, with
  #B5C8DB, #C7D1B2, #DEC4B2 and #DCCBD6 at the corners between them (a sweep gradient
  through the eight, `FavoriteTile::RimShader`). A tile shows what a row shows beside
  its name, in its corners: a 10 pt speaker (crossed when muted) or the tab's other
  alert (camera, microphone, screen...) at the top right, a "/" at the top left once
  its tab left the favorite's page, and the attention dot on its icon. What Arc draws
  there was not measured (it needs a favorite that plays sound in the user's Arc). A favorite
  opens as a Chromium pinned tab of the Space's window, bound to it like a pinned item;
  the binding survives restarts as a pinned item's does. Add to
  Favorites (tab and pinned-item menus) and dragging a row onto the tiles, or onto the
  strip under the address field while there are none, make one; the tab becomes the favorite's in place. Tiles reorder by
  dragging; Remove Favorite leaves the tab open as an ordinary tab of the Space. Measured on
  Arc for macOS only; on Windows they use the same numbers.

### Pinned tree

- Pinned tree: the Space's bookmark folder (meta info `arcomium.space = <Space id>`,
  `default` outside of Spaces, `arc:<id>` for an imported Space not yet adopted by an
  Alerse Space of the same name). Rows and the Space's name row use Arc for macOS'
  measurements (On macOS), in the system font: SF Pro on macOS, Segoe UI Variable
  Text on Windows (Segoe UI before Windows 11), regular items and semibold folders.
  Folder glyphs follow Arc: a folder holding open tabs shows two dashes, otherwise its
  Arc icon (`arcomium.folder_icon`). Open state is the `arcomium.open` meta info.

### Pinned items and their tabs

- A pinned item opens as a tab bound to it; a double click renames it, as in Arc,
  also on an item whose tab is not open yet: its first click opens the tab and rebuilds
  the rows, so the sidebar counts the second press on the same item within the system's
  double-click time itself (views drops the double click on a new row).
  Bound tabs are not listed with the other tabs: they show on their item, and under a collapsed folder its bound tabs are listed
  as child rows. A bound tab that navigated away shows "/" before the item's name.
  A `javascript:` item (an imported bookmarklet) runs on the shown page, as from the
  bookmark bar, and is never bound. While the bookmarks load, a restored tab that carries
  a pinned item's id gets no row (it would show as a plain tab and jump to its item).
  A tab Memory Saver discarded shows its icon at half opacity until it is shown again.
  As in Arc, a tab stays on its item across restarts, crash restore and Ctrl+Shift+T,
  wherever it has navigated: the item's bookmark uuid is session tab extra data
  `arcomium.pinned` (`pinned-tab-binding.patch` writes it in full session rewrites and
  carries it onto restored tabs and through TabRestoreService).
  A tab binds only to an item of its own Space; once its item is deleted, or moved to
  another Space with the tab, it is an ordinary tab. A tab binds only through its
  stored item, never by its address: closing an item's tab leaves another tab showing
  the same page an ordinary tab. A click on the icon or the "/" of an item whose tab
  navigated away loads the item's saved page again and shows the tab, as Reset Tab to
  Original URL in the item's menu does.

### Sound indicator

- A tab that plays sound shows a speaker between its icon (and "/") and its title, as in
  Arc: while the tab strip's audio alert is on (it outlasts the gaps between tracks) or
  while the tab is muted. Clicking it mutes or unmutes the tab
  (`TabMutedReason::kAudioIndicator`); muted, it is crossed out. Its own press never
  activates or drags the row; a right click opens the row's menu, whose Mute item follows
  the same rule. Rows in a folder popup show it too; the popup is refilled only when one
  of its speakers changes.

### Picking several tabs

As in Arc, Cmd+click (Ctrl+click on Windows and Linux) on a tab's row adds it to the
picked tabs or takes it out, and Shift+click picks every tab from the last picked one to
it (`TabStripModel::SelectTabAt`, `DeselectTabAt`, `ExtendSelectionTo`). Picked rows take
the shown tab's plate until a plain click picks one tab again. With several picked, Copy
URL (Shift+Cmd+C) copies every picked tab's address, one a line, and Copy URL as Markdown
one link a line, skipping blank pages and empty Spaces (`look/copy-selected-urls.patch`,
`AlerseCopyPickedUrls`).

### Folder popup

- Hovering a collapsed folder pops up its links (subfolders open in place); picking
  one opens it as a bound tab without expanding the folder. The popup is a non-modal
  bubble, not a menu, so the mouse stays with the sidebar: it opens after 400 ms, fades in over 90 ms,
  follows the pointer to another folder and fades out when the pointer has been away
  for 300 ms. It opens inactive; on Windows it may activate on a click, which Windows
  would otherwise drop. On macOS an inactive popup gets no mouse moves, so its rows take
  hover from the popup's pointer polls.

### New Tab and other tabs

- Divider, "+ New Tab", then the Space's other tabs. A tab's close button shows only
  while its row is hovered.

### Row buttons

- Hovered rows' buttons follow Arc's, the same on every platform: a tab shows "×",
  which closes it; a pinned item with its tab open shows "−", which closes the tab and
  keeps the item; a pinned item with no tab open shows "×", which removes the item
  (its bookmark, as Remove Pin does). Favorites have no button. The "−" is Chromium's
  minus (`kRemoveIcon`) drawn as wide and as thick as the cross, on the same button
  and plate. Their tooltips are "Close Tab" and "Remove": Arc's pill on macOS
  (a row's own pill gives way to a child with a pill of its own,
  `alerse_tooltip.cc`), the native tooltip elsewhere.

### Folders opening and closing

- Folders open and close in 140 to 260 ms by their height (see Motion), animated on
  layers (rows clipped like a curtain, rows below shifted by transforms): resizing the rows relaid out the whole browser view
  every frame and stuttered. A folder's open state is meta info, which
  `BaseBookmarkModelObserver` does not forward, so the sidebar redraws on it itself.

### Drag and drop

- Drag and drop: rows move within the pinned tree and into folders; a tab dropped among
  the pinned items becomes pinned; a pinned item dropped among the tabs is unpinned and
  keeps its tab; tabs reorder within their Space; a tab or a pinned item dropped onto
  the middle half of a tab's row makes a split of the two (see Split View), while the
  row's top and bottom quarters still reorder. A line, a highlighted folder or a
  highlighted tab row shows where the row lands. The sidebar does not rebuild during
  a drag. A dragged row carries its page's address (a folder only a marker text), so
  other apps take it as a link and Chromium's split zone on the page's edges takes it;
  the page itself takes no drop from a sidebar row (`alerse_sidebar_drag.h`), which
  would load that address in it or paste it.
- A row dropped on another Space's icon in the Space bar moves to that Space, as Move
  to does (see Context menus): the icon lights while the row is over it, and the
  window shows that Space after the drop (`AlerseSidebarView::MoveDraggedRowToSpace`).
  Unlike Arc, hovering the icon does not switch Spaces during the drag: each Space is
  a window of its own, and the dragged row's window would be hidden under the pointer.

### Context menus

- Context menus follow Arc's, in its order and words (Share only on macOS, see On
  macOS). Pinned
  items, favorites and folders get Change Icon... (Arc's picker, see
  [Icon pickers](#icon-pickers)). Pinned items and tabs get Copy Link, Rename, Mute,
  Duplicate, Move to a Space (a pinned item or folder
  moves to that Space's pinned folder, a tab reopens in that Space; a Space
  whose last tab leaves keeps a New Tab) and Add to Favorites (on macOS only for tabs,
  as in Arc for macOS, whose pinned items go to the favorites by dragging). Mute stays in the menu,
  dimmed while the tab is silent, as in Arc. Open in Split View, as in Arc, shows the
  row's page beside the shown one (see Split View). Pinned items also get Edit Pinned Page (replace the saved address
  with the current one, or edit the name and address) and Remove Pin; once a pinned
  item's or favorite's tab has left its saved page (its row shows "/"), Reset Tab to
  Original URL loads the saved page in it again (a bookmark navigation; the item and
  its binding stay). Folders get New
  Nested Folder, Turn "folder" into Space (the folder becomes a new Space's folder,
  its items the Space's pinned ones, and the window switches to it), Copy All Links
  (one address per line), Rename, then after a line as in Arc Duplicate (titles,
  addresses and icons, not the tabs bound to them), Move to, Pin current tab here (not
  on macOS, whose Arc has no such item) and Delete. A favorite gets Copy Link, Rename, Mute, Duplicate and
  Remove Favorite.

### Space menu

- Arc's Space menu opens wherever Arc has it: on the Space's name, New Tab, the
  address field (on Windows and Linux only around its text, which keeps the text
  field's menu), the empty space below the rows, a Space's button and "+" in the Space
  bar, and on macOS the header's "...". It holds Change Space Icon... (Arc's picker,
  see [Icon pickers](#icon-pickers)), Rename Space (the Space's folder's name;
  Chromium's bookmark editor opens on the folder titled
  "Edit Space name", "Edit Space Name" in macOS' title case, instead of "Edit folder
  name": `EditDetails::alerse_title_id`, `look/space-rename-title.patch`, set only
  by `AlerseSpaceSwitcher::RenameMenuSpace`, which the Space menu, the command
  bar's Rename Space and the macOS menu bar all run; renaming a folder and New
  Folder keep Chromium's titles), Edit Theme Color, New Folder, Collapse All Folders
  (while any folder of the Space's pinned tree is open; else Expand All Folders,
  dimmed when the Space has no folders) and Delete Space. Collapse / Expand All
  Folders writes every folder's open state (`arcomium.open`) at once, nested ones
  too, so the rows change without the animation of a single folder; the
  open state syncs like any other meta info. Arc's Set Profile, Live Folders, Share Space, Export and
  Manage Spaces are left out: each Space is already a profile, and the rest
  needs Arc's account and services. The "+"
  button's click offers a new Space, folder or tab.

### Clear

- Clear: while the Space has tabs that are not pinned, a line with "↓ Clear" at its
  right end sits between the pinned items and New Tab, as in Arc. A click on Clear
  closes those tabs (the menu bar's Clear Today, Shift+Cmd+K, does the same); a tab
  open on a pinned item or a favorite stays. Arc archives the tabs; Alerse has no
  archive, so they go to the closed tabs' history, and Reopen Closed Tab
  (Shift+Cmd+T) brings them back one at a time. A tab closed with a row's "x" goes
  there too. An empty Space's hidden blank tab is never recorded there
  (`spaces/empty-space-not-in-history.patch`).

### Scrolling to the active tab

- The rows scroll to the tab that becomes active, so a tab opened by a link from
  another app, the command bar or Ctrl+Tab is never below the visible rows. The
  scroll repeats after the next frame: the compositor limits it to the height it
  last drew the rows at, a row short of a tab just added (crbug.com/40772386).

### Rebuilds

- Rebuilds: a window has the rows of its own Space only. Theme changes rebuild once
  they settle (250 ms), and the Space bar rebuilds when a profile is added or removed
  or a Space's folder changes, not on every navigation.

### Hidden sidebar

- When the sidebar is hidden it floats back over the page from the window's left edge,
  as on macOS (Hidden sidebar, under On macOS); on Windows and Linux the address moves
  to the top bar meanwhile and Ctrl+Shift+L brings the sidebar back.

### Page and Space bar

- The page is Arc for macOS' card (On macOS) on every platform; on Windows and Linux
  it starts under the top bar. The frame is #161616 everywhere. The Space bar sits at
  the bottom. Its left button is Downloads (tooltip "Downloads (Ctrl+J)"): an arrow down
  into a tray, 14 px wide in 2 px strokes, grey and brighter under the pointer, as the
  Space icons; it replaced the archive box that opened the history.

### Hover state of rebuilt rows

- A row rebuilt under the pointer (rows are rebuilt on every tab change) takes the hover
  state without its fade-in, so it draws its highlight at once rather than none, and
  checks every 100 ms whether the pointer has left, since the root view, which saw the
  old row, sends the new one no exit. A row the pointer entered the ordinary way runs
  the same check until it has left, so a lost exit (a folder's rows rebuilt or animated
  under the pointer, the pointer leaving the window) cannot leave a row lit: a folder
  row that stayed #2D2D2D after expanding was reported on macOS. That exact sequence
  (hover until the popup shows, leave, click, leave the window) no longer reproduced
  on the build before this check, so the check is a safeguard rather than a found
  cause.

### On macOS

These numbers are measured on Arc for macOS (points, Retina). Since 0.1.8 the sidebar's
own parts apply on every platform: rows, the Space's name row, folders and their
popup, the Space bar with its "+" popover, the overflow line, the pills, the outlined
globe and the context menus. Still macOS-only (`#if BUILDFLAG(IS_MAC)`) are the header
row, the address field, the inactive window's lighter frame, Share, the menu bar, and whatever needs AppKit (window shadows,
vibrancy, the window controls). The Chromium side of the header row, address field and
command bar panel is in `look/mac-arc-look.patch`. Windows and Linux keep Arc for
Windows' numbers for those:

#### Sidebar width

- Sidebar 164 pt wide (198 px elsewhere) on a #161616 frame, without the edge line
  Chromium draws along it. A window saved at 198, the width macOS builds had before,
  opens at 164 (`vertical_tab_strip_state_controller_impl.cc`); other widths stay.

#### Header

- Header: one row centred 24 pt down: the window controls (the title bar is 48 pt high
  for that), then back, forward and "..." centred at x = 88, 116, 144. "..." opens
  Arc's popover with Reload (Cmd+R, dimmed on an empty Space) and Toggle Sidebar
  (Cmd+S), and turns into a #DCDCDC cross while it is open; hovered it has a
  27 pt #262626 plate (#2A2A2A while open), and no plate once the pointer is off it
  or in the popover, as Arc's. The button paints to its own layer, as its plates reach
  past the header's bottom. Site Controls are reached from the address field's
  button and Downloads from the Window menu (Shift+Cmd+J). Pinned extensions do not
  show in the header.

#### Popovers

- Popovers (`alerse_popover.cc`), Arc for macOS' own menus rather than the system's:
  a #010101 panel with 12 pt corners and a 13 pt arrow 6 pt out of it, its tip 10 pt
  off the button and centred on it, kept 6 pt inside the window. Rows are 40 pt (8 pt
  padding above and below): a 20 pt outline glyph centred 28 pt in, the label in
  13 pt medium white 51 pt in (semibold was a point wider than Arc's labels), the
  shortcut in #787878 ending 20.5 pt from the edge, its keys 1.5 pt apart (3.5 pt of
  ink between "⌘" and "R", as Arc's);
  dimmed rows are #797880. Separators are 1 pt #232323 lines with 8 pt either side.
  Reload is Arc's arrow.clockwise, a 12 pt ring in a 2 pt stroke with a filled
  head; New Tab its plus.square in a 1.9 pt stroke with 2.8 pt corners and a 6 pt
  cross. While a popover has the keyboard the window controls are ringed in
  #2A2B2A, faces kept, as Arc shows them (the popover's window is named
  `AlersePopover`, `alerse_window_mac.mm`, and the controls' view checks the
  key window); the icon pickers leave them in color, as Arc's do. On macOS the
  panel casts Arc's faint shadow, what is under it 3 to 5 levels darker within
  about 4 pt of its edge (a shadow 1 pt down with a 4 pt blur at 28% black, painted
  by `MenuShadowHost` in a 6 pt margin round the panel, as the window has no macOS
  shadow, whose dark outline Arc's popovers do not show).
  The row nearest the arrow that can be used starts on the #232323 highlight (8 pt in,
  8 pt corners), which follows the pointer (also polled: macOS did not always send
  the "+" popover its moves) and Up and Down and goes once the
  pointer leaves the popover or rests on a dimmed row, as Arc's; Return or a click runs
  the row once the popover is gone, Escape or a click elsewhere closes it. The
  popover takes the keyboard (an activatable child window) and closes when it loses
  it; the press on its button that closed it does not open it again. The Space bar's
  "+" opens one above it, 210 pt wide and starting 6 pt from the window's left edge, as
  Arc's: New Space, New Folder, then New Split (Control+Shift+=, dimmed on an empty
  Space or a page already split, see Split View) and New Tab (Cmd+T); "+" turns into a
  cross meanwhile, on its plate only while the pointer is on it. Arc's New Easel row is left out with Easels, and New Boost with it (Boosts open from the site controls and the command bar). The same
  popover on every platform; off macOS its shortcuts read "Ctrl+Shift+=" and it is
  250 pt wide for them.

#### Icon picker measurements

- Icon picker (Change Space Icon..., Change Icon...): see [Icon pickers](#icon-pickers);
  its measurements are Arc for macOS'.

#### Address field

- Address field 148x36 pt at x = 8, y = 46, with 12 pt corners, a shade lighter than
  the frame (#2F2F2F on #161616); "Search..." is a point smaller than the address, in
  #6F6F6F, after a magnifier at x = 20. While the command bar has the location bar,
  the field stays drawn with what it showed, as Arc's does. The held location bar takes
  no mouse events until edited: a click opens the command bar and a right click Arc's
  Space menu, not the text field's menu. Windows and Linux keep the location bar's own
  events: its site icon and "Not secure" chip, shown on hover, stay clickable there.

#### Address field buttons

- The field's address starts 13 pt in, left-aligned, as Arc for macOS'. While the
  pointer is anywhere on the sidebar (polled every 80 ms, `AlerseAddressBar`), the
  field shows Arc's buttons from its end: the site controls (sliders, centred 20 pt
  from the end), Copy Link 28 pt before it, and, with exactly one pinned extension,
  that extension 28 pt before Copy Link; the address fades out over 16 pt before
  them. The link and sliders are drawn in code from lines and rounded
  rects, at Arc's sizes (#ACACAC, 1.2 pt strokes). The button under the pointer has Arc's plate, 28 pt with 8 pt corners, a 7%
  veil over the hovered field (#4D4D4F on #3F3F40), and Arc's pill tooltips: "Site Control
  Center", and "Copy URL" with the ⇧ ⌘ C keycaps (measured 2026-11-12). The address is
  set in one grey, #AEAEAE, as Arc's, not a bright site before a dim rest
  (`LocationBarView::GetAlerseSidebarAddressColor`, `look/command-bar-on-page.patch`); in
  the command bar it is the bar's white again. A single pinned extension stays in the
  field without the pointer while its icon has something to say on this page (badge
  text, or it was kept from running there), then alone at the field's end with the
  address fading before it: shown only on hover, a blocker's count would never be
  seen. Verified on 2026-11-12 with an unpacked extension that sets a badge on one
  page. A release on a button runs it (the
  field takes the press). The sliders open the site controls under themselves. Copy
  Link shows it copied: its link turns into a check mark for 1.5 s.

#### Pinned extensions

- Two or more pinned extensions are tiles under the field, always shown, as Arc for
  macOS draws them: 27 pt high in the field's shade, 8 pt apart and 9 pt below the
  field, up to four in a row with the rows evened out (five are three and two). A
  click runs the extension, a right click opens its menu (Pin/Unpin, Options,
  Remove...). Measured on Arc for macOS on 2026-11-10 with one to five pinned
  extensions.

#### Rows

- Rows 41 pt apart; the highlight is 148x36 with 12 pt corners, 10% white on hover and
  20% for the active row; favicons at x = 18, folders 19 pt, nested rows 16 pt in, their
  highlight too (it starts where the row does, as Arc's). Tab
  and folder text is #DADADA and ends 16 pt before the highlight's edge, where Arc
  elides it; New Tab's "+" is #8B8B8B and its text #7F7F7F. Every pinned tab shows
  Arc's cross on hover (11 pt, #A9A9A9; under the pointer #ADADAD on Arc's plate, 24 pt
  with 6 pt corners, an 8% veil over the hovered row, #454545 on #353535: `RowCloseButton`),
  or its "−" while its tab is open (see the
  hover buttons above). No divider line before New Tab (the view stays for drag and drop). While the
  command bar is open for a new tab, New Tab is the active row, as in Arc.

#### Space name row

- The Space's name stays put above the rows, which scroll under it, as Arc's (measured
  2026-11-12); once they are scrolled a hairline shows under the name (the scroll view's
  top overflow line). The name and the rows are one list (`list_` in
  `alerse_sidebar_view.cc`) and slide together between Spaces. No tooltip on the name's
  "...", as in Arc.
- The Space's name is a row of its own: the Space's icon, 15 pt in the active white,
  centred at x = 26 and the name in 13 pt semibold #808080. Hovered, it gets a 148x36
  plate (#2F2F2F), a chevron in place of the icon that collapses the Space's pinned
  tabs under the name (only open ones stay, as in a folder; meta info
  `arcomium.collapsed` on the Space's folder) and "..." at its end
  for the Space's menu; under the pointer "..." takes a 24 pt plate of its own
  (#3F3F3F on the row's #2F2F2F), as Arc's. While the Space's icon picker is open
  the name is not lit for it and shows no tooltip; the pointer still lights it, but
  with the Space's icon rather than the chevron, and "..." keeps its plate while the
  pointer stays on it (where the menu's item was chosen), as Arc's; macOS sends the
  window no mouse moves while the picker has the keyboard, so the name polls the
  pointer meanwhile (every 50 ms).

#### Page card and find bar

- The page card (every platform) keeps 10 pt from the window's top (on Windows and
  Linux it starts under the top bar), right and bottom edges and touches
  the sidebar, with 6 pt corners, no rim and a faint shadow; its blank color is
  #2B2B2B, and in a Space with a theme color the frame's tint a shade lighter, as Arc's
  (9% white over the frame: #324240 on #1E2F2D, Arc's #394B4D on #273A3E;
  `look/blank-page-tint.patch`). The find bar is Arc's pill
  (`alerse_find_bar.h`, `look/find-bar-pill.patch`): 300x33 pt with 6 pt corners,
  10 pt inside the card's top and right edges (`BrowserView::GetFindBarBoundingBox`),
  #151C30 under a dark frame and #F4F5F9 under a light one (Chromium's
  `kColorFindBar*` ids, set in `alerse_color_mixer.cc`), a magnifier and
  "Find in page..." in #9599A2, 13 pt text. The match counter and the two arrows
  show only while the field has text; there is no divider. Every function is
  Chromium's (Enter, Shift+Enter, Esc, the counter, the find pasteboard on macOS,
  which may prefill the field). It fades in from 4 pt above its place and out
  where it is, on its layer, over the quick token; Chromium's slide by layout is off. A split's two panes share the same rectangle. The insets are set on every
  layout (`look/page-card-insets.patch`): Chromium set its own 8 px ones for a split and
  never took them back, so after the first split a single page sat 8 pt off the sidebar
  with no top margin until restart. Full screen without the sidebar (a page's own full
  screen: a video, a presentation; F11 on Windows and Linux) shows the bare page, with
  no margins and square corners; macOS' window full screen keeps the sidebar and the
  card.

#### Folder glyphs

- Folders are Arc for macOS' outlined folder, 18.5 pt: one 1.5 pt outline whose tab
  slants down into the body, grey under the tab, a #F0F0F0 front edge, filled near
  black (#050505) below it. An open folder shows Arc's open folder, 22.5 pt wide: a
  grey back leaning left behind a slanted front. The folder's row shows no tooltip.

#### Folder popup measurements

- Folder popup, as Arc for macOS' on every platform: it opens
  after 400 ms of hover (Arc's shows between 0.3 and 0.6 s), 240 pt wide, 4 pt right of the sidebar and centred on the
  folder's row, #1A1A19 with 12 pt corners and a two-pixel rim (#3D3D3D outside
  #4F4F4E) straight over what is behind: its window has no macOS shadow, whose dark
  outline (#060606) and lightened edge Arc's popup does not show (`RemoveWindowShadow`).
  A right click on the folder keeps the popup open behind the folder's menu, and
  after Escape closes the menu, as Arc's; picking an item of the menu closes it. On top a 48 pt search row ("Search <folder>...", a magnifier 20.5 pt in)
  over a #383837 line; then rows 45 pt apart with the link's title (13 pt medium) and,
  under it in grey, how long ago it was last opened, or else added (the bookmark's
  dates), as Arc words it: minutes, hours and days under a week ("3 days ago"), then
  weeks ("1 week ago", "3 weeks ago"), months ("2 months ago") and years, in the
  browser's language (ICU's `RelativeDateTimeFormatter`; weeks are 7 days, months 30,
  years 365). The first row is selected (#2C2C2B, 4 pt in from the sides); the pointer and
  Up and Down move the selection, Return opens it, Escape closes the popup, and typing
  filters the links of the folder and its subfolders by title or address, keeping the
  popup's height (the rows left sit at its top) and the selected row's place, as
  Arc's. A link without a favicon shows Arc's outlined globe (a 12.8 pt ring, a
  meridian and the equator, #8D8D8D), here and in the sidebar's rows, not
  Chromium's filled one. Longer lists
  scroll within 320 pt. The popup takes the keyboard when it opens, as Arc's search
  field has the caret, so it gets mouse moves itself.

#### Space bar

- Space bar: Space buttons 34x32 pt side by side and centred, a faint plate as wide on
  hover (34x32 pt from 859 pt down in a 900 pt window, as Arc's, measured at the
  plate's middle). A button's plate goes once its context menu closes: the
  menu took the pointer's events, so the button never saw it leave (the Space
  bar's `MenuClosed` repaints them);
  Downloads and "+" on 32x32 pt plates 8 pt from the sidebar's sides in Arc's
  #A0A0A0, which stays under the pointer; Space icons 13 pt; Spaces other than the
  shown one in #3F3F3F, as dim as Arc's. The plates end 9 pt above the window's bottom
  (`kVerticalTabStripUncollapsedVerticalPadding`, 12 elsewhere). Downloads (Arc's
  Library button until 0.1.7) is an arrow down into a tray in the "+"'s stroke, over
  the same 13.5 pt square, and "+" is Arc's 13.5 pt cross, 1.7 pt thick. The book Space icon is
  Arc's filled open book, two pages at a spine gap, not the outlined one. "+" has no
  tooltip, as Arc's.
  In a sidebar too narrow for all the Spaces, the buttons first move closer (down to
  24 pt apart), then only the run that fits around the shown Space's stays
  (`SpaceIconRow`); the menu bar's Spaces menu and Control-1 to 9 reach the others.

#### Overflow line

- While the rows overflow, a 1 pt line, barely off the frame (#212121 on
  #161616), shows at the edge they scroll under, as Arc's; Chromium's scroll view would
  draw a bright separator.

#### Scroll bar

- The rows have an overlay scroll bar, drawn as Chrome's vertical tab strip draws its
  own: a 5 pt pill 4 pt from the trailing edge, over the rows, in the frame's color
  moved 35% toward black or white (`RowsScrollBar`). It shows only while the rows
  overflow, and then while the pointer is over the rows or they scroll (wheel or
  trackpad), and fades out a second after the pointer has left and the scrolling has
  stopped (read from the cursor: a rebuild of the hovered row can leave its exit
  unsent). The folder popup's longer lists have the same one, over the popup's color.
  A rebuild does not show it, though it may move the rows.

#### New Tab and chevron details

- While the command bar is open for a new tab, New Tab's "+" is #DADADA like its text.
  The Space name's chevron is 11 pt wide with a 1.9 pt stroke.

#### Context menus on macOS

- Context menus as Arc for macOS': a pinned tab's and a favorite's have Change Icon...
  before Rename, the favorite's also Edit Pinned Page and Move to; a folder's has
  Change Icon... before Rename. Tabs, pinned items and favorites have Arc's Share
  after Copy Link: a submenu of the system's sharing services for the page's link,
  with their icons (AirDrop, Mail, Messages, Notes, Freeform, Reminders and the
  like, Chromium's File > Share list without Safari's Reading List; Mail goes
  through a mailto: link, as in Chromium, whose Mail service crashed it), listed
  once a minute at most since the system reads them from disk. A folder's menu
  starts with Arc's Share Folder...: the system's share picker, beside the folder's
  row, for all the links of the folder and its subfolders (`alerse_share_mac.mm`). The Space menu is Change Space
  Icon..., Rename Space..., Edit Theme Color... | New Folder, Collapse / Expand All
  Folders | Delete Space, Arc's grouping without its Set Profile, Live
  Folders, Share, Export and Manage Spaces (account and services). Delete Space stays
  dimmed for home, which cannot be deleted.

#### Hover colors of the name and New Tab

- Hovered, the Space's name brightens to #8D8D8D, and New Tab's "+" to #959595 and its
  text to #8B8B8B, as Arc's; "..." on the name is 18 pt from the plate's end.

#### Back and forward arrows

- Back and forward show no plate while dimmed, as Arc's; their arrows are Arc for
  macOS' own (`alerse_mac_back.icon`, `alerse_mac_forward.icon`): 13 pt long with
  a 10.5 pt head in 1.5 pt strokes (the tip half a point in, as the round join
  reaches past it: they had drawn 13.5 pt); dimmed, white at 25% over the frame (#525252,
  #6A6A6A in an inactive window), and "..."'s dots at 60% (#A1A1A1, #B0B0B0), as Arc's
  (Chromium's arrows were 13 pt high with 2 pt strokes, in its brighter disabled grey; `look/mac-arc-arrows.patch`). The address field lightens to #39393A under the pointer; "Search..." is medium
  weight.

#### Top edge

- No line along the window's top edge: the top container's separator, which Windows
  already drops, would draw a light line over the card there.

#### Menu bar

- Menu bar (`look/mac-menu-bar.patch`, `look/mac-menu-bar-arc.patch`): Arc for macOS'
  menus in its order, with its items and keys wherever Alerse has the feature:
  - **App menu**: About, Settings... (Cmd+,; Arc's reads Preferences..., but macOS
    renames an item so titled to Settings... in apps built with its current SDK, as
    Arc is not), Set as Default Browser (Chromium's `IDC_SET_BROWSER_AS_DEFAULT`, the system asks),
    Import from Another Browser... (the import page, which also reads Arc's Spaces),
    Update Available... (only once the update check found a newer release; it opens
    its page), Services, Hide, Hide Other Windows (Arc's name for Hide Others), Show
    All, Warn Before Quitting, Quit. Chromium's Delete Browsing Data is hidden, as
    Arc's menu has none: it keeps Shift+Cmd+Delete (a hidden item's key still fires:
    `cr_firesForKeyEquivalentEvent` checks only that it is enabled). Warn Before
    Quitting stays, although Arc has none: Chromium's Cmd+Q guard has no other switch.
  - **File**: as Arc's, in its groups: New Tab... (with Arc's ellipsis: it opens the
    command bar), New Window, New Incognito Window, Restore Last Closed Tab
    (Shift+Cmd+T); Open Command Bar (Cmd+L, Arc's name for Chromium's Open Location);
    Close Window; Chromium's Share submenu, Save Page As (Shift+Cmd+S) and Print
    (Cmd+P), without ellipses as Arc's. Open File is hidden, as Arc's menu has none,
    keeping Cmd+O. Close Tab stays (Arc has none): Chromium's app controller swaps
    Close Window and Close Tab between Cmd+W and Shift+Cmd+W with the window, and
    shows them again.
  - **Edit**: Chromium's, with Arc's copy items right after Copy, as in Arc: Copy URL (Shift+Cmd+C), Copy URL as Markdown
    (Option+Shift+Cmd+C, "[Title](url)", `IDC_ALERSE_COPY_URL_MARKDOWN`,
    `look/command-bar-actions.patch`) and Copy URL as Quote (Control+Shift+Cmd+C,
    dimmed without a selection: the selected text as a Markdown quote, a blank line,
    then "[Title](url)"; `IDC_ALERSE_COPY_URL_QUOTE`, done in
    `browser_native_widget_mac.mm`).
  - **View**: Hide Sidebar, Show Sidebar while hidden (Cmd+S; Save Page As moves to
    Shift+Cmd+S, as in Arc), Collapse Pinned
    Tabs (Expand Pinned Tabs while collapsed, dimmed for a Space without pinned
    tabs), Stop, Reload, Force Reload, the Split View items (see Split View), zoom,
    Cast, Developer and, last after a separator as in Arc, full screen. Its keys read
    as Arc's: Add Split View Control+Shift+= and Close this Split Pane
    Control+Shift+- (the menu item shows the key itself with Shift, not Chromium's
    "+" and "_"; as such an item does not match the key press, the press is matched
    by key code in `global_keyboard_shortcuts_mac.mm`), Enter Full Screen
    Control+Cmd+F (Chromium's adds the Globe key and an alternate item; here
    `AcceleratorForEnterFullscreen` is Arc's and the alternate is gone). macOS still
    draws the full screen item's key as Globe+F, as it does for apps built with its
    current SDK; the key itself is Control+Cmd+F.
    Chromium's Always Show Toolbar in Full Screen is not there: Alerse for
    macOS has no toolbar. Chromium's bookmark bar, toolbar and tab strip
    switches are not there; turning vertical tabs off would remove the sidebar.
  - **Spaces**: New Space..., Edit Theme... and Rename Space (Arc's ellipses: none
    on Rename Space, which edits the name in place) for the shown Space (Edit
    Theme's picker opens over the Space bar, as there is no menu to open it at), the
    next and previous Space (Option+Cmd+Right and Left, `IDC_ALERSE_*_SPACE`, run by
    the Space bar through `alerse_space_command_hook.h`), then, as in Arc, every
    Space of the Space bar in its order with its icon, about 12.5 pt as Arc's (a
    glyph drawn as a template image, so the menu tints it, or for a Space with a
    theme in the theme's light shade, as Arc's light blue Work book; an emoji in
    its colors), the shown one checked, and
    Control+1 to 9 on the first nine. The list is filled each time the menu opens
    (`AlerseMenuListDelegate` in `main_menu_builder.mm`, answered by
    `AlerseSpaceSwitcher::GetMenuBarList` through `alerse_menu_bar_hook.h`).
  - **Tabs**: Arc's items only: New Tab..., Pin Tab (Cmd+D, which unpins a pinned tab
    and then reads Unpin Tab, as Arc's Pin/Unpin Tab, and which Chromium gives to
    bookmarking: here pinning is bookmarking), New Folder..., the next and previous tab
    (Option+Cmd+Down and Up), Reveal Tab in Sidebar (scrolls the sidebar to the shown
    tab's row; dimmed for a favorite's tab, which has none) and Clear Today
    (Shift+Cmd+K: the shown Space's tabs that are not pinned close; Arc archives
    them, Alerse has no archive), run by the sidebar through the same hook.
    Chromium's tab items and its list of the window's tabs (`TabMenuBridge`, which
    here would list every Space's tabs) are not there.
  - **Archive**: Chromium's History without Home, under Arc's names: Go Back, Go
    Forward, recently closed and visited, View History (Cmd+Y). Arc's View Archive and
    Clear Archive are left out with the archive.
  - **Extensions**: as Arc's, the installed extensions by name with their icons (a
    click runs one as its button would: its popup or its action, as the site controls
    do), then Add Extension... (the Web Store) and Manage Extensions... (the
    extensions page). Filled each time it opens, like the Spaces.
  - **Window**: Minimize Window (Arc's name), macOS' items, View Downloads...
    (Shift+Cmd+J, Arc's name) and the window list; Chromium's Show As Tab and Name
    Window are not there, as in Arc. Chromium's
    Extensions and Task Manager items moved to their places. A window is named
    after the Space it shows ("home"), as Arc's, in this list, Mission Control
    and the Dock, not after its page (`BrowserView::GetWindowTitle`, from the
    Space bar through `alerse_menu_bar_hook.h`; it is renamed on every Space
    switch or rename). An incognito window keeps the page's title (on its empty
    page, which has none, "Incognito" as its sidebar says:
    `look/incognito-window-title.patch`; it read "about:blank#arcomium-space"), and so does
    the window's accessible name (VoiceOver), which Chromium takes from the page
    (`GetAccessibleWindowTitle`; Arc's reads "home" there too).
  - The items AppKit adds to an Edit menu (Start Dictation..., Emoji &
    Symbols, AutoFill) are not there, as in Arc: the menu builder registers
    `NSDisabledDictationMenuItem`, `NSDisabledCharacterPaletteMenuItem` and
    turns AutoFill's `NSAutoFillOrSystemInsertMenuEnabled` and
    `NSAutoFillSystemInsertMenuEnabled` off (registration defaults only, for this
    app; nothing is written to the user's defaults).
  - **Help**: Chromium's help page and Arc's Troubleshooting > Open Task Manager.
  - The menu bar is macOS only (`main_menu_builder.mm` and
    `browser_native_widget_mac.mm` are macOS files): Windows and Linux keep the logo
    menu, as Arc for Windows has no menu bar, and Ctrl+1 to 9 stay Chromium's there.
    The hooks the Space bar and the sidebar answer (the Spaces list, the shown Space's
    name, Reveal Tab, Collapse Pinned Tabs, Copy URL as Quote) serve the logo menu and
    the command bar on every platform.
  - **Cmd+1 to 9**, as Arc's Go to Sidebar Item 1 to 8 and Go to last Sidebar Item:
    the sidebar's pages as drawn, favorites first, then the Space's pinned items and
    its tabs (a split's halves one by one); folders, the Space's name and New Tab do
    not count. Chromium's Select Tab 1 to 8 would count every Space's tabs in the tab
    strip. In no menu, as in Arc; without a sidebar (a popup window) Chromium's tabs.
  Bookmarks stays in the menu bar hidden, for the code that fills it by tag. Profiles
  stays hidden as well: its controller needs the menu (without it, rebuilding the
  menu never ends and startup hangs), so it is filled for the Dock menu and never
  shown (`profile_menu_controller.mm`); Arc has no Profiles menu.

#### Tooltips

- Tooltips are Arc's black pills (`alerse_tooltip.cc`), not the system's: 29 pt
  high with round ends (the corner at the pointer 9.5 pt, as Arc's), #020202 inside a
  #1B1B1B and a #3B3B3C hairline, the name in 12 pt medium #DADADA 11 pt in, the
  shortcut as #CDCDCD keycaps (18 by 18.25 pt with a 2.5 pt #777777 lip at the bottom,
  4 pt apart, 6 pt after the name and 11.5 pt before the end) at the end: "Go back" Cmd [, "Go forward" Cmd ], "More options", "home" Control 1,
  "Downloads" Shift Cmd J. A pinned tab's has a second line, "Double-click to rename" in 11 pt
  #909090, 44 pt high, half a point left of and above the name's inset, ending 9.5 pt
  after its longer line (141.5 pt for "Double-click to rename"), as Arc's, unless the
  item has a name of its own: Arc's renamed pinned tabs (its items with a title, as
  iCloud Mail and Twitch) show the name alone. Alerse marks an item named by
  Rename... (the editor's new title, `BookmarkNodeChanged`) or imported with Arc's own
  title with meta info `arcomium.renamed` = "1". They
  show a second after the pointer settles (0.9 s for a two-line one: Arc's row hint,
  also a folder's nested row's, shows within 1.05 s), 7 pt
  right of and below it (the two-line one 11 pt right of it and 5 pt below, as Arc's), or, near the window's bottom (the Space bar), centred 3 pt
  above the button; if another hid within half a second, as soon as the pointer
  stops (400 ms without a move), where it stops, as Arc's (shown at once, it sat
  where the pointer entered the button); they hide when it
  leaves or clicks. Downloads' stays under the pointer, reaching out of the
  window, as Arc's Library button's does. Favorites, folders, New Tab
  and "+" have none, as in Arc. The rows' cross is 11 pt (a 24 pt icon), centred 18 pt
  from the highlight's end.

#### Hover colors of Space buttons and the field

- A hovered Space button other than the shown one takes a light shade of its Space's
  theme color (hue kept, 45% of its saturation, 66% lightness), as Arc's book turns
  #96B6BC; a Space without a theme stays grey.
- The Space name's chevron and "..." are #ABABAB on hover, as Arc's.
- The sidebar field's magnifier brightens under the pointer, #828282 to #898888,
  as Arc's.

#### New window position

- A new window opens 29 pt right of and below the last one, as Arc's
  (`window_sizer_mac.mm`; Chromium's 22).

#### Cocoa arguments

- Started with a Cocoa argument such as `-AppleLanguages (de)` (how a Mac app is
  run in another language), Chromium took the value for a URL and opened
  http://(de)/, whose host a debug build's DCHECK rejects
  (`IsCanonicalizedHostCompliant`, from the bounce tracking cleanup); on macOS
  the value after a `-Apple...` or `-NS...` key in the process' own argv is
  not a tab (`startup_tab_provider.cc`).

#### Compact command bar

- Compact command bar: a click on the sidebar's field opens Arc for macOS' compact
  command bar over it, 348 pt wide from a point right of the field's left edge and two
  points above it (its dark outer ring 8 pt in and 44 pt down, as Arc's),
  with a 45 pt input in 15 pt (3 pt below its row's middle) and the same rows. On a page it
  opens on the page's address, selected, as Arc's (measured 2026-11-12): typing replaces
  it and Cmd+C copies it; on an empty Space it opens empty, listing the open tabs.
  File > New Tab, Cmd+T, Cmd+L and New Tab open the centred one, as
  in Arc (`compact_command_bar_` in `alerse_address_bar.cc`). Cmd+L and Ctrl+L
  (`IDC_FOCUS_LOCATION`) open it the way a click on the address does
  (`look/focus-location-command-bar.patch`): Chromium's own focus path lost the focus
  when the field moved into the panel on an empty Space, which left the panel showing
  the idle field and no rows.

#### Pinned items in the command bar

- A sidebar item picked in the command bar (a row of the empty list or a bookmark
  match) shows its pinned tab, as a click on it does: its tab if it is open, else a new
  tab bound to it, never a second, unbound tab (`look/command-bar-pinned-items.patch`,
  `AlerseSidebarView::OpenPinnedItemOf`). Only the shown Space's items count.

#### Command bar on a page's address

- On a page's address (the click above, or Cmd+L) the command bar shows the page's icon
  in the magnifier's place and lists the page itself first, by its title, the row
  selected, then up to four pages from history, the latest first, as "Title — site/path"
  (`look/command-bar-on-page.patch`: `open_tab_provider.cc`, `omnibox_edit_model.cc`,
  `LocationBarView::GetAlerseCommandBarIcon`). History here is Chromium's in-memory
  database, the addresses typed or picked in the command bar: it is read at once, with
  no wait for the history thread; pages reached only by links are not in it. Arc's fifth
  row, Contact the Team, is left out. Chromium's "Ask Google about this page" row
  (`ContextualSearchProvider`) is gone from the on-focus list: there is no Lens to ask. The suggestions
  follow the panel's width; their old 560 px minimum, from before the command bar
  took the location bar into its panel, is 320 (`omnibox_popup_presenter_base.h`). Windows keeps the centred one for a click on the
  address: Arc for Windows opens its command bar from the top bar's address in the
  window's middle. The caret in the command bar is Arc's blue #3478F6 as the screen shows
  it (`Textfield::SetCursorColor` with #0A7AFE: views colors are sRGB, and the
  display's color matching showed #3478F6 as #4676EE), what is typed #E4E5E8 (`omnibox_view_views.cc`) in medium weight,
  as Arc's (regular was 17% lighter and 4.5 pt narrower over its placeholder, which
  takes the same font); a row's address
  after the title is #656565, #989898 on the selected row, and the selected row's
  magnifier white (the icon's own rule tints it with
  `--color-searchbox-results-dim-selected`, so that is what is set white).

#### Hidden sidebar on macOS

- Hidden sidebar (View > Hide Sidebar, Cmd+S; the menu then says Show Sidebar; every
  platform, see below for Windows and Linux): as in Arc for macOS the page takes the whole window but its 10 pt margin, with no toolbar
  and no window controls, and the sidebar floats back over the page while the pointer
  is at the window's left edge (Chromium's expand on hover, 200 ms in and 160 ms out): a #0D0D0D panel
  5 pt in from the window's edges with 11 pt corners and a 1 pt #565656 rim, casting
  a faint shadow (a quarter of Chromium's, a couple of levels off the page beside it
  as Arc's), holding
  the whole sidebar, its header and the window controls. What the sidebar shades
  off the frame is then shaded off that darker panel, as Arc's: the field #262626,
  the dimmed arrows #4A4A4A, "..." #9D9D9D, Downloads and "+" #9C9C9C, the other
  Spaces #383838, the window controls' faces #595957 (the window property
  `alerse::kFloatingSidebarProperty`, read by `FrameColor` and `InkOverFrame`
  in `alerse_frame_color.h`; the window's theme is refreshed when it floats or
  docks). With no page to show (an
  empty Space's blank tab) it stays floating, as Arc's does
  (`look/mac-floating-sidebar.patch`). A bubble that points at the app menu (as
  after a crash, "Restore pages?") points at the window when no toolbar or sidebar
  button is there, or when it opens at startup before the window is registered
  (`app_menu_button.cc`; Chromium CHECKs there and quit the browser on every launch
  after a crash). "Restore pages?" then shows in the middle of the window
  (`look/crash-bubble-in-window.patch`): with its top-right arrow it went under the
  window's bottom edge, outside it. Windows and Linux float the same panel since 0.1.8, Chromium's
  collapsed tab list no longer shows; their window controls and the address stay in
  the top bar while it is hidden, and Ctrl+S toggles it, as in Arc and as Cmd+S on
  macOS (Ctrl+Shift+L, Chromium's own shortcut, still does; Save Page As moves to
  Ctrl+Shift+S). Off macOS the menu bar's other Arc shortcuts are keys too:
  Ctrl+Alt+Right and Ctrl+Alt+Left go to the next and previous Space, Ctrl+Shift+K
  clears Today (`look/arc-shortcuts-off-mac.patch`).

#### Inactive window

- Inactive window (another app in front): Arc for macOS lightens the whole window,
  its #161616 frame to #393939, and every shade off it follows. Alerse's
  `kColorFrameInactive` is the active frame blended 15% toward white (#393939, a
  Space's tint as much), and what it paints off the frame (the sidebar field, the
  favorites' tiles, the overflow lines, the frame around the page card) takes the
  color the window paints now (`alerse::FrameColor()`, `alerse_frame_color.h`);
  the browser view repaints them when the window turns active or not. Every other
  opaque shade the sidebar draws (its text, the Space's name, New Tab, the
  magnifier, the rows' plates) keeps its share of white over the lighter frame
  (`alerse::InkOverFrame`; #808080 on #161616 is #929292 on #393939, as Arc's
  Space name), and the sidebar rebuilds its rows when the window turns active or
  not. "Search..." takes #838383 (`kColorAlerseSidebarPlaceholderInactive`, set
  with the inactive frame). The Space bar's icons are dimmed to half that contrast,
  as Arc's (Downloads and "+" #747474, the shown Space #969696, the others #4A4A4A).
  The blank page lightens with the frame (#2B2B2B to #4B4B4B, Arc's #4C4C4C): the
  page card and the page's own background (`contents_web_view.cc`) follow it. "..."
  shows no plate (Arc's #474747 had been its hover stuck), and the window controls
  are grey (below) (`look/mac-inactive-window.patch`). Windows
  and Linux keep one frame color whatever the window's state, as Arc for Windows:
  there `alerse::FrameColor()` is always the active frame.

#### Window controls

- Window controls: the zoom button sits 21.5 pt after the minimize button, 1.5 pt
  closer than AppKit's spacing, where Arc's is. Arc shows them in an active window as
  #60605D discs in a 1 pt ring of their color (#592F2A, #5F4E0F, #2A4D10), in an
  inactive one as #767674 discs in #444444 rings, the zoom button's face a narrower 9x11 pt oval, and in color
  with their symbols only under the pointer. The frame
  (`browser_native_widget_window_mac.mm`) draws that look in a view over the buttons,
  which go nearly transparent meanwhile (2%: AppKit disables a button at alpha 0, and
  with the close button File > Close Window, Shift+Cmd+W) and still take the clicks; the view's own tracking
  area tells it when the pointer is over them. In the floating sidebar the frame moves
  them 5 pt right and down with the panel's inset (`_closeButtonOrigin`,
  which the other two follow, and `_zoomButtonOrigin`).

#### Hover highlight

- Hover: the highlight follows the pointer at once, without Chromium's fade: the
  button's hover animation did not advance on macOS.

#### Text width

- Text width: SF Pro carries an AAT tracking table that tightens text as the size
  grows. HarfBuzz applies it for the font's `ptem`, and Chromium's views text
  (`ui/gfx/harfbuzz_font_skia.cc`) never set one, so it was tracked as 12 pt: at
  Arc's 14 pt rows each glyph sat about 0.15 pt wider, 1 to 1.5 pt over a label
  (Blink sets `ptem`, so web text was already right). On macOS the HarfBuzz font
  now gets `ptem` = its size, the way Blink does, which makes all views text match
  AppKit's width (mac-arc-look.patch). Windows and Linux fonts have no tracking
  table, and the change is gated to macOS. Measured against AppKit's own rendering
  of the same strings (ink width at 2x), views text now matches within half a
  point (Gmail 35.5, Development 87.5, Shopping 63 pt, as Arc's). Where kerning
  pairs are strong ("Yo", "Tu", "Tw") views text was narrower still: YouTube
  53 pt against AppKit's and Arc's 55, Twitch 40.5 against 41.5. SF Pro is a
  variable font and its kerning varies with the optical size axis; CoreText
  sets `opsz` from the point size, while HarfBuzz, given no variations, kerned
  at the font's default instance, whose pairs are tighter. On macOS the
  HarfBuzz font now gets the typeface's axes and `opsz` = its size
  (`harfbuzz_font_skia.cc`, `look/mac-arc-polish.patch`), as CoreText; glyph
  advances already came from Skia's CoreText font. Subpixel positioning adds
  nothing.
  What remained wider was the weight or size Alerse picked: the popovers' labels
  were semibold where Arc's are medium ("Toggle Sidebar" 93.5 against 92.5 pt), the
  compact command bar's placeholder 15 pt where Arc's is 13 (149.5 against 135.5).

#### Known gaps on macOS

Known gaps on macOS: the command bar's caret is 22 pt high where Arc's is 21 (the
text field's line height); Arc's Edit menu ends with Format (Font: Bold, Italic,
Underline), which Alerse leaves out, as Chromium has no rich-text commands for
the menu to run; macOS gives Find its magnifier icon, Arc's has none, and adds
a separator after Find that Arc's menu does not show. Arc's frame and sidebar are translucent (they take a little of
what lies under the window, #141414 to #191919); Alerse's are a solid #161616.
Row text, favicons and favorites sit half a point above Arc's, the Space icon half a
point below, the bottom bar's icons half a point left: views lay out in whole points.
The folder popup casts no shadow at all (Arc's darkens what is beside it by 2 or 3
levels; the macOS shadow it had drew a dark outline Arc's has not). A command bar
opened in a new window lights the row under a pointer that has not moved, where
Arc's waits for a move (the popup's page takes the hover from the pointer's place
when it shows). The Go forward tooltip shown right after Back's sometimes sits where
the pointer entered the button, 4 pt left of and 2 pt above where it stopped (seen
once in two runs; not reproduced here), and the Space bar's and "More options"
tooltips are up to 1.5 pt wider or narrower than Arc's (the text is measured, the
keycaps are fixed). The folder popup's dates read in the browser's language, which
the comparison runs set to English: Arc's follow the system's ("1 неделю назад").

#### Menu bar gaps

Menu bar gaps:
Arc's Show Toolbar (Shift+Cmd+D; Alerse for macOS has no toolbar to show), View >
Appearance (Arc's light or dark web pages), Blank Window, Stay On Top, Find and
Replace (Chromium has no replace), Reset all tabs in this Space, Change Profile and
Manage Spaces are not there (Arc's New Little Arc Window is New Little Window, see
Peek and the Little window); Arc-only items (Share Arc, Sign Out, Easels,
Notes, Capture, Library, View Archive, Clear Archive, Arc's help pages) are
left out on purpose. Set as Default Browser stays enabled when Alerse is the
default already (asking the system blocks, which the menu must not); Pin Tab, New
Folder and Clear Today stay enabled where they would do nothing. An extension run from the
Extensions menu opens its popup at the window's top left (its hidden header button
is the anchor), not under the menu bar as Arc's. Copy URL as Quote's
layout (quote, blank line, link) is Alerse's: Arc's was not tried, as that would
take a selection on a page of the user's own browser. Menu items keep Chromium's
names where Chromium has the item (Reload This Page, not Arc's Refresh the Page), as
those are translated, except the items the last comparison named (Hide Other Windows, Restore Last Closed Tab, Save Page As, Print, Go Back, Go Forward,
View History, Minimize Window, View Downloads...), which take Arc's names in English
only, as Alerse's other strings. Arc's File > Share... is a single item; Alerse
keeps Chromium's Share submenu under Chromium's name, which the app controller looks
the submenu up by. Arc's Edit menu dims Undo, Redo, Cut, Paste, Paste and Match Style,
Delete and Select All while no text has the focus; Chromium's (and so Alerse's)
keep them enabled, as the page decides. The Archive menu keeps Chromium's recently
closed and visited lists, which Arc's has not (Chromium's history menu fills them by
tag). The Space bar's left button, Arc's Library (archive and media), is
Downloads since 0.1.7: tooltip "Downloads" Shift+Cmd+J, it opens `chrome://downloads`,
and it has no menu (Arc's "Show media from" is left out).

### What a tab is doing

What a tab is doing shows on its row (and in a favorite's tile, see Favorites):

- while its page loads, Chromium's throbber (`views::Throbber`, in the sidebar's subtle
  grey) stands in the icon's place, except for the tabs the tab strip hides it for
  (restored in the background). The start and the end of a load change only that, on a
  pinned item's row too, without rebuilding the rows (`TabChangeType::kLoadingOnly`);
  memory and CPU updates (`kResourceUsageOnly`) change nothing;
- a background tab that waits for the user (a dialog of its page, a question before
  leaving it, a page that asked for attention: `TabNeedsUser`) has a 6 pt dot at its
  icon's corner, in the glyphs' ink with a ring of the frame;
- the one-color icons of the browser's own pages (Settings, History, Extensions,
  Downloads) go through Chromium's `favicon::ThemeFavicon` against the sidebar's frame:
  recolored to the item ink where they would not read (`TabUIHelper::
  ShouldThemifyFavicon`). On the dark frame and on a light Space color they read as
  they are (blue), so nothing changes there;
- the row or tile that has the keyboard's focus has a 2 pt ring in the Space's item ink
  just inside its plate, and Up and Down move the focus to the row above or below (a
  split's halves one after the other). Rows keep the platform's focus rule: on Windows
  and Linux Tab reaches them; on macOS only with the system's keyboard navigation on,
  as for every button.

### Known gaps

Known gaps: the focus ring and the arrow keys were not seen on screen (the Mac used for
testing has keyboard navigation off, and it is a system setting); rows have no
tree roles; Alerse's own text is English only: most of it is in
`alerse_strings.grdp` and translatable, but the sync page's error messages are
hard-coded English (`alerse_sync_handler.cc`).

## Motion

One header, `alerse_motion.h`, holds every duration, curve and reduced-motion rule of
the sidebar and its popups. Animations run on compositor layers (transform and
opacity), never through layout, a rebuild or a timer used as a frame clock.

| Token | Duration | Curve (`gfx::Tween`) | Used for |
|---|---|---|---|
| instant | 0 ms | none | Hover and selection highlights, pressed states, closing anything that holds the keyboard, the drop indicator's place during a drag |
| `kQuick` | 90 ms in, 63 ms out | in `LINEAR_OUT_SLOW_IN`, out `FAST_OUT_LINEAR_IN` | Things that appear in place, by opacity alone: the address field's hover buttons, popovers and pickers, Arc tooltips, site controls, the folder popup, the command bar, the copy check mark, the drop indicator, the rows' scroll bar (in; out over 140 ms) |
| `kStandard` | 160 ms, or by distance | `LINEAR_OUT_SLOW_IN` | Movement inside the sidebar: the folder curtain, the mini player rising in, a swipe settling |
| `kEmphasized` | 240 ms in, 200 ms out (floating sidebar 200 / 160) | `LINEAR_OUT_SLOW_IN` for the rows, `ACCEL_20_DECEL_100` for the sidebar | A Space's rows entering, sidebar show and hide, the floating sidebar |

Rules:

- **Curves decelerate from the first frame.** The folder curtain first used
  `FAST_OUT_SLOW_IN`; a screen recording showed about 100 ms after the click with a pixel
  or two of movement, which reads as lag.
- **Distance.** The folder curtain takes `clamp(110 ms + 0.18 ms x px, 140, 260)`; a swipe
  that is let go takes `clamp(260 ms x the way left, 70, 220)`.
- **Interruption.** Every layer animation uses `IMMEDIATELY_ANIMATE_TO_NEW_TARGET`: a new
  target starts from where the layer is drawn. Nothing queues or restarts from zero.
- **Leaving is shorter than arriving** (0.7x) and never delays the action: a popover that
  holds the keyboard hides at once, and its pick runs at once.
- **Reduced motion** (`gfx::Animation::PrefersReducedMotion()`): movement takes 0 ms, fades
  stay and are capped at 90 ms. `Move()` and `Fade()` decide it, nowhere else.
- A rebuild of the rows waits for a running folder curtain instead of cutting it short.
- Layers made for an animation go when it ends (the curtain's rows, the mini player's).

What animates:

- **Space switch** (click, shortcut, menu, command bar, routed link). The next Space's
  window is put where the slide starts before it shows: its rows a sidebar's width to
  the side the Space lies on in the Space bar, and a picture of the rows of the Space
  left in their place (`PrepareSpaceEnter`). When that first frame is on screen both
  slide as one strip for 240 ms (`PlaySpaceEnter`), as Arc's do (measured in Arc: 240 ms,
  decelerating). The header, the field and the favorites stand still. A swipe has brought
  the rows in already, so its commit plays no second entrance.
- **The swap of the two windows** (`alerse_window_stack.h`). The window left hides only
  when the next one has presented a frame, with a 250 ms limit. On macOS a window that
  was hidden shows the last frame it drew (the rows as they were, in an inactive
  window's lighter colors; seen as a grey flash in a 60 fps recording), so the next
  window comes in transparent and turns opaque with that first frame, and both windows
  keep an active window's colors meanwhile (`LockPaintAsActive`). In full screen the
  swap is as before: each member is a desktop of its own.
- **Folder curtain**: rows clipped and the rows below shifted on layers, driven on the
  compositor's vsync (`views::AnimationDelegateViews`), with layers only for rows the
  scroll view can show.
- **Tab rows coming and going**. The rows are still rebuilt on most changes; a rebuild
  compares the plain tabs' rows before and after it (`AnimateTabRowChanges`). One
  row that came fades in from half a row up over the standard token while the rows
  below it slide down by a row; one that went lets the rows that followed it slide up
  by a row (`ForDistance`, `kEnter`). Anything else (two rows at once, a split, a
  pinned item's or a favorite's tab, a swipe's preview, a folder's curtain) changes at
  once. Each moving row gets a layer for its move only.
- **The shown tab changing** between two plain tabs' rows rebuilds nothing: the old
  row's plate goes and the new row's comes in place (`SwapActiveRows`), so the rows
  under the pointer and their hover stay. A pinned item's, a favorite's, a split's or
  an empty Space's tab, a pending rebuild, a drag or a curtain still rebuild.
- **A row's close button** fades in over the quick token as the pointer enters the
  row and goes at once as it leaves; the title re-elides at once either way.
- **Sidebar show / hide** (Ctrl+S / Command-S) runs Chromium's animation at the
  emphasized token (`look/sidebar-motion.patch`), but the sidebar keeps its full
  width and slides on its layer (`look/sidebar-slide.patch`); the page's edge follows
  it. Chromium narrowed the sidebar on every frame, which squeezed the favorites into
  slivers and cut the rows' titles down a letter at a time (seen frame by frame,
  2026-11-14). The page is resized once, when hiding starts or showing ends.
- **Floating sidebar** (`look/floating-sidebar-slide.patch`). The panel has its full
  size from the first frame to the last and slides on its layer; Chromium grew its
  width, which squeezed the rows anew on every frame, and emptied it before it shrank
  away. It stays whole until it has slid out (`alerse_float_leaving_`). The window
  controls are the system's and cannot slide: they show and go at once. Hidden, the
  panel's last 5 pt stay under the pointer at the window's edge, which is what keeps
  it open. The pointer in the 5 pt margin round the open panel counts as on it: at the
  screen's edge it cannot be anywhere else, and the panel used to open, close 170 ms
  later and stay closed (seen in a recording).
- **Caption buttons' dim** (Windows): the quick fade (`look/caption-buttons-dim.patch`).
- **Alerse's pages** (sync, import, links): controls change state over 90 ms and a part
  that appears fades in over 160 ms (`alerse_page_style.inc`).

Not animated, on purpose: row hover, closing popovers, and the Space's tint. Each
Space's window has its own tint, so a switch cuts from one color to the next with the
windows. A fade would cover the new window's frame-colored surfaces with the old color
and fade that out, but in Chromium 157 (Windows) those surfaces share their layers
with content: the frame color behind the top bar is painted into the window's root
layer together with the top bar's back, forward and reload buttons; the sidebar's is
painted into `VerticalTabStripRegionView`'s own layer together with its header
buttons and the Space bar; the caption buttons paint it behind themselves on their
own layer. A cover under the root layer's children would hide the top bar's buttons
and fade them in, and it would sit under the sidebar's and the caption buttons'
layers, whose color would cut while the rest faded. Both are seams; the cut is clean. The picker's old six-step blend
(`SpaceColorTransition`) is gone too: a picked color applies at once.

## Icon pickers

Arc's Change Icon... and Change Space Icon... open its picker, built after Arc for macOS'
(`ShowArcIconPicker`, `alerse_popover.cc`) on every platform:

- A popover under the row, its arrow's tip over the row's highlight under the row's
  icon (a favorite's tile: its middle; a Space whose name is not shown: over its
  button in the Space bar, arrow down), and above the anchor when the screen has no
  room below. As in Arc, the row (or the Space's name, or the favorite's tile) is
  not kept lit while the picker is open: only the pointer lights it. No
  sidebar tooltip shows over it (any showing one hides when it opens). Measured on
  Arc for macOS whole (its panel reaches past the window's left edge, which earlier
  captures cut at the window, so their "210 pt" and 6 columns were a part of it):
  273 pt wide from its dark rim, the panel 24 pt below the middle of the row (or the
  Space's name; a 41 pt row's middle is between pixels, and so is the panel's top,
  drawn a fraction into its widget, which starts on a whole point), its left edge 67.5 pt left of the window
  (screen 12.5 to 285.5 pt for a window at 80), 14 pt corners, an arrow 11.5 pt out to
  its dark rim with straight sides at 45 degrees from a rounded tip, flaring into the
  panel (27.5 pt across where it meets it), its tip 94 pt from the panel's left edge,
  cells 31 pt in 8 columns from 13 pt in, glyphs 10 to 14 pt across as Arc's (each
  drawing measured against Arc's cell; a few are drawn up to 20% larger,
  `Glyph::scale`). The panel has a sheen under its top edge fading within 4 pt
  (#646464 at the rim, then #545454, #484848, #454545 every half point, as Arc's) and
  Arc's rim, a half-point dark line outside a point of light, running round the
  arrow too (strokes along the union of panel and arrow, clipped to it). On macOS it
  is vibrant as Arc's: the window server blurs what is behind the window wherever it
  paints (`CGSSetWindowBackgroundBlurRadius`, radius 20, `BlurBehindPopover` in
  `alerse_window_mac.mm`; the blur follows the painted shape, so the corners and
  the arrow's sides stay clear), and the panel is #565656 at 50% over it, which is
  what Arc's pixels give (#2B over black, about #39 over the sidebar, #42 over the
  page; an `NSVisualEffectView` was tried and tints far more, its HUD material
  showing #42 over black). Elsewhere it is #424242 at 97%.
- Folders: a 145 pt panel starting 24 pt below the row's middle, an "Icon" title
  in #ECECEC (centred 136.5 pt in, 22.5 pt down, as Arc's) over Arc's 24 glyphs in 3
  rows of 8 (cells from 42 pt down): star, bookmark, heart, bolt, triangle,
  asterisk, bell, bulb; chat, people, tools, egg, circle, moon, planet, leaf; cloud,
  paw, fork and knife, airplane, note, camera, code, skull. The picked glyph goes
  inside the folder at half size (`arcomium.folder_icon`).
- Pinned items and favorites: Arc's Emoji and Icon segments (centred, 80 pt in; no
  trash), opening on Emoji unless a glyph is picked; the Icon page holds the Space
  picker's 56 glyphs, as Arc's. The pick replaces the favicon (`arcomium.glyph`: a
  glyph's name, or "emoji:" and the emoji, drawn in its colors at the favicon's
  size). Arc's picker has no "none": the current glyph sits on a plate, and picking
  it again goes back to the item's own icon.
- Spaces: a 273 pt panel with Emoji and Icon segments on a faint track (the shown one
  on Arc's blue, #3D81F9, 24 pt high from 14 pt down, from 64.5 pt in) and a trash
  button (centred 197.5 pt in), over 56 glyphs in 7 rows of 8 (cells from 47.75 pt
  down) in Arc's order: star, bookmark, heart, flag, bolt, triangle, asterisk, bell;
  bulb, shapes, grid, apps, layers, server, tray, copy; folder, archive, calendar,
  mail, checkbox, document, book, chat; people, terminal, tools, square, egg, circle,
  moon, sun; planet, leaf, cloud, paw, basket, gift, bed, fork and knife; barbell,
  airplane, note, palette, camera, bandage, code, ball; cloud outline, map, bug,
  pizza, skull, newspaper, thumbs up, train. The picker opens on the page of the
  current icon, which sits on a plate (#FFFFFF at 24%); the trash goes back to the
  default (home's sun, others' dot).
- The Emoji page (a Space's and a pinned item's) is Arc's: a search field (a dark
  #1C1C1C capsule 23 pt high from 12 to 260.5 pt across, 49 pt down, in a 3.25 pt
  #4A769E ring, a #989898 magnifier at 25 pt and the text from 37.5 pt, placeholder
  "Search") that has the caret when the page shows, over a grid of the whole emoji
  set in 8 columns (31 pt apart, rows 33 pt apart, the first centred 91.5 pt down,
  emoji 18.5 pt across as Arc's) that scrolls with the wheel or trackpad beside a faint scroll track,
  a #FFFFFF-at-38% hairline at 228.5 pt, then Arc's category bar: eight outlined
  #929292 icons (smileys and people, nature, food, activities, travel, objects,
  symbols, flags) that scroll the grid to their category. The set is Unicode's
  emoji up to Emoji 15.0 (what Apple Color Emoji and Segoe UI Emoji both draw),
  fully qualified and without skin tones, in the macOS palette's order (1870;
  `alerse_emoji_data.inc`, written by `tools/gen_emoji_data.py` from the
  `emoji-test.txt` the Chromium tree carries). Typing filters it by the emoji's
  English names (every word must appear); Down then moves into the results, Return
  picks the selected one (or the first), Left and Right move the caret until a cell
  is selected. An emoji is stored as `emoji:` and its text (`arcomium.space_icon`,
  `arcomium.glyph`); emoji picked from the earlier 42 still show and are found. A
  Space's emoji is drawn with the system's emoji font (Segoe UI Emoji named
  on Windows), at full strength on the active Space and in the Space's name and
  faded on the other Space buttons (35%, 60% hovered) on every theme: the fade is a
  fixed opacity, not taken from the ink, whose dark shade on a light theme would
  otherwise fade the active Space's emoji too.
- The glyphs are Alerse's own vector drawings in the spirit of Arc's solid set
  (`alerse_glyphs.cc`: filled shapes with holes cut through, in one layer), named as
  Arc names its icons (`heart`, `flash`, `construction`, `restaurant`, `planet`,
  `chatBubbleEllipses`, ...), so icons imported from Arc show as picked ones. The
  names of Alerse's first icons still draw (`bolt`, `lightbulb`, `chat`, `pets`,
  `flight` are shown as their Arc names' cells; `home`, `globe`, `rocket` draw as
  before but are no longer offered; `star` is now Arc's solid star). Arc's sun and
  book keep their earlier drawings.
- The pointer's cell takes a fainter plate; arrows move a keyboard selection, Return
  picks it, Tab switches a Space's page, Escape or a click elsewhere closes the picker.
- Windows and Linux show the same picker. Arc for Windows' picker has not been
  compared yet: its look there follows Arc for macOS until it is.
- Known gaps: Arc's emoji search goes by the system's names in the system's
  language (its placeholder reads in the system's language), Alerse's by Unicode's
  English names, with an English placeholder; the system palette's newer emoji (Emoji
  15.1 and later), skin tones and category headings are not there; the category bar
  does not mark the shown category; there is no custom image; the rim does not run
  round the arrow. The ten new glyphs (star, bookmark, shapes, folder, archive,
  people, terminal, barbell, cloud outline, map) are named after Ionicons, as Arc's
  other names are; Arc's own names for them were not seen in an import.

## Window, address bar and command bar

### Top bar on Windows

- **Windows**: a 32 px top bar in the frame color, as in Arc for Windows 1.129: back,
  forward and reload, the site centered in a pill between Copy Link and the site
  controls, Split View and the window buttons. Chromium's pinned toolbar actions are not
  there (`look/top-bar-no-pinned-actions.patch`): a download showed Chromium's Downloads
  button in the top bar beside the Space bar's, which the download bubble hangs from.
  The page is an inset card with Arc's rim,
  corners and shadow, and Arc's thin icons (`components/vector_icons/alerse_*.icon`).
  Overlay scrollbars follow the Windows setting.

### Window on macOS

- **macOS**: no toolbar while the sidebar shows; the sidebar's header is one row after
  the window buttons, as in Arc for macOS: back, forward and "..." (see Sidebar look,
  On macOS). Header buttons are 28 pt targets and take clicks instead of dragging the
  window. The sidebar and mini player use the system font, as Arc does on macOS (on
  Windows, Segoe UI Variable).

### Compact address

- The compact address shows only the site, centered, with no page actions (Always Show
  Full URLs, in the field's right-click menu, shows the whole address); Copy Link
  and the site controls show on hover (Copy Link not while the command bar has the
  field, nor on an empty Space's blank page, as in Arc). It hides the site's icon, but
  never the
  "Not secure" or dangerous-site warning: without Safe Browsing it is the last warning
  left. It also hides the page actions and content-setting icons (blocked popups,
  location, downloads); camera and microphone use still shows through Chromium's
  activity indicator, and the rest is in the site controls: their first rows are the
  page's zoom while it is not the default (a click sets it back) and one row for each
  content-setting icon Chromium would show now ("Pop-ups were blocked on this page.",
  a blocked download, the clipboard...), whose click opens Chromium's own bubble for
  it under the address field (`SiteControlsView::AddPageStates`). Its extension tiles
  follow an icon, badge or pin that changes while it is open; an extension added or
  removed meanwhile shows the next time it opens. Translate and the
  password key are page actions of their own and still have no row. Clicking it opens the
  command bar. The address field's placeholder is Arc's "Search..." whatever the
  search engine (`look/sidebar-search-placeholder.patch`).

### Command bar

- **Command bar**: Chromium's omnibox in a 722 px panel centered in the window, opened
  by clicking the address or New Tab (which then opens a new tab), closed by a click
  outside. The browser's New Tab command (Ctrl+T, Cmd+T, File > New Tab) opens it the
  same way while the sidebar shows, as in Arc on both platforms: no tab opens until
  something is picked, so Escape leaves none behind (`alerse_command_bar_hook.h`,
  set by the sidebar, called from `BrowserCommandController` and from the New Tab
  action in `browser_actions.cc`, which is what a shortcut or menu runs on Windows;
  `spaces/new-tab-command-bar.patch`). One Escape closes the
  command bar, whatever it shows, as Arc's (Chromium's would first revert what was
  typed, then close the suggestions). Its placeholder stays while it has focus, dimmed;
  on Windows and Linux the top bar's pill shows it bright again once the command bar
  closes (`AlerseAddressBar::StopEditing`). With
  the field empty (a new tab, an empty Space) the command bar asks for its rows itself
  once the field is in the panel: moving the field there takes focus away and back,
  which drops the on-focus suggestions, so a click on the address used to open it
  bare. The popup shows once its page has painted
  (`kOmniboxWebUIDeferShowUntilVisualStateReady` on, `defaults/feature-defaults.patch`):
  Chromium's default shows it transparent until the page reports a new height, and
  rows of the same height as before never did, so the first command bar after start
  opened without rows. Empty,
  it lists up to five open tabs to switch to, the most recently shown first, leaving
  out the active tab and empty Spaces' blank pages (`open_tab_provider.cc`), as Arc
  lists its tabs; when fewer are open, the sidebar's items (bookmarks, the most
  recently used first, then in the sidebar's order) fill the five, as Arc lists its
  pinned tabs even with no page open. As in Arc, those rows show the title alone and an
  open tab's ends, at the row's end, in "Switch to Tab" (dim, white on the selected
  row; `searchbox_handler.cc` renames Chromium's "Switch to this tab"); the first is
  selected (an open tab or else a sidebar item), so Return opens it, and arrowing
  through them leaves the input empty
  (`open_tab_provider.cc`, `omnibox_edit_model.cc`). A new window's popup page is
  bound after the rows are chosen, so its handler sends it the selection along with
  the rows (`webui_omnibox_handler.cc`); else nothing showed selected there. No row shows Chromium's keyword
  chip ("Search Google"). A new window (Cmd+N, Ctrl+N, File > New Window) is an empty Space
  with the command bar open, as Arc's on both platforms, where the sidebar shows;
  otherwise (the sidebar hidden, or no sidebar) a new tab replaces the blank one
  (`browser_commands.cc`, `OpenEmptyWindow()`). The
  sidebar may get the location bar just after the window shows; the command bar
  waits for it. On macOS the panel is
  Arc for macOS': 765 pt wide, its 327 pt top 5 pt above the window's middle, a 63 pt
  input row in 18 pt with "Search or Enter URL...", 12 pt corners, #1C1C1C (Arc's
  #1B to #1D) with a #505050 rim (Arc's #515051) with a one-pixel #030303 ring outside
  it, a broad soft shadow as Arc's (one shadow 20 pt down with a 109 pt blur, about
  32 pt sigma, at 53% black; the page under it keeps 0.62 of its brightness at the
  panel's bottom edge, 0.74 20 pt down and 0.86 40 pt down, against Arc's 0.65, 0.73
  and 0.81; beside it 0.75 and 0.86 at 20 pt against 0.74 and 0.82; over it 0.86 and
  0.95 against 0.85 and 0.92. Chromium's elevation 16 shadow darkened only 8 pt round
  it. Its widget reaches 109 pt beyond the panel's sides (89 above, 129 below) for
  it, and passes the mouse events there on to the browser window, as Chromium's
  frame does for its own shadow; `AlerseShadowBorder` in `rounded_omnibox_results_frame.cc`, whose
  `GetShadowInsets()` gives the popup's widget room for it), #303030 for the selected row
  and a one-pixel #313030 line under the input, 6 pt above the first row and 7 below
  the last. Its rows are Arc for macOS' (`<if expr="is_macosx">` in the popup's CSS):
  46 pt high and 50 apart, 8 pt in from the panel, the title 13 pt medium #D1D1D1
  (white when selected; the empty bar's rule that colors the title's text itself
  white is overridden) 51 pt in, Switch to Tab 13 pt from the row's end before Arc's
  arrow on a 24 pt square (#252525 with a light arrow, white with a dark one when
  selected). Elsewhere rows are 48 px, regular weight; searches show only their words, history rows
  a remove ×, bookmarks the site's icon (a plain page, never Chromium's star, in either
  of its icon sets). A page without an icon of its own shows the sidebar's outlined globe, not Chromium's filled one (`page_cr23.svg`, `look/command-bar-globe.patch`). A row with no second text (a sidebar item) shows no dash after its
  title (`searchbox_match.ts`). On Windows and Linux (aura) the suggestions are a
  child of the browser window, not of the command bar's panel
  (`omnibox_popup_presenter_base.cc`): parented to the panel, they were no transient
  child of the window, which stacked itself over them whenever it activated (after one
  Escape the command bar opened without rows). Their window covers the input row (its
  cutout); clicks there pass through to the input over the whole row, shadow
  included, measured again at every layout (`RoundedOmniboxResultsFrame::Layout`):
  Chromium sets that once, for the location bar the window was made for, and the
  command bar's taller row lost its lower part, Arc's (i) with it, which then did
  nothing on Windows. Open tabs are suggested without `@tabs`, and
  picking one switches to it. Tabs and bookmarks also match fuzzily
  (`look/command-bar.patch`): at least two letters, close together and starting a word
  unless they are contiguous, at most three bookmarks. The input is 18 pt in medium
  weight on every platform, Arc for macOS' (`look/windows-mac-command-bar.patch` lifts
  the macOS gates of `location_bar_view.cc` the earlier patches set; the caret is the
  system's blue on macOS only), as are the panel's 12 pt corners, rim, dark ring and
  broad shadow, the rows' layout and colors and "Search or Enter URL..." (15 pt in the
  compact command bar, where "Search or Enter URL..." is 13 pt, and the
  magnifier, the input and the (i) sit 2, 2 and 3 pt lower in its row than
  centred, with no line under the input, as Arc's). The centred panel is 764 pt
  wide, its dark outer ring making Arc's 765. On Windows and Linux the top bar's pill
  and the command bar it opens say "Search or Enter URL..." whatever the search engine
  (`look/top-bar-placeholder.patch`): Chromium put back "Search Google or type a URL"
  each time the field took the focus, and in a new Space's pill once its search engines
  loaded.

### Command bar actions

- **Command bar actions** (`look/command-bar-actions.patch`,
  `alerse_command_provider.cc`): as in Arc, typing an action's name lists it as a
  row of its own with its icon and keyboard shortcut, and picking it runs it in the
  window instead of opening a page: Copy URL, Copy URL as Markdown (`[title](url)`),
  New Window, New Incognito Window, Pin Tab or Unpin Tab (by whether the shown tab is
  pinned), Add Split View (see Split View), New Space, Rename Space…, Edit Theme…, New Folder, Collapse or Expand
  Pinned Tabs (macOS, where the pinned tabs collapse; a Space with pinned items),
  Settings and View History, and Switch to <Space> for each other Space of the window
  (found by the Space's name, "switch" or "space"). Arc's archive actions (Archive Tab, View Archive, Clear
  Today, which archives) are not there: Alerse has no tab archive (see Left out on
  purpose). A name matches as a whole (3), by its start (2) or when every typed
  word starts a word of the name or of a few aliases ("preferences" finds Settings,
  "library" View History) (1); at most three rows, the best first, then the shorter
  name; actions that cannot run now (Pin Tab on an empty Space's blank page; Copy URL and
  Copy URL as Markdown there or on any blank page, whose address means nothing;
  Rename Space without a Space bar, anything of the Spaces in incognito) are
  left out. Copy URL and Copy URL as Markdown copy nothing there by their keys either
  (`chrome::GetAlerseCopyableContents`). The best row leads the list (relevance 1350: over the search for what was
  typed, 1300, under a site the address completes to, 1400 and up, as in Arc, where
  "theme" runs Edit Theme… but "pin" opens pinterest.com) once three letters are
  typed, so Return runs it; the others follow the search and the sites. Not for an
  address being typed, nor in a site's search. The rows are Chromium's standalone
  action matches (`kPedal` with a takeover action, as the Lens entry), made by an
  autocomplete provider the location bar adds (`AutocompleteProvider::Type::
  kAlerseCommand`, `AutocompleteController::AlerseAddProvider`), so the popup,
  the arrow keys and the location bar's icon treat them as Chromium's own; each has
  its own `chrome://alerse-command/<id>` address only so they are not merged as
  duplicates, never opened; the popup knows them by that address
  (`searchbox_match.ts`), so Chromium's own pedals stay ordinary rows. Picking one
  first gives the page the keyboard, which closes the command bar (the sidebar does
  so in a task of its own), then, in a later task, runs the command, so a dialog it
  opens (Rename Space…, New Folder) does not take the focus first and leave the
  command bar open under it (a browser command, or the Space
  bar's and sidebar's through `alerse_space_command_hook.h`, whose
  `GetSpaceCommandState` says whether one can run and which way a toggle goes).
  The shortcut is the window's own accelerator for the command (`BrowserView::
  GetAccelerator`): "⇧⌘C" on macOS, "Ctrl+Shift+C" on Windows, none where the
  command has no key. Windows and Linux have Arc for Windows' keys for them
  (`accelerator_table.cc`): Ctrl+Shift+C copies the URL, Ctrl+Shift+Alt+C copies it as
  Markdown (⌥⇧⌘C on macOS, as Arc for macOS) and Ctrl+D pins or unpins the tab.
  Chromium gives Ctrl+Shift+C to DevTools' inspect (F12, Ctrl+Shift+I and DevTools'
  own window keep it) and Ctrl+D to bookmarking, which here is pinning. Chromium shows a match's description first when it has
  one, so the name is the row's description and the shortcut its contents; the
  icons have no SVG resource, so the handler skips the lookup for these rows
  (`searchbox_handler.cc`). In the popup an action's icon sits on Arc's
  24 px tile (#252525 with a #B5B5B5 glyph, white with a #5D5D5D one on the selected
  row; the handler sends it as a 32 px image, `webui_omnibox_handler.cc`) and its
  shortcut at the row's end on a key chip of the same colors (`searchbox_match.ts`),
  as Arc for macOS'; Windows and Linux show the same, Arc for Windows' not having
  been measured. Pin Tab in the menu bar (Cmd+D) now also unpins a pinned tab, as
  Arc's Pin/Unpin Tab.

### Command bar input

- **Command bar input**: the magnifier gives way to the icon of what the input
  completes to, as in Arc: the site's favicon for an address or a page, the action's
  icon for an action (Chromium's `OmniboxView::GetIcon`); searches keep the
  magnifier. While Tab would search the site the address completes to (a keyword
  search engine, Chromium's keyword hint), the input ends in Arc's "Search <site>"
  in grey and a "Tab" key cap, in place of the (i); Tab then starts the site's
  search, whose chip (Chromium's selected keyword view, with its own icon in the
  magnifier's place, its text without subpixel antialiasing on the panel's
  translucent widget) starts the input, without the placeholder after it. Chromium
  offers the hint only once the input is a whole keyword ("bing.com", not "bing").
  Arc's (i), a dark 24 px disc 32 px from the panel's end (on macOS 33 pt, a
  point lower, its glyph 18 pt (a 15 pt ring) in #8D8D8D, as Arc for macOS'), opens
  the site's controls (as the address field's button does); what Arc's own (i) opens
  has not been checked. On Windows and Linux it first closes the command bar, as its
  actions do (the page takes the keyboard), and the controls open at the top bar's
  button in a later task (`LocationBarView::ShowAlerseSiteControls`): opened over
  the command bar they took its focus and left it half closed, its input showing the
  compact address and, once they closed, no rows. Known gaps: the (i), the action tiles, the key chips and their
  colors come from Arc for macOS and show the same on Windows and Linux, Arc for
  Windows' command bar not having been compared; the selected keyword row ends in Chromium's small keyword button.

### Alerse menu (the logo at the top of the sidebar)

- **Logo menu** (Windows, Linux; `alerse_app_menu.cc`): Check for Updates (or
  "Update to Alerse X.Y.Z"), Import from Another Browser..., new tab, window and
  incognito window, Spaces, Tabs, Extensions and Developer submenus, Downloads,
  History, Bookmarks, Sync, Search Engine, Link routing, Settings, Print, About, More...
  (Chromium's full menu) and Close Window. As macOS' menu bar has them, the Spaces
  submenu lists the window's Spaces after Next and Previous Space (the shown one
  checked; picking one switches to it), and the Tabs submenu starts with Copy URL, Copy
  URL as Markdown, Copy URL as Quote (dimmed without selected text), Reveal Tab in
  Sidebar and Collapse (or Expand) Pinned Tabs, which the command bar offers too. Share
  stays macOS-only: Chromium has no share menu of the system's services on Windows.
  The window is named after the shown Space on every platform, as Arc for macOS
  names it (the taskbar and Alt+Tab say "home"). The submenus have no command of their own
  (id 0), so the menu says they are enabled itself: the browser's command state calls
  0 disabled, which greyed them out and kept them shut. On macOS the logo opens Chromium's app menu,
  which carries Alerse's items (Sync, Link routing; Update and Import come with the
  first macOS build on 157), and the menu bar stays. The logo button
  (`AlerseLogoButton`, `look/sidebar-logo-menu.patch`, all platforms) draws
  Alerse's "A" from `artwork/` as strokes in the header's icon color on a 16 px
  grid: the left leg a trunk (1.8 px) with a short branch off it to the upper left
  (1.2 px), the right leg (1.5 px), and the crossbar (1.3 px) rising from the left
  leg and curling up into the right leg, as the icon's bowl does. Strokes rather
  than an `.icon` file, so it keeps the color logic the arch had (`kColorIcon` on
  macOS, `GetArcHeaderIconColor` elsewhere).

### Extensions and site controls

- **Extensions** sit at the end of the address row, sized like its buttons. The sliders
  button opens the site controls (`alerse_site_controls.cc`), Arc's Site Control
  Center: a 236 pt panel that paints itself (the bubble's own frame shows nothing),
  its left edge 117 pt left of the sliders button's middle, an arrow 9 pt high whose
  tip sits 3 pt under the button, 13 pt corners, 10 pt margins, Arc's rim, and on
  macOS the icon pickers' vibrancy (half of #565656 over the window server's blur).
  Its top row is four 48x32 pt near black tiles: Share (macOS' picker for the page's
  link; Copy Link elsewhere), Boost, Screenshot and Reading Mode (Arc's Capture has
  no counterpart; Find in Page stays on Ctrl+F and in the command bar). Under "Extensions" every extension is a tile on a
  faint veil, four a row (click runs it, right click opens its menu with Pin and
  Unpin, `alerse_site_controls::ExtensionMenu`: Chromium's ExtensionContextMenuController
  runs only from a menu button and crashed on a tile); a pinned one carries a pin on a
  light disc over its lower right corner. "Settings" lists Arc's rows, a 32 pt disc
  (white while on, grey while off) and a name over its state: what the page did that
  the toolbar's icons would tell (the zoom while it is not 100%, a click resets it;
  every content setting icon Chromium would show, a click opens its bubble under the
  field; translate available; the password key), then the site's permissions (a
  click turns one on or off; one set by a policy or an extension is not clickable).
  The footer has the connection (opens page info) and a horizontal "..." on a darker
  disc with site settings, extension site access and extension management. The panel
  closes when its tab closes or leaves the page.

### Strings and colors

Text the user sees is in `IDS_ALERSE_*` messages (except the sync page's errors) and colors are `kColorAlerse*` ids
(see [patching](patching.md)), set from the frame: Arc's greys on a dark frame, inverted
on a light one, system colors in high contrast. The command bar's rows are a web
page: their light set is in `look/command-bar-light.patch`, under
`prefers-color-scheme: light`. The Space bar and the address field repaint when
the theme changes (`alerse::RepaintTree`), as their views paint its colors
themselves.

## Split View

Arc's Split View on Chromium's split tabs (`look/split-view.patch`,
`alerse_split.h`): two pages side by side in one window, each a tab of the Space.
The user asked for it back on 2026-11-08 after it had been taken out.

- **Add Split View** is Chromium's `IDC_NEW_SPLIT_TAB` (`chrome::NewSplitTab`): the
  shown tab and a new pane on Chromium's "Choose a tab" page
  (`chrome://tab-search.top-chrome/split_new_tab_page.html`). As in Arc, the command
  bar opens for that pane (`alerse::OpenCommandBar`, `alerse_command_bar_hook.h`,
  called at the end of `NewSplitTab`, so the menu, the key, the "+" popover, the
  command bar action, the Windows top bar's split button and Chromium's own Tab menu
  item all do it): the field opens empty, and what is typed loads in the pane. The
  sidebar lists the pane as "Choose a tab" (the page's title). A tab of the same Space
  picked meanwhile, by a sidebar row or the command bar's Switch to Tab row, takes the
  pane's place in the split and the chooser closes (`AlerseSidebarView::
  FillSplitChooser`, Chromium's `UpdateTabInSplit` with `kReplace`, done one task
  after the activation, since a split is updated from one of its panes); Chromium's
  own list on the page picks too. Escape leaves the chooser pane open. The pane is the
  chooser also when the window has no other tab to pick, then an empty pane with its
  close button (`look/split-chooser-one-tab.patch`): Chromium loaded its
  full new tab page then (in `NewSplitTab` and from the page itself), and a Space's
  window often has one tab, so the pane showed the search engine's logo, Web Store and
  Customize.
- Where it is: Control+Shift+= on both platforms (Windows and Linux: Ctrl+Shift+=, which
  Chromium gives to zoom, as is Ctrl+Shift+-; Ctrl+= and Ctrl+- still zoom, and
  Chromium's Alt+Shift+N is dropped so menus show Arc's key); macOS' View menu (Add
  Split View, Close this Split Pane, Separate Page from Split View, Expand Current
  Split, after Force Reload as in Arc); the logo menu's Tabs submenu on Windows and
  Linux (Arc for Windows has no menu bar); New Split in macOS' "+" popover; Add
  Split View in the command bar ("split", "new split"); Open in Split View in the
  sidebar's tab, pinned item and favorite menus (a pinned item not open opens and
  splits with the shown tab; the shown tab itself adds a split as above). Disabled on
  an empty Space's blank tab and on a page already split.
- **By dragging**, as in Arc: a tab row (or an open or closed pinned item) dropped onto
  the middle half of another tab's row in the sidebar splits the two, the dropped
  page on the right, and shows it; the target row is highlighted while it would. A
  row dragged onto the page shows Chromium's split drop zone at the page's left, right
  or bottom edge; dropping there puts the row's own tab beside the shown page on that
  side (below for the bottom zone), instead of Chromium's new pane loading the address
  again (`AlerseSidebarView::SplitDraggedRow`, called first in
  `MultiContentsViewDelegateImpl::HandleLinkDrop`). A pinned item not open opens bound
  to its row first. A dropped tab already in a split leaves it, its other pane
  becoming a plain tab. No split zone: onto a tab already in a split (two panes at
  most), onto the row itself, for folders, and onto the page while it shows a split
  (Chromium shows no zone then). The split target is the row's plate with a 2 px
  outline in the item ink, so it reads apart from the shown tab's plate. Escape ends
  a drag with nothing moved (the platform's own drag session cancels it). The
  drop zone goes away whenever a row's drag ends, dropped, cancelled with Escape
  or refused: the row's `OnDragDone` (or a rebuild that ends the drag) posts
  `BrowserView::HandleDragEnded` to every window, Chromium's own end of a drag
  that started in a page, which stops the zone's show timers and hides it.
  Chromium hides the zone only for drags that start in a page or drop on the
  zone, so a row dropped on the page itself (before the zone's 500 ms delay ran
  out, or beside it) left the zone up, shown by its timer after the drop, until
  the next drag; a drop that split the tabs hid it only because the shown tab
  changed. Shared
  code, so the same on macOS, Windows and Linux. Chromium's zone takes a drop as a
  link, so on Windows and Linux a row's drag offers a link as well as a move
  (`GetDragOperationsForView`): Windows' drag and drop delivers no drop whose
  operation the source does not offer, and the zone showed but the drop did nothing.
- **The split's row**, as in Arc: a split's two tabs share one row, each half with
  its own icon, title and menu (`SplitPair` in `alerse_sidebar_view.cc`); a 1 px
  line in the "/" ink parts them. A half has no close button (on a half that narrow
  it would cover the title and catch clicks meant to show the pane); a middle click,
  the menu or Close this Split Pane closes a pane. While the split
  is shown the whole row has the active plate, the shown pane's title in the item ink
  and the other's dimmed; a click on a half shows that pane. A half dragged out, to a
  gap between rows (or back onto its own row's edge) or into the pinned items, leaves
  the split and its other pane becomes a plain tab; dragged onto another tab's row it
  splits with that one instead. A split's row takes no third page: a drop over it goes
  above or below it. Tabs of a split bound to pinned items keep their own rows.
- **Close this Split Pane** (Control+Shift+-): closes the shown pane, the other fills the
  window; Reopen Closed Tab brings it back. **Separate Page from Split View**: two
  separate tabs again. **Expand Current Split**: the shown pane takes 90% of the
  window (Chromium keeps the other at its minimum), again evens them.
  **Switch to Split View 1 / 2** (Control+Shift+1 and 2, in no menu, as in Arc; on
  macOS keys outside the menu, `global_keyboard_shortcuts_mac.mm`): shows the left or
  right pane (mirrored in right-to-left layouts). These are `IDC_ALERSE_*_SPLIT*`
  commands, enabled only while the shown tab is in a split (`OnSplitTabChanged`).
- Both platforms behave the same; only where the items sit differs (macOS' menu bar
  and "+" popover, the logo menu and the top bar's button elsewhere).

Known gaps: Chromium's split holds two panes, Arc's up to four, so there are no
Switch to Split View 3 and 4 or next and previous split keys, and a split takes no
third page. The split row's look (line, dimmed pane) is drawn from memory of Arc,
not measured: Arc was not put in a split for a reference, since that would change the
user's own browser. A tab of another Space or a favorite picked for the chooser is just shown
(moving it into this Space would unbind it), as is a tab already in a split.
Windows' "+" menu has New Split too (Arc for Windows' "+" was not measured). The chooser's
command bar lists the split's other page among the open tabs; picking it just shows
that pane. Dragging: a favorite dropped on a tab's row only moves among the tabs (its
tab stays pinned outside the Spaces), and on the page's edge opens a new pane at its
address as any link would; a row dropped on another window's page edge does the same;
a row dropped on the page's edge while the shown tab is a favorite's or the row's own
does nothing; a row dropped onto a pinned item's row pins or reorders as before, it
makes no split.

Return on a row of the command bar's empty field (the open tabs it lists at once)
logs its timing with the default match no older than the input
(`look/command-bar.patch`, `OmniboxEditModel::OpenMatch`): the rows come before the
field's first change, which Chromium's debug check took for a missed notification
and stopped the browser on (New Tab, then Down and Return, did it too).

## Left out on purpose

The user decided against these Arc features, so they are deliberately not built:

- **Tab archive**, Arc's archive of closed and unused tabs (its Library's Archive,
  auto-archiving after 12 hours, Archive Tab, View Archive, Clear Archive): the
  user's choice (2026-11-08). A closed tab is closed, as in Chromium (Reopen Closed
  Tab and the history bring it back), and an unused one stays open. The Space bar's
  left button, Arc's archive, is Downloads (since 0.1.7; it opened the history
  before); Clear Today (macOS' Tabs menu,
  Shift+Cmd+K) closes the Space's unpinned tabs. There is no archive page, setting,
  command or key.

Peek and Little Arc were on this list from 2026-11-08 until 2026-11-14, when the user
asked for both, each with a switch (see Peek and the Little window).

Wanted and built (2026-11-08): Split View without Peek (see Split View), Arc's icon
pickers for pinned items, folders and Spaces (see Icon pickers), the macOS menu bar
and the Arc look.

## Boosts

Arc's Boosts: a site's look of the user's own, seen by no one else
(`chrome/browser/ui/webui/alerse_boost/`, `tabs/alerse_boost.h`,
`views/tabs/vertical/alerse_boost_panel.cc`, `look/boosts.patch`; asked for on
2026-11-14).

- **Opening**: Boost in the site controls' top row, or Boost This Site in the command
  bar. The panel is chrome://alerse-boost/?tab=<tab> in a bubble over the page's top
  right corner, 340 px wide; the page stays live under it, and each change shows at
  once. The same command closes it; so do Esc and its close button.
- **What a boost holds**, per site (the registrable domain: www. and m.youtube.com
  share youtube.com's), in the profile pref `alerse.boosts`: on or off; a color (the
  page's hues turned round the wheel, `hue-rotate`); Dark (lightness inverted, hues
  kept); a font (Sans, Serif, Mono or Round, never on code or icon fonts); a size (60
  to 160 %, `zoom`); zapped elements (one CSS selector each); and the user's CSS.
  Pictures, video, canvases and frames get the opposite filter first, so they keep
  their own colors.
- **Zap**: the panel steps aside, a red box follows the element under the pointer,
  a click hides it on this site and the panel comes back with it listed (Restore
  shows it again). Esc gives up. The page receives neither the click nor the key.
  The selector prefers a unique id, else tag, two stable classes and the place among
  its siblings, up to `body`.
- **How it reaches the page**: `BoostTabHelper`, which each tab gets on its first
  navigation (`MaybeAttachBoosts` in the navigation throttles' registration, which
  adds no throttle), puts a constructed style sheet on the document from Chrome's
  internal isolated world when the page commits, when its DOM is ready and when it
  has loaded, and again whenever the boosts change. A constructed sheet is not
  refused by a page's Content-Security-Policy as a `<style>` element is, and the
  page's scripts cannot see the world it comes from.
- **chrome://alerse-boost/** lists every boost (on or off, what it changes, Delete)
  and holds the switch for all of them (`alerse.boosts_enabled`, on by default; a
  site without a boost is untouched either way). The panel's All Boosts opens it.
- **Not built**: Arc's boost JavaScript (a page's own script would need the page's
  world, and Content-Security-Policy refuses an injected `<script>` on most sites) and
  sharing boosts.

## Peek and the Little window

Arc's two ways to look at a page without a tab (`chrome/browser/ui/tabs/alerse_peek.h`,
`views/tabs/vertical/alerse_peek.cc`, `look/peek-and-little-window.patch`). Each has a
switch on the Link routing page and a step of the welcome page: the Local State prefs
`alerse.peek` and `alerse.little_window`, shared by every Space, both on by default as
in Arc.

- **Peek** opens a link in a panel over the window's page: the page area dimmed, the
  panel inset 20 px top and bottom and a fourteenth of the width (at least 24 px) at
  the sides, 12 px corners, rising 8 px and fading in over 160 ms (none with reduced
  motion). Its header has the address (editable: Enter loads what is typed, a site or
  a search), Open as Tab and Close. Shift+click a link opens it in Peek (Chromium's
  new window; `OpenLinkInPeek` in Navigate()). In a pinned tab (a pinned item's or a
  favorite's, or one pinned in the tab strip) so does a link that opens a new tab in
  front (`target="_blank"`, whatever its site), and a click on a link to another site
  (`PeekThrottle`: a renderer-initiated link navigation with a user gesture, not a form
  post, to another registrable domain on start or after a redirect, as Google's
  google.com/url, cancelled and shown in Peek). Ctrl+click still opens a tab behind. Esc, Close or a click on the dimmed part closes it; another link replaces its
  page. Open as Tab gives the page to the window's tab strip with its history and
  state. Peek follows the window when it moves or resizes and closes with it.
- **The Little window** is a small window of its own (560 x 720, centered at the top
  of the screen the pointer is on, resizable) with the address and Open in Alerse.
  New Little Window opens one on an empty page with the address focused: Ctrl+Alt+N on
  Windows and Linux (the logo menu has it while the switch is on), Option-Command-N
  and File > New Little Window on macOS. A link from another app opens in one when no
  link route matches it and the window it would go to has tabs (a browser starting
  for the link opens it as a tab). Open in Alerse gives the page to the Space's last
  active window, or opens it in the Space when it has none.
- In both, a link the page opens in a new tab or window opens as a tab of the Space;
  the page has no tab helpers until it is a tab, so autofill, password saving,
  JavaScript dialogs and extensions' content scripts wait for Open as Tab.

## First start

The first start opens `chrome://alerse-welcome` (`look/welcome-page.patch`, in place of
ungoogled-chromium's page about itself), one step at a time, with dots for the steps,
Back, and Skip on every step but the search engine. The idea follows Ardia's
onboarding; the code is Alerse's own.

1. **Welcome**: what Alerse is, in three lines.
2. **Where are you coming from?**: the browsers found on this computer, each with
   Import: the import page's own handler and import, so Arc's Spaces arrive as Spaces.
3. **Search engine**: DuckDuckGo, Brave Search, Startpage, Ecosia, Qwant, Google, Bing,
   with DuckDuckGo picked, so Google is a choice and not the default; set in every Space.
   Only those the profile's engine list has are offered: the list is the region's. No
   Skip here.
4. **Light or dark**: Match the system, Light or Dark, for every Space.
5. **Sync**: opens the Sync page in a new tab, recommending a server of your own.
6. **Make Alerse your browser**: the system's default-browser request (Windows opens its
   settings), and whether Alerse already is.
7. **You are set**: what was picked, and the three things to know: the sidebar, Ctrl+T for
   the command bar, Ctrl+D to pin, the Alerse menu.

## Import

`chrome://alerse-import` (Import from Another Browser... in the logo
menu and the app menu, `IDC_ALERSE_IMPORT`) reads Arc's `StorableSidebar.json` and
the `Bookmarks` files of Chromium-based browsers on the thread pool (shared reads,
64 MB limit; Arc's sidebar is read up to 100,000 items and 32 MB of text) and writes
the bookmark model on the UI thread. Each node an import adds is marked
`arcomium.imported` = HMAC-SHA256 (first 16 bytes, hex) of the source and the node's
uuid under the profile's own key (`arcomium.import_key`, a local pref): a copy the user
made (another uuid) and what another device imported (synced, another key) are not
marked here, so importing again replaces only this profile's imported nodes and keeps
the rest. Arc's first Space goes to Home's Space folder. Every other Arc Space
becomes a Space of its own (`MakeSpaceOfFolder` in `alerse_import_handler.cc`): the
import writes its folder into the importing profile, moves what the folder holds into
the Space folder of a new profile, and removes the folder. No window opens: the new
Space shows in the Space bar. The new Space's profile entry in `Local State` records
Arc's id of the Space (`alerse_arc_space`), so an import again finds that Space
whatever Arc's order and replaces in it only what the earlier import brought; the
Space keeps its own icon and color. In the importing profile a folder is bound to
Arc's Space by `arcomium.arc_space` (Arc's id) and `arcomium.arc_space_mark` (the same
HMAC); a folder whose Space Arc no longer has loses its imported items and the
binding. Cached favicons
(`SidebarItemsFaviconCache`, PNG, named after the MD5 of the item id) go to
`arcomium.icon`. A browser profile's bookmarks go to one folder in the current
Space's (or Home's) pinned folder, marked `arcomium.import = <browser>/<profile>` and
recorded by uuid in the local pref `arcomium.import_folders`; importing it again
replaces the folder's imported contents. Sources are found in the per-user known folders (Windows), Application
Support (macOS) or the XDG config directory (Linux). Alerse's own user data directory
is skipped, even when Alerse still runs from Chromium's or the earlier name's (see Name), and so
is any
profile whose Preferences have an `alerse` key; profiles without a `Bookmarks` file
are not listed. `tools/arc_import.py` is the offline fallback for the Arc
part: it writes the `Bookmarks` file of a closed profile, marking what it adds the
same way and replacing an earlier import the same way (the same folders, the user's
own bookmarks in them kept). Its earlier versions marked every node `arcomium.imported = "1"`
and the folder of a Space no Space had adopted `arcomium.space = "arc:<id>"`; an
Arc import takes such Space folders under Other bookmarks, and the nodes in them
marked "1", as this profile's earlier import (re-marked, `arc:<id>` folders bound to
that Space; their Space icons stay, as the user may have picked them). "1" tells
neither a copy the user made nor what the old tool imported on another device (synced
here) from this profile's own import, so such nodes in those folders are replaced too.

The page:

- lists every source it finds with its bookmark count and an Import button, which
  becomes Import again after an import; when it finds nothing it says so. Firefox,
  Safari and HTML files go to Chromium's importer (`chrome://settings/importData`)
  through a link on the page;
- is a regular profile's only: incognito and guest profiles do not get it, as the
  bookmark model of an incognito profile is the original profile's;
- takes a source id only from its latest list, at most 512 bytes, and renders with
  `textContent`, never `innerHTML`; an import answers `busy`, `notReady` (bookmarks not
  loaded), `readFailed` (including an Arc file with no Spaces, which would otherwise
  wipe the earlier import) or `unknown`;
- reads Arc's sidebar with one budget for the whole file: 100,000 items and 32 MB of
  text (titles cut to 4,096 bytes, addresses over Chromium's URL limit skipped), so a
  folder listed many times cannot exhaust memory, and 8 MB of icons (PNG up to 64 KB,
  read once per Arc item). A tab id repeated in one folder is read once; Spaces with an
  empty, longer than 256 bytes or repeated id are skipped.

Matching Space folders on an import again: first by Arc id (with the folder's mark), then
by title among folders bound to no Arc Space, so an Arc Space named like one of the
user's Spaces fills that Space instead of making a second one. A matched folder is kept
(id, position, title, color, icon); only its imported items are replaced.
Its icon changes only while it is still the one Arc gave it (`arcomium.arc_icon`). Home
takes Arc's first Space unless Home is already bound to another. The user's own
bookmarks in an imported folder are kept: each largest unmarked subtree is moved out
before the imported nodes are removed (`alerse_import_bookmarks.cc`), to the root of
the Space's or the import's folder when its subfolder goes.

### Extensions

Under each source the page lists the extensions it installed from the Chrome Web Store
that this Space does not have, with Add (`getExtensions`, `importExtensions` in
`alerse_import_handler.cc`). The welcome page's first step shows the same row. An
extension counts as the store's when its newest version directory has a manifest with
a `key`, the store's `update_url` and a signed `_metadata/verified_contents.json`;
hosted apps and the browser's own are left out, and at most 200 are read. Arc's are
read from every profile under `User Data`, a Chromium browser's from the profile.

Add copies each one to a temporary directory and installs the copy
(`CrxInstaller::InstallUnpackedCrx`) with the manifest's key, so it keeps its id: the
Web Store updates it, sync finds its data, and the content verifier checks its files
against the store's signed list. It is installed silently with its permissions
granted, as the user granted them in the source, and marked as the store's
(`FROM_WEBSTORE`). The source's files are never written. Settings and data an
extension kept in the source's profile do not come with it.

The import's unit tests are in `alerse_import_data_unittest.cc` and
`alerse_import_bookmarks_unittest.cc`, built into `unit_tests` by
`look/import-from-browsers.patch`.

Known gaps:

- import on Linux was never built; on macOS it is built (0.1.12 and later), but other
  browsers' paths there were not tried; the paths follow the browsers' documentation;
- icons come only from Arc; other browsers' bookmarks get their icons on the first
  visit;
- an imported Arc item the user moved out of its folder is added again by the next
  import;
- Home is shared by all devices, so importing different Arc data on two devices may
  leave both devices' pins in Home;
- on Windows the import item also shows in Chromium's own app menu (More...).

## Sync

Sync goes to an [alerse-sync](https://github.com/VizzleTF/alerse-sync) server, set up
at `chrome://alerse-sync` (Sync in the logo menu and the app menu; every link to
Chromium's sync settings leads there). The page recommends the user's own server, by its
address, with a link to the server's setup guide, and offers the Alerse server,
`https://sync.alerse.app`, preselected for those who would rather not run one. A note
under it says the maintainer offers it voluntarily, without any guarantee or
responsibility for the data on it, and links its terms and privacy policy. Either way the
server only ever sees ciphertext. The design is
[ADR-0002](adr/0002-sync-accounts.md); this section is what is built (0.1.14).

Each Space is a profile with its own sync service, so each Space syncs as its own
chain. The setup is the browser's, not a Space's: the server and the account are in
`Local State` (`alerse.sync.*`), and the Sync page of any Space sets sync up or stops
it for all of them. Home syncs as the chain of the account key. Every other Space has
an id (the profile pref `alerse.sync.space`), and its chain's key is derived from the
account key and that id. Home's chain carries the list of Spaces, so a second device
makes the same Spaces and joins their chains.
[ADR-0001](adr/0001-space-profiles.md#sync) has the derivation and the list's format;
the code is `alerse_sync_account.cc`, `alerse_space_list.h` and
`sync/space-sync-chains.patch`.

### An account: the 24 words and a password

Chromium only starts sync for a signed-in account, and ungoogled-chromium has no Google
sign-in. Alerse signs in to an account of the alerse-sync server instead (scheme
`sk-v1`, `alerse_sync_sk.cc`):

- **Create** (first device): Alerse makes three random 256-bit values: the secret key
  that the 24 words encode (BIP39 words with an 8-bit SHA-256 checksum,
  `alerse_sync_code.cc`), the account key, and this device's Ed25519 key. From the
  words and the password it derives, with scrypt (N = 2^17, r = 8, p = 1: 128 MiB,
  about half a second, on the thread pool) and HKDF-SHA256, the envelope's id, an
  authentication key and a key that wraps the account key with AES-256-GCM. The
  server gets the id, the salt, the parameters, the authentication key (kept as
  Argon2id), the wrapped account key and the device's public key and name.
- **Sign in** (another device): the 24 words and the password. The device fetches the
  salt and parameters for the id, derives the same keys, proves the authentication
  key, gets the wrapped account key, opens it, and joins with this installation's
  device key.
- **One installation, one device key.** The Ed25519 key is made once and kept in Local
  State, encrypted, across Stop syncing and account deletion, so signing in again does
  not add a device: the server takes the same key as the same device. Only if the
  account removed that key does the sign-in make a new one, once. The server limits
  keys new to an account (5 a day by default, 3 on sync.alerse.app) and every new one
  needs the words and the password.
- A new device needs both: the words alone open nothing, and the server's database
  alone cannot be used to guess the password without the words. The exact
  derivations are in the server's
  [protocol notes](https://github.com/VizzleTF/alerse-sync/blob/main/docs/protocol.md).
- The device keeps the account id, the account key, its device key and the words'
  secret, the last three encrypted by the OS (DPAPI, Keychain) through Chromium's
  `os_crypt_async`; if that fails nothing is kept and sync does not start. The words
  are kept so that the page can show them again and remove devices with the password
  alone; the account key opens the data anyway.
- The password needs 6 characters, digits alone too (the owner's choice), up to 256,
  counted as characters, not bytes. It is the second of two secrets: only whoever holds
  the 24 words can name the account and try a password, online at the server's rate (10
  an hour), and a copy of the server's database without the words allows no guessing at
  all. Someone with both the words and a copy of the database can try a 6-digit password
  offline in days, at one scrypt of 128 MiB a guess. The password is normalized to
  Unicode NFC, so the same password typed on another system opens the account. A device
  refuses scrypt parameters below the default, so a server cannot make it send a key
  that is cheap to attack.
- Nobody can reset the password or the words. With both lost and no device signed in,
  the synced data is gone; the page says so where the account is created.

### Chains, tokens and encryption

- Every chain's key derives from the account key with HKDF-SHA256: an Ed25519 key pair,
  whose public key names the chain (`hex(SHA-256(public key))`), and the Nigori custom
  passphrase. The chain's sync URL is `<server>/v3/chains/<chain ID>`; Chromium adds
  `/command/`, and the events client `/events`.
- Every request carries a token signed by this device's own key: the account id, the
  device's public key and a timestamp. The server answers only active devices of the
  account, so a removed device is refused from its next request on. The browser makes
  a new token every hour; a token is valid for 24 hours.
- The passphrase is applied with `encrypt_everything` on, so bookmarks, tabs, Spaces,
  passwords and history are encrypted on the device. Chromium refuses to go back from a
  custom passphrase to the server's keystore key.
- End-to-end or nothing (`sync/e2e-before-data.patch`): until the chain's data is
  encrypted with its passphrase, only the device list (`DEVICE_INFO`) syncs; the other
  data types are configured once the passphrase type is custom. The Alerse server also
  refuses unencrypted data (`REQUIRE_CUSTOM_PASSPHRASE`).
- Client side: Chromium's local sync mode with a local primary account for the chain,
  the token in the Authorization header, and the chain's URL. The server URL must be
  https; plain http only for a server on this computer.
- `--alerse-sync-url` sets the server for tests; official builds ignore it.
- **Stop syncing on this device** first signs the device out on the server (no
  password needed: a device can only drop itself), then forgets the account and stops
  sync. If that fails, offline or on the account's only device, the key stays the
  account's and the next sign-in reuses it. The other devices keep their data, and so
  does the server. **Delete account** (with the
  password) deletes the account and all its chains on the server; every device stops
  syncing, and each keeps its own data.

### Devices

The page lists the account's devices with their names (the computer's name) and when
each was last seen, and removes one with the password. A removed device keeps its own
copy of the data but gets nothing new; its key cannot join the account again, so it
signs in anew with the words and the password if it comes back. The account's only
device cannot be removed: deleting the account is the way. Chromium's local sync mode
does not turn the server's 401 into an error, so the page asks the server for the
device list and says so on a removed device.

### What syncs, on the Sync page

Under the server and the last sync, the page lists every Space: a dot (green when its
chain syncs), its name, how many tabs this device has open in it, and under it each other
device with its tab count in that Space. The heading sums it up ("Syncing 3 Spaces with
12 tabs from this device"). The counts are what the Space's sessions data holds, as this
device last got it (`OpenTabsUIDelegate`): web pages only, without the empty tab that
keeps a hidden Space's window or browser pages, which do not sync. The page asks for
them every 5 seconds while it is visible; that reads local data only.

### Tabs from other devices

Chromium's history page shows other devices' tabs only to a signed-in Google account.
Alerse syncs through Chromium's local sync mode, so the page counts that as signed in,
and tab and history sync as on when they are selected
(`sync/history-other-devices.patch`): History, "Tabs from other devices" lists each
device with its tabs, per Space, since each Space syncs on its own.

### How often devices sync

Only changes are sent, in batches. The server tells every device how long to wait
after a change before it sends everything changed since: 15 seconds for open tabs and
windows (Chromium's own value), 60 seconds for bookmarks, Spaces, passwords, settings
and the rest. A device with no changes sends nothing but an hourly check and keeps one
event stream per Space open. The other devices fetch a change as soon as the server's
event says there is one, so a tab shows on another device 15 to 20 seconds after it
opened, a bookmark about a minute after.

The waits are the server's request to a well-behaved browser. The server enforces its
own limits whatever a client sends: requests per device key, commits per chain (one
per 10 seconds on average), and it shuts out for 15 minutes an address that keeps
sending refused requests. The server's
[protocol notes](https://github.com/VizzleTF/alerse-sync/blob/main/docs/protocol.md#refusing-abuse)
list the checks in order.

### Recovery sheet

"Print recovery sheet" (like 1Password's Emergency Kit) prints the 24 words, the
server and the date, with an empty line for the password, written by hand; the print
dialog's Save as PDF makes a file of it. The sheet is a print-only part of the Sync
page, so nothing is sent anywhere. After an account is created the words stay on the
page, and the restart that starts sync waits for "I saved the words".

### Saving to a password manager

Password manager extensions cannot run on `chrome://` pages, and Chromium's own
password manager does not save from them, so the words panel exports one entry
instead: the words as the login, the password while the account was created on this
page (the browser keeps no password), the server and how to add a device.

- **Copy for Bitwarden**: Bitwarden's CSV (`reprompt` set, so the vault asks for the
  master password before it shows the entry) goes to the clipboard. Its import page
  takes it pasted, with the file format "Bitwarden (csv)": in the extension it is
  Settings, Vault options, Import items; in the web app, Tools, Import data. No file
  is written. The page clears the clipboard
  after 2 minutes if it still holds the entry.
- **Download CSV**: `alerse-sync.csv` in Chrome's format (name, url, username,
  password, note), which 1Password, Proton Pass, KeePassXC, Edge and Brave import. The
  page says to delete the file after the import.
- Until the restart after creating or joining an account, the status reads "Starts
  after a restart of Alerse": the sync engine takes the account only on start.
- The sign-in form's fields carry `autocomplete="username"` and
  `current-password`, for managers that fill such a page.

### From the scheme before accounts

Up to 0.1.13 a sync chain had no account: the 24 words alone were the chain's secret,
with an optional login name and password and pairing by short code. The server no
longer serves that scheme (alerse-sync 0.2.0). On the first start of 0.1.14 the
browser forgets such a setup (`arcomium.sync.*`), keeps all its data, and the Sync page
says that sync is to be set up again. Creating the account uploads the data of every
Space into its new chains; the server deleted the old chains when it moved to 0.2.0.

When a second device signs in with data of its own, the data of both ends up on both,
without duplicates (checked with two devices that each had a home and a work Space
and pinned items the other lacked):

- After the sign-in only home syncs (`AlerseSyncAccount::IsJoining`). Once home's
  bookmarks have synced, a Space of this device with the name of one of the account's
  takes that Space's id (`AdoptSpacesOfTheSameName`, `alerse_space_list.h`), the
  other Spaces of this device join the account's list, and Alerse restarts by itself
  once so that every Space starts syncing. Without this the two devices' "work"
  Spaces stayed two Spaces of the same name, as their ids differ.
- Chromium then merges each Space's data as it merges home's: a pinned item with the
  same address in the same folder becomes one, the others are added. Passwords are
  merged the same way; open tabs stay per device.

Known gaps: email accounts, a recovery code and pairing a device with a short code
(ADR-0002's later steps) are not built; changing the password or the keys is not built;
iOS still pins the core of the scheme before accounts and is not updated.

The page shows one state at a time with the `hidden` attribute. The pages' shared style
(`alerse_page_style.inc`, also the import and link-routing pages) makes every section a
grid, and an author `display` beats the user agent's `[hidden]` rule, so the same style
carries `[hidden] { display: none !important; }`.

### Server: alerse-sync

Written for Alerse in Go: [alerse-sync](https://github.com/VizzleTF/alerse-sync). The
Go module keeps the earlier name, `github.com/VizzleTF/arcomium-sync`. The protocol's
identifiers are part of the wire format and never change: the `sk-v1` derivation
contexts (`alerse-sk-*`), the device token's `alerse-sync-device-token-v1`, and the
chains' key derivation salt `arcomium-sync`. It serves only `/v3` since 0.2.0, and
speaks the Chromium sync protocol (`POST /v3/chains/<chain>/command/`), with the protocol buffers of
the Chromium tree we build, so every data type Chromium syncs works without server
changes.

- PostgreSQL store. Each chain has a version counter; a commit runs in one transaction
  that holds the chain's lock, so versions only grow and a device never misses a change.
  Any Postgres works: a container, CloudNativePG, or a free managed database (Neon,
  Supabase), which also makes the server itself stateless.
- `GET /v3/chains/<chain>/events` Server-Sent Events: after a Commit, the server tells the chain's other
  devices which data types changed, through Postgres `NOTIFY` so several instances can
  share a database. The client (`alerse_sync_events.cc`) subscribes, reconnects with
  backoff from 5 seconds to 5 minutes, and triggers a sync cycle; without events the
  client polls once an hour. SSE rather than WebSocket because Chromium's network
  stack streams it with a plain URL loader.
- Limits per chain (requests, bytes, entities per data type, event streams) and per
  client address, history that expires after 90 days and gives way to other data,
  removal of old tombstones, and an option to refuse anything not end-to-end encrypted.
  The server's README lists them.
- One static binary, a distroless image, and a compose file with Postgres and Caddy for
  HTTPS.

Existing servers were not a fit: [brave/go-sync](https://github.com/brave/go-sync) is
built around DynamoDB and Redis, and [selfsync](https://github.com/loyalpartner/selfsync)
(SQLite) has no authentication and expects a real sign-in.

### Where the server runs

| Option | For whom | Cost | Notes |
|---|---|---|---|
| Home server | NAS, Raspberry Pi, homelab | free | `docker compose up -d` (server + Postgres). Reach it from outside through Tailscale, NetBird or a Cloudflare Tunnel (all free); no open ports needed. |
| A VPS or cloud VM | no home server | a small VM | The same compose file; Caddy gets the certificate. |
| The Alerse server | everyone | free | `https://sync.alerse.app`, in the owner's homelab behind the NetBird Reverse Proxy, with the public-server limits of the server's README. Data is end-to-end encrypted. |
| Own free cloud (later) | no server at all | free | A Cloudflare Workers port (one SQLite-backed Durable Object per chain; WebSocket hibernation for events) behind a "Deploy to Cloudflare" button. The free plan's 100k requests a day fit one person's devices. |

Moving between options: create an account on the new server and the data is uploaded
again; accounts do not move between servers.

### Data mapping

No new sync data types are needed. Each Space syncs these types in its own chain:

| Alerse data | Chromium sync type |
|---|---|
| Pinned tree of each Space (nested folders), Space colors and icons | `BOOKMARKS` (the Space folders with `arcomium.*` meta info) |
| Open tabs on other devices | `SESSIONS` |
| The list of Spaces | `BOOKMARKS` of Home's chain (`arcomium.spaces` on Home's Space folder) |
| Space settings | `PREFERENCES` (syncable prefs) |
| History and its deletions, tabs sent to a device | `HISTORY`, `HISTORY_DELETE_DIRECTIVE`, `SEND_TAB_TO_SELF`, encrypted with the chain's passphrase; deletions reach other devices without Google's web history |
| Passwords, extensions | their standard types |

Link routes (`alerse.link_routes` in `Local State`) and import bookkeeping
(`arcomium.import_key`, `arcomium.import_folders`) stay local.

### Order of work

1. Server: Postgres store, current protos, events, limits, Docker image (done).
2. Client: sync code page, local account, token signing, sync URL setting (done).
3. Client side of the events channel (SSE) for real-time updates (done).
4. Public instance with quotas.
5. Cloudflare Workers port.

## Platforms

The patches in this repository change the desktop UI code in
`chrome/browser/ui/views/`, which Windows, macOS and Linux share, so one patch series
serves all three. Each platform repository adds this one as a submodule, applies the
series on top of the matching ungoogled-chromium port, and publishes builds as GitHub
Releases:

| Platform | Repository | Based on |
|---|---|---|
| Windows | [alerse-win](https://github.com/VizzleTF/alerse-win) | ungoogled-chromium-windows |
| macOS | [alerse-mac](https://github.com/VizzleTF/alerse-mac) | ungoogled-chromium-macos |
| Linux | [alerse-linux](https://github.com/VizzleTF/alerse-linux) | ungoogled-chromium-portablelinux |
| iPhone | [alerse-ios](https://github.com/VizzleTF/alerse-ios) (experimental) | Chromium's iOS app (`ios/chrome`) |

Alerse follows ungoogled-chromium's releases, so it runs about one Chromium major
version behind Chrome stable. The series on `main` is for Chromium 157; the branch
`chromium-156` keeps the 156 series. Windows is on 157. macOS (pre-releases up to 0.1.13) is built from a 156 tree
kept by hand on the build Mac, and Linux (release 0.1.1) from the 156 series; both
move to 157 when ungoogled-chromium-macos and ungoogled-chromium-portablelinux release
it. alerse-linux pins an older core until then, as the series does not apply to 156. iOS pins
the core at 0.1.1 (156). What came in 0.1.2 to 0.1.4 (own install identity,
end-to-end-only sync, import, the update notice) is on Windows only until then.

Install identity: on Windows Alerse installs as itself, not as Chromium or its earlier name
(`defaults/own-install-identity.patch`): `%LOCALAPPDATA%\Alerse\Application`, its own
registry key (`Software\Alerse`), app id `Alerse`, ProgIDs `AlerseHTM` and `AlersePDF`,
direct-launch URL scheme `alerse://`, Active Setup GUID
`{FB04E068-D560-40D7-BA8D-318A3484D91E}` and toast activator CLSID
`{09758798-6B12-4B6E-8D92-273BF804BCC2}`, all new, so neither a Chromium nor an earlier
install can overwrite it or open its profile. On Linux the profile is `~/.config/alerse`;
on macOS Alerse has its own bundle ID, `io.github.vizzletf.alerse`
(`tools/rename_product.py`), and profile directory, `~/Library/Application
Support/Alerse` (CrProductDirName, `defaults/mac-own-profile-dir.patch`). See
[Name](#name) for how Alerse takes over the earlier profile and installation.

Platform differences: Windows and Linux look and behave like macOS except at the top of
the window. The Arc for Windows top bar, the logo menu and Alt+1 to Alt+9 are not on
macOS, where Arc itself looks different (window buttons, the
menu bar), and the header's "..." menu holds what the Windows header has as buttons.
The sidebar's rows, Space name, folders and folder popup (searchable, with dates), Space
bar, popovers (shortcuts spelled out as "Ctrl+T" off macOS), the page card, the floating
hidden sidebar, pills (`look/mac-arc-tooltips.patch`) and context menus follow Arc for
macOS everywhere (Sidebar look, On macOS). Still macOS-only: the header, the address
field, the window controls in the floating hidden sidebar
(`look/mac-floating-sidebar.patch`),
the inactive window's lighter frame, Share, the menu bar (`look/mac-menu-bar.patch`),
Cmd+1 to 9 for the sidebar's items and the key chips' spaced glyphs. The command bar
looks and works the same everywhere. A pinned item's or
folder's Move to lists every Space, also one not open in the window (into its pinned
folder), as Arc's does; a plain tab still moves only to an open Space.

The macOS build tree (Chromium 156) has `spaces/pinned-tab-binding.patch` applied by
hand (it applies to 156 as it is); its `alerse_sidebar_view.cc` is the core one.
Changes to the sidebar are applied to it by hand until macOS moves to 157. Its omnibox popup is the pre-157 one, whose rows put
the dash before `#description`; there an empty description still holds a span, so the
dash shows only before one with text (`:has(span:not(:empty))`, in that tree only).

The iPhone app is a different code base (WebKit inside, its own UI); it shares the sync
engine and the server. It was not changed for the profiles: it syncs the chain of the
sync code, which is Home's, and does not join the other Spaces' chains. Separate
accounts for each Space stay desktop-only unless each Space maps to its own
`WKWebsiteDataStore` (iOS 17+).

## Link routing

Routes of the form "site -> Space", set at **Link routing** in the app menu
(`chrome://alerse-links`). A site is a host with its subdomains, optionally with a path
("github.com/acme"); the longest match wins.

Routes apply to links that arrive from outside the browser, with Alerse as the default
browser: `Navigate()` sees a new tab with no source tab and an `AUTO_TOPLEVEL` transition,
opens it in the route's Space: that Space's window takes the place of the one shown,
loading the Space's profile if it was not open (`alerse_link_router.h`). A link routed
to a Space that was deleted opens in the Space shown, and the Link routing page shows
the name the Space had. Links clicked inside pages stay in their Space, same as Arc.

Routes are the `Local State` pref `alerse.link_routes`, shared by every Space and not
synced: a route names its Space by the profile's directory (`Profile 1`; empty for
Home), which differs between devices. The profile pref `arcomium.link_routes` of 0.1
is not read.

## Protected video

Streaming services that play only DRM video in a browser (`alerse_protected_video.cc`:
Netflix, Disney+, Prime Video, Max, Hulu, Peacock, Paramount+, Apple TV, Crunchyroll,
Spotify's and Tidal's web players, Kinopoisk HD, ivi, Okko, START, Wink, PREMIER) open
in `chrome://alerse-webkit/?u=<site>` (`look/protected-video.patch`). A navigation
throttle of the primary main frame catches the site on start and on a redirect, cancels
the navigation and loads the page in the same tab. Their players need Widevine, which
Google licenses only to browsers it has signed (see `docs/legal.md`).

- **macOS**: the page is a 40 pt bar (back, reload, the site, Open in Safari, Open in
  Chromium) over a `WKWebView` (`alerse_webkit_host_mac.mm`), WebKit with Apple's
  FairPlay, as in Safari and in Ardia. Its frame follows the tab's view; a link that
  opens a window opens in it; it may go fullscreen. Each Space keeps its own WebKit
  cookies and storage (`WKWebsiteDataStore` by an id made from the profile's path, on
  macOS 14 and later; one store for all Spaces on macOS 13); incognito keeps none. The
  user agent ends in Safari's, which these services look for. The bar's address and the
  tab's title follow the WebKit page, and the tab's address stays
  `chrome://alerse-webkit/?u=<the page>`, so it restores and syncs. Checked on 156:
  Netflix loads with its title, and EME there answers yes for `com.apple.fps` and no
  for `com.widevine.alpha`.
- **Windows and Linux**: the page says why the video cannot play. On Windows it offers
  Microsoft Edge (`microsoft-edge:<site>`), which has Microsoft's DRM.
- **Open in Chromium** (and Open here anyway) loads the site in Chromium; the tab keeps
  that choice for the site's host until it closes.

Not there: the sidebar's mini player and sound indicator do not see WebKit's media, a
WebKit page's history is its own (the toolbar's Back leaves the page), and extensions
do not run in it.

## Media and picture-in-picture

- **Mini player** at the bottom of the sidebar (`AlerseMiniPlayer`): the media
  session that last took audio focus in the Space shown, in any of its windows, while its
  tab is not the one you are looking at. It shows the artwork (or the site's icon), the
  title and the artist, pauses and resumes, and a click goes to the tab and its Space.
  It lists the media of every Space of the window (see Spaces), and a click shows that
  Space with the tab; an incognito tab's media never shows in a regular window, nor the
  other way round. It is push-based (Chromium's media
  session controller), with no polling.
- **Automatic picture-in-picture** when you leave a playing video's tab or Space, on any
  site: Chromium's browser-initiated automatic picture-in-picture
  (`BrowserInitiatedAutomaticPictureInPicture`), enabled by default. Its permission
  defaults to Allow, as in Arc, which also skips Chromium's media engagement check; a
  site can be blocked in its settings.
- **The picture-in-picture window, as Arc's** (`media/pip-window-arc.patch`): rounded
  corners (10 px on macOS, where the window is translucent and its layer is rounded; the
  system's rounding on Windows 11, where a resizable window cannot be translucent), and
  the site's name with the dark gradient behind it only while the pointer is on the
  window, with the other controls. Chromium shows them for the first seconds and, until
  you interact with a site, all the time, so the name cannot be hidden from a site that
  opens its own window; Alerse opens the window only for a video you left, so the name
  is not needed to tell it from the page. The 10-second seek buttons are Chromium's;
  they do not show for a live stream.
- **No taskbar button of its own on Windows** (`look/pip-off-taskbar-win.patch`): the
  window floats over the browser, as in Arc, instead of showing next to it on the
  taskbar as "Picture in picture". It asks to stay off the taskbar, and Windows windows
  that do get `WS_EX_TOOLWINDOW`, so it is also not in Alt+Tab.

## Defaults

Alerse keeps ungoogled-chromium's defaults (no Safe Browsing, preloading off), with
these changes:

- Vertical tabs on, so the sidebar shows (`defaults/vertical-tabs-by-default.patch`);
  startup continues the last session (see Spaces).
- Google is the default search engine, without Chrome's preconnect to it or prefetch of
  its likely results; Search Engine in the logo menu picks another.
- The Chrome Web Store installs and updates extensions, so they sync between devices
  (`extensions/web-store.patch`): CRX3 publisher proofs are still checked, update URLs
  are forced to https, and on Windows extensions registered in the registry are not
  loaded. The store's address is split in C++ strings and written with an escaped "."
  (`\u002e`) in `_api_features.json`, so ungoogled-chromium's domain substitution leaves
  it alone; substituted, the store page lost `webstorePrivate` and offered "Switch to
  Chrome" instead of Add to Chrome.
- The bookmark bar does not show on the New Tab page, as pinned items live in the
  sidebar; `--bookmark-bar-ntp=always` brings it back.
- The window stays open when its last tab closes (see Spaces).
- Cmd+W closes a favorite's tab at once, as in Arc: Chromium asks for a second press
  before it closes a pinned tab, which a favorite's tab is
  (`look/close-favorite-at-once.patch`). The tile stays in the sidebar.
- A download goes to the Downloads folder without the Save As question
  ungoogled-chromium asks, as in Arc (`defaults/download-without-prompt.patch`);
  Settings > Downloads turns the question on.
- `defaults/feature-defaults.patch`: Back in a tab a link opened returns to the opener
  tab; parallel downloads everywhere; Memory Saver on; three performance-manager
  policies Chrome runs from its field trials (`kTransientKeepAlivePolicy`,
  `kPMLoadingPageVoter`, `kEnableBestEffortTaskInhibitingPolicy`); a reduced
  Accept-Language header; the Topics API off. Ctrl+Tab goes in most-recently-used
  order (the `kCtrlTabMru` pref on; the feature of that name is Chromium's default since
  157).
- Alerse's own browser commands (`IDC_ALERSE_*`: Pin Tab and Ctrl+D, Copy URL as
  Markdown, the Space commands, Link routing, Import, the update) run on Chromium 157,
  whose browser commands go through actions (`kUseActionsForBrowserCommands`, on by
  default since 157; 156 has it off): there `CommandActionUpdater` knows only the
  commands mapped to an action, so Alerse's were neither supported nor enabled
  (Ctrl+D, Ctrl+Shift+Alt+C and the command bar's Pin Tab and Copy URL as Markdown did
  nothing, the logo menu's Import and Link routing were grey). Commands without an
  action are now kept by a `CommandUpdaterImpl` inside it and run by
  `BrowserCommandController` as before (`defaults/commands-without-actions.patch`).
  Copy URL itself is an action there, which skipped Alerse's check, so the action
  also copies nothing on an empty Space's or a blank page (`chrome::
  GetAlerseCopyableContents`), as the command does.
- The dangling `raw_ptr` detector never stops the browser
  (`defaults/no-dangling-ptr-crash.patch`), as in an official build; BackupRefPtr keeps
  those pointers safe. Official builds do not compile the detector at all, so the patch
  only matters for development builds.

## Not there yet

- The rename to Alerse left out, for now: the iOS app is renamed in its
  strings, name and icon but was not built since; the sync server's Go module path.
- The macOS build on 156 has the older Sync page (no email and password sign-in,
  no pairing, no recovery sheet): `alerse_sync_login*`, `alerse_sync_pairing*`
  and the handler and page of that change are not in its tree yet.
- Updates: no updater. The browser only tells about a newer release
  ([versioning](versioning.md)); the user downloads it.
- Builds are not code-signed: Windows SmartScreen warns, and macOS builds are signed ad
  hoc and not notarized.
- No crash dumps or debug symbols in releases, so crashes are known only by symptoms.
- Few automated tests, and CI runs none of the browser's: the import and update-check
  unit tests run only locally.
- Sync, the later steps of ADR-0002: email accounts with verified addresses and a
  recovery code, pairing a device with a short code, changing the password or the
  keys, OPAQUE (RFC 9807) for email sign-in. iOS on accounts.
- From Arc: Share in the sidebar's menus on Windows and Linux (macOS has it on the
  system's sharing services; Arc for Windows' menus were not compared);
  the Ctrl+T hint on New Tab (each new string means a full resource rebuild, so they
  wait).
