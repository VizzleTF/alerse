# Versions

Arcomium has its own version, `MAJOR.MINOR.PATCH`, the same on every platform. A
release is tagged `vMAJOR.MINOR.PATCH` in each platform repository; its packages
carry that version in their names.

The version in `VERSION` is the next release's. Development is on the 0.1 line:
each release raises the patch number. The minor number goes up only when the
project decides on it.

## The Chromium version inside the browser

Each release gets its own Chromium version number: Windows' installer cannot
install a version over itself while the browser runs, so two releases on the
same Chromium could not update each other. `tools/rename_product.py`, which
every platform runs over the patched tree, writes the release's patch number
into the last two digits of `chrome/VERSION`'s PATCH:

    PATCH = Chromium's PATCH x 100 + Arcomium's PATCH

Arcomium 0.1.4 on Chromium 157.0.8103.38 is 157.0.8103.3804. `chrome://version`,
the Windows file version and install directory, and the macOS bundle version
show it. Websites do not: the User-Agent, its client hints and
`navigator.userAgentData` report Chromium's version, 157.0.8103.38, which
`chrome/VERSION` keeps as UPSTREAM_PATCH
(`patches/defaults/upstream-web-version.patch`). Release notes name Chromium's
version, as in "Arcomium 0.1.0, Chromium 156.0.8078.79".

The numbers only grow when:

- Arcomium's PATCH stays below 100; the script refuses 100 and up.
- Arcomium's PATCH does not go down while Chromium's version stays: a
  release that raises Arcomium's MINOR or MAJOR on the same Chromium keeps
  counting the patch number (0.1.7 is followed by 0.2.8, not 0.2.0), or comes
  with a newer Chromium. A newer Chromium outranks every earlier stamp. The
  platform repositories' release workflows (`.github/scripts/release-version.sh`)
  compare the stamp with that of the latest release, whose notes name its
  Chromium, and fail before building when it is not higher.
- Chromium's PATCH is at most 655, or the stamp no longer fits in the 16 bits
  of a Windows file version; the script refuses it too. Stable Chromium
  patch numbers stay far below that.
- Every release raises `VERSION`, a release of one platform repository
  too: the release workflows accept only the tag `v` + `VERSION`, as the
  update check below needs.

## Telling about a newer release

`tools/rename_product.py` also writes the release into `chrome/VERSION` as
ARCOMIUM_VERSION, and `chrome/browser/arcomium_update` compiles it in. Once a
day the browser reads the `tag_name` of the platform repository's latest
GitHub release and, when that `vMAJOR.MINOR.PATCH` is above its own version,
shows "Update to Arcomium MAJOR.MINOR.PATCH" at the top of its menus. So the
tag of every release must be its `VERSION`, with a `v`; drafts and
pre-releases are not "latest" and are not offered. A build whose
`chrome/VERSION` has no ARCOMIUM_VERSION does not check, nor does a browser
in headless mode or started with `--disable-background-networking`.

Any answer from GitHub counts, one with no release or with a tag that is
not `vMAJOR.MINOR.PATCH` too (the last release found stays): the next check
is a day later. Only no answer at all, GitHub's rate limit (HTTP 403, 429)
or a server error (5xx) is retried an hour later.

The Local State pref `arcomium.update_check.enabled` (true by default) turns
the check off; `arcomium.update_check.latest` and `.last_time` keep the last
answer, so the menu item survives restarts until the newer release is
installed.
