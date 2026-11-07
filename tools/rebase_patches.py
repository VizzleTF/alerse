"""Moves Arcomium's patch series to a new Chromium base with git.

The series is a stack of diffs against one Chromium tree (after the
ungoogled-chromium and platform patches). To move it to a new tree, git
replays each patch as a commit with a three-way merge, which resolves most of
what a fuzzy `patch` cannot, and remembers resolutions (rerere) for the next
time. Arcomium's own files in src/ are not patches and need no rebase.

In a git repository that has both trees as commits (OLD and NEW, each the
Chromium tree with the ungoogled-chromium and platform patches applied):

    python tools/rebase_patches.py import --repo REPO --base OLD
        Commits each patch of patches/series onto OLD, on branch
        arcomium-patches, one commit per patch.

    git -C REPO config rerere.enabled true
    git -C REPO rebase --onto NEW OLD arcomium-patches
        Resolve conflicts as git stops; `git rebase --continue`.

    python tools/rebase_patches.py export --repo REPO --base NEW
        Writes each commit of NEW..arcomium-patches back to its patch file.

The working tree of REPO is used only by the rebase; import works on a
temporary index.
"""

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent
PATCHES = CORE / 'patches'
BRANCH = 'arcomium-patches'
PREFIX = 'arcomium-patch: '


def git(repo, *args, env=None, input_text=None):
    result = subprocess.run(['git', '-C', str(repo), *args], check=True,
                            capture_output=True, text=True, env=env,
                            input=input_text, encoding='utf-8')
    return result.stdout.strip()


def read_series():
    lines = (PATCHES / 'series').read_text(encoding='utf-8').splitlines()
    return [l.strip() for l in lines if l.strip() and not l.startswith('#')]


def do_import(repo, base):
    parent = git(repo, 'rev-parse', base)
    with tempfile.TemporaryDirectory() as tmp:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(tmp) / 'index'))
        git(repo, 'read-tree', parent, env=env)
        for name in read_series():
            patch = PATCHES / name
            subprocess.run(['git', '-C', str(repo), 'apply', '--cached',
                            str(patch)], check=True, env=env)
            tree = git(repo, 'write-tree', env=env)
            parent = git(repo, 'commit-tree', tree, '-p', parent, '-m',
                         PREFIX + name, env=env)
            print('imported', name)
    git(repo, 'branch', '-f', BRANCH, parent)
    print(f'{BRANCH} = {parent}')


def do_export(repo, base):
    commits = git(repo, 'rev-list', '--reverse', f'{base}..{BRANCH}').split()
    exported = []
    for commit in commits:
        subject = git(repo, 'log', '-1', '--format=%s', commit)
        if not subject.startswith(PREFIX):
            sys.exit(f'{commit} is not an imported patch: {subject}')
        name = subject[len(PREFIX):]
        diff = subprocess.run(
            ['git', '-C', str(repo), 'diff', '--no-color', '--no-renames',
             f'{commit}~1', commit], check=True, capture_output=True).stdout
        (PATCHES / name).write_bytes(diff)
        exported.append(name)
        print('exported', name)
    missing = set(read_series()) - set(exported)
    if missing:
        sys.exit(f'not in {BRANCH}: {sorted(missing)}')


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('command', choices=('import', 'export'))
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--base', required=True)
    args = parser.parse_args()
    if args.command == 'import':
        do_import(args.repo, args.base)
    else:
        do_export(args.repo, args.base)


if __name__ == '__main__':
    main()
