"""Imports Arc's sidebar (Spaces, pinned tabs, nested folders, favicons) into
an Arcomium profile as bookmarks, the way Arcomium's sidebar stores them.

Each Arc Space becomes a folder under "Other bookmarks". The first Space is
shown outside of Spaces (meta info arcomium.space = "default"); the others are
adopted by an Arcomium Space with the same name. Re-running replaces what an
earlier run imported. Close Arcomium first: it rewrites the file on exit.

Usage:
  python tools/arc_import.py [--arc-dir DIR] [--profile DIR]
"""

import argparse
import base64
import hashlib
import json
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


class Importer:
    def __init__(self, arc_dir, next_id):
        self.icons = arc_dir / 'SidebarItemsFaviconCache'
        self.next_id = next_id
        self.counts = {'folders': 0, 'links': 0, 'icons': 0}

    def new_id(self):
        self.next_id += 1
        return str(self.next_id)

    def folder(self, name, meta, children):
        self.counts['folders'] += 1
        return {'type': 'folder', 'id': self.new_id(), 'guid': str(uuid.uuid4()),
                'name': name, 'date_added': chrome_time(),
                'date_last_used': '0', 'date_modified': '0',
                'meta_info': meta, 'children': children}

    def link(self, item_id, name, url):
        meta = {'arcomium.imported': '1'}
        # Arc names cached favicons after the md5 of the item id.
        icon = self.icons / hashlib.md5(item_id.encode()).hexdigest()
        if icon.exists():
            meta['arcomium.icon'] = base64.b64encode(icon.read_bytes()).decode()
            self.counts['icons'] += 1
        self.counts['links'] += 1
        return {'type': 'url', 'id': self.new_id(), 'guid': str(uuid.uuid4()),
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
                meta = {'arcomium.imported': '1'}
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

    other = bookmarks['roots']['other']
    other['children'] = [
        child for child in other['children']
        if child.get('meta_info', {}).get('arcomium.imported') != '1']

    importer = Importer(arc_dir, max(max_id(root)
                                     for root in bookmarks['roots'].values()))
    first = True
    for container in sidebar['sidebar']['containers']:
        if 'spaces' not in container:
            continue
        items = pairs(container['items'])
        for space_id, space in pairs(container['spaces']).items():
            ids = space.get('containerIDs', [])
            pinned = dict(zip(ids[::2], ids[1::2])).get('pinned')
            if not pinned or not space.get('title'):
                continue
            meta = {'arcomium.imported': '1',
                    'arcomium.space': 'default' if first else f'arc:{space_id}'}
            # Arc's Space icon ("sunny", "book", ...): the Space bar shows it.
            icon = (space.get('customInfo') or {}).get('iconType', {}).get('icon')
            if icon:
                meta['arcomium.space_icon'] = icon
            first = False
            other['children'].append(importer.folder(
                space['title'], meta, importer.tree(items, pinned)))

    bookmarks_path.write_text(json.dumps(bookmarks, ensure_ascii=False,
                                         indent=3), 'utf-8')
    print(f'Imported {importer.counts["folders"]} folders, '
          f'{importer.counts["links"]} links, {importer.counts["icons"]} icons '
          f'into {bookmarks_path}')


if __name__ == '__main__':
    main()
