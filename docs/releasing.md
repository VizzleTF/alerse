# Making a release

How to publish Alerse X.Y.Z on every platform that can build it. The version rules
and the list of past releases are in [versioning.md](versioning.md).

Each platform repository has its own GitHub Release with the packages; this
repository gets an overview release that links to them. Every platform release must
carry its packages and a `SHA256SUMS`: never publish an empty one.

## 1. Core

1. Raise `VERSION` to X.Y.Z and add the release's row to the table in
   [versioning.md](versioning.md).
2. Commit it as `chore(release): X.Y.Z`, push `main`, and tag that commit:

   ```sh
   git tag vX.Y.Z && git push origin vX.Y.Z
   ```

## 2. Windows (alerse-win)

1. Point the `alerse` submodule at the release commit, commit, push `main`.
2. Tag and push the tag: `release.yml` builds x64 and arm64 and adds each
   architecture's installer and zip to the release as soon as it is built.

   ```sh
   git tag vX.Y.Z && git push origin vX.Y.Z
   ```

3. Optional, to have a package out before CI's two days: package a local x64 build
   with `python package.py` in `ungoogled-chromium-windows`, rename the files to
   `alerse_vX.Y.Z_installer_x64.exe` and `alerse_vX.Y.Z_windows_x64.zip`, write
   `SHA256SUMS`, and create the release with them:

   ```sh
   gh release create vX.Y.Z -R VizzleTF/alerse-win --verify-tag \
     --title "Alerse X.Y.Z for Windows" --notes-file notes.md \
     alerse_vX.Y.Z_installer_x64.exe alerse_vX.Y.Z_windows_x64.zip SHA256SUMS
   ```

   CI then replaces the x64 files and adds arm64.

A change to `chrome/VERSION` rebuilds most of the tree, so a local build of a release
takes hours, not minutes ([patching.md](patching.md#incremental-windows-builds)).

## 3. macOS (alerse-mac)

macOS is built on the build Mac from a Chromium 156 tree kept by hand until
ungoogled-chromium-macos releases 157. Releases are pre-releases (ad hoc signed).

1. In the tree, set `PATCH` in `chrome/VERSION` to Chromium's patch × 100 + Y.Z's
   patch (156.0.8078.79 and 0.1.13 give 7913) and `ALERSE_VERSION=X.Y.Z`.
2. Build and package:

   ```sh
   ninja -C out/Release chrome
   OUT_DIR=out/Release ./package.sh vX.Y.Z   # build/alerse_vX.Y.Z_macos_arm64.dmg
   ```

3. Run the kit's `smoke` on the build, then create the pre-release in alerse-mac with
   the dmg and `SHA256SUMS`.

## 4. Linux (alerse-linux)

Not released while the series targets Chromium 157 and
ungoogled-chromium-portablelinux is on 156. Once it moves to 157: point the `alerse`
submodule at the release commit, push, and push the tag; `release.yml` builds and
publishes x86_64 and arm64.

## 5. Overview release

Create `vX.Y.Z` in this repository with a table of the platforms: each row links to
that platform's release, or says why it has none, followed by the changes. Edit the
table when a platform's release appears later.

## Release notes

- Open with the platform, the Chromium version and the build number, as in
  "Alerse 0.1.13 for Windows, Chromium 157.0.8103.50 (ungoogled-chromium). Build
  number 157.0.8103.5013."
- List the changes since the previous release, what the user sees first.
- End with known gaps: an unsigned installer, a local first upload, a platform that
  is behind.
