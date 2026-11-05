# Writing and refreshing patches

Arcomium patches are diffs against the Chromium tree after the ungoogled-chromium and
platform patches. You edit them in a separate working tree, inside a platform checkout
(the examples use [arcomium-win](https://github.com/VizzleTF/arcomium-win)), and export
them into this repository, which is that checkout's `arcomium` submodule.

## Set up a working tree

Run from the root of an arcomium-win checkout. `src/` is ignored by git.

```cmd
cd ungoogled-chromium-windows
python ungoogled-chromium\utils\downloads.py unpack -i ungoogled-chromium\downloads.ini -c build\download_cache ..\src
python ungoogled-chromium\utils\patches.py apply ..\src ungoogled-chromium\patches patches
cd ..\src
git init -q && git add -A && git commit -qm base
```

Then apply the patches listed in `arcomium\patches\series`, in order:

```cmd
git apply ..\arcomium\patches\spaces\space-sidebar.patch
```

and copy Arcomium's own files over the tree:

```cmd
xcopy /e /y /q ..\arcomium\src .
```

## Arcomium's own files

Files Arcomium adds (the sidebar, Spaces, the command bar, sync) are not in patches.
They live in `src/` at their paths in the Chromium tree, and each platform's build
copies them over the source tree after the patches, as it does `branding/`. Patches
change only Chromium's own files, including the `BUILD.gn` lines that build Arcomium's.

Edit these files in the working tree, then copy them back into `arcomium\src\`. A new
file goes there too, never into a patch.

Text the user sees goes in `src/chrome/app/arcomium_strings.grdp` as an `IDS_ARCOMIUM_*`
message, read with `l10n_util::GetStringUTF16()`. Colors are `kColorArcomium*` ids, set in
`src/chrome/browser/ui/color/arcomium_color_mixer.cc` for dark, light and high-contrast
frames; a view reads them from its color provider instead of holding an `SkColor`.

## Add a patch

1. Make the change in `src/`.
2. Export it into the submodule:

   ```cmd
   git -C src diff --relative > arcomium\patches\<area>\<name>.patch
   ```

3. Add `<area>/<name>.patch` to `arcomium\patches\series`.
4. Check that the whole series still applies to a clean tree. In
   [arcomium-linux](https://github.com/VizzleTF/arcomium-linux), `./check-patches.sh`
   applies it to the Chromium release source in a few minutes, Arcomium's patches with
   no fuzz; CI runs it on every push. In the working tree:

   ```cmd
   cd src
   set GIT_INDEX_FILE=%TEMP%\arcomium-check.index
   git read-tree HEAD
   for /f "eol=# delims=" %p in (..\arcomium\patches\series) do git apply --cached ..\arcomium\patches\%p
   ```

## Refresh after a Chromium update

Set up the working tree on the new tag, then for each patch in series order run
`git apply --3way`, resolve conflicts, and export it again as in "Add a patch".

## iOS patches

ungoogled-chromium has no iOS build, so iOS has its own series, `patches/series-ios`,
applied to a clean Chromium iOS checkout of the same version. It shares
`sync/arcomium-sync-core.patch`, the platform-independent sync client, with the desktop
series; keep that patch applying to both trees.

## Branding and the product name

Binary files, such as the app icons from `tools/make_icons.py`, live in `branding/` at
their paths in the Chromium tree; each platform's build copies them over the source tree
after patching. `tools/rename_product.py` then renames Chromium to Arcomium in the
strings and moves the translations to the new message IDs. Neither belongs in a patch.
