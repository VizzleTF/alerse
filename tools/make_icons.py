"""Draws Arcomium's app icon and writes it over Chromium's branding files.

The glyph is the sidebar's logo (an arch crossed by a swoosh, see
ArcLogoButton) in white on a rounded square with a blue-violet gradient. The
files land in branding/ at their paths in the Chromium tree; build.py copies
them over the source tree after patching.

Usage:
  python tools/make_icons.py
"""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'branding'
THEME = 'chrome/app/theme'

# The logo on its 16-unit grid: cubic Bezier segments, as in ArcLogoButton.
ARCH = [((2.5, 14), (3.5, 4), (6, 1.5), (8, 1.5)),
        ((8, 1.5), (10, 1.5), (12.5, 4), (13.5, 14))]
SWOOSH = [((1, 10.5), (5, 6.5), (10, 6.5), (15, 9.5))]
GLYPH_CENTER = (8, 7.75)

TOP = (0x5B, 0x6C, 0xFF)
BOTTOM = (0x8A, 0x4D, 0xFF)
SUPERSAMPLE = 8


def bezier(p0, p1, p2, p3, steps=200):
    for i in range(steps + 1):
        t = i / steps
        u = 1 - t
        yield (u ** 3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] +
               t ** 3 * p3[0],
               u ** 3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] +
               t ** 3 * p3[1])


def draw(size):
    big = size * SUPERSAMPLE
    # The rounded square with a vertical gradient.
    gradient = Image.new('RGBA', (big, big))
    pixels = gradient.load()
    for y in range(big):
        f = y / (big - 1)
        color = tuple(round(a + (b - a) * f) for a, b in zip(TOP, BOTTOM)) + (255,)
        for x in range(big):
            pixels[x, y] = color
    mask = Image.new('L', (big, big), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, big - 1, big - 1), radius=round(big * 0.225), fill=255)
    icon = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    icon.paste(gradient, (0, 0), mask)

    # The glyph: 14 grid units across ~62% of the icon, strokes thickened a
    # little at small sizes to stay legible.
    scale = big * 0.62 / 14
    stroke = big * (0.085 if size <= 24 else 0.07)
    offset = (big / 2 - GLYPH_CENTER[0] * scale, big / 2 - GLYPH_CENTER[1] * scale)
    pen = ImageDraw.Draw(icon)
    for curve in (ARCH, SWOOSH):
        for segment in curve:
            for x, y in bezier(*segment):
                cx, cy = offset[0] + x * scale, offset[1] + y * scale
                r = stroke / 2
                pen.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255, 255))
    return icon.resize((size, size), Image.LANCZOS)


def save_png(path, size):
    target = OUT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    draw(size).save(target)


def main():
    for size in (16, 24, 48, 64, 128, 256):
        save_png(f'{THEME}/chromium/product_logo_{size}.png', size)
    for scale, factor in (('default_100_percent', 1), ('default_200_percent', 2)):
        for size in (16, 32):
            save_png(f'{THEME}/{scale}/chromium/product_logo_{size}.png',
                     size * factor)
    save_png(f'{THEME}/chromium/win/tiles/Logo.png', 600)
    save_png(f'{THEME}/chromium/win/tiles/SmallLogo.png', 176)

    sizes = (16, 24, 32, 48, 64, 128, 256)
    images = [draw(size) for size in sizes]
    for name in ('chromium.ico', 'app_list.ico'):
        target = OUT / f'{THEME}/chromium/win/{name}'
        target.parent.mkdir(parents=True, exist_ok=True)
        images[-1].save(target, sizes=[(s, s) for s in sizes],
                        append_images=images[:-1])
    print(f'Wrote icons to {OUT}')


if __name__ == '__main__':
    main()
