# ADR-0001: A Chromium profile and a window for each Space

Status: accepted, 2026-11-11. Steps 1 to 4 and 6 to 8 of the [order of work](#order-of-work) are
built on the branch `space-profiles`; [design.md](../design.md) describes release 0.1.

## Context

A Space in Alerse 0.1 is a tab group of one window and one Chromium profile, and its
tabs live in a storage partition of their own. A partition separates only what content
stores for sites: cookies, localStorage, IndexedDB, service workers, the HTTP cache.
History, passwords, autofill, extensions, site permissions, settings and the favorites
belong to the profile, so every Space shares them.

The Space of a tab is not stored anywhere. It is derived from the partition the tab
happens to be in, from its tab group, or from the title of a bookmark folder. Every way
of creating a tab that Alerse did not patch therefore left the Space. On 2026-11-11
three such ways were found and closed: a `noopener` window (every `target=_blank`
link), a tab that started on a `chrome://` page, and a reopened tab. The first one
showed the home account behind a site's "Sign in" in the Work Space. Chromium creates
tabs in many more places, and each Chromium update can add one.

Arc, which Alerse follows, gives a Space a Chromium profile. On the machine this was
checked on, Arc's `home` uses `Default` and `Work` uses `Profile 1`; each has its own
`Cookies`, `History`, `Login Data`, `Web Data`, `Extensions`, `Preferences` and its own
favorites (`topAppsContainerIDs` in `StorableSidebar.json` maps a profile to a favorites
container). In Arc several Spaces may share a profile. The owner of Alerse decided that
here each Space is separate, always.

Chromium ties a window (`Browser`) to one profile. Sessions, the address bar, the
extensions toolbar, history, permission prompts and several thousand other call sites
under `chrome/browser/ui` read the window's profile, not the tab's.

## Decision

We will make each Space a Chromium profile, and show each Space in a `Browser` window
of its own profile. The windows of one Alerse window share one frame on screen, and
exactly one of them is visible: switching Spaces hides one and shows the next in the
same place. This keeps Chromium's rule of one profile for each window, so the isolation
of profiles, which Chromium maintains and tests, is the isolation of Spaces, and Alerse
patches nothing in content to get it.

## Design

### Terms

| Term | Meaning |
|---|---|
| Space | A Chromium profile with Alerse's attributes: id, name, color, icon, position |
| Home | The Space of the `Default` profile. It cannot be deleted |
| Stack | The windows of one Alerse window: one `Browser` for each open Space, one visible |
| Member | One `Browser` of a stack |
| Space registry | The list of Spaces, read from Chromium's `ProfileAttributesStorage` |

### Space registry

The registry is Chromium's `ProfileAttributesStorage`, kept in `Local State`
(`profile.info_cache`). A Space's name, icon and color belong to its own profile: its
home folder records them in the bookmark meta info `arcomium.space_icon` and
`arcomium.space_color`, as in 0.1, so they sync with the Space's bookmarks. The
profile's entry keeps a copy under `alerse_space_name`, `alerse_space_icon` and
`alerse_space_color`, and the Space's own window rewrites the copy when they change.
The Space bar of every window draws the other Spaces from the copies, without loading
their profiles. The entry's name follows the Space's name. The Space bar lists home
first and then the other Spaces in the order they were made.

"+" in the Space bar creates a profile with `ProfileManager::CreateMultiProfileAsync()`.
Delete Space runs `DeleteProfileHelper::MaybeScheduleProfileForDeletion()`, which
closes the profile's windows and removes its directory, so a deleted Space leaves no
data on disk. Chromium's profile picker, profile menu and "Add profile" are turned off:
the Space bar is the only way to manage profiles.

### Stack

`AlerseWindowStack` owns the members and is the only code that shows, hides, moves and
closes them. Its rules:

- One member is visible. The others are hidden with `BrowserWindow::Hide()`, not
  minimized, so the Dock, the taskbar, Cmd+` and Alt+Tab show one window.
- Bounds, the maximized and full-screen state and the display are the stack's. A change
  on the visible member is stored in the stack and applied to a member when it is shown.
- Closing the visible member closes the stack: every member closes, each saving its own
  session. Minimize acts on the visible member only.
- A member is created on first use of its Space in that stack, and the profile is
  loaded then. A `ScopedProfileKeepAlive` holds each Space's profile while a stack has
  a member for it.
- New Window opens a second stack that starts in the Space of the first. An incognito
  window is outside the stacks and belongs to the Space it was opened from.

Switching (the Space bar, Control+1 to 9, the Spaces menu, a swipe, a routed link)
shows the next member at the stack's bounds, with the sidebar as wide and as hidden
as in the previous one, then hides the previous one once the next has presented a frame, so that no frame
shows the desktop (docs/design.md, "Motion"). Each member has its own tint: there is
no color blend between Spaces.

### Swipe preview

A hidden window does not paint, so the neighbour that follows the fingers during a
swipe is a picture. When a swipe starts, the visible sidebar paints the rows of the
neighbour's hidden member off screen into a bitmap, so the picture is never stale and
nothing is kept between swipes. The swipe moves the visible rows and the picture
together; lifting past 60 pt switches members and the real window replaces the
picture. A Space that has no member in the stack yet shows only its name. The page
card does not move, as in 0.1.

### Sidebar

A member's sidebar shows its own profile only, so the code that filters by Space goes:

- Tabs: every tab of the member's tab strip. Tab groups in focus mode are no longer
  Spaces, and `GetSpaceOfContents()`, `GetSpaceOfGroup()` and
  `GetSpaceOfGroupOrFolder()` are deleted.
- Pinned tree: the Space's folder, one folder of the profile's Other Bookmarks with
  the meta info `arcomium.space = default`. That is the mark home's folder had in 0.1,
  so home's pinned items need no migration.
- Favorites: the profile's favorites folder, opened as Chromium pinned tabs of that
  member, in that Space's storage. Each Space has its own, as in Arc.
- Bindings of tabs to pinned items stay as they are (the item's uuid in the tab's
  session data); the check that a tab binds only inside its own Space goes.
- An empty Space keeps its hidden blank tab, so that its member stays open.

The Space bar reads the registry. The Spaces menu of the macOS menu bar, the logo menu
and the command bar's Space commands call the stack.

### What is separate and what is shared

Each Space has its own cookies and site data, history, address bar suggestions,
passwords, passkeys, autofill, extensions and their data, site permissions, Chromium
settings (search engine, languages, privacy), downloads list, bookmarks, pinned items,
favorites, sessions, recently closed tabs and theme color.

Shared by all Spaces, in `Local State` or in the browser process: the Space registry,
link routes, the update check, the sync account, and Alerse's own interface settings
(sidebar width, hidden sidebar, keyboard shortcuts). Media playing in a hidden member
keeps playing, and the mini player shows it, because audio focus is tracked for the
whole browser.

### Startup and sessions

Each profile keeps its own session, with Chromium's session service unchanged. At
startup Alerse opens the stack with the Space that was visible last and loads no other
profile. The other members are created when they are first shown or routed to
(decided on 2026-11-11), so a messenger in another Space starts only once that Space
is opened. A Space shown for the first time since startup opens on a blank tab, and
its session takes that tab's place when it has been read. Chromium's profile picker
never shows at startup (`spaces/start-in-last-space.patch`). The patches that carried
a Space through session restore, tab restore and tab discarding are deleted.

### Tabs between Spaces

Move to a Space and a route open the address again in the other profile, and the
stack switches to that Space. History of the tab and form state do not move, as in
0.1. A pinned item or folder is copied to the other profile's bookmark model and then
removed from its own, which closes the tabs bound to it; the Space shown does not
change. Turn folder into Space creates a Space, moves the folder's items there and
shows it.

### Link routing

A route names a Space by the directory of its profile (`Profile 1`; empty for home).
A link from another app opens in the member of that Space in the last active stack,
which creates the member if needed and switches to it. A route whose Space was deleted
does not apply. Routes move from the profile pref `arcomium.link_routes` to the
`Local State` pref `alerse.link_routes`.

### Sync

Each profile has its own `SyncService`, so each Space syncs as its own chain: the
server keeps Spaces as separate as the browser does and cannot tell that the chains
belong to one user. The user still signs in or enters the sync code once.

- The setup is the browser's: the server, the secret behind the sync code and the
  sign-in live in `Local State`, not in a profile. Setting sync up or leaving it on
  the Sync page of any Space does so for all of them.
- Home syncs as the chain of that secret. Every other Space has an id, 16 random
  bytes made with its profile and kept in the profile pref `alerse.sync.space`. Its
  chain secret is HKDF-SHA256 of the browser's secret with the info `space-chain:`
  and the id.
- Home's chain carries the list of Spaces: home's Space folder holds it as the
  bookmark meta info `arcomium.spaces`, a JSON object from id to the name, icon and
  color the Space had when it was listed. Home's window keeps the list and the
  device in line: it lists the Spaces the list lacks and makes a profile for each id
  the device lacks, and that profile joins the Space's chain. A Space's current
  name, icon and color come through its own chain.
- Deleting a Space marks its id deleted in the list, so the list does not bring the
  Space back on that device. Other devices keep their copy of the Space until it is
  deleted there.
- A Space is listed after its window was shown once on the device that made it,
  and the list is read and written only while home is loaded.

`SAVED_TAB_GROUP` no longer represents a Space. The server needs no change. Its
limits for one address count every Space's chain: with the defaults, more than ten
new Spaces a day from one address are refused (`NEW_CHAINS_PER_IP_PER_DAY`).

### Import

Arc: each Arc Space becomes a Space; its pinned tree and the favorites of its Arc
profile go to that Space's bookmarks. The first Arc Space goes to home. The profile
entry of a Space made by the import records Arc's id of the Space
(`alerse_arc_space`), and importing again replaces in that Space what the earlier
import brought. Other browsers: each source profile is imported
into the Space that is visible, as today.

### No migration from 0.1

Decided on 2026-11-11: 0.1 had no users besides the owner, so nothing is migrated. A
profile of 0.1 opens as home with home's pinned items, logins and tabs. The folders of
its other Spaces stay in Other Bookmarks as ordinary folders, and Chromium deletes
those Spaces' storage partitions, with their logins, when the profile loads.

### Platforms

macOS, Windows and Linux get the same stack. Hiding and showing differ: `orderOut` and
`orderFront` on macOS, where a full-screen member also has to hand over its full-screen
space; `SW_HIDE` and `SW_SHOWNA` on Windows; unmap and map on X11 and Wayland, where a
window cannot choose its position on Wayland and the compositor places the member that
is shown. iOS has no Spaces and does not change.

### Code that goes

Deleted in step 3: `spaces/space-storage-partitions.patch`,
`fixed-partition-blank-pages.patch`, `space-new-windows.patch`, `space-identity.patch`,
`space-group-identity.patch`, `space-sync.patch`, `synced-empty-space.patch`,
`lazy-saved-space-tabs.patch`, `open-space-quietly.patch`, most of `alerse_spaces.h`,
and the Space filtering in the sidebar and the Space bar.

`keep-old-space-data.patch`, which was left of `space-data-lifecycle.patch`, went
with the migration.

`enable-tab-group-focus.patch` and `no-focus-freezing.patch` stay: Chromium builds
the swipe controller the sidebar uses only with `kTabGroupsFocusing`.

## Features on the stack

Every feature Alerse has or plans from Arc, and what the stack does to it. "Same"
means the feature runs inside one member and its code does not change. Arc's archive,
Peek, Little Arc, Easels, Boosts, the AI features and Set Profile stay left out
([design.md](../design.md), Left out on purpose).

### Spaces

| Feature | On the stack |
|---|---|
| Space bar, Control+1 to 9, Next and Previous Space, the Spaces menu | Read the registry and call the stack. Every member draws the same bar and watches `ProfileAttributesStorage` for changes |
| New Space | Creates a profile, then its member, and switches to it |
| Rename Space, Change Space Icon, emoji, Edit Theme Color | Write the registry entry. The pickers and dialogs stay |
| Space color when switching | Each member has its own color; the rows slide, the color changes with the window |
| Delete Space | Deletes the profile and its data. Home cannot be deleted |
| A Space returns to the tab that was active in it | Free: each member keeps its own active tab |
| Empty Space | The member keeps a hidden blank tab, as today |
| Turn folder into Space | Creates a Space and moves the folder's subtree into its bookmarks |
| Move to a Space (tab, pinned item, folder) | A tab opens again in the other profile; an item or folder moves between the two bookmark models with its icon and name |
| The same Space in two windows | Two members of one profile, which Chromium supports. Today's gap ("looks closed in a second window") goes |
| Swipe between Spaces | Moves a snapshot of the neighbour (Swipe preview) |
| Incognito | A window outside the stacks, of the Space it was opened from, with no Space bar |

### Sidebar

| Feature | On the stack |
|---|---|
| Pinned items, nested folders, folder popup, collapse and expand all | Same, on the profile's own bookmarks |
| A tab bound to its pinned item, "/", click on the icon to return, Reset Tab | Same |
| Favorites | A set for each Space, opened in that Space's storage |
| Tabs list, close buttons, Clear Today, rename by double click | Same |
| Drag and drop inside the sidebar, drag to split | Same. Dragging to another Space is Move to a Space |
| Speaker and mute on a row | Same |
| Command+1 to 9 for sidebar items | Same |
| Icon pickers for items and folders | Same |
| Sidebar width, hidden and floating sidebar | State of the stack, kept in `Local State`, so every member shows the same sidebar and nothing jumps on a switch |
| Share, Copy Link, Duplicate, Edit Pinned Page | Same |

### Window, address bar and command bar

| Feature | On the stack |
|---|---|
| Position, size, maximized and full screen | State of the stack |
| The window named after its Space | The member's title |
| Address field, Copy Link, site controls, extension tiles | Same; the extensions are the Space's |
| Command bar | Same. Suggestions, history and open tabs come from the shown Space only, as with Arc's profiles |
| Command bar actions for Spaces (New Space, Rename Space, Edit Theme) | Call the stack and the registry |
| Search engine | A setting of each Space |
| macOS menu bar | The Spaces menu reads the registry; the Extensions menu lists the shown Space's; the Window menu and the Dock menu list only visible members |
| Logo menu (Windows, Linux) | Same; its Space list reads the registry |
| Settings, History, Downloads, Extensions pages | Pages of the shown Space |
| Sync, Link routing and Import pages | Show the browser's shared state from any Space |
| Update notice, default browser | The browser's, shared |
| DevTools, popup windows, picture-in-picture | Windows outside the stack that belong to a Space's profile and stay visible when its member is hidden |

### Split View

Same. Both panes are tabs of one member, so a split never mixes Spaces.

### Media

| Feature | On the stack |
|---|---|
| Sound in a Space that is not shown | Keeps playing: its tabs are background tabs of a hidden window, throttled as any background tab and never frozen |
| Mini player | Shows media of any Space, because the browser tracks audio focus for all profiles. A click switches to that Space and tab |
| Automatic picture-in-picture when leaving a playing video | The stack tells the member it hides, as leaving a tab does today |

### Data and privacy

| Feature | On the stack |
|---|---|
| Passwords, passkeys, autofill | Of each Space |
| Extensions | Installed for each Space, with their own data |
| Site permissions | Of each Space |
| Notifications and push | Of each Space, and they work, which they do not in a partition. A click on one switches to its Space |
| Downloads | A list for each Space. A download in a hidden member goes on; closing the stack with one running first shows that member with Chromium's question |
| Clear browsing data | Clears the shown Space only |
| Reopen Closed Tab, History | Of each Space |
| Memory Saver | Same; hidden members' tabs are unloaded as background tabs are |

### Things that need the user in a hidden member

A page dialog, a permission prompt, a sign-in prompt, "Restore pages?" or a crashed
tab in a hidden member cannot show. The Space's icon in the Space bar gets a dot, and
the prompt shows when the Space is opened. Closing the stack switches to a member that
must ask first (unsaved form, running download).

### Outside the window

| Feature | On the stack |
|---|---|
| Link routing | A route names a Space; the link opens in its member and the stack switches. A link with no route opens in the Space shown last |
| Import from Arc | Each Arc Space becomes a Space. Arc Spaces that shared an Arc profile become separate Spaces, each with a copy of that profile's favorites. Logins are not imported |
| Import from other browsers | Into the shown Space |
| Sync | A chain for each Space, one sign-in (Sync) |
| Profile takeover from the earlier name | Unchanged |
| iOS | No Spaces; unchanged |

## Consequences

Spaces become as separate as Chromium profiles, with no patch in content, and the
class of bug found on 2026-11-11 cannot return: a tab cannot be in the wrong profile of
its window. Deleting a Space deletes its data. A Chromium update can no longer break
isolation; it can only break the stack.

The costs are real. A password or an extension is needed in every Space that uses it,
and settings are made for each Space. Every loaded Space is a loaded profile with its
own services, which costs memory beyond the renderers, which 0.1 already did not
share. The stack is new window management on three platforms, and its faults will be
visible ones: a flash when switching, a window in the wrong place, a hidden window
that reappears. The swipe shows a picture of the neighbour, not live content. Sync
changes on the client and on the server, and devices on 0.1 cannot share a chain with
devices after the change. The other Spaces of a 0.1 profile lose their logins.

## Alternatives considered

- Keep one profile with a partition for each Space, and separate favorites, history
  and passwords by Space one store at a time. Rejected: each store is a patch to a
  Chromium database and its sync type, extensions and settings stay shared, and the
  Space of a tab stays derived, which is the cause of the leaks.
- Tabs of several profiles in one `Browser`, as Arc shows them. Rejected: Arc has its
  own shell over content, while Alerse uses Chromium's, where the window's profile is
  read in thousands of places. Each one left unpatched is a leak between Spaces, and
  the patch set would have to be redone at every Chromium update.
- One browser process for each Space. Rejected: separate user data directories give
  separate apps in the Dock and no shared window, Space bar or routing.

## Order of work

1. Prototype on macOS: a stack of two profiles, switching by a key, measured for
   flashes and for the time to switch. The decision stands or falls here, so nothing
   else starts first.
   Done on 2026-11-11 on Chromium 156 behind `--alerse-stack-proto`: the first switch
   to a Space, which loads the profile and makes the window, took 98 ms, later
   switches 15 to 18 ms, and a 55 frames per second screen recording of four switches
   has no frame that shows the desktop. Not measured: a full-screen member.
2. Space registry, create and delete, the Space bar on the registry.
   Done on 2026-11-11. After a restart each profile's window opens as a stack of its
   own; step 4 stacks them again.
3. Sidebar on one profile; delete the partition patches.
   Done on 2026-11-11, with these gaps: a tab's binding to its pinned item after a restart, and Windows and Linux were not
   tested.
4. Startup, sessions, the empty Space, New Window, incognito.
   Done on 2026-11-11. A new Space and a Space with no tabs show an empty card with
   the command bar closed; New Window opens a second stack with the command bar open;
   the incognito window's Space bar lists no Spaces. The sidebar's width
   and hidden state follow the stack since 2026-11-11 (`shared-sidebar-width.patch`).
   The member shown next takes the shown one's full screen state before it shows.
   On macOS each full screen member is a desktop of its own, so a switch in full
   screen slides between desktops. Not built: the dot for a prompt in a hidden
   member.
5. Migration from 0.1. Dropped on 2026-11-11 (No migration from 0.1).
6. Link routing, import, the command bar and menu bar.
   Done on 2026-11-11 for link routing (in step 3), the Spaces menu of the menu bar
   and the import from Arc. Not tested: the command bar's Space commands.
7. Swipe preview.
   Done on 2026-11-11 on macOS.
8. Sync: derived chains, the registry in Home's chain, the server.
   Done on 2026-11-11 on the client; the server did not change. Not tested: the
   sign-in with a login and pairing, which now carry the browser's secret; iOS,
   which has no Spaces and keeps the setup in its profile.
9. Windows and Linux.

## Open questions

- Is the switch free of a visible flash on macOS with a full-screen member, and on
  Wayland? Step 1 answers the first, step 9 the second.
- An "Install in every Space" action for extensions is likely to be asked for. It is
  not part of this decision.
