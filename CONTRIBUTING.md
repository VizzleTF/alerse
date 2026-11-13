# Contributing to Alerse

Alerse is built in the open, and help of any size is welcome: a bug report with steps, a
patch, a test on a platform we cannot reach, a translation. This page says where each
kind of help goes and how a change gets in.

## Report a bug or ask for a feature

Open an issue in the repository the problem is in:

| Problem with | Repository |
|---|---|
| The browser: sidebar, Spaces, command bar, import, sync client | [alerse](https://github.com/VizzleTF/alerse/issues) |
| The sync server or sync.alerse.app | [alerse-sync](https://github.com/VizzleTF/alerse-sync/issues) |
| An installer or a build of one platform | [alerse-win](https://github.com/VizzleTF/alerse-win/issues), [alerse-mac](https://github.com/VizzleTF/alerse-mac/issues), [alerse-linux](https://github.com/VizzleTF/alerse-linux/issues) |

Say which version (Alerse menu, About) and platform, what you did, what you expected
and what happened. A screenshot or screen recording helps with anything visual: Alerse
follows Arc's look to the pixel, so "the gap is wider than in Arc" is a fine bug.

Security problems go privately, as [SECURITY.md](SECURITY.md) says.

## Send a change

1. Read [How Alerse works](docs/design.md) for the part you change, and
   [Writing and refreshing patches](docs/patching.md) for how changes to Chromium are
   kept.
2. Fork, branch from `main`, and keep one change per pull request.
3. Code that Alerse owns lives in `src/` (new files) and `patches/` (changes to
   Chromium's files). Follow Chromium's style; `git cl format` or `clang-format
   --style=Chromium` formats C++.
4. Update `docs/design.md` in the same pull request when behavior changes: every
   decision is written down there.
5. Commit messages follow [Conventional Commits](https://www.conventionalcommits.org):
   `feat(sidebar): ...`, `fix(sync): ...`, `docs: ...`.
6. Say in the pull request how you tested it and on which platform.

By sending a change you agree that it is published under the project's
[BSD 3-Clause license](LICENSE).

## Where help matters most

- Testing on Linux, which the maintainer does not use daily.
- Comparing Alerse with Arc on Windows: Arc for Windows' look is not fully measured.
- Translations of Alerse's own strings (`src/chrome/app/alerse_strings.grdp`).
