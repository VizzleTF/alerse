"""Writes Alerse's app icon over Chromium's branding files.

The icon is drawn, not generated: artwork/ holds it as a 1024 px tile
(alerse_1024.png, a rounded white square with the brown and green "A" of an
alerce's trunk and branch), the same tile prepared at 16 to 512 px, and
alerse.ico, the Windows icon with 16 to 256 px images. This script only puts
them where Chromium's build looks: every file lands in branding/ at its path in
the Chromium tree, and each platform's build copies branding/ over the source
tree after patching. macOS icons follow Apple's grid (the tile takes 824 of
1024 pixels), and on macOS the script also compiles app.icns (iconutil) and
Assets.car (actool). iOS masks the corners itself, so its icons are the tile's
inside, without the rounded corners.

Usage:
  python tools/make_icons.py
"""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parent.parent
ART = ROOT / 'artwork'
OUT = ROOT / 'branding'
THEME = 'chrome/app/theme'

# The sizes artwork/ has prepared; any other size is scaled from the nearest
# larger one.
PREPARED = (16, 24, 32, 48, 64, 128, 256, 512, 1024)


def tile(size):
    """The rounded tile at `size` px."""
    for prepared in PREPARED:
        if prepared >= size:
            image = Image.open(ART / f'alerse_{prepared}.png').convert('RGBA')
            return image if prepared == size else image.resize((size, size), Image.LANCZOS)
    return Image.open(ART / 'alerse_1024.png').convert('RGBA').resize(
        (size, size), Image.LANCZOS)


def mac_tile(size):
    # Apple's icon grid: the shape takes 824 of 1024 pixels, centred.
    inner = round(size * 824 / 1024)
    shape = tile(inner)
    canvas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    canvas.paste(shape, ((size - inner) // 2, (size - inner) // 2), shape)
    return canvas


def full_square(size):
    """The tile without its rounded corners, filling the square (iOS)."""
    big = Image.open(ART / 'alerse_1024.png').convert('RGBA')
    # The corners' radius is about a fifth of the side; cutting 64 px from
    # every side leaves no transparent corner and keeps the whole letter.
    inside = big.crop((64, 64, 1024 - 64, 1024 - 64))
    background = Image.new('RGBA', inside.size, (255, 255, 255, 255))
    background.alpha_composite(inside)
    return background.resize((size, size), Image.LANCZOS)


def save(path, image):
    target = OUT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    image.save(target)


def make_ios_icons():
    res = 'ios/chrome/app/resources'
    legacy = f'{res}/ChromiumIconsLegacy.xcassets/AppIcon.appiconset'
    icon = full_square(1024)
    save(f'{legacy}/Icon-1024.png', icon)
    # The dark variant keeps the letter's colours; the tinted one is grey,
    # which iOS colours with the user's tint.
    save(f'{legacy}/Icon-1024Dark.png', icon)
    save(f'{legacy}/Icon-1024Tinted.png', ImageOps.grayscale(icon).convert('RGBA'))
    # The single layer of the iOS 26 icon, which Icon Composer files describe.
    # The file keeps its name, which ios/arcomium-ios-icon.patch refers to.
    save(f'{res}/ChromiumIcons.xcassets/AppIcon.icon/Assets/arcomium.png', icon)


def make_mac_icons():
    """app.icns and Assets.car, which Chromium's mac build ships prebuilt."""
    mac = OUT / f'{THEME}/chromium/mac'
    appiconset = mac / 'Assets.xcassets/AppIcon.appiconset'
    for size in (16, 32, 64, 128, 256, 512, 1024):
        save(appiconset.relative_to(OUT) / f'appicon_{size}.png', mac_tile(size))
    iconset = mac / 'Assets.xcassets/Icon.iconset'
    save(iconset.relative_to(OUT) / 'icon_256x256.png', mac_tile(256))
    save(iconset.relative_to(OUT) / 'icon_256x256@2x.png', mac_tile(512))
    if sys.platform != 'darwin':
        print('Not on macOS: app.icns and Assets.car left as they are '
              '(run this script on a Mac to make them)')
        return
    with tempfile.TemporaryDirectory() as tmp:
        full = Path(tmp) / 'AppIcon.iconset'
        full.mkdir()
        for size in (16, 32, 128, 256, 512):
            mac_tile(size).save(full / f'icon_{size}x{size}.png')
            mac_tile(size * 2).save(full / f'icon_{size}x{size}@2x.png')
        subprocess.run(['iconutil', '-c', 'icns', '-o', str(mac / 'app.icns'), str(full)],
                       check=True)
        compiled = Path(tmp) / 'compiled'
        compiled.mkdir()
        subprocess.run(['xcrun', 'actool', '--compile', str(compiled),
                        '--platform', 'macosx', '--minimum-deployment-target', '12.0',
                        '--app-icon', 'AppIcon', '--output-partial-info-plist',
                        str(Path(tmp) / 'partial.plist'),
                        str(mac / 'Assets.xcassets')], check=True, stdout=subprocess.DEVNULL)
        shutil.copy(compiled / 'Assets.car', mac / 'Assets.car')


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--mac-only':
        make_mac_icons()
        return
    for size in (16, 24, 48, 64, 128, 256):
        save(f'{THEME}/chromium/product_logo_{size}.png', tile(size))
    for scale, factor in (('default_100_percent', 1), ('default_200_percent', 2)):
        for size in (16, 32):
            save(f'{THEME}/{scale}/chromium/product_logo_{size}.png', tile(size * factor))
    save(f'{THEME}/chromium/win/tiles/Logo.png', tile(600))
    save(f'{THEME}/chromium/win/tiles/SmallLogo.png', tile(176))
    for name in ('chromium.ico', 'app_list.ico'):
        target = OUT / f'{THEME}/chromium/win/{name}'
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(ART / 'alerse.ico', target)
    make_mac_icons()
    make_ios_icons()
    print(f'Wrote icons to {OUT}')


if __name__ == '__main__':
    main()
