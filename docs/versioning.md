# Versions

Arcomium has its own version, `MAJOR.MINOR.PATCH`, the same on every platform. A
release is tagged `vMAJOR.MINOR.PATCH` in each platform repository; its packages
carry that version in their names.

The version in `VERSION` is the next release's. Development is on the 0.1 line:
each release raises the patch number. The minor number goes up only when the
project decides on it.

Chromium's own version stays as it is inside the browser: the User-Agent,
extensions' `minimum_chrome_version`, `chrome://version`, the macOS bundle
version and the Windows file version use it. Release notes name it, as in
"Arcomium 0.1.0, Chromium 156.0.8078.79".
