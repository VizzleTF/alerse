# How Arcomium works

How each Arcomium feature maps onto Chromium, and why it is built that way.

Goals: Spaces with separate accounts in one window, a sidebar for people moving from Arc,
a command bar, Split View, real-time sync across Windows, macOS, Linux and iPhone, link
routing to Spaces, media controls and picture-in-picture. Not yet: a preview window for
links opened from other apps.

Arcomium follows how Arc behaves, not how it looks in detail: its own names, icons, logo
and texts, and none of Arc's code or artwork.

## Spaces

A Space is a named set of tabs with its own color and its own
cookie jar. All Spaces live in one window and one Chromium profile.

Implementation (patches in `patches/spaces/`):

- A Space is a tab group shown in Chromium's tab group focus mode
  (`features::kTabGroupsFocusing`, enabled by default). In focus mode the vertical tab
  strip lists only the focused group's tabs, new tabs land in that group, and the focused
  group survives restarts. Pinned tabs are shared by all Spaces (Arc's favorites).
- Each Space owns a storage partition: `StoragePartitionConfig::Create(profile, "arcomium",
  <space id>, /*in_memory=*/false)`, where the Space id is a random token picked when the
  Space is created. Its tabs are created with
  `SiteInstance::CreateForFixedStoragePartition()`, so cookies, localStorage, IndexedDB,
  cache and service workers are per Space. Passwords, history, bookmarks and extensions
  stay per profile, so the password manager serves every Space. Isolation cannot be
  turned off: a new Space starts logged out of every site. Incognito windows never get
  Space partitions.
- Space tabs are ordinary tabs, not guest processes, so extensions and their content
  scripts run in them as in any other tab.
- Isolation costs memory and disk: renderer processes are not shared across partitions,
  so a site open in two Spaces runs in two processes, and each Space has its own network
  context and HTTP cache of the full default size.
- The Space of a group is the partition of its tabs, so there is no separate registry.
  A new tab gets the Space of the page that opened it, else of its group (explicit or
  focused). The New Tab page is WebUI and cannot live in a partition, so a new tab of a
  Space opens `about:blank`. Content does not assign a site to `about:blank`, and a
  SiteInstance without a site reports the default partition, so
  `fixed-partition-blank-pages.patch` assigns one in fixed-partition BrowsingInstances,
  as content already does for `<webview>` guests.
- Session restore: the window extra data `arcomium.spaces` holds `{tab group id: Space id}`
  (written on every rebuild and on focus changes); restored tabs of those groups are
  recreated in their partition, without a renderer until shown. Startup continues the
  last session by default (`defaults/continue-session.patch`): otherwise the Spaces' tabs
  would be lost on restart.
- Each Space has one saved tab group, whose id is a UUIDv5 of the Space id
  (`SavedGroupGuidForSpace()`), so every device and every restored session share one
  group, as a client tag does in Chromium sync. Every saved Space opens in the window
  once sync, session restore and bookmarks are ready, without changing the Space shown;
  its tabs from another device load only when shown (`spaces/lazy-saved-space-tabs.patch`).
- An empty Space stays, as in Arc: closing its last tab leaves a hidden blank tab
  (`about:blank#arcomium-space`, `spaces/empty-space.patch`), and the window stays open.
  `--close-window-with-last-tab=always` turns this off. The empty page takes no focus
  and opens no command bar until asked. A Space whose tab group is gone (all its tabs
  closed) stays in the Space bar through its bookmark folder and opens again from it.
- Tabs of the Spaces not shown are not frozen (`spaces/no-focus-freezing.patch`): focus
  mode would freeze them, stopping music or a messenger in another Space, which Arc
  does not do. Memory Saver, on by default, unloads idle tabs instead.
- Data: Clear browsing data clears every Space's partition, open or not, which logs the
  user out in every Space. Chromium's partition cleanup always keeps
  `Storage/ext/arcomium`, so a closed Space keeps its logins. A tab Memory Saver
  discarded comes back in its Space's partition (`spaces/space-data-lifecycle.patch`).
- UI: `ArcomiumSpaceSwitcher`, the Space bar at the bottom of the sidebar: the archive
  at the left (opens the history), an icon per Space (the active one bright), "+" at the
  right. A Space's icon is picked from its menu and stored in its bookmark folder as
  `arcomium.space_icon`, so it syncs; Home is a sun until another is picked. The same
  menu has Edit Theme Color and Delete Space (after a question; it closes the tabs and
  removes the saved group and the folder). Alt+1 to Alt+9 show the Spaces in the bar's
  order, Home first, except on macOS, where Option+digit types characters. A horizontal
  trackpad swipe walks the bar, closed Spaces included: the sidebar follows the fingers
  with the next Space's rows beside the current ones, and a lift past 60 pt switches.
  Each Space returns to the tab that was active in it.
- Color: Edit Theme Color (in the Space's menu, or a right click in the sidebar's empty
  space) picks hue and saturation, light or dark, intensity and brightness, or a preset.
  The color tints the window frame and the sidebar and is stored in the Space's folder
  as `arcomium.space_color`, so it syncs; on a light frame the sidebar inverts its
  colors. Switching Spaces blends the tint into the next Space's over 200 ms in six fixed
  steps (`arcomium_space_color.cc`). Every tint is a new color provider key, so
  `look/bounded-tint-color-providers.patch` keeps at most 32 tinted providers, oldest out
  first, instead of all of them for the session.

Known gaps:

- a deleted Space's partition stays on disk;
- a tab dragged into a Space's group keeps its old partition;
- recently closed tabs (Ctrl+Shift+T) and some restored tabs come back in the default
  partition, without their logins: the Space of a tab is found from its partition or
  its folder's name, not stored per tab;
- a Space open in one window looks closed in a second window and opens there again;
- notifications and push messages of a Space's sites use the default partition;
- the incognito window's Space bar lists the regular profile's Spaces;
- moving a tab to another Space reopens it there, losing its history and form state;
- not yet checked: downloads and the PDF viewer in a Space tab. Bitwarden's passkeys
  work in 0.1.4, DevTools too.

## Sidebar look

The expanded sidebar is Arc's, measured pixel by pixel on Arc for Windows at 100% scale
(see `ArcomiumSidebarView`):

- Header row, 32 px: Arcomium menu, sidebar toggle, downloads at x = 23, 58, 93.
- Space name (Segoe UI Variable Small, 12 px, #848484).
- Pinned tree: the Space's bookmark folder (meta info `arcomium.space = <Space id>`,
  `default` outside of Spaces, `arc:<id>` for an imported Space not yet adopted by an
  Arcomium Space of the same name). Rows are 43.3 px apart, icon at x = 16, text at
  x = 44 in Segoe UI Variable Text 14 px (semilight items, semibold folders).
  Folder glyphs follow Arc: a folder holding open tabs shows two dashes, otherwise its
  Arc icon (`arcomium.folder_icon`). Open state is the `arcomium.open` meta info.
- A pinned item opens as a tab bound to it. Bound tabs are not listed with the other
  tabs: they show on their item, and under a collapsed folder its bound tabs are listed
  as child rows. A bound tab that navigated away shows "/" before the item's name.
  As in Arc, a tab stays on its item across restarts, crash restore and Ctrl+Shift+T,
  wherever it has navigated: the item's bookmark uuid is session tab extra data
  `arcomium.pinned` (`pinned-tab-binding.patch` writes it in full session rewrites and
  carries it onto restored tabs and through TabRestoreService), and for a tab of a
  Space's saved group the bookmark's meta `arcomium.saved_tab` holds the saved tab's
  guid, so a closed Space or one from another device reopens its tabs on their items.
  A tab binds only to an item of its own Space; once its item is deleted, or moved to
  another Space with the tab, it is an ordinary tab. Tabs with no stored binding (from
  before 0.1.5) are bound by URL.
- A tab that plays sound shows a speaker between its icon (and "/") and its title, as in
  Arc: while the tab strip's audio alert is on (it outlasts the gaps between tracks) or
  while the tab is muted. Clicking it mutes or unmutes the tab
  (`TabMutedReason::kAudioIndicator`); muted, it is crossed out. Its own press never
  activates or drags the row; a right click opens the row's menu, whose Mute item follows
  the same rule. Rows in a folder popup show it too; the popup is refilled only when one
  of its speakers changes.
- Hovering a collapsed folder pops up its links (subfolders open in place); picking
  one opens it as a bound tab without expanding the folder. The popup is a non-modal
  bubble, not a menu, so the mouse stays with the sidebar: it opens after 200 ms, fades
  in over 120 ms, follows the pointer to another folder and closes when the pointer
  leaves. It opens inactive; on Windows it may activate on a click, which Windows
  would otherwise drop. On macOS an inactive popup gets no mouse moves, so its rows take
  hover from the popup's pointer polls.
- Divider, "+ New Tab", then the Space's other tabs. A tab's close button shows only
  while its row is hovered.
- Folders open and close in 150 ms, animated on layers (rows clipped like a curtain,
  rows below shifted by transforms): resizing the rows relaid out the whole browser view
  every frame and stuttered. A folder's open state is meta info, which
  `BaseBookmarkModelObserver` does not forward, so the sidebar redraws on it itself.
- Drag and drop: rows move within the pinned tree and into folders; a tab dropped among
  the pinned items becomes pinned; a pinned item dropped among the tabs is unpinned and
  keeps its tab; tabs reorder within their Space. A line or a highlighted folder shows
  where the row lands. The sidebar does not rebuild during a drag.
- Context menus: pinned items and tabs get Copy Link, Duplicate, Mute, Open in Split
  View (Chromium's split view) and Move to a Space (a pinned item or folder moves to that
  Space's pinned folder, a tab reopens in that Space's partition; a Space whose last tab
  leaves keeps a New Tab). Pinned items also get Edit Pinned Page: replace the saved
  address with the current one, or edit the name and address. The "+" next to New Tab
  offers a new Space, folder or tab.
- Rebuilds: rows exist only for the active Space. Changes to tabs of hidden Spaces that
  are not bound to a pinned item are ignored, theme changes rebuild once they settle
  (250 ms), and the Space bar rebuilds only when a saved Space's title, color, open
  state or emptiness changes, not on every navigation.
- When the sidebar is collapsed, Chromium's own vertical tab list shows, with its expand
  button.
- The page is a card with 10 px corners next to the sidebar; the Space bar sits at the
  bottom.

Known gaps: no keyboard navigation of the sidebar (rows draw no focus ring and have no
tree roles); Arcomium's own text is English only: most of it is in
`arcomium_strings.grdp` and translatable, but the sync page's error messages are
hard-coded English (`arcomium_sync_handler.cc`); tab rows show a speaker for sound but
no camera, microphone or screen-capture indicator.

## Window, address bar and command bar

- **Windows**: a 32 px top bar in the frame color, as in Arc for Windows 1.129: back,
  forward and reload, the site centered in a pill between Copy Link and the site
  controls, Split View and the window buttons. The page is an inset card with Arc's rim,
  corners and shadow, and Arc's thin icons (`components/vector_icons/arcomium_*.icon`).
  Overlay scrollbars follow the Windows setting.
- **macOS**: no toolbar while the sidebar shows; the sidebar's header row starts after
  the window buttons, and back, forward and reload sit in a second header row. Header
  buttons are 28 px targets, 32 px apart, and take clicks instead of dragging the
  window. The sidebar and mini player use the system font, as Arc does on macOS (on
  Windows, Segoe UI Variable).
- The compact address shows only the site, centered, with no page actions; Copy Link
  and the site controls show on hover. It hides the site's icon, but never the
  "Not secure" or dangerous-site warning: without Safe Browsing it is the last warning
  left. It also hides the page actions and content-setting icons (blocked popups,
  location, downloads); camera and microphone use still shows through Chromium's
  activity indicator, and the rest is in the site controls. Clicking it opens the
  command bar. The address field's placeholder is Arc's "Search..." whatever the
  search engine (`look/sidebar-search-placeholder.patch`).
- **Command bar**: Chromium's omnibox in a 722 px panel centered in the window, opened
  by clicking the address or New Tab (which then opens a new tab), closed by a click
  outside. Rows are 48 px, regular weight; searches show only their words, history rows
  a remove ×, bookmarks the site's icon. Open tabs are suggested without `@tabs`, and
  picking one switches to it. Tabs and bookmarks also match fuzzily
  (`look/command-bar.patch`): at least two letters, close together and starting a word
  unless they are contiguous, at most three bookmarks. On Windows and Linux the input is
  16 px, larger than the rows, as in Arc for Windows; macOS keeps the address's font.
- **Logo menu** (Windows, Linux; `arcomium_app_menu.cc`): Check for Updates (or
  "Update to Arcomium X.Y.Z"), Import from Another Browser..., new tab, window and
  incognito window, Spaces, Tabs, Extensions and Developer submenus, Downloads,
  History, Bookmarks, Sync, Search Engine, Link routing, Settings, Print, About, More...
  (Chromium's full menu) and Close Window. On macOS the logo opens Chromium's app menu,
  which carries Arcomium's items (Sync, Link routing; Update and Import come with the
  first macOS build on 157), and the menu bar stays.
- **Extensions** sit at the end of the address row, sized like its buttons. The sliders
  button opens the site controls (`arcomium_site_controls.cc`): Copy Link, Find,
  Screenshot, Reading mode; every extension as a tile (click runs it, right click pins
  it); the site's permissions; the connection (opens page info); and a menu with site
  settings, extension site access and extension management. They close when their tab
  closes or leaves the page.

Text the user sees is in `IDS_ARCOMIUM_*` messages (except the sync page's errors) and colors are `kColorArcomium*` ids
(see [patching](patching.md)), set from the frame: Arc's greys on a dark frame, inverted
on a light one, system colors in high contrast.

## Import

`chrome://arcomium-import` (Import from Another Browser... in the logo
menu and the app menu, `IDC_ARCOMIUM_IMPORT`) reads Arc's `StorableSidebar.json` and
the `Bookmarks` files of Chromium-based browsers on the thread pool (shared reads,
64 MB limit; Arc's sidebar is read up to 100,000 items and 32 MB of text) and writes
the bookmark model on the UI thread. Each node an import adds is marked
`arcomium.imported` = HMAC-SHA256 (first 16 bytes, hex) of the source and the node's
uuid under the profile's own key (`arcomium.import_key`, a local pref): a copy the user
made (another uuid) and what another device imported (synced, another key) are not
marked here, so importing again replaces only this profile's imported nodes and keeps
the rest. Arc's Spaces become folders under Other bookmarks: the first one is Home
(`default`), the others get a fresh Space id and a Space opened in the background (its
tab group with the hidden blank tab). Each folder is bound to Arc's Space by
`arcomium.arc_space` (Arc's id) and `arcomium.arc_space_mark` (the same HMAC), so an
import again reuses the same folders and Spaces whatever Arc's order; a folder whose
Space Arc no longer has loses its imported items and the binding. Cached favicons
(`SidebarItemsFaviconCache`, PNG, named after the MD5 of the item id) go to
`arcomium.icon`. A browser profile's bookmarks go to one folder in the current
Space's (or Home's) pinned folder, marked `arcomium.import = <browser>/<profile>` and
recorded by uuid in the local pref `arcomium.import_folders`; importing it again
replaces the folder's imported contents. Sources are found in the per-user known folders (Windows), Application
Support (macOS) or the XDG config directory (Linux). Arcomium's own user data directory
is skipped, even when Arcomium still runs from Chromium's (see Platforms), and so is any
profile whose Preferences have an `arcomium` key; profiles without a `Bookmarks` file
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
(id, position, title, color, icon, saved group); only its imported items are replaced.
Its icon changes only while it is still the one Arc gave it (`arcomium.arc_icon`). Home
takes Arc's first Space unless Home is already bound to another. The user's own
bookmarks in an imported folder are kept: each largest unmarked subtree is moved out
before the imported nodes are removed (`arcomium_import_bookmarks.cc`), to the root of
the Space's or the import's folder when its subfolder goes.

The import's unit tests are in `arcomium_import_data_unittest.cc` and
`arcomium_import_bookmarks_unittest.cc`, built into `unit_tests` by
`look/import-from-browsers.patch`.

Known gaps:

- import on macOS and Linux was never built or run there; the paths follow the
  browsers' documentation;
- icons come only from Arc; other browsers' bookmarks get their icons on the first
  visit;
- an imported Arc item the user moved out of its folder is added again by the next
  import;
- Home is shared by all devices, so importing different Arc data on two devices may
  leave both devices' pins in Home;
- on Windows the import item also shows in Chromium's own app menu (More...).

## Sync

Sync goes to an [arcomium-sync](https://github.com/VizzleTF/arcomium-sync) server the
user picks, set up at `chrome://arcomium-sync` (Sync in the logo menu and the app menu;
every link to Chromium's sync settings leads there). There is no default server: the
user runs one, at home or in their own cloud account, and the browser only needs its
URL. A free public server is planned, not running. The server only ever sees ciphertext,
so such a server would need no trust in its operator.

### Identity and encryption: a sync code instead of an account

Chromium only starts sync for a signed-in account, and ungoogled-chromium has no Google
sign-in. As in Brave Sync v2:

- The first device starts a sync chain: a random 256-bit secret, shown as a 24-word
  sync code (BIP39 words with an 8-bit SHA-256 checksum, `arcomium_sync_code.cc`).
  Other devices join by typing it; any case works, and so do the words' first four
  letters. The page checks that the server answers before it saves anything.
- HKDF-SHA256 over the secret derives, with separate labels, an Ed25519 key pair and the
  Nigori custom passphrase (`arcomium_sync_account.cc`). Every request carries a token:
  the public key and a timestamp, signed with the private key. The server checks the
  signature and freshness and knows the chain only by a hash of the public key. No
  passwords, no emails, nothing to leak but a public key. The exact format is in
  [arcomium-sync's protocol notes](https://github.com/VizzleTF/arcomium-sync/blob/main/docs/protocol.md).
  The browser makes a new token every hour; a token is valid for 24 hours.
- The passphrase is applied with `encrypt_everything` on, so bookmarks, tabs, Spaces,
  passwords and history are encrypted on the device. Chromium refuses to go back from a
  custom passphrase to the server's keystore key.
- End-to-end or nothing (`sync/e2e-before-data.patch`; Windows from 0.1.2, Linux and
  macOS from their first build on 157, not iOS): until the chain's data is
  encrypted with its passphrase, only the device list (`DEVICE_INFO`) syncs; the other
  data types are configured once the passphrase type is custom. Checked against a
  server that refuses unencrypted data: none of the bookmarks reached it readable.
- The secret is stored encrypted by the OS (DPAPI, Keychain) through Chromium's
  `os_crypt_async`; if that fails it is not stored and sync does not start.
- Client side: Chromium's local sync mode with a local primary account for the chain,
  the access token in the Authorization header, and the server URL from the page. The
  URL must be https; plain http only for a server on this computer, since the data is
  encrypted but its metadata and the token are not.
- `--arcomium-sync-url` and `--arcomium-sync-secret` set a chain from the command line
  for tests; official builds ignore them, so a shortcut cannot join a profile to
  someone else's chain.
- Leave sync chain forgets the server and the code on this device and stops sync. The
  other devices keep their data, and so does the server.

Known gaps: the sync code cannot be changed or revoked, a single device cannot be
removed, and nothing deletes the chain's data on the server, although the server
supports Chromium's ClearServerData; the iOS page accepts plain http to any host; the
code is shown and copied without asking for the OS password. Linux 0.1.1, macOS 0.1.0
and the iOS app do not hold data back until the passphrase is set, so a chain started
on one of them may upload its first data unencrypted: start a chain on Windows 0.1.2 or
later and only join it from the others.

### Server: arcomium-sync

Written for Arcomium in Go: [arcomium-sync](https://github.com/VizzleTF/arcomium-sync).
It speaks the Chromium sync protocol (`POST /v2/command/`), with the protocol buffers of
the Chromium tree we build, so every data type Chromium syncs works without server
changes.

- PostgreSQL store. Each chain has a version counter; a commit runs in one transaction
  that holds the chain's lock, so versions only grow and a device never misses a change.
  Any Postgres works: a container, CloudNativePG, or a free managed database (Neon,
  Supabase), which also makes the server itself stateless.
- `GET /v2/events` Server-Sent Events: after a Commit, the server tells the chain's other
  devices which data types changed, through Postgres `NOTIFY` so several instances can
  share a database. The client (`arcomium_sync_events.cc`) subscribes, reconnects with
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
| Public server (planned) | everyone | free | One instance with the public-server limits of the server's README. Data is end-to-end encrypted. |
| Own free cloud (later) | no server at all | free | A Cloudflare Workers port (one SQLite-backed Durable Object per chain; WebSocket hibernation for events) behind a "Deploy to Cloudflare" button. The free plan's 100k requests a day fit one person's devices. |

Moving between options: the sync code stays the same; point the browser at the new URL
and it uploads everything again.

### Data mapping

No new sync data types are needed:

| Arcomium data | Chromium sync type |
|---|---|
| Pinned tree of each Space (nested folders), Space colors and icons | `BOOKMARKS` (the Space folders with `arcomium.*` meta info) |
| Open tabs on other devices | `SESSIONS` |
| Spaces: name, color, open tabs | `SAVED_TAB_GROUP`, one saved group per Space |
| Space settings | `PREFERENCES` (syncable prefs) |
| History and its deletions, tabs sent to a device | `HISTORY`, `HISTORY_DELETE_DIRECTIVE`, `SEND_TAB_TO_SELF`, encrypted with the chain's passphrase; deletions reach other devices without Google's web history |
| Passwords, extensions | their standard types |

Link routes and import bookkeeping (`arcomium.import_key`, `arcomium.import_folders`)
stay local.

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
| Windows | [arcomium-win](https://github.com/VizzleTF/arcomium-win) | ungoogled-chromium-windows |
| macOS | [arcomium-mac](https://github.com/VizzleTF/arcomium-mac) | ungoogled-chromium-macos |
| Linux | [arcomium-linux](https://github.com/VizzleTF/arcomium-linux) | ungoogled-chromium-portablelinux |
| iPhone | [arcomium-ios](https://github.com/VizzleTF/arcomium-ios) (experimental) | Chromium's iOS app (`ios/chrome`) |

Arcomium follows ungoogled-chromium's releases, so it runs about one Chromium major
version behind Chrome stable. The series on `main` is for Chromium 157; the branch
`chromium-156` keeps the 156 series. Windows is on 157. Linux (release 0.1.1) and macOS
(pre-release 0.1.0) were released from the 156 series and move to 157 when
ungoogled-chromium-portablelinux and ungoogled-chromium-macos release it; their
repositories already pin the core's `main`, so they do not build until then. iOS pins
the core at 0.1.1 (156). What came in 0.1.2 to 0.1.4 (own install identity,
end-to-end-only sync, import, the update notice) is on Windows only until then.

Install identity: on Windows Arcomium installs as itself, not as Chromium
(`defaults/own-install-identity.patch`): `%LOCALAPPDATA%\Arcomium\Application`, its own
registry key, app id, ProgIDs `ArcomiumHTM` and `ArcomiumPDF`, URL scheme `arcomium://`,
Active Setup GUID and toast activator CLSID, so a Chromium install can neither
overwrite it nor open its profile. On Linux the profile is `~/.config/arcomium` from
the first release on 157 (0.1.1 keeps it in `~/.config/chromium`). On
macOS Arcomium has its own bundle ID (`tools/rename_product.py`) and profile directory
(`defaults/mac-own-profile-dir.patch`). On first start, a profile in Chromium's directory
that Arcomium used (its Preferences have an `arcomium` key) is moved to Arcomium's; if
the move fails, for example while an older Arcomium still runs, Arcomium keeps using it
where it is.

Platform differences: the Arc for Windows top bar, the logo menu, the 16 px command bar
input and Alt+1 to Alt+9 are not on macOS, where Arc itself looks different (window
buttons, the menu bar); there the logo opens Chromium's app menu. Context menus, the
command bar's results and the rest of the sidebar are the same everywhere.

The iPhone app is a different code base (WebKit inside, its own UI); it shares the sync
engine, so it gets bookmarks, tabs and Spaces through the same server. Spaces there are a
tab list only: WebKit has no storage partition per tab, so separate accounts per Space
stay desktop-only unless each Space maps to its own `WKWebsiteDataStore` (iOS 17+).

## Link routing

Routes of the form "site -> Space", set at **Link routing** in the app menu
(`chrome://arcomium-links`). A site is a host with its subdomains, optionally with a path
("github.com/acme"); the longest match wins.

Routes apply to links that arrive from outside the browser, with Arcomium as the default
browser: `Navigate()` sees a new tab with no source tab and an `AUTO_TOPLEVEL` transition,
opens it in the route's Space, in that Space's partition, and switches to the Space. A
closed Space (its tab group is gone; an emptied Space keeps a hidden blank tab) is
opened again under the name it had. Links clicked inside pages stay in their Space, same as Arc.

Routes are a local pref (`arcomium.link_routes`), not synced, by choice: Space ids do
sync (the folder meta `arcomium.space`), so a route would resolve on another device
too, and syncing routes is possible later.

## Media and picture-in-picture

- **Mini player** at the bottom of the sidebar (`ArcomiumMiniPlayer`): the media
  session that last took audio focus, in any Space or window of the profile, while its
  tab is not the one you are looking at. It shows the artwork (or the site's icon), the
  title and the artist, pauses and resumes, and a click goes to the tab and its Space.
  Only media of the window's own profile shows: an incognito tab's media never shows
  in a regular window, nor the other way round. It is push-based (Chromium's media
  session controller), with no polling.
- **Automatic picture-in-picture** when you leave a playing video's tab or Space, on any
  site: Chromium's browser-initiated automatic picture-in-picture
  (`BrowserInitiatedAutomaticPictureInPicture`), enabled by default. Its permission
  defaults to Allow, as in Arc, which also skips Chromium's media engagement check; a
  site can be blocked in its settings.

## Defaults

Arcomium keeps ungoogled-chromium's defaults (no Safe Browsing, preloading off), with
these changes:

- Vertical tabs on, so the sidebar shows (`defaults/vertical-tabs-by-default.patch`);
  startup continues the last session (see Spaces).
- Google is the default search engine, without Chrome's preconnect to it or prefetch of
  its likely results; Search Engine in the logo menu picks another.
- The Chrome Web Store installs and updates extensions, so they sync between devices
  (`extensions/web-store.patch`): CRX3 publisher proofs are still checked, update URLs
  are forced to https, and on Windows extensions registered in the registry are not
  loaded.
- The bookmark bar does not show on the New Tab page, as pinned items live in the
  sidebar; `--bookmark-bar-ntp=always` brings it back.
- The window stays open when its last tab closes (see Spaces).
- `defaults/feature-defaults.patch`: Back in a tab a link opened returns to the opener
  tab; parallel downloads everywhere; Memory Saver on; three performance-manager
  policies Chrome runs from its field trials (`kTransientKeepAlivePolicy`,
  `kPMLoadingPageVoter`, `kEnableBestEffortTaskInhibitingPolicy`); a reduced
  Accept-Language header; the Topics API off. Ctrl+Tab goes in most-recently-used
  order (the `kCtrlTabMru` pref on; the feature of that name is Chromium's default since
  157).
- The dangling `raw_ptr` detector never stops the browser
  (`defaults/no-dangling-ptr-crash.patch`), as in an official build; BackupRefPtr keeps
  those pointers safe. Official builds do not compile the detector at all, so the patch
  only matters for development builds.

## Not there yet

- Updates: no updater. The browser only tells about a newer release
  ([versioning](versioning.md)); the user downloads it.
- Builds are not code-signed: Windows SmartScreen warns, and macOS builds are signed ad
  hoc and not notarized.
- No crash dumps or debug symbols in releases, so crashes are known only by symptoms.
- Few automated tests, and CI runs none of the browser's: the import and update-check
  unit tests run only locally.
- No public sync server.
- From Arc: Little Arc and Peek for links from other apps, archiving idle tabs, commands
  in the command bar; Copy All Links and Duplicate in a folder's menu; the Ctrl+T hint
  on New Tab (each new string means a full resource rebuild, so they wait).
