#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Draws the application icon, for Android and iOS, from rectangles, as the rest of the game's art is drawn.

    python3 tools/icons/make_icons.py

It needs Pillow (`pip install pillow`) and writes:

  apps/snake/packaging/ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png   the iOS icon, 1024 by 1024, no alpha
  apps/snake/packaging/android/package-source/res/mipmap-xxxhdpi/ic_launcher_foreground.png
                                                                                the Android adaptive icon's mark, 432 by 432

The picture is the head of the snake, face on, with the eyes and tongue of the board and a blueberry in the corner, on
the yellow of the page, on a grid of 16 by 16 squares. Colours are the design tokens (docs/design/tokens.json).
"""

import pathlib

from PIL import Image, ImageDraw

BACKGROUND = (0xFF, 0xC9, 0x3C)
INK = (0x2B, 0x1D, 0x52)
SHADOW = (0x1A, 0x10, 0x35)
SURFACE = (0xFF, 0xF1, 0xD6)
ACCENT = (0xFF, 0x6B, 0x4A)
FOOD = (0x4C, 0x6B, 0xFF)
SHINE = (0xDD, 0xE3, 0xFF)
LEAF = (0x5B, 0xD1, 0x6A)
TONGUE = (0xFF, 0x2E, 0x63)
EYE = (0xFF, 0xFF, 0xFF)

GRID = 16


def mark(size: int, background: bool) -> Image.Image:
    """The picture on a square of `size` pixels; transparent behind it unless `background`."""
    image = Image.new("RGBA", (size, size), BACKGROUND + (255,) if background else (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    unit = size / GRID

    def box(x: float, y: float, w: float, h: float, colour) -> None:
        if w <= 0 or h <= 0:
            return
        draw.rectangle(
            [round(x * unit), round(y * unit), round((x + w) * unit) - 1, round((y + h) * unit) - 1], fill=colour
        )

    def block(x: float, y: float, w: float, h: float, colour, cut: float = 0.5) -> None:
        """A square-cornered block with its corners cut a step, as the board's pieces are drawn."""
        box(x + cut, y, w - 2 * cut, h, colour)
        box(x, y + cut, w, h - 2 * cut, colour)

    def shaded(x: float, y: float, w: float, h: float, colour, cut: float = 0.5, drop: float = 0.5) -> None:
        block(x, y + drop, w, h, SHADOW, cut)
        block(x, y, w, h, colour, cut)
        # The shade along the bottom, as the segments of the board have.
        box(x + cut, y + h - 1.0, w - 2 * cut, 1.0 - cut, tuple(round(c * 0.78) for c in colour))
        box(x, y + h - 1.0 - cut, w, cut, tuple(round(c * 0.78) for c in colour))

    # The head, face on: coral, with two cream stripes across the top, as a snake of the board.
    shaded(3, 5, 10, 9, ACCENT, cut=1.0, drop=0.6)
    box(5, 5, 6, 1, SURFACE)
    box(6, 6.5, 4, 0.7, SURFACE)
    # The eyes: white, with an ink pupil that looks a little to the side.
    for x in (4.6, 9.0):
        box(x, 7.3, 2.4, 2.6, EYE)
        box(x + 0.9, 8.2, 1.2, 1.4, INK)
    # The tongue, forked, below the mouth.
    box(7.5, 12.2, 1.0, 2.2, TONGUE)
    box(6.9, 14.2, 0.7, 0.9, TONGUE)
    box(8.4, 14.2, 0.7, 0.9, TONGUE)
    # The blueberry it is after, in the corner, with its shine and leaf.
    shaded(11.4, 1.4, 3.2, 3.2, FOOD, cut=0.6, drop=0.4)
    box(12.0, 2.0, 0.8, 0.8, SHINE)
    box(12.5, 0.7, 1.2, 0.7, LEAF)
    return image


def main() -> None:
    root = pathlib.Path(__file__).resolve().parents[2]
    ios = root / "apps/snake/packaging/ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png"
    android = root / "apps/snake/packaging/android/package-source/res/mipmap-xxxhdpi/ic_launcher_foreground.png"

    # iOS: full-bleed and without alpha: the system rounds the corners.
    mark(1024, background=True).convert("RGB").save(ios)

    # Android: the adaptive icon's mark is 108 dp, of which the middle 66 percent is always seen: the picture goes in that.
    canvas = Image.new("RGBA", (432, 432), (0, 0, 0, 0))
    inner = mark(288, background=False)
    canvas.paste(inner, (72, 72), inner)
    canvas.save(android)
    print(f"wrote {ios.relative_to(root)} and {android.relative_to(root)}")


if __name__ == "__main__":
    main()
