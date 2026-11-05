# Arcomium

An Arc-style browser built on [ungoogled-chromium](https://github.com/ungoogled-software/ungoogled-chromium):
Arc's sidebar, Spaces with separate logins in one window, and sync you can host yourself.

This repository holds what all platforms share: the Chromium patches, the design notes
and the Arc import tool. Builds come from the platform repositories below.

![Arcomium: the Personal Space with pinned items, nested folders and open tabs](docs/images/sidebar.png)

## Features

- **Arc's sidebar**: Space name, pinned items in nested folders, open tabs under
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
- **Import from Arc**: Spaces, pinned tabs, nested folders and icons.

## Download

| Platform | Builds |
|---|---|
| Windows | [arcomium-win releases](https://github.com/VizzleTF/arcomium-win/releases) |
| macOS (Apple Silicon) | [arcomium-mac releases](https://github.com/VizzleTF/arcomium-mac/releases) |
| iPhone | build from source: [arcomium-ios](https://github.com/VizzleTF/arcomium-ios) |
| Linux | planned |

## Move from Arc

Close Arcomium and Arc, then run (Python 3, Windows):

```cmd
python tools\arc_import.py
```

The first Arc Space appears outside of Spaces. Each new Space you create with "+" takes
the next imported Arc Space with its pinned items. Running the import again replaces what
it imported before.

## Repositories

| Repository | Contents |
|---|---|
| arcomium (this one) | Patches, design notes, Arc import |
| [arcomium-win](https://github.com/VizzleTF/arcomium-win) | Windows build and releases |
| [arcomium-sync](https://github.com/VizzleTF/arcomium-sync) | Sync server |
| [arcomium-mac](https://github.com/VizzleTF/arcomium-mac) | macOS build on ungoogled-chromium-macos |
| [arcomium-ios](https://github.com/VizzleTF/arcomium-ios) | iOS build on Chromium iOS (`patches/series-ios`): sync and branding |

## Status

| Area | State |
|---|---|
| Spaces with separate logins | done |
| Arc sidebar, Arc import, drag and drop | done |
| Sync | done with your own server: Windows, macOS and iOS in one chain; public server to do |
| macOS | builds and syncs; Arc sidebar as on Windows |
| iPhone | builds and syncs in the simulator; Spaces and the Arc sidebar not on iOS |
| Link routing to Spaces | done: routes by site at Link routing in the app menu |
| Media mini player, auto picture-in-picture | done |

## Privacy

Arcomium keeps ungoogled-chromium's privacy defaults, with these exceptions:

| Exception | What reaches Google | Off by |
|---|---|---|
| Google is the default search engine | Your searches, only when you search | Picking another engine in the Arc logo menu, Search Engine |
| Search suggestions can be turned on | What you type in the address bar | They are off unless you turn them on in settings |
| The Chrome Web Store installs and updates extensions | Installed extensions' IDs and versions, in update checks | Not installing extensions from the Web Store |

ungoogled-chromium rewrites Google domains in the source after the patches apply. The
Web Store's addresses in `extensions/web-store.patch` are written split
(`"clients2.goo" "gle.com"`) so that this rewriting leaves them working.

## Learn more

- [How Arcomium works](docs/design.md): Spaces, the sidebar, sync, platforms.
- [Writing and refreshing patches](docs/patching.md).

## License

BSD-3-Clause, as Chromium and ungoogled-chromium. Arc is a trademark of The Browser
Company; Arcomium is not affiliated with it.
