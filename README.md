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
- **Sync on your terms** (in progress): a free public server or your own, end-to-end
  encrypted, with no account; see [arcomium-sync](https://github.com/VizzleTF/arcomium-sync).
- **Import from Arc**: Spaces, pinned tabs, nested folders and icons.

## Download

| Platform | Builds |
|---|---|
| Windows | [arcomium-win releases](https://github.com/VizzleTF/arcomium-win/releases) |
| macOS, Linux, iPhone | planned |

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
| arcomium-mac, arcomium-ios | planned |

## Status

| Area | State |
|---|---|
| Spaces with separate logins | done |
| Arc sidebar, Arc import | done; drag and drop to do |
| Sync | server and browser done; setup page to do |
| Link routing to Spaces | links from other apps open in the current Space; rules to do |
| Media mini player, auto picture-in-picture | to do |

## Learn more

- [How Arcomium works](docs/design.md): Spaces, the sidebar, sync, platforms.
- [Writing and refreshing patches](docs/patching.md).

## License

BSD-3-Clause, as Chromium and ungoogled-chromium. Arc is a trademark of The Browser
Company; Arcomium is not affiliated with it.
