"""Renames Chromium to Alerse in the browser's strings, keeping translations.

A translation is found by its message's ID, a fingerprint of the English text.
Rewriting "Chromium" in the English text alone would orphan every translation,
so this script reads the IDs with grit before and after the change and moves
the translations, renamed too, to the new IDs. "The Chromium Authors" and
"Chromium OS" keep their names. Run it on a patched source tree; running it
again changes nothing. Alerse was called Arcomium up to 0.1.7: a tree renamed
to Arcomium is renamed to Alerse the same way.

ungoogled-chromium's name goes too: from the version line ("Official Build,
ungoogled-chromium") and from the descriptions of its flags in chrome://flags
("ungoogled-chromium flag").

It also sets the product name in BRANDING and gives Alerse its own macOS
bundle ID and profile directory, so it shares neither with Chromium nor
ungoogled-chromium installed beside it (nor with Arcomium), and gives each
release its own Chromium version number (see docs/versioning.md).

Usage:
  python tools/rename_product.py SRC_DIR
"""

import re
import sys
from pathlib import Path

# The grd files whose messages name the product, those that exist on the
# platform being built: the *chromium_strings.grd files, and the unbranded
# ones that still say "Chromium" (autofill, passwords, page info, the search
# engine choice, security interstitials, omnibox actions...).
GRDS = [
    'chrome/app/chromium_strings.grd',
    'components/components_chromium_strings.grd',
    'ios/chrome/app/strings/ios_chromium_strings.grd',
    'chrome/app/generated_resources.grd',
    'components/components_strings.grd',
    'components/privacy_sandbox_strings.grd',
]

# ungoogled-chromium's flag entries, whose descriptions end in
# "ungoogled-chromium flag".
FLAG_FILES = [
    'chrome/browser/bromite_flag_entries.h',
    'chrome/browser/existing_switch_flag_entries.h',
    'chrome/browser/flag_descriptions.h',
    'chrome/browser/ungoogled_flag_entries.h',
    'chrome/browser/ungoogled_platform_flag_entries.h',
]

PRODUCT = 'Alerse'
# Chromium's name, Chrome's where unbranded strings use it for the browser, and
# Arcomium's, which a tree renamed before 0.1.8 has (this literal keeps the
# earlier name on purpose). Kept: "Google Chrome" (another browser, such as
# one to import from), the Chrome Web Store (the store's name), Chrome OS and
# Chrome Enterprise, and "The Chromium Authors".
NAME = re.compile(
    r'(?<!Google )\b(?:Chromium|Chrome|Arcomium)\b'
    r'(?! Authors| OS|OS| Web Store| Enterprise)(?!<ph name="END_LINK_CHROMIUM")')
# ", ungoogled-chromium" after "Official Build" in the version line; anywhere
# else ungoogled-chromium's name stands for the browser.
UNGOOGLED_BUILD = re.compile(r',\s*ungoogled-chromium\b')
UNGOOGLED = re.compile(r'\bungoogled-chromium\b')
# The About page's "made possible by the Chromium open source project" links to
# Chromium itself: that name stays, and earlier runs, which renamed it too, are
# undone.
CHROMIUM_LINK = re.compile(
    r'\b(?:Alerse|Arcomium)(?=<ph name="END_LINK_CHROMIUM")')
MESSAGE = re.compile(r'(<message\b[^>]*>)(.*?)(</message>)', re.S)
TRANSLATION = re.compile(r'<translation id="(\d+)">(.*?)</translation>', re.S)


def rename_text(text):
    text = UNGOOGLED.sub(PRODUCT, UNGOOGLED_BUILD.sub('', text))
    return CHROMIUM_LINK.sub('Chromium', NAME.sub(PRODUCT, text))


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
    # Parts include parts of their own (omnibox_strings.grdp includes the
    # omnibox actions' strings); a part's path is relative to the file that
    # includes it.
    files = [grd]
    for f in files:
        text = original if f == grd else f.read_text(encoding='utf-8')
        for part in re.findall(r'<part file="([^"]+)"', text):
            path = f.parent / part
            if path.exists() and path not in files:
                files.append(path)
    texts = {f: f.read_text(encoding='utf-8') for f in files}
    renamed = {f: MESSAGE.sub(
        lambda m: m.group(1) + rename_text(m.group(2)) + m.group(3), text)
        for f, text in texts.items()}
    if renamed == texts:
        return 0
    before = message_ids(grd)
    # Until the translations have moved, a failure puts the texts back: a
    # tree left renamed would look done to the next run, which would then
    # leave every translation behind.
    try:
        for f, text in renamed.items():
            f.write_text(text, encoding='utf-8', newline='\n')
        after = message_ids(grd)
        if [name for name, _ in before] != [name for name, _ in after]:
            sys.exit(f'{grd}: messages changed order while renaming')
        moved = {old: new for (_, old), (_, new) in zip(before, after) if old != new}
        move_translations(grd, original, moved)
    except BaseException:
        for f, text in texts.items():
            f.write_text(text, encoding='utf-8', newline='\n')
        raise
    return len(moved)


def move_translations(grd, original, moved):
    """Moves the translations in the grd's .xtb files to the IDs in `moved`."""
    # Written only once all of them are read and changed.
    changed = {}
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
            return f'<translation id="{new}">{rename_text(m.group(2))}</translation>'

        changed[xtb] = TRANSLATION.sub(move, text)
    for xtb, text in changed.items():
        xtb.write_text(text, encoding='utf-8', newline='\n')


def drop_duplicate_translations(grd):
    """Keeps one translation per ID in the grd's .xtb files.

    Messages that differed only in "Chrome" and "Chromium" are the same text
    once renamed, so their translations moved to one ID; grit refuses an .xtb
    with an ID twice. Run each time, so a tree renamed before this was added
    is mended too."""
    original = grd.read_text(encoding='utf-8')
    for path in re.findall(r'<file path="([^"]+\.xtb)"', original):
        xtb = grd.parent / path
        if not xtb.exists():
            continue
        seen = set()

        def keep_first(m):
            if m.group(1) in seen:
                return ''
            seen.add(m.group(1))
            return m.group(0)

        text = xtb.read_text(encoding='utf-8')
        # The translation with the line break after it, so none is left
        # empty.
        write_if_changed(xtb, re.sub(
            r'<translation id="(\d+)">.*?</translation>\n?', keep_first, text,
            flags=re.S))


def write_if_changed(path, text):
    """Writes `text` unless the file has it already: a newer modification time
    alone would rebuild everything that depends on the file."""
    if path.read_text(encoding='utf-8') != text:
        path.write_text(text, encoding='utf-8', newline='\n')


BRANDING = {
    'PRODUCT_FULLNAME': PRODUCT,
    'PRODUCT_SHORTNAME': PRODUCT,
    'PRODUCT_INSTALLER_FULLNAME': f'{PRODUCT} Installer',
    'PRODUCT_INSTALLER_SHORTNAME': f'{PRODUCT} Installer',
    # Arcomium's was io.github.vizzletf.arcomium; Alerse is another app, which
    # takes over Arcomium's profile (defaults/mac-own-profile-dir.patch).
    'MAC_BUNDLE_ID': 'io.github.vizzletf.alerse',
    # Who publishes the binary, in its file properties and Info.plist: Alerse,
    # not the Chromium project, which it is built on and credits.
    'COMPANY_FULLNAME': 'The Alerse Authors',
    'COMPANY_SHORTNAME': 'The Alerse Authors',
    'COPYRIGHT': ('Copyright @LASTCHANGE_YEAR@ The Alerse Authors and '
                  'The Chromium Authors. All rights reserved.'),
}
PRODUCT_DIR_KEY = '<key>CrProductDirName</key>'
PRODUCT_DIR = re.compile(re.escape(PRODUCT_DIR_KEY) + r'(\s*)<string>[^<]*</string>')


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
    write_if_changed(branding, text)
    # ~/Library/Application Support/Alerse, not .../Chromium (or .../Arcomium,
    # where a tree renamed before 0.1.8 has it).
    text = plist.read_text(encoding='utf-8')
    if PRODUCT_DIR_KEY not in text:
        text = text.replace(
            '<dict>\n',
            f'<dict>\n\t{PRODUCT_DIR_KEY}\n\t<string>{PRODUCT}</string>\n', 1)
    else:
        text = PRODUCT_DIR.sub(
            lambda m: f'{PRODUCT_DIR_KEY}{m.group(1)}<string>{PRODUCT}</string>', text)
    write_if_changed(plist, text)


# The release, MAJOR.MINOR.PATCH.
CORE_VERSION = Path(__file__).resolve().parent.parent / 'VERSION'
VERSION_LINE = re.compile(r'^(\w+)=(.*)$', re.M)


def stamp_version(src):
    """Numbers the release in chrome/VERSION's PATCH, Chromium's x 100 + ours.

    Windows' installer cannot install a version over itself while the browser
    runs, so no two releases may share a version: release 0.1.4 on Chromium
    157.0.8103.38 is 157.0.8103.3804. Chromium's patch number is kept as
    UPSTREAM_PATCH, which the User-Agent and its client hints report, and
    the release itself as ALERSE_VERSION, which the update check compares
    with the latest release (chrome/browser/alerse_update). The platform
    repositories' release workflows check that a release's tag is VERSION and
    its stamp above the latest release's (.github/scripts/release-version.sh).
    """
    chrome_version = src / 'chrome/VERSION'
    if not chrome_version.exists():
        return
    release = CORE_VERSION.read_text(encoding='utf-8').strip()
    match = re.fullmatch(r'\d+\.\d+\.(\d+)', release)
    if not match:
        sys.exit(f'{CORE_VERSION} is {release!r}, not MAJOR.MINOR.PATCH')
    ours = int(match.group(1))
    if ours >= 100:
        sys.exit(f'{PRODUCT} {release}: its patch number must be below 100 to fit '
                 "in the last two digits of Chromium's")
    values = dict(VERSION_LINE.findall(chrome_version.read_text(encoding='utf-8')))
    upstream = int(values.get('UPSTREAM_PATCH', values['PATCH']))
    patch = upstream * 100 + ours
    # Windows file versions hold each number in 16 bits.
    if patch > 0xFFFF:
        sys.exit(f'chrome/VERSION: PATCH {patch} for {PRODUCT} {release} on Chromium '
                 f'patch {upstream} does not fit in 16 bits')
    values['PATCH'] = str(patch)
    values['UPSTREAM_PATCH'] = str(upstream)
    values['ALERSE_VERSION'] = release
    write_if_changed(chrome_version, ''.join(f'{k}={v}\n' for k, v in values.items()))
    print(f'chrome/VERSION: {PRODUCT} {release} is '
          f'{values["MAJOR"]}.{values["MINOR"]}.{values["BUILD"]}.{patch}')


STRING_LITERAL = re.compile(r'"(?:[^"\\\n]|\\.)*"')


def rename_flags(src):
    """Renames the product in the string literals of the flag entries: "Chromium
    feature, ungoogled-chromium flag" says only that Alerse has the flag, and
    descriptions such as "Chromium didn't shut down correctly" name Alerse."""
    def rename_literal(m):
        text = m.group(0)
        for old in ('ungoogled-chromium flag', f'{PRODUCT} flag'):
            text = text.replace(f'Chromium feature, {old}', f'{PRODUCT} flag')
        return NAME.sub(PRODUCT, UNGOOGLED.sub(PRODUCT, text))

    for path in FLAG_FILES:
        flags = src / path
        if flags.exists():
            write_if_changed(flags, STRING_LITERAL.sub(
                rename_literal, flags.read_text(encoding='utf-8')))


def main():
    src = Path(sys.argv[1]).resolve()
    set_branding(src)
    stamp_version(src)
    rename_flags(src)
    sys.path.insert(0, str(src / 'tools' / 'grit'))
    for path in GRDS:
        grd = src / path
        if grd.exists():
            print(f'{path}: {rename(grd)} messages renamed')
            drop_duplicate_translations(grd)


if __name__ == '__main__':
    main()
