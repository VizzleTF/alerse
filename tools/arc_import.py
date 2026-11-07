"""Imports Arc's sidebar (Spaces, pinned tabs, nested folders, favicons) into
an Arcomium profile as bookmarks, the way Arcomium's sidebar stores them.

Each Arc Space becomes a folder under "Other bookmarks". The first Space is
shown outside of Spaces (meta info arcomium.space = "default"); the others are
adopted by an Arcomium Space with the same name. Re-running replaces what an
earlier import imported, the way chrome://arcomium-import does: the same
folders, the user's own bookmarks in them kept. Close Arcomium first: it
rewrites the file on exit.

What it adds is marked as chrome://arcomium-import marks it, with the
profile's own key (the arcomium.import_key pref, made here when the profile
has none), so that importing again from either one replaces it.

Usage:
  python tools/arc_import.py [--arc-dir DIR] [--profile DIR]
"""

import argparse
import base64
import hashlib
import hmac
import json
import secrets
import os
import sys
import time
import uuid
from pathlib import Path

LOCAL = Path(os.environ.get('LOCALAPPDATA', ''))
DEFAULT_PROFILE = LOCAL / 'Chromium' / 'User Data' / 'Default'


def find_arc_dir():
    packages = LOCAL / 'Packages'
    for package in sorted(packages.glob('TheBrowserCompany.Arc_*')):
        candidate = package / 'LocalCache' / 'Local' / 'Arc'
        if (candidate / 'StorableSidebar.json').exists():
            return candidate
    sys.exit('Arc data not found; pass --arc-dir')


def pairs(flat):
    """Arc stores maps as flat [key, value, key, value] lists."""
    return {flat[i]: flat[i + 1]
            for i in range(0, len(flat) - 1, 2) if isinstance(flat[i], str)}


def chrome_time():
    # Microseconds since 1601-01-01, as Chromium stores bookmark dates.
    return str(int((time.time() + 11644473600) * 1_000_000))


def import_key(profile):
    """The profile's key for import marks, made and saved if it has none."""
    path = profile / 'Preferences'
    prefs = json.loads(path.read_text('utf-8')) if path.exists() else {}
    arcomium = prefs.setdefault('arcomium', {})
    if not arcomium.get('import_key'):
        arcomium['import_key'] = secrets.token_hex(32)
        path.write_text(json.dumps(prefs, ensure_ascii=False), 'utf-8')
    return arcomium['import_key']


def sign(key, kind, subject, guid):
    # As ImportMark::Sign() in arcomium_import_bookmarks.cc.
    message = f'{kind}\n{subject}\n{guid}'.encode()
    return hmac.new(key.encode(), message, hashlib.sha256).hexdigest()[:32]


HOME = 'default'
# arcomium.imported of what this tool imported before it signed its marks.
LEGACY_MARK = '1'


def meta(node, key):
    return node.get('meta_info', {}).get(key, '')


def set_meta(node, key, value):
    info = node.setdefault('meta_info', {})
    if value:
        info[key] = value
    else:
        info.pop(key, None)


def is_folder(node):
    return node.get('type') == 'folder'


def is_imported(key, node):
    return meta(node, 'arcomium.imported') == sign(
        key, 'node', 'arc', node.get('guid', '').lower())


def is_group_space(space):
    # A Space with its own tab group, not Home and not one an import left
    # for a Space to adopt.
    return bool(space) and space != HOME and not space.startswith('arc:')


def bind(key, folder, arc_space):
    """Binds `folder` to Arc's Space `arc_space` in this profile."""
    set_meta(folder, 'arcomium.arc_space', arc_space)
    set_meta(folder, 'arcomium.arc_space_mark',
             sign(key, 'arc_space', arc_space, folder['guid'].lower()))


def arc_space_of(key, folder):
    arc_space = meta(folder, 'arcomium.arc_space')
    mark = sign(key, 'arc_space', arc_space, folder.get('guid', '').lower())
    return arc_space if arc_space and meta(
        folder, 'arcomium.arc_space_mark') == mark else ''


def set_arc_icon(folder, icon):
    """Records Arc's icon of the Space of `folder` and shows it, unless the
    user has picked another one since the last import."""
    shown = meta(folder, 'arcomium.space_icon')
    if not shown or shown == meta(folder, 'arcomium.arc_icon'):
        set_meta(folder, 'arcomium.space_icon', icon)
    set_meta(folder, 'arcomium.arc_icon', icon)


def walk(node):
    stack = [node]
    while stack:
        top = stack.pop()
        yield top
        stack.extend(reversed(top.get('children', [])))


def adopt_legacy(key, other):
    """Signs what this tool imported before it signed its marks: each Space
    folder under Other bookmarks marked "1" and each node in it marked "1".
    A folder no Space has adopted yet ("arc:<id>") is bound to its Arc Space.
    As AdoptLegacyArcImport() in arcomium_import_bookmarks.cc."""
    for folder in other['children']:
        space = meta(folder, 'arcomium.space')
        if (not is_folder(folder) or not space or
                meta(folder, 'arcomium.imported') != LEGACY_MARK):
            continue
        if space.startswith('arc:') and len(space) > 4 and not arc_space_of(
                key, folder):
            bind(key, folder, space[4:])
        for node in walk(folder):
            if meta(node, 'arcomium.imported') == LEGACY_MARK:
                set_meta(node, 'arcomium.imported',
                         sign(key, 'node', 'arc', node['guid'].lower()))


def unmarked_subtrees(key, node):
    """The largest subtrees of `node` that no import here brought, in order:
    what the user put in it."""
    kept = []
    stack = list(reversed(node.get('children', [])))
    while stack:
        top = stack.pop()
        if not is_imported(key, top):
            kept.append(top)
        else:
            stack.extend(reversed(top.get('children', [])))
    return kept


def remove_marked_children(key, folder):
    """Removes what earlier imports put in `folder`, keeping the user's nodes
    in it, and returns where the imported nodes were."""
    children = folder['children']
    first = next((i for i, child in enumerate(children)
                  if is_imported(key, child)), None)
    folder['children'] = [child for child in children
                          if not is_imported(key, child)]
    for child in children:
        if is_imported(key, child):
            folder['children'].extend(unmarked_subtrees(key, child))
    return len(folder['children']) if first is None else first


def remove(parent, node):
    parent['children'] = [child for child in parent['children']
                          if child is not node]


class Importer:
    def __init__(self, arc_dir, next_id, key):
        self.icons = arc_dir / 'SidebarItemsFaviconCache'
        self.next_id = next_id
        self.key = key
        self.counts = {'folders': 0, 'links': 0, 'icons': 0}

    def mark(self, meta, guid):
        meta['arcomium.imported'] = sign(self.key, 'node', 'arc', guid)
        return meta

    def new_id(self):
        self.next_id += 1
        return str(self.next_id)

    def folder(self, name, meta, children):
        self.counts['folders'] += 1
        guid = str(uuid.uuid4())
        self.mark(meta, guid)
        return {'type': 'folder', 'id': self.new_id(), 'guid': guid,
                'name': name, 'date_added': chrome_time(),
                'date_last_used': '0', 'date_modified': '0',
                'meta_info': meta, 'children': children}

    def link(self, item_id, name, url):
        guid = str(uuid.uuid4())
        meta = self.mark({}, guid)
        # Arc names cached favicons after the md5 of the item id.
        icon = self.icons / hashlib.md5(item_id.encode()).hexdigest()
        if icon.exists():
            meta['arcomium.icon'] = base64.b64encode(icon.read_bytes()).decode()
            self.counts['icons'] += 1
        self.counts['links'] += 1
        return {'type': 'url', 'id': self.new_id(), 'guid': guid,
                'name': name, 'url': url, 'date_added': chrome_time(),
                'date_last_used': '0', 'meta_info': meta}

    def tree(self, items, item_id):
        nodes = []
        for child_id in items.get(item_id, {}).get('childrenIds', []):
            item = items.get(child_id)
            if not item:
                continue
            data = item.get('data', {})
            if 'tab' in data:
                tab = data['tab']
                url = tab.get('savedURL') or ''
                if url.startswith('arc://'):
                    url = 'chrome://' + url[len('arc://'):]
                if not url:
                    continue
                name = item.get('title') or tab.get('savedTitle') or url
                nodes.append(self.link(child_id, name, url))
            elif 'list' in data:
                meta = {}
                icon = (data['list'].get('customInfo') or {}).get(
                    'iconType', {}).get('icon')
                if icon:
                    meta['arcomium.folder_icon'] = icon
                nodes.append(self.folder(item.get('title') or 'Folder', meta,
                                         self.tree(items, child_id)))
        return nodes


def max_id(node):
    best = int(node.get('id', 0))
    for child in node.get('children', []):
        best = max(best, max_id(child))
    return best


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--arc-dir', type=Path)
    parser.add_argument('--profile', type=Path, default=DEFAULT_PROFILE)
    args = parser.parse_args()

    arc_dir = args.arc_dir or find_arc_dir()
    sidebar = json.loads((arc_dir / 'StorableSidebar.json').read_text('utf-8'))

    bookmarks_path = args.profile / 'Bookmarks'
    if bookmarks_path.exists():
        bookmarks = json.loads(bookmarks_path.read_text('utf-8'))
        bookmarks.pop('checksum', None)
    else:
        args.profile.mkdir(parents=True, exist_ok=True)
        empty = lambda id_, name: {'type': 'folder', 'id': id_, 'name': name,
                                   'children': [], 'date_added': chrome_time(),
                                   'date_modified': '0'}
        bookmarks = {'version': 1, 'roots': {
            'bookmark_bar': empty('1', 'Bookmarks bar'),
            'other': empty('2', 'Other bookmarks'),
            'synced': empty('3', 'Mobile bookmarks')}}

    key = import_key(args.profile)
    other = bookmarks['roots']['other']
    adopt_legacy(key, other)
    importer = Importer(arc_dir, max(max_id(root)
                                     for root in bookmarks['roots'].values()),
                        key)

    # Arc's Spaces with pinned tabs, in Arc's order: (id, title, icon, nodes).
    spaces = []
    for container in sidebar['sidebar']['containers']:
        if 'spaces' not in container:
            continue
        items = pairs(container['items'])
        for space_id, space in pairs(container['spaces']).items():
            ids = space.get('containerIDs', [])
            pinned = dict(zip(ids[::2], ids[1::2])).get('pinned')
            if not pinned or not space.get('title') or not space_id:
                continue
            # Arc's Space icon ("sunny", "book", ...): the Space bar shows it.
            icon = (space.get('customInfo') or {}).get('iconType', {}).get('icon')
            spaces.append((space_id, space['title'], icon or '',
                           importer.tree(items, pinned)))

    # The folder each Space goes to, as ImportArcSpaces() in
    # arcomium_import_bookmarks.cc picks it: the one bound to it here; for the
    # first Space, the user's Home folder, else an earlier import's; then a
    # Space's folder of the same name bound to no Arc Space.
    folders = [child for child in other['children'] if is_folder(child)]
    bound = {id(folder): arc_space_of(key, folder) for folder in folders}
    has_home = any(meta(folder, 'arcomium.space') == HOME for folder in folders)
    targets = [None] * len(spaces)
    taken, taken_spaces = set(), set()

    def find(matches):
        return next((folder for folder in folders
                     if id(folder) not in taken and
                     meta(folder, 'arcomium.space') not in taken_spaces and
                     matches(folder)), None)

    def take(i, folder):
        targets[i] = folder
        taken.add(id(folder))
        if meta(folder, 'arcomium.space'):
            taken_spaces.add(meta(folder, 'arcomium.space'))

    for i, (space_id, _, _, _) in enumerate(spaces):
        folder = find(lambda folder: bound[id(folder)] == space_id)
        if folder:
            take(i, folder)
    if spaces and not targets[0]:
        for imported in (False, True):
            folder = find(lambda folder: is_imported(key, folder) == imported
                          and meta(folder, 'arcomium.space') == HOME)
            if folder:
                take(0, folder)
                break
    new_home = bool(spaces) and not targets[0] and not has_home
    for i, (_, title, _, _) in enumerate(spaces):
        if targets[i] or (i == 0 and new_home):
            continue
        folder = find(lambda folder: folder.get('name') == title and
                      is_group_space(meta(folder, 'arcomium.space')) and
                      not meta(folder, 'arcomium.arc_space'))
        if folder:
            take(i, folder)

    folder_by_space = {}
    for i, (space_id, title, icon, nodes) in enumerate(spaces):
        folder = targets[i]
        if folder:
            index = remove_marked_children(key, folder)
            folder['children'][index:index] = nodes
        else:
            space = HOME if i == 0 and new_home else f'arc:{space_id}'
            folder = importer.folder(title, {'arcomium.space': space}, nodes)
            other['children'].append(folder)
        bind(key, folder, space_id)
        target = meta(folder, 'arcomium.space')
        # The icon of the user's own Home is theirs alone.
        if target != HOME or is_imported(key, folder):
            set_arc_icon(folder, icon)
        if target:
            folder_by_space.setdefault(target, folder)

    # The rest of what earlier imports here brought goes; what the user put in
    # it stays: in the folder of the same Space, or in its own folder when Arc
    # no longer has that Space. A folder of the user's that a Space Arc no
    # longer has was bound to stays, unbound.
    for folder in folders:
        imported = is_imported(key, folder)
        if id(folder) in taken or (not imported and not bound[id(folder)]):
            continue
        space = meta(folder, 'arcomium.space')
        if imported and space in folder_by_space:
            folder_by_space[space]['children'].extend(
                unmarked_subtrees(key, folder))
            remove(other, folder)
            continue
        remove_marked_children(key, folder)
        # An empty folder no Space adopted would only name the next Space the
        # "+" button makes: it goes.
        if imported and not folder['children'] and not is_group_space(space):
            remove(other, folder)
            continue
        if imported:
            set_meta(folder, 'arcomium.imported', '')
        for name in ('arcomium.arc_space', 'arcomium.arc_space_mark',
                     'arcomium.arc_icon'):
            set_meta(folder, name, '')

    bookmarks_path.write_text(json.dumps(bookmarks, ensure_ascii=False,
                                         indent=3), 'utf-8')
    print(f'Imported {importer.counts["folders"]} folders, '
          f'{importer.counts["links"]} links, {importer.counts["icons"]} icons '
          f'into {bookmarks_path}')


if __name__ == '__main__':
    main()
