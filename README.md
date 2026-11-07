# Arcomium

A sidebar browser built on [ungoogled-chromium](https://github.com/ungoogled-software/ungoogled-chromium)
for people moving from Arc: a vertical sidebar, Spaces with separate logins in one window,
a command bar, and sync you can host yourself.

This repository holds what all platforms share: the Chromium patches, the design notes
and a fallback Arc import script. Builds come from the platform repositories below.

![Arcomium: the Personal Space with pinned items, nested folders and open tabs](docs/images/sidebar.png)

## Features

- **The sidebar**: Space name, pinned items in nested folders, open tabs under
  collapsed folders, a popup of a folder's links on hover, New Tab, then the Space's tabs.
- **Spaces in one window**: each Space keeps its own cookies and site data, so you can be
  logged into different accounts side by side. Spaces survive restarts.

  ![The Work Space, switched to with the dots at the bottom of the sidebar](docs/images/spaces.png)
- **Sync on your terms**: bookmarks, tabs, passwords and settings through your own
  [arcomium-sync](https://github.com/VizzleTF/arcomium-sync) server, end-to-end
  encrypted, with no account: devices join with a 24-word sync code. Set it up from **Sync**
  in the app menu. A free public server is planned.
- **Link routing**: links from other apps open in the Space you choose for their site,
  such as github.com in Work.
- **Media**: a mini player at the bottom of the sidebar for media playing in another
  tab, and picture-in-picture when you leave a playing video.
- **Import from Arc and other browsers**: Arc's Spaces, pinned tabs, nested folders and
  icons, and the bookmarks of Chrome, Edge, Brave, Vivaldi, Opera, Yandex Browser and
  Chromium, built in.

## Download

| Platform | Builds |
|---|---|
| Windows | [arcomium-win releases](https://github.com/VizzleTF/arcomium-win/releases) |
| macOS (Apple Silicon) | [arcomium-mac releases](https://github.com/VizzleTF/arcomium-mac/releases) |
| iPhone | build from source: [arcomium-ios](https://github.com/VizzleTF/arcomium-ios) |
| Linux (x86_64, arm64) | [arcomium-linux releases](https://github.com/VizzleTF/arcomium-linux/releases) |

## Move from Arc

Open the Arcomium menu (the logo at the top of the sidebar; on macOS the app menu) and
pick **Import from Another Browser...**, or go to `chrome://arcomium-import`. The page
lists what it finds on this computer, with an **Import** button for each:

- **Arc** (Windows and macOS): every Space with its pinned tabs, nested folders, folder
  icons and cached favicons. The first Space goes to Home; each of the others becomes a
  Space in the Space bar.
- **Chrome, Edge, Brave, Vivaldi, Opera, Yandex Browser, Chromium** (Windows, macOS,
  Linux), each profile on its own: the bookmarks go to a folder named after the browser
  (and the profile, when there are several), pinned in the current Space.

Arcomium only reads the other browsers' files, so they can stay open. Importing a source
again replaces what it brought in before. For Firefox, Safari or an HTML bookmarks file,
the page links to the standard importer.

Fallback, for a profile Arcomium is not running on: close Arcomium and run
`python tools\arc_import.py` (Python 3, Windows). It writes the same Arc bookmarks
straight into the profile's `Bookmarks` file; each new Space you then create with "+"
takes the next imported Arc Space.

## Repositories

| Repository | Contents |
|---|---|
| arcomium (this one) | Patches, design notes, fallback Arc import script |
| [arcomium-win](https://github.com/VizzleTF/arcomium-win) | Windows build and releases |
| [arcomium-sync](https://github.com/VizzleTF/arcomium-sync) | Sync server |
| [arcomium-mac](https://github.com/VizzleTF/arcomium-mac) | macOS build on ungoogled-chromium-macos |
| [arcomium-linux](https://github.com/VizzleTF/arcomium-linux) | Linux build on ungoogled-chromium-portablelinux |
| [arcomium-ios](https://github.com/VizzleTF/arcomium-ios) | iOS build on Chromium iOS (`patches/series-ios`): sync and branding |

## Status

| Area | State |
|---|---|
| Spaces with separate logins | done |
| Sidebar, import from Arc, drag and drop | done |
| Sync | done with your own server: Windows, macOS and iOS in one chain; public server to do |
| macOS | builds and syncs; the same sidebar as on Windows |
| iPhone | builds and syncs in the simulator; Spaces and the sidebar not on iOS |
| Link routing to Spaces | done: routes by site at Link routing in the app menu |
| Media mini player, auto picture-in-picture | done |

## Privacy

Arcomium keeps ungoogled-chromium's privacy defaults, with these exceptions:

| Exception | What reaches Google | Off by |
|---|---|---|
| Google is the default search engine | Your searches, only when you search | Picking another engine in the menu of the sidebar's logo, Search Engine |
| Search suggestions can be turned on | What you type in the address bar | They are off unless you turn them on in settings |
| The Chrome Web Store installs and updates extensions | Installed extensions' IDs and versions, in update checks | Not installing extensions from the Web Store |

ungoogled-chromium rewrites Google domains in the source after the patches apply. The
Web Store's addresses in `extensions/web-store.patch` are written split
(`"clients2.goo" "gle.com"`) so that this rewriting leaves them working.

## Learn more

- [How Arcomium works](docs/design.md): Spaces, the sidebar, sync, platforms.
- [Writing and refreshing patches](docs/patching.md).

## License

BSD-3-Clause, as Chromium and ungoogled-chromium.

Arcomium is an independent project. It is not affiliated with, endorsed by or sponsored by
The Browser Company. Arc is a trademark of its owner and is named here only to say what
Arcomium imports from and who it is meant for; Arcomium uses none of Arc's code, artwork
or branding.
