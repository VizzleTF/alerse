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

## Add a patch

1. Make the change in `src/`.
2. Export it into the submodule:

   ```cmd
   git -C src diff --relative > arcomium\patches\<area>\<name>.patch
   ```

3. Add `<area>/<name>.patch` to `arcomium\patches\series`.
4. Check that the whole series still applies to a clean tree:

   ```cmd
   cd src
   set GIT_INDEX_FILE=%TEMP%\arcomium-check.index
   git read-tree HEAD
   for /f "eol=# delims=" %p in (..\arcomium\patches\series) do git apply --cached ..\arcomium\patches\%p
   ```

## Refresh after a Chromium update

Set up the working tree on the new tag, then for each patch in series order run
`git apply --3way`, resolve conflicts, and export it again as in "Add a patch".
