#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Makes the fallback fonts of the game: Noto Sans for Japanese, Simplified Chinese, Hebrew and Yiddish.

The game's own font, Pixelify Sans, has Latin letters only. Text in the other scripts uses these fonts, which are cut
down to the characters that the translations use, so that the game does not carry thousands of glyphs it never shows.
Run this after a translation gains a character the font lacks; the test `the bundled fonts cover every character of
every translation` says when that is.

    python3 tools/fonts/subset_fonts.py [--sources DIR] [--out DIR]

It needs fontTools and brotli (`pip install fonttools brotli`). The source fonts are the variable fonts of the Google
Fonts project, downloaded on the first run into --sources (default: a "fonts-src" folder next to the output) and checked
against the SHA-256 pinned below. Each is made into a Regular (400) and a Bold (700) font, subset, and written as WOFF2.
All of them are under the SIL Open Font License 1.1 (see apps/snake/fonts/LICENSE-noto-sans.txt).
"""

import argparse
import hashlib
import pathlib
import sys
import urllib.request
import xml.etree.ElementTree as ElementTree

BASE = "https://raw.githubusercontent.com/google/fonts/main/ofl"

# The family, the file to download, its pinned hash, the output name, and the languages whose characters it holds.
FONTS = [
    {
        "family": "Noto Sans JP",
        "url": f"{BASE}/notosansjp/NotoSansJP%5Bwght%5D.ttf",
        "sha256": "c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f",
        "name": "noto-sans-jp",
        "languages": ["ja"],
    },
    {
        "family": "Noto Sans SC",
        "url": f"{BASE}/notosanssc/NotoSansSC%5Bwght%5D.ttf",
        "sha256": "a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da",
        "name": "noto-sans-sc",
        "languages": ["zh_CN"],
    },
    {
        "family": "Noto Sans Hebrew",
        "url": f"{BASE}/notosanshebrew/NotoSansHebrew%5Bwdth,wght%5D.ttf",
        "sha256": "7ef36a2c3593758cdb622e1bdef4f84523e92fbc3ccc667438dd80ff54c2de88",
        "name": "noto-sans-hebrew",
        "languages": ["he", "yi"],
    },
]

WEIGHTS = {"400": 400, "700": 700}


def characters_of(translations_dir: pathlib.Path, languages: list[str]) -> list[int]:
    """The code points above ASCII that the catalogues of `languages` use. ASCII is Pixelify Sans's."""
    found = set()
    for language in languages:
        path = translations_dir / f"snake_{language}.ts"
        if not path.exists():
            continue
        for translation in ElementTree.parse(path).getroot().iter("translation"):
            found.update(ord(c) for c in (translation.text or "") if ord(c) > 127)
    return sorted(found)


def download(font: dict, sources: pathlib.Path) -> pathlib.Path:
    target = sources / font["url"].rsplit("/", 1)[1].replace("%5B", "[").replace("%5D", "]")
    if not target.exists():
        sources.mkdir(parents=True, exist_ok=True)
        print(f"downloading {font['url']}")
        urllib.request.urlretrieve(font["url"], target)  # noqa: S310 (a fixed https URL)
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    if digest != font["sha256"]:
        sys.exit(f"{target} has the SHA-256 {digest}, not the pinned {font['sha256']}")
    return target


def main() -> None:
    from fontTools import subset, ttLib
    from fontTools.varLib import instancer

    root = pathlib.Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=pathlib.Path, default=root / "apps/snake/fonts")
    parser.add_argument("--sources", type=pathlib.Path, default=None)
    parser.add_argument("--translations", type=pathlib.Path, default=root / "apps/snake/translations")
    args = parser.parse_args()
    sources = args.sources or args.out.parent / "fonts-src"

    args.out.mkdir(parents=True, exist_ok=True)
    for font in FONTS:
        characters = characters_of(args.translations, font["languages"])
        if not characters:
            print(f"{font['family']}: no characters in {font['languages']}, skipped")
            continue
        source = download(font, sources)
        for label, weight in WEIGHTS.items():
            variable = ttLib.TTFont(source)
            axes = {"wght": weight}
            if any(axis.axisTag == "wdth" for axis in variable["fvar"].axes):
                axes["wdth"] = 100
            static = instancer.instantiateVariableFont(variable, axes, updateFontNames=True)

            options = subset.Options()
            options.flavor = "woff2"
            options.layout_features = ["*"]
            options.name_IDs = ["*"]
            options.notdef_outline = True
            options.hinting = False
            subsetter = subset.Subsetter(options)
            subsetter.populate(unicodes=[0x20, 0xA0, *characters])
            subsetter.subset(static)

            target = args.out / f"{font['name']}-{label}.woff2"
            static.flavor = "woff2"
            static.save(target)
            print(f"{target.name}: {len(characters)} characters, {target.stat().st_size} bytes")


if __name__ == "__main__":
    main()
