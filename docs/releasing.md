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

3. Optional, to have a package out before CI finishes (a full build takes GitHub's
   runners up to two days, in chained six-hour jobs): package a local x64 build
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

macOS is built on the build Mac from a Chromium 156 tree kept by hand
(`ungoogled-chromium-macos/build/src` in an alerse-mac checkout) until
ungoogled-chromium-macos releases 157; this section is for that tree. Releases are
pre-releases (signed with the build Mac's own identity, see alerse-mac README).

1. In the tree, set `PATCH` in `chrome/VERSION` to Chromium's patch × 100 + X.Y.Z's
   patch (Chromium 156.0.8078.79 and Alerse 0.1.13 give 7913) and
   `ALERSE_VERSION=X.Y.Z`.
2. Build in the tree, then package from the alerse-mac checkout's root, where
   `package.sh` is:

   ```sh
   ninja -C out/Release chrome
   OUT_DIR=out/Release ./package.sh vX.Y.Z   # ungoogled-chromium-macos/build/alerse_vX.Y.Z_macos_arm64.dmg
   ```

3. Check that the build opens a second window, closes it and quits without a crash
   (the test kit's `smoke` command, if you have it). Then write the checksum and
   create the pre-release:

   ```sh
   shasum -a 256 alerse_vX.Y.Z_macos_arm64.dmg > SHA256SUMS
   gh release create vX.Y.Z -R VizzleTF/alerse-mac --prerelease --verify-tag \
     --title "Alerse X.Y.Z for macOS" --notes-file notes.md \
     alerse_vX.Y.Z_macos_arm64.dmg SHA256SUMS
   ```

   The tag comes first, on an alerse-mac commit that names the release.

## 4. Linux (alerse-linux)

Not released while the series targets Chromium 157 and
ungoogled-chromium-portablelinux is on 156. Once it moves to 157: point the `alerse`
submodule at the release commit, push, and push the tag; `release.yml` builds and
publishes x86_64 and arm64.

## 5. Overview release

Create `vX.Y.Z` in this repository with a table of the platforms: each row links to
that platform's release, or says why it has none, followed by the changes. Edit the
table when a platform's release appears later.

```sh
gh release create vX.Y.Z -R VizzleTF/alerse --verify-tag --title "Alerse X.Y.Z" --notes-file notes-core.md
gh release edit vX.Y.Z -R VizzleTF/alerse --notes-file notes-core.md   # later changes
```

## iOS

alerse-ios is built from source only and has no releases (see
[patching.md](patching.md#ios-patches)).

## Release notes

- Open with the platform, the Chromium version and the build number, as in
  "Alerse 0.1.13 for Windows, Chromium 157.0.8103.50 (ungoogled-chromium). Build
  number 157.0.8103.5013."
- List the changes since the previous release, what the user sees first.
- End with known gaps: an unsigned installer, a local first upload, a platform that
  is behind.
