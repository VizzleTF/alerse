# How Arcomium works

How each Arcomium feature maps onto Chromium, and why it is built that way.

Goals: Spaces with separate accounts in one window, a sidebar for people moving from Arc,
a command bar, Split View, real-time sync across Windows, macOS, Linux and iPhone, link routing to
Spaces, media controls and picture-in-picture. Left out by the user's choice: Arc's tab
archive, Peek and Little Arc (see [Left out on purpose](#left-out-on-purpose)).

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
  `--close-window-with-last-tab=always` turns this off. Closing through Chromium's
  close commands (Ctrl+W / Cmd+W, Close Other Tabs) keeps the Space before Chromium
  counts the groups to delete, so its "Close Tab and Delete Group?" dialog never
  shows for a Space's last tab. The empty page takes no focus
  and opens no command bar until asked. It shows the page card's blank color rather
  than white or the dark canvas (#121212) Blink gives a user's `about:blank` in dark
  mode: Blink keeps the page marked `#arcomium-space` clear (`local_frame_view.cc`), so
  the card looks empty, as Arc's does on both platforms. It opens no context menu
  either, as Arc's empty card. A Space whose tab group is gone (all its tabs
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
  removes the saved group and the folder; dimmed for Home, which cannot go, as Arc's
  menu always has it). Alt+1 to Alt+9 show the Spaces in the bar's order, Home first;
  on macOS, where Option+digit types characters, Control+1 to Control+9, as in Arc
  for macOS, whose tooltips name them ("home ^1"). A horizontal
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
- Favorites: Arc's tiles above the Space's name, the same in every Space. They are
  the bookmarks of a folder beside the Spaces' (meta info `arcomium.favorites = 1`,
  created by the first Add to Favorites), so they sync like pinned tabs. Tiles are 47
  high, 8 apart, up to four in a row filling the sidebar's width (two are 70 pt wide on
  macOS, as in Arc), a shade off the frame (#2F2F2F on #161616), lighter on hover
  (#383839) and while their tab shows; the site's icon sits in the middle. A favorite
  opens as a Chromium pinned tab outside the Spaces, in their default storage, bound to
  it like a pinned item; the binding survives restarts as a pinned item's does. Add to
  Favorites (tab and pinned-item menus) and dragging a row onto the tiles, or onto the
  strip under the address field while there are none, make one; a Space's tab opens
  anew outside its Space for it, as its storage stays in the Space. Tiles reorder by
  dragging; Remove Favorite leaves the tab open as an ordinary tab of home. Measured on
  Arc for macOS only; on Windows they use the same numbers.
- Space name (Segoe UI Variable Small, 12 px, #848484).
- Pinned tree: the Space's bookmark folder (meta info `arcomium.space = <Space id>`,
  `default` outside of Spaces, `arc:<id>` for an imported Space not yet adopted by an
  Arcomium Space of the same name). Rows are 43.3 px apart, icon at x = 16, text at
  x = 44 in Segoe UI Variable Text 14 px (semilight items, semibold folders).
  Folder glyphs follow Arc: a folder holding open tabs shows two dashes, otherwise its
  Arc icon (`arcomium.folder_icon`). Open state is the `arcomium.open` meta info.
- A pinned item opens as a tab bound to it; a double click renames it, as in Arc,
  also on an item whose tab is not open yet: its first click opens the tab and rebuilds
  the rows, so the sidebar counts the second press on the same item within the system's
  double-click time itself (views drops the double click on a new row).
  Bound tabs are not listed with the other tabs: they show on their item, and under a collapsed folder its bound tabs are listed
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
  bubble, not a menu, so the mouse stays with the sidebar: it opens after 200 ms (400 ms
  on macOS), fades in over 120 ms, follows the pointer to another folder and closes
  when the pointer leaves. It opens inactive; on Windows it may activate on a click, which Windows
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
  keeps its tab; tabs reorder within their Space; a tab or a pinned item dropped onto
  the middle half of a tab's row makes a split of the two (see Split View), while the
  row's top and bottom quarters still reorder. A line, a highlighted folder or a
  highlighted tab row shows where the row lands. The sidebar does not rebuild during
  a drag. A dragged row carries its page's address (a folder only a marker text), so
  other apps take it as a link and Chromium's split zone on the page's edges takes it;
  the page itself takes no drop from a sidebar row (`arcomium_sidebar_drag.h`), which
  would load that address in it or paste it.
- Context menus follow Arc's, in its order and words (Share only on macOS, see On
  macOS). Pinned
  items, favorites and folders get Change Icon... (Arc's picker, see
  [Icon pickers](#icon-pickers)). Pinned items and tabs get Copy Link, Rename, Mute,
  Duplicate, Move to a Space (a pinned item or folder
  moves to that Space's pinned folder, a tab reopens in that Space's partition; a Space
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
- Arc's Space menu opens wherever Arc has it: on the Space's name, New Tab, the
  address field (on Windows and Linux only around its text, which keeps the text
  field's menu), the empty space below the rows, a Space's button and "+" in the Space
  bar, and on macOS the header's "...". It holds Change Space Icon... (Arc's picker,
  see [Icon pickers](#icon-pickers)), Rename Space (the Space's folder's name, which
  its tab group takes on save; Chromium's bookmark editor opens on the folder titled
  "Edit Space name", "Edit Space Name" in macOS' title case, instead of "Edit folder
  name": `EditDetails::arcomium_title_id`, `look/space-rename-title.patch`, set only
  by `ArcomiumSpaceSwitcher::RenameMenuSpace`, which the Space menu, the command
  bar's Rename Space and the macOS menu bar all run; renaming a folder and New
  Folder keep Chromium's titles), Edit Theme Color, New Folder, Collapse All Folders
  (while any folder of the Space's pinned tree is open; else Expand All Folders,
  dimmed when the Space has no folders) and Delete Space. Collapse / Expand All
  Folders writes every folder's open state (`arcomium.open`) at once, nested ones
  too, so the rows change without the 150 ms animation of a single folder; the
  open state syncs like any other meta info. Arc's Set Profile, Live Folders, Share Space, Export and
  Manage Spaces are left out: Spaces are already separate partitions, and the rest
  needs Arc's account and services. The "+"
  button's click offers a new Space, folder or tab.
- Rebuilds: rows exist only for the active Space. Changes to tabs of hidden Spaces that
  are not bound to a pinned item are ignored, theme changes rebuild once they settle
  (250 ms), and the Space bar rebuilds only when a saved Space's title, color, open
  state or emptiness changes, not on every navigation.
- When the sidebar is collapsed, Chromium's own vertical tab list shows, with its expand
  button (Windows, Linux; macOS floats Arc's sidebar instead, see On macOS).
- The page is a card with 10 px corners next to the sidebar; the Space bar sits at the
  bottom.
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

Arc for macOS is laid out differently from Arc for Windows, so these numbers, measured
on Arc for macOS (points, Retina), apply there only (`#if BUILDFLAG(IS_MAC)`; the
Chromium side of the header row, address field, command bar panel and Space bar is in
`look/mac-arc-look.patch`); Windows and Linux keep Arc for Windows':

- Sidebar 164 pt wide (198 px elsewhere) on a #161616 frame, without the edge line
  Chromium draws along it. A window saved at 198, the width macOS builds had before,
  opens at 164 (`vertical_tab_strip_state_controller_impl.cc`); other widths stay.
- Header: one row centred 24 pt down: the window controls (the title bar is 48 pt high
  for that), then back, forward and "..." centred at x = 88, 116, 144. "..." opens
  Arc's popover with Reload (Cmd+R, dimmed on an empty Space) and Toggle Sidebar
  (Cmd+S), and turns into a #DCDCDC cross while it is open; hovered it has a
  27 pt #262626 plate (#2A2A2A while open), and no plate once the pointer is off it
  or in the popover, as Arc's. The button paints to its own layer, as its plates reach
  past the header's bottom. Site Controls are reached from the address field's
  button and Downloads from the Window menu (Shift+Cmd+J). Pinned extensions do not
  show in the header.
- Popovers (`arcomium_popover.cc`), Arc for macOS' own menus rather than the system's:
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
  `ArcomiumPopover`, `arcomium_window_mac.mm`, and the controls' view checks the
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
  cross meanwhile, on its plate only while the pointer is on it. Arc's New Easel and New Boost rows are left out with those features. Windows keeps the plain menu: Arc for
  Windows' "+" has not been measured.
- Icon picker (Change Space Icon..., Change Icon...): see [Icon pickers](#icon-pickers);
  its measurements are Arc for macOS'.
- Address field 148x36 pt at x = 8, y = 46, with 12 pt corners, a shade lighter than
  the frame (#2F2F2F on #161616); "Search..." is a point smaller than the address, in
  #6F6F6F, after a magnifier at x = 20. While the command bar has the location bar,
  the field stays drawn with what it showed, as Arc's does. The held location bar takes
  no mouse events until edited: a click opens the command bar and a right click Arc's
  Space menu, not the text field's menu. Windows and Linux keep the location bar's own
  events: its site icon and "Not secure" chip, shown on hover, stay clickable there.
- Rows 41 pt apart; the highlight is 148x36 with 12 pt corners, 10% white on hover and
  20% for the active row; favicons at x = 18, folders 19 pt, nested rows 16 pt in, their
  highlight too (it starts where the row does, as Arc's). Tab
  and folder text is #DADADA and ends 16 pt before the highlight's edge, where Arc
  elides it; New Tab's "+" is #8B8B8B and its text #7F7F7F. Every pinned tab shows
  Arc's cross on hover (11 pt, #A9A9A9), also one with no tab open, where it does
  nothing. No divider line before New Tab (the view stays for drag and drop). While the
  command bar is open for a new tab, New Tab is the active row, as in Arc.
- The Space's name is a row of its own: the Space's icon, 15 pt in the active white,
  centred at x = 26 and the name in 13 pt semibold #808080. Hovered, it gets a 148x36
  plate (#2F2F2F), a chevron in place of the icon that collapses the Space's pinned
  tabs under the name (only open ones stay, as in a folder; meta info
  `arcomium.collapsed` on the Space's folder, read on macOS only) and "..." at its end
  for the Space's menu; under the pointer "..." takes a 24 pt plate of its own
  (#3F3F3F on the row's #2F2F2F), as Arc's. While the Space's icon picker is open
  the name is not lit for it and shows no tooltip; the pointer still lights it, but
  with the Space's icon rather than the chevron, and "..." keeps its plate while the
  pointer stays on it (where the menu's item was chosen), as Arc's; macOS sends the
  window no mouse moves while the picker has the keyboard, so the name polls the
  pointer meanwhile (every 50 ms).
- The page card keeps 10 pt from the window's top, right and bottom edges and touches
  the sidebar, with 6 pt corners, no rim and a faint shadow; its blank color is
  #2B2B2B.
- Folders are Arc for macOS' outlined folder, 18.5 pt: one 1.5 pt outline whose tab
  slants down into the body, grey under the tab, a #F0F0F0 front edge, filled near
  black (#050505) below it. An open folder shows Arc's open folder, 22.5 pt wide: a
  grey back leaning left behind a slanted front. The folder's row shows no tooltip.
- Folder popup, as Arc for macOS' (Windows keeps Arc for Windows' plain list): it opens
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
- Space bar: Space buttons 34x32 pt side by side and centred, a faint plate as wide on
  hover (34x32 pt from 859 pt down in a 900 pt window, as Arc's, measured at the
  plate's middle). A button's plate goes once its context menu closes: the
  menu took the pointer's events, so the button never saw it leave (the Space
  bar's `MenuClosed` repaints them);
  the archive and "+" on 32x32 pt plates 8 pt from the sidebar's sides in Arc's
  #A0A0A0, which stays under the pointer; Space icons 13 pt; Spaces other than the
  shown one in #3F3F3F, as dim as Arc's. The plates end 9 pt above the window's bottom
  (`kVerticalTabStripUncollapsedVerticalPadding`, 12 elsewhere). The archive is Arc's
  Library glyph (a box with a paper, a ball and a cone spilling out, its lid apart from
  the box by 1 pt) and "+" its 13.5 pt cross, 1.7 pt thick. The book Space icon is
  Arc's filled open book, two pages at a spine gap, not the outlined one. "+" has no
  tooltip, as Arc's.
- While the rows overflow, a 1 pt line, barely off the frame (#212121 on
  #161616), shows at the edge they scroll under, as Arc's; Chromium's scroll view would
  draw a bright separator.
- While the command bar is open for a new tab, New Tab's "+" is #DADADA like its text.
  The Space name's chevron is 11 pt wide with a 1.9 pt stroke.
- Context menus as Arc for macOS': a pinned tab's and a favorite's have Change Icon...
  before Rename, the favorite's also Edit Pinned Page and Move to; a folder's has
  Change Icon... before Rename. Tabs, pinned items and favorites have Arc's Share
  after Copy Link: a submenu of the system's sharing services for the page's link,
  with their icons (AirDrop, Mail, Messages, Notes, Freeform, Reminders and the
  like, Chromium's File > Share list without Safari's Reading List; Mail goes
  through a mailto: link, as in Chromium, whose Mail service crashed it), listed
  once a minute at most since the system reads them from disk. A folder's menu
  starts with Arc's Share Folder...: the system's share picker, beside the folder's
  row, for all the links of the folder and its subfolders (`arcomium_share_mac.mm`). The Space menu is Change Space
  Icon..., Rename Space..., Edit Theme Color... | New Folder, Collapse / Expand All
  Folders | Delete Space, Arc's grouping without its Set Profile, Live
  Folders, Share, Export and Manage Spaces (account and services). Delete Space stays
  dimmed for home, which cannot be deleted.
- Hovered, the Space's name brightens to #8D8D8D, and New Tab's "+" to #959595 and its
  text to #8B8B8B, as Arc's; "..." on the name is 18 pt from the plate's end.
- Back and forward show no plate while dimmed, as Arc's; their arrows are Arc for
  macOS' own (`arcomium_mac_back.icon`, `arcomium_mac_forward.icon`): 13 pt long with
  a 10.5 pt head in 1.5 pt strokes (the tip half a point in, as the round join
  reaches past it: they had drawn 13.5 pt); dimmed, white at 25% over the frame (#525252,
  #6A6A6A in an inactive window), and "..."'s dots at 60% (#A1A1A1, #B0B0B0), as Arc's
  (Chromium's arrows were 13 pt high with 2 pt strokes, in its brighter disabled grey; `look/mac-arc-arrows.patch`). The address field lightens to #39393A under the pointer; "Search..." is medium
  weight.
- No line along the window's top edge: the top container's separator, which Windows
  already drops, would draw a light line over the card there.
- Menu bar (`look/mac-menu-bar.patch`, `look/mac-menu-bar-arc.patch`): Arc for macOS'
  menus in its order, with its items and keys wherever Arcomium has the feature:
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
    (Option+Shift+Cmd+C, "[Title](url)", `IDC_ARCOMIUM_COPY_URL_MARKDOWN`,
    `look/command-bar-actions.patch`) and Copy URL as Quote (Control+Shift+Cmd+C,
    dimmed without a selection: the selected text as a Markdown quote, a blank line,
    then "[Title](url)"; `IDC_ARCOMIUM_COPY_URL_QUOTE`, done in
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
    Chromium's Always Show Toolbar in Full Screen is not there: Arcomium for
    macOS has no toolbar. Chromium's bookmark bar, toolbar and tab strip
    switches are not there; turning vertical tabs off would remove the sidebar.
  - **Spaces**: New Space..., Edit Theme... and Rename Space (Arc's ellipses: none
    on Rename Space, which edits the name in place) for the shown Space (Edit
    Theme's picker opens over the Space bar, as there is no menu to open it at), the
    next and previous Space (Option+Cmd+Right and Left, `IDC_ARCOMIUM_*_SPACE`, run by
    the Space bar through `arcomium_space_command_hook.h`), then, as in Arc, every
    Space of the Space bar in its order with its icon, about 12.5 pt as Arc's (a
    glyph drawn as a template image, so the menu tints it, or for a Space with a
    theme in the theme's light shade, as Arc's light blue Work book; an emoji in
    its colors), the shown one checked, and
    Control+1 to 9 on the first nine. The list is filled each time the menu opens
    (`ArcomiumMenuListDelegate` in `main_menu_builder.mm`, answered by
    `ArcomiumSpaceSwitcher::GetMenuBarList` through `arcomium_menu_bar_hook.h`).
  - **Tabs**: Arc's items only: New Tab..., Pin Tab (Cmd+D, which unpins a pinned tab
    and then reads Unpin Tab, as Arc's Pin/Unpin Tab, and which Chromium gives to
    bookmarking: here pinning is bookmarking), New Folder..., the next and previous tab
    (Option+Cmd+Down and Up), Reveal Tab in Sidebar (scrolls the sidebar to the shown
    tab's row; dimmed for a favorite's tab, which has none) and Clear Today
    (Shift+Cmd+K: the shown Space's tabs that are not pinned close; Arc archives
    them, Arcomium has no archive), run by the sidebar through the same hook.
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
    Space bar through `arcomium_menu_bar_hook.h`; it is renamed on every Space
    switch or rename). An incognito window keeps the page's title, and so does
    the window's accessible name (VoiceOver), which Chromium takes from the page
    (`GetAccessibleWindowTitle`; Arc's reads "home" there too).
  - The items AppKit adds to an Edit menu (Start Dictation..., Emoji &
    Symbols, AutoFill) are not there, as in Arc: the menu builder registers
    `NSDisabledDictationMenuItem`, `NSDisabledCharacterPaletteMenuItem` and
    turns AutoFill's `NSAutoFillOrSystemInsertMenuEnabled` and
    `NSAutoFillSystemInsertMenuEnabled` off (registration defaults only, for this
    app; nothing is written to the user's defaults).
  - **Help**: Chromium's help page and Arc's Troubleshooting > Open Task Manager.
  - All of it is macOS only (`main_menu_builder.mm` and `browser_native_widget_mac.mm`
    are macOS files; the hook code in the Space bar and the sidebar is under
    `BUILDFLAG(IS_MAC)`): Windows and Linux keep the logo menu, as Arc for Windows
    has no menu bar, and Ctrl+1 to 9 stay Chromium's there.
  - **Cmd+1 to 9**, as Arc's Go to Sidebar Item 1 to 8 and Go to last Sidebar Item:
    the sidebar's pages as drawn, favorites first, then the Space's pinned items and
    its tabs (a split's halves one by one); folders, the Space's name and New Tab do
    not count. Chromium's Select Tab 1 to 8 would count every Space's tabs in the tab
    strip. In no menu, as in Arc; without a sidebar (a popup window) Chromium's tabs.
  Bookmarks stays in the menu bar hidden, for the code that fills it by tag. Profiles
  stays hidden as well: its controller needs the menu (without it, rebuilding the
  menu never ends and startup hangs), so it is filled for the Dock menu and never
  shown (`profile_menu_controller.mm`); Arc has no Profiles menu.
- Tooltips are Arc's black pills (`arcomium_tooltip.cc`), not the system's: 29 pt
  high with round ends (the corner at the pointer 9.5 pt, as Arc's), #020202 inside a
  #1B1B1B and a #3B3B3C hairline, the name in 12 pt medium #DADADA 11 pt in, the
  shortcut as #CDCDCD keycaps (18 by 18.25 pt with a 2.5 pt #777777 lip at the bottom,
  4 pt apart, 6 pt after the name and 11.5 pt before the end) at the end: "Go back" Cmd [, "Go forward" Cmd ], "More options", "home" Control 1,
  "Archive" Cmd Y. A pinned tab's has a second line, "Double-click to rename" in 11 pt
  #909090, 44 pt high, half a point left of and above the name's inset, ending 9.5 pt
  after its longer line (141.5 pt for "Double-click to rename"), as Arc's, unless the
  item has a name of its own: Arc's renamed pinned tabs (its items with a title, as
  iCloud Mail and Twitch) show the name alone. Arcomium marks an item named by
  Rename... (the editor's new title, `BookmarkNodeChanged`) or imported with Arc's own
  title with meta info `arcomium.renamed` = "1". They
  show a second after the pointer settles (0.9 s for a two-line one: Arc's row hint,
  also a folder's nested row's, shows within 1.05 s), 7 pt
  right of and below it (the two-line one 11 pt right of it and 5 pt below, as Arc's), or, near the window's bottom (the Space bar), centred 3 pt
  above the button; if another hid within half a second, as soon as the pointer
  stops (400 ms without a move), where it stops, as Arc's (shown at once, it sat
  where the pointer entered the button); they hide when it
  leaves or clicks. The archive button's stays under the pointer, reaching out of the
  window, as Arc's Library button's does; it says Archive (Cmd+Y opens the history),
  not Arc's View Library, which Arcomium does not have. Favorites, folders, New Tab
  and "+" have none, as in Arc. The rows' cross is 11 pt (a 24 pt icon), centred 18 pt
  from the highlight's end.
- A hovered Space button other than the shown one takes a light shade of its Space's
  theme color (hue kept, 45% of its saturation, 66% lightness), as Arc's book turns
  #96B6BC; a Space without a theme stays grey.
- The Space name's chevron and "..." are #ABABAB on hover, as Arc's.
- The sidebar field's magnifier brightens under the pointer, #828282 to #898888,
  as Arc's.
- A new window opens 29 pt right of and below the last one, as Arc's
  (`window_sizer_mac.mm`; Chromium's 22).
- Started with a Cocoa argument such as `-AppleLanguages (de)` (how a Mac app is
  run in another language), Chromium took the value for a URL and opened
  http://(de)/, whose host a debug build's DCHECK rejects
  (`IsCanonicalizedHostCompliant`, from the bounce tracking cleanup); on macOS
  the value after a `-Apple...` or `-NS...` key in the process' own argv is
  not a tab (`startup_tab_provider.cc`).
- Compact command bar: a click on the sidebar's field opens Arc for macOS' compact
  command bar over it, 348 pt wide from a point right of the field's left edge and two
  points above it (its dark outer ring 8 pt in and 44 pt down, as Arc's),
  with a 45 pt input in 15 pt (3 pt below its row's middle) and the same rows; it opens empty, listing the open
  tabs, as Arc's. File > New Tab, Cmd+T, Cmd+L and New Tab open the centred one, as
  in Arc (`compact_command_bar_` in `arcomium_address_bar.cc`). The suggestions
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
- Hidden sidebar (View > Hide Sidebar, Cmd+S; the menu then says Show Sidebar): as in
  Arc for macOS the page takes the whole window but its 10 pt margin, with no toolbar
  and no window controls, and the sidebar floats back over the page while the pointer
  is at the window's left edge (Chromium's expand on hover, 350 ms): a #0D0D0D panel
  5 pt in from the window's edges with 11 pt corners and a 1 pt #565656 rim, casting
  a faint shadow (a quarter of Chromium's, a couple of levels off the page beside it
  as Arc's), holding
  the whole sidebar, its header and the window controls. What the sidebar shades
  off the frame is then shaded off that darker panel, as Arc's: the field #262626,
  the dimmed arrows #4A4A4A, "..." #9D9D9D, archive and "+" #9C9C9C, the other
  Spaces #383838, the window controls' faces #595957 (the window property
  `arcomium::kFloatingSidebarProperty`, read by `FrameColor` and `InkOverFrame`
  in `arcomium_frame_color.h`; the window's theme is refreshed when it floats or
  docks). With no page to show (an
  empty Space's blank tab) it stays floating, as Arc's does
  (`look/mac-floating-sidebar.patch`). A bubble that points at the app menu (as
  after a crash, "Restore pages?") points at the window when no toolbar or sidebar
  button is there, or when it opens at startup before the window is registered
  (`app_menu_button.cc`; Chromium CHECKs there and quit the browser on every launch
  after a crash). Windows and Linux keep Chromium's collapsed
  list: Arc for Windows' hidden sidebar has not been measured, and its top bar differs.
- Inactive window (another app in front): Arc for macOS lightens the whole window,
  its #161616 frame to #393939, and every shade off it follows. Arcomium's
  `kColorFrameInactive` is the active frame blended 15% toward white (#393939, a
  Space's tint as much), and what it paints off the frame (the sidebar field, the
  favorites' tiles, the overflow lines, the frame around the page card) takes the
  color the window paints now (`arcomium::FrameColor()`, `arcomium_frame_color.h`);
  the browser view repaints them when the window turns active or not. Every other
  opaque shade the sidebar draws (its text, the Space's name, New Tab, the
  magnifier, the rows' plates) keeps its share of white over the lighter frame
  (`arcomium::InkOverFrame`; #808080 on #161616 is #929292 on #393939, as Arc's
  Space name), and the sidebar rebuilds its rows when the window turns active or
  not. "Search..." takes #838383 (`kColorArcomiumSidebarPlaceholderInactive`, set
  with the inactive frame). The Space bar's icons are dimmed to half that contrast,
  as Arc's (archive and "+" #747474, the shown Space #969696, the others #4A4A4A).
  The blank page lightens with the frame (#2B2B2B to #4B4B4B, Arc's #4C4C4C): the
  page card and the page's own background (`contents_web_view.cc`) follow it. "..."
  shows no plate (Arc's #474747 had been its hover stuck), and the window controls
  are grey (below) (`look/mac-inactive-window.patch`). Windows
  and Linux keep one frame color whatever the window's state, as Arc for Windows:
  there `arcomium::FrameColor()` is always the active frame.
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
- Hover: the highlight follows the pointer at once, without Chromium's fade: the
  button's hover animation did not advance on macOS.

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
  What remained wider was the weight or size Arcomium picked: the popovers' labels
  were semibold where Arc's are medium ("Toggle Sidebar" 93.5 against 92.5 pt), the
  compact command bar's placeholder 15 pt where Arc's is 13 (149.5 against 135.5).

Known gaps on macOS: the command bar's caret is 22 pt high where Arc's is 21 (the
text field's line height); Arc's Edit menu ends with Format (Font: Bold, Italic,
Underline), which Arcomium leaves out, as Chromium has no rich-text commands for
the menu to run; macOS gives Find its magnifier icon, Arc's has none, and adds
a separator after Find that Arc's menu does not show. Arc's frame and sidebar are translucent (they take a little of
what lies under the window, #141414 to #191919); Arcomium's are a solid #161616.
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
the test kit sets to English: Arc's follow the system's ("1 неделю назад").
Menu bar gaps:
Arc's Show Toolbar (Shift+Cmd+D; Arcomium for macOS has no toolbar to show), View >
Appearance (Arc's light or dark web pages), Blank Window, Stay On Top, Find and
Replace (Chromium has no replace), Reset all tabs in this Space, Change Profile and
Manage Spaces are not there; Arc-only items (Share Arc, Sign Out, Little Arc, Easels,
Notes, Boosts, Capture, Library, View Archive, Clear Archive, Arc's help pages) are
left out on purpose. Set as Default Browser stays enabled when Arcomium is the
default already (asking the system blocks, which the menu must not); Pin Tab, New
Folder and Clear Today stay enabled where they would do nothing. An extension run from the
Extensions menu opens its popup at the window's top left (its hidden header button
is the anchor), not under the menu bar as Arc's. Copy URL as Quote's
layout (quote, blank line, link) is Arcomium's: Arc's was not tried, as that would
take a selection on a page of the user's own browser. Menu items keep Chromium's
names where Chromium has the item (Reload This Page, not Arc's Refresh the Page), as
those are translated, except the items the last comparison named (Hide Other Windows, Restore Last Closed Tab, Save Page As, Print, Go Back, Go Forward,
View History, Minimize Window, View Downloads...), which take Arc's names in English
only, as Arcomium's other strings. Arc's File > Share... is a single item; Arcomium
keeps Chromium's Share submenu under Chromium's name, which the app controller looks
the submenu up by. Arc's Edit menu dims Undo, Redo, Cut, Paste, Paste and Match Style,
Delete and Select All while no text has the focus; Chromium's (and so Arcomium's)
keep them enabled, as the page decides. The Archive menu keeps Chromium's recently
closed and visited lists, which Arc's has not (Chromium's history menu fills them by
tag). The archive button has no "Show media from" menu (Arc's Library
media is left out): as at 9b03cf8, its tooltip is "Archive" Cmd+Y and it opens the
history.

Known gaps: no keyboard navigation of the sidebar (rows draw no focus ring and have no
tree roles); Arcomium's own text is English only: most of it is in
`arcomium_strings.grdp` and translatable, but the sync page's error messages are
hard-coded English (`arcomium_sync_handler.cc`); tab rows show a speaker for sound but
no camera, microphone or screen-capture indicator.

## Icon pickers

Arc's Change Icon... and Change Space Icon... open its picker, built after Arc for macOS'
(`ShowArcIconPicker`, `arcomium_popover.cc`) on every platform:

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
  `arcomium_window_mac.mm`; the blur follows the painted shape, so the corners and
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
  `arcomium_emoji_data.inc`, written by `tools/gen_emoji_data.py` from the
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
- The glyphs are Arcomium's own vector drawings in the spirit of Arc's solid set
  (`arcomium_glyphs.cc`: filled shapes with holes cut through, in one layer), named as
  Arc names its icons (`heart`, `flash`, `construction`, `restaurant`, `planet`,
  `chatBubbleEllipses`, ...), so icons imported from Arc show as picked ones. The
  names of Arcomium's first icons still draw (`bolt`, `lightbulb`, `chat`, `pets`,
  `flight` are shown as their Arc names' cells; `home`, `globe`, `rocket` draw as
  before but are no longer offered; `star` is now Arc's solid star). Arc's sun and
  book keep their earlier drawings.
- The pointer's cell takes a fainter plate; arrows move a keyboard selection, Return
  picks it, Tab switches a Space's page, Escape or a click elsewhere closes the picker.
- Windows and Linux show the same picker. Arc for Windows' picker has not been
  compared yet: its look there follows Arc for macOS until it is.
- Known gaps: Arc's emoji search goes by the system's names in the system's
  language (its placeholder reads in the system's language), Arcomium's by Unicode's
  English names, with an English placeholder; the system palette's newer emoji (Emoji
  15.1 and later), skin tones and category headings are not there; the category bar
  does not mark the shown category; there is no custom image; the rim does not run
  round the arrow. The ten new glyphs (star, bookmark, shapes, folder, archive,
  people, terminal, barbell, cloud outline, map) are named after Ionicons, as Arc's
  other names are; Arc's own names for them were not seen in an import.

## Window, address bar and command bar

- **Windows**: a 32 px top bar in the frame color, as in Arc for Windows 1.129: back,
  forward and reload, the site centered in a pill between Copy Link and the site
  controls, Split View and the window buttons. The page is an inset card with Arc's rim,
  corners and shadow, and Arc's thin icons (`components/vector_icons/arcomium_*.icon`).
  Overlay scrollbars follow the Windows setting.
- **macOS**: no toolbar while the sidebar shows; the sidebar's header is one row after
  the window buttons, as in Arc for macOS: back, forward and "..." (see Sidebar look,
  On macOS). Header buttons are 28 pt targets and take clicks instead of dragging the
  window. The sidebar and mini player use the system font, as Arc does on macOS (on
  Windows, Segoe UI Variable).
- The compact address shows only the site, centered, with no page actions; Copy Link
  and the site controls show on hover (Copy Link not while the command bar has the
  field, nor on an empty Space's blank page, as in Arc). It hides the site's icon, but
  never the
  "Not secure" or dangerous-site warning: without Safe Browsing it is the last warning
  left. It also hides the page actions and content-setting icons (blocked popups,
  location, downloads); camera and microphone use still shows through Chromium's
  activity indicator, and the rest is in the site controls. Clicking it opens the
  command bar. The address field's placeholder is Arc's "Search..." whatever the
  search engine (`look/sidebar-search-placeholder.patch`).
- **Command bar**: Chromium's omnibox in a 722 px panel centered in the window, opened
  by clicking the address or New Tab (which then opens a new tab), closed by a click
  outside. The browser's New Tab command (Ctrl+T, Cmd+T, File > New Tab) opens it the
  same way while the sidebar shows, as in Arc on both platforms: no tab opens until
  something is picked, so Escape leaves none behind (`arcomium_command_bar_hook.h`,
  set by the sidebar, called from `BrowserCommandController` and from the New Tab
  action in `browser_actions.cc`, which is what a shortcut or menu runs on Windows;
  `spaces/new-tab-command-bar.patch`). One Escape closes the
  command bar, whatever it shows, as Arc's (Chromium's would first revert what was
  typed, then close the suggestions). Its placeholder stays while it has focus, dimmed;
  on Windows and Linux the top bar's pill shows it bright again once the command bar
  closes (`ArcomiumAddressBar::StopEditing`). With
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
  frame does for its own shadow; `ArcomiumShadowBorder` in `rounded_omnibox_results_frame.cc`, whose
  `GetShadowInsets()` gives the popup's widget room for it), #303030 for the selected row
  and a one-pixel #313030 line under the input, 6 pt above the first row and 7 below
  the last. Its rows are Arc for macOS' (`<if expr="is_macosx">` in the popup's CSS):
  46 pt high and 50 apart, 8 pt in from the panel, the title 13 pt medium #D1D1D1
  (white when selected; the empty bar's rule that colors the title's text itself
  white is overridden) 51 pt in, Switch to Tab 13 pt from the row's end before Arc's
  arrow on a 24 pt square (#252525 with a light arrow, white with a dark one when
  selected). Elsewhere rows are 48 px, regular weight; searches show only their words, history rows
  a remove ×, bookmarks the site's icon (a plain page, never Chromium's star, in either
  of its icon sets). A row with no second text (a sidebar item) shows no dash after its
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
  unless they are contiguous, at most three bookmarks. On Windows and Linux the input is
  16 px, larger than the rows, as in Arc for Windows; on macOS 18 pt (15 pt in the
  compact command bar, where "Search or Enter URL..." is 13 pt, and the
  magnifier, the input and the (i) sit 2, 2 and 3 pt lower in its row than
  centred, with no line under the input, as Arc's). The centred panel is 764 pt
  wide, its dark outer ring making Arc's 765.
- **Command bar actions** (`look/command-bar-actions.patch`,
  `arcomium_command_provider.cc`): as in Arc, typing an action's name lists it as a
  row of its own with its icon and keyboard shortcut, and picking it runs it in the
  window instead of opening a page: Copy URL, Copy URL as Markdown (`[title](url)`),
  New Window, New Incognito Window, Pin Tab or Unpin Tab (by whether the shown tab is
  pinned), Add Split View (see Split View), New Space, Rename Space…, Edit Theme…, New Folder, Collapse or Expand
  Pinned Tabs (macOS, where the pinned tabs collapse; a Space with pinned items),
  Settings and View History. Arc's archive actions (Archive Tab, View Archive, Clear
  Today, which archives) are not there: Arcomium has no tab archive (see Left out on
  purpose). A name matches as a whole (3), by its start (2) or when every typed
  word starts a word of the name or of a few aliases ("preferences" finds Settings,
  "library" View History) (1); at most three rows, the best first, then the shorter
  name; actions that cannot run now (Pin Tab on an empty Space's blank page; Copy URL and
  Copy URL as Markdown there or on any blank page, whose address means nothing;
  Rename Space without a Space bar, anything of the Spaces in incognito) are
  left out. Copy URL and Copy URL as Markdown copy nothing there by their keys either
  (`chrome::GetArcomiumCopyableContents`). The best row leads the list (relevance 1350: over the search for what was
  typed, 1300, under a site the address completes to, 1400 and up, as in Arc, where
  "theme" runs Edit Theme… but "pin" opens pinterest.com) once three letters are
  typed, so Return runs it; the others follow the search and the sites. Not for an
  address being typed, nor in a site's search. The rows are Chromium's standalone
  action matches (`kPedal` with a takeover action, as the Lens entry), made by an
  autocomplete provider the location bar adds (`AutocompleteProvider::Type::
  kArcomiumCommand`, `AutocompleteController::ArcomiumAddProvider`), so the popup,
  the arrow keys and the location bar's icon treat them as Chromium's own; each has
  its own `chrome://arcomium-command/<id>` address only so they are not merged as
  duplicates, never opened; the popup knows them by that address
  (`searchbox_match.ts`), so Chromium's own pedals stay ordinary rows. Picking one
  first gives the page the keyboard, which closes the command bar (the sidebar does
  so in a task of its own), then, in a later task, runs the command, so a dialog it
  opens (Rename Space…, New Folder) does not take the focus first and leave the
  command bar open under it (a browser command, or the Space
  bar's and sidebar's through `arcomium_space_command_hook.h`, whose
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
  button in a later task (`LocationBarView::ShowArcomiumSiteControls`): opened over
  the command bar they took its focus and left it half closed, its input showing the
  compact address and, once they closed, no rows. Known gaps: the (i), the action tiles, the key chips and their
  colors come from Arc for macOS and show the same on Windows and Linux, Arc for
  Windows' command bar not having been compared; the selected keyword row ends in Chromium's small keyword button.
- **Logo menu** (Windows, Linux; `arcomium_app_menu.cc`): Check for Updates (or
  "Update to Arcomium X.Y.Z"), Import from Another Browser..., new tab, window and
  incognito window, Spaces, Tabs, Extensions and Developer submenus, Downloads,
  History, Bookmarks, Sync, Search Engine, Link routing, Settings, Print, About, More...
  (Chromium's full menu) and Close Window. The submenus have no command of their own
  (id 0), so the menu says they are enabled itself: the browser's command state calls
  0 disabled, which greyed them out and kept them shut. On macOS the logo opens Chromium's app menu,
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

## Split View

Arc's Split View on Chromium's split tabs (`look/split-view.patch`,
`arcomium_split.h`): two pages side by side in one window, each a tab of the Space.
The user asked for it back on 2026-11-08 after it had been taken out; Peek stays out.

- **Add Split View** is Chromium's `IDC_NEW_SPLIT_TAB` (`chrome::NewSplitTab`): the
  shown tab and a new pane on Chromium's "Choose a tab" page
  (`chrome://tab-search.top-chrome/split_new_tab_page.html`). As in Arc, the command
  bar opens for that pane (`arcomium::OpenCommandBar`, `arcomium_command_bar_hook.h`,
  called at the end of `NewSplitTab`, so the menu, the key, the "+" popover, the
  command bar action, the Windows top bar's split button and Chromium's own Tab menu
  item all do it): the field opens empty, and what is typed loads in the pane. The
  sidebar lists the pane as "Choose a tab" (the page's title). A tab of the same Space
  picked meanwhile, by a sidebar row or the command bar's Switch to Tab row, takes the
  pane's place in the split and the chooser closes (`ArcomiumSidebarView::
  FillSplitChooser`, Chromium's `UpdateTabInSplit` with `kReplace`, done one task
  after the activation, since a split is updated from one of its panes); Chromium's
  own list on the page picks too. Escape leaves the chooser pane open.
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
  again (`ArcomiumSidebarView::SplitDraggedRow`, called first in
  `MultiContentsViewDelegateImpl::HandleLinkDrop`). A pinned item not open opens bound
  to its row first. A dropped tab already in a split leaves it, its other pane
  becoming a plain tab. No split zone: onto a tab already in a split (two panes at
  most), onto the row itself, for folders, and onto the page while it shows a split
  (Chromium shows no zone then). The split target is the row's plate with a 2 px
  outline in the item ink, so it reads apart from the shown tab's plate. Escape ends
  a drag with nothing moved (the platform's own drag session cancels it). Shared
  code, so the same on macOS, Windows and Linux. Chromium's zone takes a drop as a
  link, so on Windows and Linux a row's drag offers a link as well as a move
  (`GetDragOperationsForView`): Windows' drag and drop delivers no drop whose
  operation the source does not offer, and the zone showed but the drop did nothing.
- **The split's row**, as in Arc: a split's two tabs share one row, each half with
  its own icon, title and menu (`SplitPair` in `arcomium_sidebar_view.cc`); a 1 px
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
  right pane (mirrored in right-to-left layouts). These are `IDC_ARCOMIUM_*_SPLIT*`
  commands, enabled only while the shown tab is in a split (`OnSplitTabChanged`).
- Both platforms behave the same; only where the items sit differs (macOS' menu bar
  and "+" popover, the logo menu and the top bar's button elsewhere).

Known gaps: Chromium's split holds two panes, Arc's up to four, so there are no
Switch to Split View 3 and 4 or next and previous split keys, and a split takes no
third page. The split row's look (line, dimmed pane) is drawn from memory of Arc,
not measured: Arc was not put in a split for a reference, since that would change the
user's own browser. A tab of another Space or a favorite picked for the chooser is just shown
(moving it into this Space would unbind it), as is a tab already in a split.
Windows' "+" menu has no New Split (Arc for Windows' "+" was not measured). The chooser's
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
  archive button opens the history, as before; Clear Today (macOS' Tabs menu,
  Shift+Cmd+K) closes the Space's unpinned tabs. There is no archive page, setting,
  command or key.
- **Peek**, Arc's floating preview of a link over the page: Shift+click opens a
  new window and a pinned tab's links open new tabs, as in Chromium. There is no
  Peek setting on the Link routing page.
- **Little Arc**, Arc's small window for a quick look at a page (New Little Arc
  Window, Option-Command-N / Ctrl+Alt+N, and links from other apps opening in it):
  the user's choice (2026-11-08). There is no Little Arc command, menu item, key,
  window or setting; links from other apps open in a tab of the Space a route sends
  them to, else of the Space last shown, as before.

Wanted and built (2026-11-08): Split View without Peek (see Split View), Arc's icon
pickers for pinned items, folders and Spaces (see Icon pickers), the macOS menu bar
and the Arc look.

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
buttons, the menu bar). There the sidebar, its header, the address field, the page
card and the command bar follow Arc for macOS' own measurements (Sidebar look, On
macOS), and the header's "..." menu holds what the Windows header has as buttons.
Context menus, the command bar's results and the sidebar's behaviour are the same
everywhere, except the folder popup (searchable, with dates, on macOS as in Arc for
macOS), the menu bar, which only macOS has (`look/mac-menu-bar.patch`), the hidden
sidebar (floating on macOS, `look/mac-floating-sidebar.patch`) and the tooltips (Arc's
pills on macOS, `look/mac-arc-tooltips.patch`). The command bar's row layout and
colors are macOS-only too; what its rows offer (Switch to Tab at the row's end, the
sidebar's items in an empty command bar) is the same everywhere. A pinned item's or
folder's Move to lists every Space, also one not open in the window (into its pinned
folder), as Arc's does; a plain tab still moves only to an open Space.

The macOS build tree (Chromium 156) has `spaces/pinned-tab-binding.patch` applied by
hand (it applies to 156 as it is); its `arcomium_sidebar_view.cc` is the core one.
Changes to the sidebar are applied to it by hand until macOS moves to 157. Its omnibox popup is the pre-157 one, whose rows put
the dash before `#description`; there an empty description still holds a span, so the
dash shows only before one with text (`:has(span:not(:empty))`, in that tree only).

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
- Arcomium's own browser commands (`IDC_ARCOMIUM_*`: Pin Tab and Ctrl+D, Copy URL as
  Markdown, the Space commands, Link routing, Import, the update) run on Chromium 157,
  whose browser commands go through actions (`kUseActionsForBrowserCommands`, on by
  default since 157; 156 has it off): there `CommandActionUpdater` knows only the
  commands mapped to an action, so Arcomium's were neither supported nor enabled
  (Ctrl+D, Ctrl+Shift+Alt+C and the command bar's Pin Tab and Copy URL as Markdown did
  nothing, the logo menu's Import and Link routing were grey). Commands without an
  action are now kept by a `CommandUpdaterImpl` inside it and run by
  `BrowserCommandController` as before (`defaults/commands-without-actions.patch`).
  Copy URL itself is an action there, which skipped Arcomium's check, so the action
  also copies nothing on an empty Space's or a blank page (`chrome::
  GetArcomiumCopyableContents`), as the command does.
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
- From Arc: Share in the sidebar's menus on Windows and Linux (macOS has it on the
  system's sharing services; Arc for Windows' menus were not compared);
  the Ctrl+T hint on New Tab (each new string means a full resource rebuild, so they
  wait).
