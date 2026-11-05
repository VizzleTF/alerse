"""Renames Chromium to Arcomium in the browser's strings, keeping translations.

A translation is found by its message's ID, a fingerprint of the English text.
Rewriting "Chromium" in the English text alone would orphan every translation,
so this script reads the IDs with grit before and after the change and moves
the translations, renamed too, to the new IDs. "The Chromium Authors" and
"Chromium OS" keep their names. Run it on a patched source tree; running it
again changes nothing.

It also sets the product name in BRANDING and gives Arcomium its own macOS
bundle ID and profile directory, so it shares neither with Chromium nor
ungoogled-chromium installed beside it.

Usage:
  python tools/rename_product.py SRC_DIR
"""

import re
import sys
from pathlib import Path

# The *chromium_strings.grd files that exist on the platform being built.
GRDS = [
    'chrome/app/chromium_strings.grd',
    'components/components_chromium_strings.grd',
    'ios/chrome/app/strings/ios_chromium_strings.grd',
]

NAME = re.compile(r'\bChromium\b(?! Authors| OS)')
MESSAGE = re.compile(r'(<message\b[^>]*>)(.*?)(</message>)', re.S)
TRANSLATION = re.compile(r'<translation id="(\d+)">(.*?)</translation>', re.S)


def message_ids(grd):
    """(name, id) of every message, in document order."""
    from grit import grd_reader
    from grit.node import message
    # <if> conditions name build variables; their values do not change any
    # message's text, so unknown ones are taken as false.
    defines = {}
    while True:
        try:
            root = grd_reader.Parse(str(grd), str(grd.parent), stop_after=None,
                                    first_ids_file=None, debug=False, defines=defines)
            break
        except AssertionError as e:
            unknown = re.search(r'undefined Grit variable found: (\w+)', str(e))
            if not unknown or unknown.group(1) in defines:
                raise
            defines[unknown.group(1)] = False
    return [(node.attrs['name'], node.GetCliques()[0].GetMessage().GetId())
            for node in root.Preorder() if isinstance(node, message.MessageNode)]


def rename(grd):
    original = grd.read_text(encoding='utf-8')
    # The grd and the .grdp parts it includes.
    files = [grd] + [grd.parent / part
                     for part in re.findall(r'<part file="([^"]+)"', original)]
    texts = {f: f.read_text(encoding='utf-8') for f in files}
    renamed = {f: MESSAGE.sub(
        lambda m: m.group(1) + NAME.sub('Arcomium', m.group(2)) + m.group(3), text)
        for f, text in texts.items()}
    if renamed == texts:
        return 0
    before = message_ids(grd)
    for f, text in renamed.items():
        f.write_text(text, encoding='utf-8', newline='\n')
    after = message_ids(grd)
    if [name for name, _ in before] != [name for name, _ in after]:
        for f, text in texts.items():
            f.write_text(text, encoding='utf-8', newline='\n')
        sys.exit(f'{grd}: messages changed order while renaming')
    moved = {old: new for (_, old), (_, new) in zip(before, after) if old != new}

    # The grd's own translation files.
    for path in re.findall(r'<file path="([^"]+\.xtb)"', original):
        xtb = grd.parent / path
        if not xtb.exists():
            continue
        text = xtb.read_text(encoding='utf-8')
        if not any(f'id="{old}"' in text for old in moved):
            continue

        def move(m):
            new = moved.get(m.group(1))
            if new is None:
                return m.group(0)
            return f'<translation id="{new}">{NAME.sub("Arcomium", m.group(2))}</translation>'

        xtb.write_text(TRANSLATION.sub(move, text), encoding='utf-8', newline='\n')
    return len(moved)


BRANDING = {
    'PRODUCT_FULLNAME': 'Arcomium',
    'PRODUCT_SHORTNAME': 'Arcomium',
    'PRODUCT_INSTALLER_FULLNAME': 'Arcomium Installer',
    'PRODUCT_INSTALLER_SHORTNAME': 'Arcomium Installer',
    'MAC_BUNDLE_ID': 'io.github.vizzletf.arcomium',
}
PRODUCT_DIR_KEY = '<key>CrProductDirName</key>'


def set_branding(src):
    branding = src / 'chrome/app/theme/chromium/BRANDING'
    plist = src / 'chrome/app/app-Info.plist'
    if not branding.exists() or not plist.exists():
        return
    # Set whatever the platform repository made them (ungoogled-chromium-macos
    # changes the bundle ID), so no patch depends on their values.
    text = branding.read_text(encoding='utf-8')
    for key, value in BRANDING.items():
        text = re.sub(rf'^{key}=.*$', f'{key}={value}', text, flags=re.M)
    branding.write_text(text, encoding='utf-8', newline='\n')
    # ~/Library/Application Support/Arcomium, not .../Chromium.
    text = plist.read_text(encoding='utf-8')
    if PRODUCT_DIR_KEY not in text:
        text = text.replace(
            '<dict>\n',
            f'<dict>\n\t{PRODUCT_DIR_KEY}\n\t<string>Arcomium</string>\n', 1)
        plist.write_text(text, encoding='utf-8', newline='\n')


def main():
    src = Path(sys.argv[1]).resolve()
    set_branding(src)
    sys.path.insert(0, str(src / 'tools' / 'grit'))
    for path in GRDS:
        grd = src / path
        if grd.exists():
            print(f'{path}: {rename(grd)} messages renamed')


if __name__ == '__main__':
    main()
