"""Checks the patch series without a Chromium tree; CI runs it on every push.

- every patch is listed in patches/series (patches/ios/ in series-ios), once,
  and every entry has its file;
- no patch or source file has a name domain substitution made up, which
  would not apply to a tree before substitution;
- no patch creates a file, except under patches/ios/: Alerse's own files
  live in src/ (docs/patching.md);
- no hunk has much more context on one side than git's three lines: GNU
  patch, which the Windows build uses, takes such a hunk for one at the start
  or the end of the file and rejects it anywhere else;
- the patches named on the command line (CI names the ones a change adds)
  say why they are there, in a comment that starts with "Alerse:". Not every
  hunk can: one that only adds an #include or a build file's line has no
  place for a comment.

Usage:
  python tools/check_patches.py [NEW_PATCH...]
"""

import re
import sys
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent
PATCHES = CORE / 'patches'
SUBSTITUTED = re.compile(r'9oo91e|qjz9zk|m1cr050ft|ch40m1um')
TAG = re.compile(r'^\+.*Alerse:', re.M)


def read_series(name):
    lines = (PATCHES / name).read_text(encoding='utf-8').splitlines()
    return [line.strip() for line in lines if line.strip() and not line.startswith('#')]


def check_series():
    errors = []
    listed = {'series': read_series('series'), 'series-ios': read_series('series-ios')}
    for name, entries in listed.items():
        for entry in sorted({e for e in entries if entries.count(e) > 1}):
            errors.append(f'patches/{name}: {entry} is listed twice')
        for entry in entries:
            if not (PATCHES / entry).is_file():
                errors.append(f'patches/{name}: {entry} has no file')
    for path in sorted(PATCHES.rglob('*.patch')):
        entry = path.relative_to(PATCHES).as_posix()
        name = 'series-ios' if entry.startswith('ios/') else 'series'
        if entry not in listed[name]:
            errors.append(f'patches/{entry} is not in patches/{name}')
    return errors


def check_contents():
    errors = []
    for path in sorted(PATCHES.rglob('*.patch')):
        entry = path.relative_to(PATCHES).as_posix()
        text = path.read_text(encoding='utf-8')
        if not entry.startswith('ios/') and re.search(r'^new file mode', text, re.M):
            errors.append(f'patches/{entry} creates a file; put it in src/')
    for path in sorted([*PATCHES.rglob('*.patch'), *(CORE / 'src').rglob('*')]):
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding='utf-8')
        except UnicodeDecodeError:
            continue
        for match in sorted(set(SUBSTITUTED.findall(text))):
            errors.append(f'{path.relative_to(CORE).as_posix()} has the substituted name {match}')
    return errors


def check_context():
    errors = []
    for path in sorted(PATCHES.rglob('*.patch')):
        entry = path.relative_to(PATCHES).as_posix()
        hunks = re.split(r'^@@ -(\d+).*\n', path.read_text(encoding='utf-8'), flags=re.M)
        for start, body in zip(hunks[1::2], hunks[2::2]):
            kinds = ''.join(line[:1] for line in body.split('\ndiff --git ')[0].splitlines()
                            if line[:1] in ' +-')
            before = len(kinds) - len(kinds.lstrip(' '))
            after = len(kinds) - len(kinds.rstrip(' '))
            # Up to two lines of difference apply with fuzz.
            if abs(before - after) > 2 and max(before, after) > 3:
                errors.append(f'patches/{entry}: the hunk at line {start} has {before} lines of '
                              f'context before and {after} after; GNU patch rejects it')
    return errors


def check_tags(patches):
    errors = []
    for name in patches:
        if not TAG.search(Path(name).read_text(encoding='utf-8')):
            errors.append(f'{name}: no added line has an "Alerse:" comment')
    return errors


def main():
    errors = check_series() + check_contents() + check_context() + check_tags(sys.argv[1:])
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
