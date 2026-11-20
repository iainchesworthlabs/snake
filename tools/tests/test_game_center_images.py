# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for tools/game-center/make_images.py and the twelve Game Center achievement images it draws (CTest label: unit).

Run with: python3 -m unittest discover -s tools/tests -p "test_game_center_images.py" -v
The committed images are in apps/snake/packaging/ios/game-center/images. App Store Connect wants each to be 1024 by 1024
pixels, RGB without an alpha channel and at least 72 pixels per inch. The script is run again into a temporary folder:
twice, which must give the same bytes, and the pictures must be those of the committed files (the decoded pixels are
compared, not the bytes, because another zlib may compress the same pixels to other bytes).
"""

from __future__ import annotations

import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/game-center/make_images.py"
IMAGES = ROOT / "apps/snake/packaging/ios/game-center/images"
SLUGS = [
    "wrap",
    "bonus",
    "combo",
    "best",
    "speed",
    "berries100",
    "berries1000",
    "portals",
    "portal",
    "hotpotato",
    "shed",
    "allmodes",
]
SIDE = 1024
CELL = 32
# The subject stays inside the middle 22 of the 32 cells, so that a circle can crop the image.
SAFE_FIRST, SAFE_END = 5 * CELL, 27 * CELL
BACKGROUND = bytes((0xFF, 0xC9, 0x3C))  # the sunshine of docs/design/tokens.json


def chunks(data: bytes) -> list[tuple[bytes, bytes]]:
    """The chunks of a PNG file, as (type, data)."""
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG file"
    found = []
    at = 8
    while at < len(data):
        (length,) = struct.unpack(">I", data[at : at + 4])
        kind = data[at + 4 : at + 8]
        body = data[at + 8 : at + 8 + length]
        (crc,) = struct.unpack(">I", data[at + 8 + length : at + 12 + length])
        assert crc == zlib.crc32(kind + body), f"bad checksum in {kind!r}"
        found.append((kind, body))
        at += 12 + length
    return found


def decode(data: bytes) -> bytes:
    """The raw RGB rows of a PNG made of filter types 0, 1 or 2 (which is all that make_images.py writes)."""
    found = chunks(data)
    width, height, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", found[0][1])
    assert (width, height, depth, colour, interlace) == (SIDE, SIDE, 8, 2, 0)
    packed = zlib.decompress(b"".join(body for kind, body in found if kind == b"IDAT"))
    stride = width * 3
    rows = []
    previous = bytes(stride)
    for y in range(height):
        line = packed[y * (stride + 1) : (y + 1) * (stride + 1)]
        mode, row = line[0], line[1:]
        if mode == 0:
            current = bytes(row)
        elif mode == 1:
            out = bytearray(row)
            for i in range(3, stride):
                out[i] = (out[i] + out[i - 3]) & 0xFF
            current = bytes(out)
        elif mode == 2:
            current = bytes((a + b) & 0xFF for a, b in zip(row, previous))
        else:
            raise AssertionError(f"filter type {mode} is not handled by this test")
        rows.append(current)
        previous = current
    return b"".join(rows)


def run_script(out: Path) -> None:
    subprocess.run([sys.executable, str(SCRIPT), "--out", str(out)], check=True, capture_output=True)


class GameCenterImages(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls._folder = tempfile.TemporaryDirectory()
        base = Path(cls._folder.name)
        cls.first = base / "first"
        cls.second = base / "second"
        run_script(cls.first)
        run_script(cls.second)

    @classmethod
    def tearDownClass(cls) -> None:
        cls._folder.cleanup()

    def test_each_image_exists(self) -> None:
        for slug in SLUGS:
            with self.subTest(slug=slug):
                self.assertTrue((IMAGES / f"{slug}.png").is_file(), f"{slug}.png is missing from {IMAGES}")
        extra = sorted(path.name for path in IMAGES.glob("*") if path.name not in {f"{slug}.png" for slug in SLUGS})
        self.assertEqual(extra, [], "files that are no achievement's image")

    def test_each_image_is_1024_square_rgb_without_alpha(self) -> None:
        for slug in SLUGS:
            with self.subTest(slug=slug):
                found = chunks((IMAGES / f"{slug}.png").read_bytes())
                width, height, depth, colour, compression, filtering, interlace = struct.unpack(">IIBBBBB", found[0][1])
                self.assertEqual((width, height), (SIDE, SIDE))
                self.assertEqual((depth, colour), (8, 2), "colour type 2 is RGB: 4 and 6 have alpha, 3 is a palette")
                self.assertEqual((compression, filtering, interlace), (0, 0, 0))
                kinds = [kind for kind, _ in found]
                self.assertNotIn(b"tRNS", kinds, "a transparency chunk is alpha")
                (resolution,) = [body for kind, body in found if kind == b"pHYs"]
                across, down, unit = struct.unpack(">IIB", resolution)
                self.assertEqual(unit, 1)
                self.assertGreaterEqual(min(across, down) * 0.0254, 72, "Apple asks for at least 72 pixels per inch")

    def test_the_subject_is_inside_the_middle_70_percent(self) -> None:
        for slug in SLUGS:
            with self.subTest(slug=slug):
                raw = decode((IMAGES / f"{slug}.png").read_bytes())
                inside = False
                for y in range(SIDE):
                    row = raw[y * SIDE * 3 : (y + 1) * SIDE * 3]
                    if SAFE_FIRST <= y < SAFE_END:
                        inside = inside or row != BACKGROUND * SIDE
                        outer = row[: SAFE_FIRST * 3] + row[SAFE_END * 3 :]
                        self.assertEqual(outer, BACKGROUND * (len(outer) // 3), f"row {y} has something in the margin")
                    else:
                        self.assertEqual(row, BACKGROUND * SIDE, f"row {y} has something in the margin")
                self.assertTrue(inside, "the picture is empty")

    def test_running_the_script_again_gives_the_same_bytes(self) -> None:
        for slug in SLUGS:
            with self.subTest(slug=slug):
                self.assertEqual((self.first / f"{slug}.png").read_bytes(), (self.second / f"{slug}.png").read_bytes())

    def test_the_committed_images_are_the_ones_the_script_draws(self) -> None:
        for slug in SLUGS:
            with self.subTest(slug=slug):
                committed = decode((IMAGES / f"{slug}.png").read_bytes())
                fresh = decode((self.first / f"{slug}.png").read_bytes())
                self.assertEqual(committed, fresh, "run tools/game-center/make_images.py and commit the images")


if __name__ == "__main__":
    unittest.main()
