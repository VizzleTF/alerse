# Versions

Alerse has its own version, `MAJOR.MINOR.PATCH`, the same on every platform
(0.1.0 to 0.1.7 were released under the project's earlier name, Arcomium, which
stored settings still use: `arcomium.*`). A
release is tagged `vMAJOR.MINOR.PATCH` in each platform repository; its packages
carry that version in their names.

`VERSION` holds the newest release's version: a release raises it in its
`chore(release): X.Y.Z` commit, and that commit gets the tag. Development is on the 0.1
line: each release raises the patch number. The minor number goes up only when the
project decides on it.

To make a release, follow [releasing.md](releasing.md).

## Releases

A release is titled "Alerse X.Y.Z for <platform>" and its notes name the
Chromium version it is built on. Every release carries its files: on Windows the
x64 and arm64 installers and portable zips, on Linux the AppImages, their `.zsync`
files and tar.xz, on macOS the arm64 dmg (pre-releases so far), and `SHA256SUMS` over
all of them. The platform repositories' release workflows
publish each architecture as soon as it is built (`publish.yml`, `gh release
upload --clobber`) and rewrite `SHA256SUMS` from every file of the release, so one
architecture never waits for the other. A Windows x64 build made locally may be
uploaded first, so a release is downloadable before CI finishes; CI then replaces
those files under the same names. Package names come from the tag, not from the
stamped Chromium version: `alerse_<tag>_...`.

| Release | Platforms | Chromium | What it brought |
|---|---|---|---|
| 0.1.0 | macOS (pre-release) | 156.0.8078.79 | Not published on Windows or Linux (its build was cancelled) |
| 0.1.1 | Windows, Linux | 156.0.8078.79 | First Windows release (local x64 build without PGO) and first Linux release |
| 0.1.2 | Windows | 157.0.8103.38 | Chromium 157, its own install identity, end-to-end-only sync, PGO |
| 0.1.3 | Windows | 157.0.8103.38 | Built-in import from Arc and other browsers |
| 0.1.4 | Windows | 157.0.8103.3804 | Per-release version numbers, the new-version notice, the tab speaker, DevTools fixed, older `arc_import.py` imports taken over |
| 0.1.5 | Windows | 157.0.8103.3805 | Tabs opened from pinned items stay on them across restarts, reopened tabs and restored groups |
| 0.1.6 | Windows | 157.0.8103.3806 | Command bar actions and shortcuts, Split View (keys, menus, drag a tab onto a tab or the page edge), Arc's icon pickers for Spaces, pinned items, folders and favorites; macOS look and menus matched to Arc |
| 0.1.7 | macOS (pre-release) | 156.0.8078.7907 | Last release under the earlier name. First official macOS build (ThinLTO, PGO) since 0.1.0, with the changes of 0.1.1 to 0.1.6 that apply to it; Rename Space titled "Edit Space name", the Space bar's Downloads button in place of the archive |
| 0.1.8 | Windows, macOS (pre-release) | 157.0.8103.3808 (Windows), 156.0.8078.7908 (macOS) | First release named Alerse, with its icon and install identity; takes over the earlier name's profile. Windows looks like Arc for macOS but for the top bar (sidebar, Space bar and popovers, page card, floating hidden sidebar, command bar); the logo menu lists the Spaces and has Copy URL as Quote, Reveal Tab and Collapse Pinned Tabs; the window is named after its Space. Sync sign-in with a login and password, pairing by code, recovery sheet |
| 0.1.9 | macOS (pre-release) | 156.0.8078.7909 | The address field's buttons and site controls as in Arc, pinned extensions in the field. Crashed when a window closed |
| 0.1.10 | macOS (pre-release) | 156.0.8078.7910 | Fixes 0.1.9's crash on window close; sync sign-in with a login and password on macOS; Arc's picture-in-picture window |
| 0.1.11 | Windows (x64 only), macOS (pre-release) | 157.0.8103.3811 (Windows), 156.0.8078.7911 (macOS) | Alerse instead of Chromium or Chrome everywhere in the app; no ungoogled-chromium welcome page; `chrome://alerse-*` pages on macOS |
| 0.1.12 | Windows, macOS (pre-release) | 157.0.8103.5012 (Windows, on 157.0.8103.50), 156.0.8078.7912 (macOS) | Each Space is its own Chromium profile with its own window (ADR-0001): own cookies, logins and history; a 0.1 Space left in home becomes a Space of its own with its pinned items (its tabs, cookies and logins are not carried over). Motion system; Arc's find pill, site controls, command bar on the page's address; downloads popover on the Space bar |
| 0.1.13 | Windows, macOS (pre-release) | 157.0.8103.5013 (Windows, on 157.0.8103.50), 156.0.8078.7913 (macOS) | Windows fixes after the first day on 0.1.12: one taskbar button for all Spaces, no profile avatar or per-Space shortcut; the Web Store installs again; Ctrl+L on an empty Space; Split View on a one-tab Space; "Search or Enter URL..." placeholder; no Chromium Downloads button in the top bar; incognito window title; a sidebar item picked in the command bar shows its pinned tab; Ctrl+S, Ctrl+Alt+Left/Right and Ctrl+Shift+K as in Arc. macOS gets the fixes that are not Windows-only, and site controls tiles that follow an extension's icon |
| 0.1.14 | macOS (pre-release); Windows skipped, its CI stopped for 0.1.16 | 156.0.8078.7914 (macOS) | Boosts (a site's colors, dark mode, font, size, zapped parts and own CSS, with a switch); Peek and the Little window, each with a switch; picking several tabs and copying their links; a welcome page at first start; sync accounts with the 24 words and a password; drop a sidebar row on a Space's icon to move it; overlay scroll bar in the sidebar; the sidebar slides as a whole when hidden or shown; Beta Updates in the Developer menu |
| 0.1.15 | macOS (pre-release) | 156.0.8078.7915 (macOS) | The Little window gets Arc's Open in <Space> button, a caret listing the other Spaces, and Ctrl+O / Ctrl+Alt+O (Command-O / Command-Option-O on macOS); Windows has it in 0.1.14 |
| 0.1.16 | Windows, macOS (pre-release) | 157.0.8103.5016 (Windows, on 157.0.8103.50), 156.0.8078.7916 (macOS) | A link from another app opens in the Little window without bringing the Space's window over it |
| 0.1.17 | Windows, macOS (pre-release) | 157.0.8103.5017 (Windows, on 157.0.8103.50), 156.0.8078.7917 (macOS) | Allow in Incognito in an extension's menu; a browser started with several addresses opens them all as tabs; Arc import goes to Home from any Space and joins Spaces of the same name; the Boost panel closes with its tab and its font buttons fit; the find bar sits in the page's corner on Windows too; sync server label spacing |
| 0.1.18 | macOS (pre-release); Windows from CI only | 157.0.8103.5018 (Windows, CI), 156.0.8078.7918 (macOS) | Arc's history and saved passwords import, also on the welcome page; keyboard shortcuts of your own (chrome://alerse-shortcuts on Windows and Linux, App Shortcuts on macOS); a calmer welcome page whose steps stay at one height and whose last button closes it |
| 0.1.19 | Windows, macOS (pre-release) | 157.0.8103.5019 (Windows, on 157.0.8103.50), 156.0.8078.7919 (macOS) | The same favorites in every Space, as in Arc, with a switch in a favorite's menu |
| 0.1.20 | Windows, macOS (pre-release) | 157.0.8103.5020 (Windows, on 157.0.8103.50), 156.0.8078.7920 (macOS) | No Google unless picked: DuckDuckGo is the default search engine, the welcome page's pick goes to every Space, and the Chrome Web Store is a switch, off on a new install; on Windows, Downloads only at the sidebar's bottom, not also in the header row |
| 0.1.21 | macOS (pre-release) | 156.0.8078.7921 (macOS) | Apps' Google sign-ins (System Settings' Internet Accounts, Spark) open in Safari instead of getting lost while Alerse is the default browser |
| 0.1.22 | macOS (pre-release) | 156.0.8078.7922 (macOS) | No login keychain password prompt after each update: the Safe Storage key is kept in a file in Alerse's folder |
| 0.1.23 | Windows, macOS (pre-release) | 156.0.8078.7923 (macOS), 157.0.8103.5023 (Windows) | A new install never asks for the login keychain password on macOS; pinned items' icons update in place instead of rebuilding the sidebar; five of Chrome's field-trial speed features on by default |
| 0.1.26 | Windows, macOS (pre-release) | 157.0.8103.7826 | Sync no longer makes a second profile of a Space after an update (a duplicate Work that Open in Work from the Little window went to), and deleting such a duplicate no longer deletes the Space on every device |
| 0.1.25 | Windows, macOS (pre-release) | 157.0.8103.7825 | Chromium 157.0.8103.78 on both platforms, macOS built from the core for the first time. Fixes from two full tests: favorites kept when a Space is deleted and no duplicate loop with sync, no crash on an extension's menu after Allow in Incognito, subfolders in folder popups open to the right, new tabs at the top, each window restored on its own Space, the command bar finds tabs of every Space, deleting data, history, downloads and Reset settings cover every Space, no search engine icons fetched from Google or Bing, password saving offered, Boosts hidden from page scripts, a link from another app on macOS shows only the Little window, and about 70 smaller fixes |
| 0.1.24 | Windows, macOS (pre-release) | 156.0.8078.7924 (macOS), 157.0.8103.5024 (Windows) | A playing video goes to picture-in-picture when you switch Spaces, as when you switch tabs, and comes back when its Space shows again; a page opened from the Little window keeps its site icon in the sidebar |

Linux gets the changes from 0.1.2 on with its first release on Chromium 157.

## The Chromium version inside the browser

Each release gets its own Chromium version number: Windows' installer cannot
install a version over itself while the browser runs, so two releases on the
same Chromium could not update each other. `tools/rename_product.py`, which
every platform runs over the patched tree, writes the release's patch number
into the last two digits of `chrome/VERSION`'s PATCH:

    PATCH = Chromium's PATCH x 100 + the release's PATCH

Release 0.1.4 on Chromium 157.0.8103.38 is 157.0.8103.3804. `chrome://version`,
the Windows file version and install directory, and the macOS bundle version
show it. Websites do not: the User-Agent, its client hints and
`navigator.userAgentData` report Chromium's version, 157.0.8103.38, which
`chrome/VERSION` keeps as UPSTREAM_PATCH
(`patches/defaults/upstream-web-version.patch`). Release notes name Chromium's
version, as in "Alerse 0.1.8, Chromium 157.0.8103.38".

The numbers only grow when:

- The release's PATCH stays below 100; the script refuses 100 and up.
- The release's PATCH does not go down while Chromium's version stays: a
  release that raises its MINOR or MAJOR on the same Chromium keeps
  counting the patch number (0.1.7 is followed by 0.2.8, not 0.2.0), or comes
  with a newer Chromium. A newer Chromium outranks every earlier stamp. The
  platform repositories' release workflows (`.github/scripts/release-version.sh`)
  compare the stamp with that of the latest release, whose notes name its
  Chromium, and fail before building when it is not higher.
- Chromium's PATCH is at most 655, or the stamp no longer fits in the 16 bits
  of a Windows file version; the script refuses it too. Stable Chromium
  patch numbers stay far below that.
- Every release raises `VERSION`, a release of one platform repository
  too: the release workflows accept only the tag `v` + `VERSION`, as the
  update check below needs.

Up to 0.1.3 every release had Chromium's own number, so installing one over a
running browser failed (installer exit code 7) and rolled back.

Stamping runs again safely: when `chrome/VERSION` already has UPSTREAM_PATCH, the
script takes Chromium's PATCH from it instead of stamping twice. Linux copies the
core's `VERSION` into its build container for this. The stamped version still shows
outside the browser in one place: the `prodversion` of extension and component
update checks.

## Telling about a newer release

`tools/rename_product.py` also writes the release into `chrome/VERSION` as
ALERSE_VERSION, and `chrome/browser/alerse_update` compiles it in. Once a
day the browser reads the `tag_name` of the platform repository's latest
GitHub release and, when that `vMAJOR.MINOR.PATCH` is above its own version,
shows "Update to Alerse MAJOR.MINOR.PATCH" at the top of its menus
(builds of 0.1.7 and before find the same releases: the repositories were renamed,
and GitHub redirects their old names). So the
tag of every release must be its `VERSION`, with a `v`; drafts and
pre-releases are not "latest" and are not offered. macOS is the exception: its
releases are all pre-releases while it trails Windows' Chromium version, so there
the browser reads the last twenty releases and takes the newest that is not a
draft (`NewestRelease()`), or it would never offer an update. A build whose
`chrome/VERSION` has no ALERSE_VERSION does not check, nor does a browser
in headless mode or started with `--disable-background-networking`.

Any answer from GitHub counts, one with no release or with a tag that is
not `vMAJOR.MINOR.PATCH` too (the last release found stays): the next check
is a day later. Only no answer at all, GitHub's rate limit (HTTP 403, 429)
or a server error (5xx) is retried an hour later.

The Local State pref `arcomium.update_check.enabled` (true by default) turns
the check off; `arcomium.update_check.latest` and `.last_time` keep the last
answer, so the menu item survives restarts until the newer release is
installed.
