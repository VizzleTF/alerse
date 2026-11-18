# Features and their status

What Alerse has, what was checked on screen, and what is not built. True for
**0.1.20** (Windows on Chromium 157, macOS on 156), as of 2026-11-16. How each feature
works is in [design.md](design.md); this page only says where it stands.

## Status words

| Word | Meaning |
|---|---|
| **checked** | Seen working in a running build (by hand, the test kit or a test), with the version where known |
| **built** | In the release, compiles and starts, but no check of the feature itself is on record |
| **partly** | Checked, except for the parts named in Notes |
| **no** | Not on that platform |

Linux is left out of the tables: its latest release is 0.1.1 on Chromium 156, without
the features below that came after it ([Platforms](design.md#platforms)).

## Spaces and the sidebar

| Feature | Windows | macOS | Notes |
|---|---|---|---|
| Spaces, each its own profile (ADR-0001) | checked 0.1.12 | checked | Spaces carried over from 0.1.11 on the PC |
| Switch Spaces: Ctrl+1..9, Space bar, swipe | checked | checked | Swipe on Windows not checked; one unreproduced report of a swipe landing back (backlog) |
| Space color, icon and theme picker | built | checked | |
| Favorites, pinned tree, folders, folder popup | partly | checked | Windows: pinned items checked in 0.1.13, the rest not recorded |
| Same favorites in every Space, with a switch | partly 0.1.19 | checked 0.1.19 | Both: a Space opened later takes them, a removal reaches the other Space. Windows: an add, rename, move and removal reach the other Space either way and survive a restart; the switch checked on macOS only (the menu needs the real cursor on Windows) |
| Drag and drop in the sidebar | built | checked | Dragging a favorite out: not checked |
| Drop a row on a Space's icon to move it | built | checked | |
| Picking several tabs, copying their links | built | checked | Shift+click, Cmd+click and Copy URL of every picked tab |
| Clear (close the Space's unpinned tabs) | built | checked | |
| Hidden sidebar and floating sidebar on hover | partly | checked | Windows: hiding checked; the floating sidebar follows the real cursor and was not seen |
| Sidebar slides as a whole when hidden or shown | built | checked | About 250 ms each way, 60 fps recording |
| Overlay scroll bar in the sidebar | built | checked | The Space's name row stays put |
| Row marks: loading, attention dot, sound, camera, dimmed when discarded | built | checked | |
| Keyboard focus ring and arrow keys in rows | built | built | Not seen: needs macOS keyboard navigation on |
| Mini player across the Space's windows | built | checked | |
| Picture-in-picture on leaving a playing video's tab or Space | built | partly | Windows: Space switch opens it and switching back closes it (2026-11-17); macOS built, not seen (screen locked) |
| Motion: Space entrance, folder curtain, row in and out | built | checked | 60 fps recordings on macOS; Windows not recorded |
| Tint cross-fade between Spaces | no | no | Removed on purpose; the tint applies at once |

## Window, address and command bar

| Feature | Windows | macOS | Notes |
|---|---|---|---|
| Arc for Windows top bar and logo menu | checked | no | macOS has the menu bar instead |
| macOS menu bar and window look | no | checked | |
| Compact address field, click selects the address | built | checked | |
| Command bar (Ctrl/Cmd+T, Ctrl/Cmd+L) and its actions | checked | checked | Space commands in the bar: not checked |
| Find bar as Arc's pill | checked 0.1.16 | checked | Windows put it under the address field until 0.1.17; now the card's corner, as on macOS |
| Site controls popover | partly | partly | Opens with its tiles, Extensions and Not Secure; translate, password and permission rows not seen |
| Split View | checked 0.1.13 | checked | |
| Icon pickers for pinned items, folders, Spaces | built | checked | |
| Link routing to a Space | partly | checked | macOS: a routed link from another app opens as a tab of its Space |
| No Google by default: DuckDuckGo, Web Store switch | partly 0.1.20 | checked 0.1.20 | Search picked in the welcome page goes to every Space, also one made later; Use Chrome Web Store in the Extensions menu. Windows: the welcome page's search and store picks reach both Spaces; DuckDuckGo default and the menu switch checked on macOS only |
| Extensions in Incognito, by choice | checked | checked | Allow in Incognito in an extension's menu in the site controls (0.1.17) and in chrome://extensions; takes effect at once |
| Keyboard shortcuts of your own | checked | partly | Windows: chrome://alerse-shortcuts, a changed key works at once in every window; macOS lists the menu keys and opens System Settings' App Shortcuts |

## Boosts, Peek and the Little window

| Feature | Windows | macOS | Notes |
|---|---|---|---|
| Boosts: color, dark, font, size, zap, own CSS | checked 0.1.16 | checked 0.1.16 | Font buttons fixed in 0.1.17; Dark leaves the body's box visible on a black canvas |
| Boosts on/off switch, chrome://alerse-boost list | checked 0.1.16 | partly | The panel stayed over another tab until 0.1.17; now it closes (checked on both) |
| Peek (Shift+click, links from pinned tabs) | checked 0.1.16 | checked | Shift+click, a pinned tab's link and Open as Tab checked on Windows; the dimmed page around it seen on macOS |
| Little window for links from other apps | checked 0.1.16 | checked 0.1.16 | Windows: the Space's window stays behind (0.1.16); macOS raises it, as Arc does. A browser started with several addresses put all but the first in Little windows until 0.1.17 |
| Open in <Space>, arrow to other Spaces, Ctrl/Cmd+O, Ctrl/Cmd+Alt+O | partly | checked 0.1.16 | Windows: button and Ctrl+O checked; the arrow's menu not checked |

## Sync, import, updates

| Feature | Windows | macOS | Notes |
|---|---|---|---|
| Sync account: 24 words and a password | checked | built | Windows: an account made on a local server, a second device signed in with the words and the password. macOS has the same account code and Sync page; no Mac has signed in yet (2026-11-18) |
| Sync with the official or own server | partly | built | Own server checked on Windows: a bookmark reached the other device in about a minute; the official server not tried |
| Import from Arc, Chrome and others | checked | checked | Spaces, pinned tabs, folders, icons, extensions, history and passwords (Windows decrypted Arc's 2 passwords, 59,647 pages); in the welcome page too |
| Update notice | built | checked | macOS 0.1.16 offered 0.1.17 five minutes after start, and the item opened its release page |
| Beta Updates in the Developer menu | built | no | macOS is always on pre-releases |
| Welcome page at first start | checked | checked | Every step, with the Arc import; Start browsing closes the tab |
| Own install identity and profile folder | checked | checked | |

## Not built yet

| What | Why |
|---|---|
| Code signing | Owner's decision: Windows SmartScreen warns; macOS is signed with the build Mac's own identity |
| Updater that installs by itself | Only a notice; the user downloads the release |
| Crash dumps and debug symbols in releases | Crashes are known only by symptoms |
| Browser tests in CI | CI builds and packages; nothing runs the browser |
| Sync: email accounts, pairing by code, password change, key rotation | Later steps of ADR-0002 |
| Share in sidebar menus on Windows | macOS has it through the system |
| Translations of Alerse's own text | English only; sync error messages are hard-coded |
| Linux on Chromium 157 | Waits for ungoogled-chromium-portablelinux 157 |
| macOS on Chromium 157 | Waits for ungoogled-chromium-macos 157 |

## Left out on purpose

Tab archive (Arc's auto-archive and Library archive) and AI features: the owner's
decision, see [Left out on purpose](design.md#left-out-on-purpose).

## Measured against Chrome

Windows, 0.1.13–0.1.14 builds, 2026-11-15:

| Test | Alerse | Chrome |
|---|---|---|
| Speedometer 3, headless, two runs | 21.4, 21.3 | 20.0, 20.0 |
| Memory, 5 sites, private bytes | 699, 705 MB | 619, 719 MB |
