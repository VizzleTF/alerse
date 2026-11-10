# Writing and refreshing patches

Alerse patches are diffs against the Chromium tree after the ungoogled-chromium and
platform patches. You edit them in a separate working tree, inside a platform checkout
(the examples use [alerse-win](https://github.com/VizzleTF/alerse-win)), and export
them into this repository, which is that checkout's `alerse` submodule.

## Set up a working tree

Run from the root of an alerse-win checkout. `src/` is ignored by git.

```cmd
cd ungoogled-chromium-windows
python ungoogled-chromium\utils\downloads.py unpack -i ungoogled-chromium\downloads.ini -c build\download_cache ..\src
python ungoogled-chromium\utils\patches.py apply ..\src ungoogled-chromium\patches patches
cd ..\src
git init -q && git add -A && git commit -qm base
```

Then apply the patches listed in `alerse\patches\series`, in order:

```cmd
git apply ..\alerse\patches\spaces\space-sidebar.patch
```

and copy Alerse's own files over the tree:

```cmd
xcopy /e /y /q ..\alerse\src .
```

## Alerse's own files

Files Alerse adds (the sidebar, Spaces, the command bar, sync) are not in patches.
They live in `src/` at their paths in the Chromium tree, and each platform's build
copies them over the source tree after the patches, as it does `branding/`. Patches
change only Chromium's own files, including the `BUILD.gn` lines that build Alerse's.

Edit these files in the working tree, then copy them back into `alerse\src\`. A new
file goes there too, never into a patch.

Text the user sees goes in `src/chrome/app/alerse_strings.grdp` as an `IDS_ARCOMIUM_*`
message, read with `l10n_util::GetStringUTF16()`; a `$1` in a message goes inside
`<ph>`, or grit fails. A new message rebuilds every resource, so batch them. Colors are `kColorAlerse*` ids, set in
`src/chrome/browser/ui/color/alerse_color_mixer.cc` for dark, light and high-contrast
frames; a view reads them from its color provider instead of holding an `SkColor`.

## Add a patch

Each hunk carries a `// Alerse: ...` comment that says why. Keep logic in Alerse's own files and only the hook in
the patch: a large hunk in a Chromium file is what conflicts on the next Chromium.

Never export a patch from the tree after ungoogled-chromium's domain substitution: its
rewritten names (`9oo91e`, `qjz9zk`, `m1cr050ft`, `ch40m1um`) must not appear in a patch.
Check every new patch for them.

1. Make the change in `src/`.
2. Export it into the submodule:

   ```cmd
   git -C src diff --relative > alerse\patches\<area>\<name>.patch
   ```

3. Add `<area>/<name>.patch` to `alerse\patches\series`.
4. Check that the whole series still applies to a clean tree. In
   [alerse-linux](https://github.com/VizzleTF/alerse-linux), `./check-patches.sh`
   applies it to the Chromium release source in a few minutes, Alerse's patches with
   no fuzz; CI runs it on every push. In the working tree:

   ```cmd
   cd src
   set GIT_INDEX_FILE=%TEMP%\alerse-check.index
   git read-tree HEAD
   for /f "eol=# delims=" %p in (..\alerse\patches\series) do git apply --cached ..\alerse\patches\%p
   ```

## Refresh after a Chromium update

Let git replay the series with three-way merges:

1. Set up the working tree on the new tag as above, without Alerse's patches, and
   commit it (the new base).
2. Fetch the old base (the previous working tree's base commit) into the same
   repository.
3. Run:

   ```cmd
   python alerse\tools\rebase_patches.py import --repo src --base OLD
   git -C src config rerere.enabled true
   git -C src rebase --onto NEW OLD alerse-patches
   python alerse\tools\rebase_patches.py export --repo src --base NEW
   ```

   `import` turns each patch into a commit on the old base (message
   `alerse-patch: <path>`), the rebase moves them to the new one (resolve conflicts
   where it stops, then `git rebase --continue`), and `export` writes every commit back
   to its patch file. rerere remembers the resolutions for the next update.

The same branch works for everyday changes: amend the patch's commit on
`alerse-patches`, then `export --base NEW`. Check that the series still applies to the
base, as in Add a patch, before committing the patch files.

When Chromium already does what a hunk did, drop the hunk (157 turned the `kCtrlTabMru`
feature on by default, so `defaults/feature-defaults.patch` lost that hunk; it still
turns the pref of the same name on). When an
API changed, adapt the hunk to it, not Chromium back to the old API.

Alerse follows ungoogled-chromium, so a new Chromium is possible only once
ungoogled-chromium and the platform's port have released it. A port that is behind
can build from the core's branch for its version (`chromium-156`). Today only
alerse-ios pins that branch; alerse-linux and alerse-mac pin `main` (0.1.4, the
157 series) while their ports are still on 156, so they do not build until those ports
release 157. Their releases so far were built from the 156 series.

## iOS patches

ungoogled-chromium has no iOS build, so iOS has its own series, `patches/series-ios`,
applied to a clean Chromium iOS checkout of the same version. It shares
`sync/alerse-sync-core.patch`, the platform-independent sync client, with the desktop
series; keep that patch applying to both trees.

## Branding and the product name

Binary files, such as the app icons, live in `branding/` at their paths in the Chromium
tree; each platform's build copies them over the source tree after patching. The icon
itself is drawn: `artwork/` holds it (`alerse_1024.png`, the same tile prepared at 16 to
512 px, `alerse.ico`), and `tools/make_icons.py` writes every branding file from it
(product logos, Windows tiles and `.ico`, macOS on Apple's grid, iOS without rounded
corners). Run it on a Mac to compile `app.icns` (iconutil) and `Assets.car` (actool) too;
`--mac-only` writes just those. `tools/rename_product.py` then renames Chromium to Alerse in the strings and moves the
translations to the new message IDs. Neither belongs in a patch.

## Building through a junction

Build from the tree's real path, kept short for MAX_PATH, not through a junction. A
junction gives esbuild two spellings of the same files, and DevTools' bundles then
imported some modules from paths outside the output (`devtools://devtools/src/chromium-.../`,
HTTP 404): DevTools opened blank in 0.1.3. `defaults/devtools-bundle-paths.patch` places
imports of `front_end` modules from their path inside `front_end`, whatever the path. After
a build, no resource in `resources.pak` should import a path containing the tree's
directory name (`chromium-<version>/third_party`).

## Incremental Windows builds

- Run ninja with `out\Default` on `PATH` (V8's build runs `gn` from there) and
  `DEPOT_TOOLS_WIN_TOOLCHAIN=0`, so it uses the installed Visual Studio.
- A change to `chrome/VERSION` (every release's stamp) rebuilds a large part of the
  tree and re-runs `chrome/updater`'s IDL step, which fails in `midl.py` with an
  `AssertionError` on the empty `.tlb` files the pruned tarball leaves under
  `third_party/win_build_output/midl/`. Delete those empty files; CI does not hit this
  (ungoogled-chromium-windows patches it out).
- A changed `cxxbridge.exe` command line makes ninja rebuild the whole tree.
