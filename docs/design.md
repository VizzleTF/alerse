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
  stay per profile.
- The Space of a group is the partition of its tabs, so there is no separate registry.
  A new tab gets the Space of the page that opened it, else of its group (explicit or
  focused). The New Tab page is WebUI and cannot live in a partition, so a new tab of a
  Space opens `about:blank`. Content does not assign a site to `about:blank`, and a
  SiteInstance without a site reports the default partition, so
  `fixed-partition-blank-pages.patch` assigns one in fixed-partition BrowsingInstances,
  as content already does for `<webview>` guests.
- Session restore: the window extra data `arcomium.spaces` holds `{tab group id: Space id}`
  (written on every rebuild and on focus changes); restored tabs of those groups are
  recreated in their partition.
- UI: `ArcomiumSpaceSwitcher` in the bottom container of the vertical tab strip shows a dot
  per Space (group color, ring on the active one) and a "+" that creates a Space.

Known gaps:

- a Space disappears when its last tab closes, and its partition stays on disk;
- a tab dragged into a Space's group keeps its old partition;
- recently closed tabs (Ctrl+Shift+T) reopen in the default partition;
- not yet checked: extensions, password manager, downloads, DevTools and PDF viewer in a
  Space tab.

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
  After a restart tabs are bound again by URL.
- Hovering a collapsed folder pops up its links (nested folders as submenus); picking
  one opens it as a bound tab without expanding the folder.
- Divider, "+ New Tab", then the Space's other tabs.
- The page is a card with 10 px corners next to the sidebar; the Space bar sits at the
  bottom.
- `tools/arc_import.py` imports Arc's Spaces, pinned tabs, nested folders, folder icons
  and cached favicons (`arcomium.icon`) into an Arcomium profile.
- To do: Arc's toolbar height (32 px) for the page header.

## Sync

Users choose where their data lives, in Settings > Sync:

- **Arcomium Cloud**: a free public server run by the project. Nothing to set up.
- **My server**: the same server run by the user, at home or on their own free cloud
  account; the browser only needs its URL.

Both use one protocol and one server codebase, and in both the server only ever sees
ciphertext, so the free public server needs no trust in its operator.

### Identity and encryption: a sync code instead of an account

Chromium only starts sync for a signed-in account, and ungoogled-chromium has no Google
sign-in. As in Brave Sync v2:

- The first device generates a 24-word sync code (BIP39). Other devices join by typing
  it or scanning a QR code from a device that is already in.
- HKDF over the code derives an Ed25519 key pair. Every request carries a token: the
  public key and a timestamp, signed with the private key. The server checks the
  signature and freshness and knows the chain only by a hash of the public key. No
  passwords, no emails, nothing to leak but a public key. The exact format is in
  [arcomium-sync's protocol notes](https://github.com/VizzleTF/arcomium-sync/blob/main/docs/protocol.md).
- The same code derives the Nigori custom passphrase with `encrypt_everything` on, so
  bookmarks, tabs, Spaces, passwords and history are encrypted on the device.
- Client side: a local primary account for the sync code (the identity layer reports it
  as signed in), the access token in the Authorization header, and the sync URL from the
  setting. Removing a device or resetting the chain is a server call signed by the code.

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
  share a database. A small client patch subscribes and triggers a sync cycle; without it
  the client polls once an hour. SSE rather than WebSocket because Chromium's network
  stack streams it with a plain URL loader.
- Per-chain limits: requests a minute, entity size, entities per chain, event streams.
- One static binary, a distroless image, and a compose file with Postgres.

Existing servers were not a fit: [brave/go-sync](https://github.com/brave/go-sync) is
built around DynamoDB and Redis, and [selfsync](https://github.com/loyalpartner/selfsync)
(SQLite) has no authentication and expects a real sign-in.

### Where the server runs

| Option | For whom | Cost | Notes |
|---|---|---|---|
| Arcomium Cloud | everyone, default | free | One instance on a free VM (Oracle Cloud Always Free, if its quota still allows; otherwise a cheap VPS), Cloudflare in front for TLS. Data is E2E encrypted. |
| Home server | NAS, Raspberry Pi, homelab | free | `docker compose up -d` (server + Postgres). Reach it from outside through Tailscale, NetBird or a Cloudflare Tunnel (all free); no open ports needed. |
| Own free cloud | no home server, no trust in the project | free | Later: a Cloudflare Workers port (one SQLite-backed Durable Object per chain; WebSocket hibernation for events) behind a "Deploy to Cloudflare" button. The free plan's 100k requests a day fit one person's devices. |

Moving between options: the sync code stays the same; point the browser at the new URL
and it uploads everything again.

### Data mapping

No new sync data types are needed:

| Arcomium data | Chromium sync type |
|---|---|
| Pinned tree of each Space (nested folders) | `BOOKMARKS` (the Space folders with `arcomium.*` meta info) |
| Open tabs on other devices | `SESSIONS` |
| Spaces: name, color, open tabs | `SAVED_TAB_GROUP`, one saved group per Space |
| Space settings | `PREFERENCES` (syncable prefs) |
| Passwords, history, extensions | their standard types |

### Order of work

1. Server: Postgres store, current protos, events, rate limits, Docker image (done);
   run it in the homelab.
2. Client: sync code UI, local account, token signing, sync URL setting; polling.
3. Client side of the events channel (SSE) for real-time updates.
4. Public Arcomium Cloud instance with quotas.
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
Space with no tabs left has no tab group, so the route brings it back under the name it
had. Links clicked inside pages stay in their Space, same as Arc.

Routes are a local pref, not synced: a Space's id is the name of its storage partition,
which each device makes up on its own.

## Media and picture-in-picture

- **Mini player** at the bottom of the sidebar (`ArcomiumMiniPlayer`): the media
  session that last took audio focus, in any Space or window of the profile, while its
  tab is not the one you are looking at. It shows the artwork (or the site's icon), the
  title and the artist, pauses and resumes, and a click goes to the tab and its Space.
- **Automatic picture-in-picture** when you leave a playing video's tab or Space, on any
  site: Chromium's browser-initiated automatic picture-in-picture
  (`BrowserInitiatedAutomaticPictureInPicture`), enabled by default. Its permission
  defaults to Allow, as in Arc, which also skips Chromium's media engagement check; a
  site can be blocked in its settings.
