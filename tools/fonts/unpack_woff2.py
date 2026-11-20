#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Unpacks WOFF2 fonts into plain TrueType fonts, glyph for glyph.

The game bundles TrueType fonts, not WOFF2: Qt reads a WOFF2 file only where its font engine can decompress it (FreeType
built with Brotli, or an operating system that does). The Qt of Windows, and of the platforms that use Qt's own FreeType,
cannot, and the game would draw all its text in some other font. A TrueType file is read everywhere.

    python3 tools/fonts/unpack_woff2.py OUT_DIR FONT.woff2 [FONT.woff2 ...]

It needs fontTools and brotli (`pip install fonttools brotli`). Each font is written to OUT_DIR under the same name with
the extension .ttf (or .otf for a font with CFF outlines). The tables, including the name table with the copyright and
licence of the font, are kept as they are, and nothing is added, so this is not a modification of the font under the SIL
Open Font License. Pixelify Sans of the design package (docs/design/fonts/) is unpacked with this into apps/snake/fonts/;
the Noto fonts there come from subset_fonts.py.
"""

import pathlib
import sys

from fontTools.ttLib import TTFont


def main() -> None:
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    out = pathlib.Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    for name in sys.argv[2:]:
        source = pathlib.Path(name)
        font = TTFont(source)
        font.flavor = None
        suffix = ".otf" if font.sfntVersion == "OTTO" else ".ttf"
        target = out / (source.stem + suffix)
        font.save(target)
        print(f"{target.name}: {target.stat().st_size} bytes")


if __name__ == "__main__":
    main()
