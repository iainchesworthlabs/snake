#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Draws the twelve achievement images for Game Center, from rectangles, as the rest of the game's art is drawn.

    python3 tools/game-center/make_images.py [--out DIRECTORY]

It needs nothing but Python 3: the PNG files are written with the standard library. By default it writes

    apps/snake/packaging/ios/game-center/images/<slug>.png

for each achievement slug of apps/snake/packaging/ios/game-center/components.json: 1024 by 1024 pixels, RGB, no alpha
channel, 72 pixels per inch, which is what App Store Connect asks of an achievement image.

Each picture is one subject, drawn on a grid of 32 by 32 cells scaled up by 32, so every edge is crisp. A piece has a
one-cell outline in the ink colour, and the whole subject has a hard shadow one cell down and to the right, in the
shadow colour: no gradients, no blur. The subject is centred on the sunshine background and kept inside the middle
22 cells (70 percent) of the picture, because Game Center may show an image in a circle. No picture has a word or a
digit in it, so the same image serves every language. The colours are read from docs/design/tokens.json; none is
typed in here, apart from the shades the game itself derives (the shade of a blueberry or a segment, which are the
same blend of the ink colour into the colour as in libs/raster/src/board_painter.cpp).

The output depends only on this file and on tokens.json, so it can be made again whenever the look changes. The
pixels are always the same; the bytes of a file are the same on the same machine, but another zlib may compress the
same pixels to other bytes, so the test (tools/tests/test_game_center_images.py) compares the decoded pixels of the
committed files with fresh ones.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOKENS = ROOT / "docs/design/tokens.json"
DEFAULT_OUT = ROOT / "apps/snake/packaging/ios/game-center/images"

GRID = 32  # cells across a picture
CELL = 32  # pixels across a cell
SIDE = GRID * CELL  # 1024: Apple's size for an achievement image
SAFE = 22  # cells: the middle 70 percent, 22.4 cells, rounded down
PIXELS_PER_METRE = 2835  # 72 pixels per inch

Colour = tuple[int, int, int]


def blend(base: Colour, over: Colour, percent: int) -> Colour:
    """`over` laid on `base` at `percent` percent, rounded as board_painter.cpp does."""
    return (
        (base[0] * (100 - percent) + over[0] * percent + 50) // 100,
        (base[1] * (100 - percent) + over[1] * percent + 50) // 100,
        (base[2] * (100 - percent) + over[2] * percent + 50) // 100,
    )


class Palette:
    """The game's colours, from the design tokens."""

    def __init__(self, tokens_path: Path = TOKENS) -> None:
        colours = json.loads(tokens_path.read_text(encoding="utf-8"))["color"]

        def rgb(name: str) -> Colour:
            value = colours[name]["value"]
            return int(value[1:3], 16), int(value[3:5], 16), int(value[5:7], 16)

        self.background = rgb("background")
        self.ink = rgb("ink")
        self.shadow = rgb("shadow")
        self.cream = rgb("surface")
        self.coral = rgb("accent")
        self.board_light = rgb("boardLight")
        self.board_dark = rgb("boardDark")
        self.berry = rgb("food")
        self.shine = rgb("foodHighlight")
        self.leaf = rgb("leaf")
        self.pink = rgb("tongue")
        self.eye = rgb("eye")
        self.gold = rgb("medalGold")
        self.silver = rgb("medalSilver")
        self.berry_shade = blend(self.berry, self.ink, 35)  # the crescent on a food's lower right
        self.gold_shade = blend(self.gold, self.ink, 35)
        self.tile_shade = blend(self.gold, self.ink, 28)


def disc_cells(diameter: int) -> set[tuple[int, int]]:
    """The cells of a pixel disc: a few cells off each corner of the square, so 4 is 2, 4, 4, 2 and 7 is 3, 5, 7, 7, 7, 5, 3."""
    centre = (diameter - 1) / 2
    limit = (diameter / 2) ** 2 - 0.3
    return {
        (i, j) for j in range(diameter) for i in range(diameter) if (i - centre) ** 2 + (j - centre) ** 2 <= limit
    }


class Layer:
    """The cells of one piece, each with its colour. A piece gets one outline round all its cells."""

    def __init__(self) -> None:
        self.cells: dict[tuple[int, int], Colour] = {}

    def dot(self, x: int, y: int, colour: Colour) -> None:
        self.cells[(x, y)] = colour

    def rect(self, x: int, y: int, w: int, h: int, colour: Colour) -> None:
        for j in range(y, y + h):
            for i in range(x, x + w):
                self.cells[(i, j)] = colour

    def block(self, x: int, y: int, w: int, h: int, colour: Colour, shade: Colour | None = None, cut: int = 1) -> None:
        """A rectangle with its corners cut a step, and the bottom row in `shade`, as the board draws a segment."""
        for j in range(h):
            for i in range(w):
                corner = (i < cut or i >= w - cut) and (j < cut or j >= h - cut)
                if not corner:
                    self.cells[(x + i, y + j)] = shade if shade is not None and j == h - 1 else colour

    def disc(self, x: int, y: int, d: int, colour: Colour, shade: Colour | None = None) -> None:
        """A disc with the crescent of `shade` on its lower right that the board gives its food."""
        cells = disc_cells(d)
        for i, j in cells:
            lit = (i + 1, j + 1) in cells
            self.cells[(x + i, y + j)] = colour if lit or shade is None else shade

    def pattern(self, x: int, y: int, rows: list[str], colour: Colour) -> None:
        for j, row in enumerate(rows):
            for i, mark in enumerate(row):
                if mark == "#":
                    self.cells[(x + i, y + j)] = colour


class Picture:
    """A set of pieces, painted one over another, with their outlines and a shadow under all of them."""

    def __init__(self, palette: Palette) -> None:
        self.palette = palette
        self.cells: dict[tuple[int, int], Colour] = {}
        self.silhouette: set[tuple[int, int]] = set()

    def part(self, paint, outline: bool = True, clip: set[tuple[int, int]] | None = None) -> None:  # noqa: ANN001
        """Paints a piece. With `clip`, only the cells of the piece inside it are kept, and the outline is drawn round
        what is left, outline too: a piece that goes through a wall is cut off by the wall."""
        layer = Layer()
        paint(layer)
        if clip is not None:
            layer.cells = {cell: colour for cell, colour in layer.cells.items() if cell in clip}
        edge = {
            (x + dx, y + dy) for x, y in layer.cells for dx in (-1, 0, 1) for dy in (-1, 0, 1)
        } - layer.cells.keys()
        if clip is not None:
            edge &= clip
        if outline:
            for cell in edge:
                self.cells[cell] = self.palette.ink
            self.silhouette |= edge
        self.silhouette |= layer.cells.keys()
        self.cells.update(layer.cells)

    def render(self) -> bytes:
        """The picture as 1024 by 1024 RGB: the raw bytes of its rows, without a filter byte."""
        shadow = {(x + 1, y + 1) for x, y in self.silhouette} - self.silhouette
        used = self.silhouette | shadow
        low_x, high_x = min(x for x, _ in used), max(x for x, _ in used)
        low_y, high_y = min(y for _, y in used), max(y for _, y in used)
        width, height = high_x - low_x + 1, high_y - low_y + 1
        if width > SAFE or height > SAFE:
            raise ValueError(f"the subject is {width} by {height} cells; the safe square is {SAFE} by {SAFE}")
        shift_x = (GRID - width) // 2 - low_x
        shift_y = (GRID - height) // 2 - low_y
        rows = []
        for y in range(GRID):
            line = bytearray()
            for x in range(GRID):
                cell = (x - shift_x, y - shift_y)
                if cell in self.cells:
                    colour = self.cells[cell]
                elif cell in shadow:
                    colour = self.palette.shadow
                else:
                    colour = self.palette.background
                line += bytes(colour) * CELL
            rows.append(bytes(line))
        return b"".join(row for row in rows for _ in range(CELL))


# The pieces the pictures are made of ---------------------------------------------------------------------------------


def berry(layer: Layer, p: Palette, x: int, y: int, d: int, leaf: bool = False) -> None:
    """A blueberry in a box of `d` cells with its top left at (x, y): body, shade and shine, and a leaf above if asked."""
    layer.disc(x, y, d, p.berry, p.berry_shade if d >= 4 else None)
    if d >= 4:
        shine = 2 if d >= 8 else 1
        layer.rect(x + 1 + (d >= 8), y + 1 + (d >= 8), shine, shine, p.shine)
    if leaf:
        stem = d // 2
        layer.rect(x + stem - 1, y - 1, 3, 1, p.leaf)
        layer.rect(x + stem, y - 2, 2, 1, p.leaf)


def gold_food(layer: Layer, p: Palette, x: int, y: int, d: int) -> None:
    """The bonus item: the board's food, in gold, with no leaf."""
    layer.disc(x, y, d, p.gold, p.gold_shade)
    shine = 3 if d >= 12 else 1
    layer.rect(x + 2, y + 2, shine, shine, p.shine)


def spark(layer: Layer, p: Palette, x: int, y: int) -> None:
    """The spark of a fuse: a gold plus, 3 by 3 cells, with its top left at (x, y)."""
    layer.pattern(x, y, [".#.", "###", ".#."], p.gold)


def bomb(layer: Layer, p: Palette, x: int, y: int, d: int, fuse: bool = True) -> None:
    """The bomb food: a cream ring, an ink ball with its shade and shine, a cream fuse and a gold spark above."""
    layer.disc(x, y, d, p.cream)
    layer.disc(x + 1, y + 1, d - 2, p.ink, p.shadow)
    layer.rect(x + 2, y + 2, 2 if d >= 10 else 1, 2 if d >= 10 else 1, p.shine)
    if fuse:
        fx = x + (d * 2) // 3
        layer.rect(fx, y - 2, 1, 2, p.cream)
        spark(layer, p, fx - 1, y - 5)


def portal(layer: Layer, p: Palette, x: int, y: int, d: int, colour: Colour, dot: bool = True) -> None:
    """A portal of the board: a ring of `colour`, a dark centre and, in the first pair, a dot in it."""
    layer.disc(x, y, d, colour)
    core = d - 4
    layer.disc(x + 2, y + 2, core, p.ink)
    if dot:
        middle = x + d // 2, y + d // 2
        layer.dot(middle[0], middle[1], colour)


def padlock_open(picture: Picture, x: int, y: int) -> None:
    """An open padlock, 10 cells wide and 11 high, with its top left at (x, y): a silver shackle with one leg out of the
    body, over a cream body with an ink keyhole. The legs are two cells apart by four, so that the outlines leave a gap."""
    p = picture.palette

    def shackle(layer: Layer) -> None:
        layer.pattern(
            x + 1,
            y,
            [
                ".######.",
                "########",
                "##....##",
                "##......",
                "##......",
                "##......",
                "##......",
            ],
            p.silver,
        )

    def body(layer: Layer) -> None:
        layer.block(x, y + 6, 10, 5, p.cream, blend(p.cream, p.ink, 28))
        layer.rect(x + 4, y + 7, 2, 2, p.ink)

    picture.part(shackle)
    picture.part(body)


def snake_head(layer: Layer, p: Palette, x: int, y: int, heading: str, k: int = 1, tongue: bool = True) -> None:
    """The head of the snake, a 5 by 5 square of `k`-cell steps with its corners cut, facing `heading`.

    `x` and `y` are the top left of its box. The eyes are on the sides of the front half, as the board draws them, and
    the forked tongue, 3 steps long, reaches out of the front."""
    size = 5 * k
    template: dict[tuple[int, int], Colour] = {}
    for a in range(5):
        for b in range(5):
            if abs(a - 2) == 2 and abs(b - 2) == 2:
                continue
            template[(a, b)] = p.coral
    shaded = {cell for cell in template if (cell[0], cell[1] + 1) not in template}
    for cell in shaded:
        template[cell] = blend(p.coral, p.ink, 28)
    for b in (0, 3):  # the eyes, 2 by 2, with the pupil at the front
        for a in (2, 3):
            for j in range(2):
                template[(a, b + j)] = p.eye
        template[(3, b + (1 if b == 0 else 0))] = p.ink
    cells: dict[tuple[int, int], Colour] = {}
    for (a, b), colour in template.items():
        for i in range(k):
            for j in range(k):
                cells[(a * k + i, b * k + j)] = colour
    if tongue:
        for i in range(k):
            for j in range(k):
                cells[(5 * k + i, 2 * k + j)] = p.pink
                cells[(6 * k + i, 2 * k + j)] = p.pink
                cells[(7 * k + i, 1 * k + j)] = p.pink
                cells[(7 * k + i, 3 * k + j)] = p.pink
    for (a, b), colour in cells.items():
        if heading == "right":
            u, v = a, b
        elif heading == "down":
            u, v = size - 1 - b, a
        elif heading == "left":
            u, v = size - 1 - a, size - 1 - b
        else:  # up
            u, v = b, size - 1 - a
        layer.dot(x + u, y + v, colour)


def segment(layer: Layer, p: Palette, x: int, y: int, size: int, index: int) -> None:
    """A segment of the snake: coral and cream in turn, with the shade along its bottom."""
    colour = p.coral if index % 2 == 0 else p.cream
    layer.block(x, y, size, size, colour, blend(colour, p.ink, 28))


# The twelve pictures -------------------------------------------------------------------------------------------------


def wrap(p: Palette) -> Picture:
    """The whole board with a cream wall round it: the snake's head comes out of the wall at the left, its body goes into
    the wall at the right, and there is plain board between them, so it is one snake that went through the wall."""
    picture = Picture(p)
    side = 19  # cells across the board with its wall, the most that fits the safe square with an outline and a shadow
    inside = {(i, j) for j in range(1, side - 1) for i in range(1, side - 1)}

    def board(layer: Layer) -> None:
        for j in range(side):
            for i in range(side):
                if (i, j) in inside:
                    layer.dot(i, j, p.board_light if ((i - 1) // 3 + (j - 1) // 3) % 2 else p.board_dark)
                else:
                    layer.dot(i, j, p.cream)

    def body(layer: Layer) -> None:
        segment(layer, p, 14, 7, 5, 0)

    picture.part(board)
    picture.part(body, clip=inside)
    picture.part(lambda layer: snake_head(layer, p, 0, 7, "right", tongue=False), clip=inside)
    return picture


def bonus(p: Palette) -> Picture:
    """The gold bonus item on a patch of the board, where the gold shows, with two sparkles."""
    picture = Picture(p)

    def board(layer: Layer) -> None:
        for j in range(19):
            for i in range(19):
                corner = (i < 2 or i > 16) and (j < 2 or j > 16)
                if not corner:
                    layer.dot(6 + i, 6 + j, p.board_light if (i // 3 + j // 3) % 2 else p.board_dark)

    picture.part(board)
    picture.part(lambda layer: gold_food(layer, p, 9, 9, 13))
    for x, y in ((8, 7), (19, 19)):
        picture.part(lambda layer, x=x, y=y: layer.pattern(x, y, [".#.", "###", ".#."], p.cream), outline=False)
    return picture


def combo(p: Palette) -> Picture:
    """A big blueberry with its leaf over a row of five small ones."""
    picture = Picture(p)
    picture.part(lambda layer: berry(layer, p, 11, 6, 9, leaf=True))
    for index in range(5):
        picture.part(lambda layer, x=6 + 4 * index: berry(layer, p, x, 18, 3))
    return picture


def best(p: Palette) -> Picture:
    """A gold medal tile, as the leaderboard's first place has it, on a cream panel, with a star in place of the digit."""
    picture = Picture(p)

    def panel(layer: Layer) -> None:
        layer.block(6, 6, 19, 19, p.cream, blend(p.cream, p.ink, 28), cut=2)

    def tile(layer: Layer) -> None:
        layer.block(9, 9, 13, 13, p.gold, p.tile_shade, cut=2)
        layer.pattern(
            11,
            10,
            [
                "....#....",
                "...###...",
                "...###...",
                "#########",
                ".#######.",
                "..#####..",
                "..#####..",
                ".###.###.",
                ".##...##.",
            ],
            p.ink,
        )

    picture.part(panel)
    picture.part(tile)
    return picture


def speed(p: Palette) -> Picture:
    """The head of the snake, large, with three speed lines behind it."""
    picture = Picture(p)

    for y, x, length in ((8, 5, 8), (12, 8, 5), (16, 5, 8)):
        picture.part(lambda layer, x=x, y=y, length=length: layer.rect(x, y, length, 1, p.cream))
    picture.part(lambda layer: snake_head(layer, p, 14, 6, "right", k=2, tongue=False))
    return picture


def heap(p: Palette, rows: list[int], d: int, pitch_x: float, pitch_y: int, left: int, top: int) -> Picture:
    """A heap of blueberries: the widest row at the bottom, narrower rows on top, the top one with a leaf."""
    picture = Picture(p)
    middle = left + ((rows[-1] - 1) * pitch_x + d) / 2
    for index in range(len(rows) - 1, -1, -1):
        count = rows[index]
        start = middle - ((count - 1) * pitch_x + d) / 2
        for j in range(count):
            x = int(start + j * pitch_x + 0.5)
            y = top + index * pitch_y
            picture.part(lambda layer, x=x, y=y, leaf=index == 0: berry(layer, p, x, y, d, leaf=leaf))
    return picture


def berries100(p: Palette) -> Picture:
    return heap(p, [1, 2], d=8, pitch_x=7, pitch_y=6, left=7, top=8)


def berries1000(p: Palette) -> Picture:
    return heap(p, [1, 2, 3, 4], d=5, pitch_x=4, pitch_y=4, left=5, top=6)


def portals(p: Palette) -> Picture:
    """A pair of portals with an open padlock under them."""
    picture = Picture(p)
    picture.part(lambda layer: portal(layer, p, 5, 5, 7, p.pink))
    picture.part(lambda layer: portal(layer, p, 17, 5, 7, p.pink))
    padlock_open(picture, 8, 13)
    return picture


def portal_through(p: Palette) -> Picture:
    """One large portal with the head of the snake, large, coming out of its dark centre and across its rim."""
    picture = Picture(p)
    d = 16

    def ring(layer: Layer) -> None:
        layer.disc(0, 0, d, p.pink)
        layer.disc(2, 2, d - 4, p.ink)
        layer.rect(d // 2 - 1, d // 2 - 1, 2, 2, p.pink)

    def head(layer: Layer) -> None:
        snake_head(layer, p, 9, 3, "right", k=2, tongue=False)

    def edge_of(paint) -> set[tuple[int, int]]:  # noqa: ANN001
        layer = Layer()
        paint(layer)
        return {(x + dx, y + dy) for x, y in layer.cells for dx in (-1, 0, 1) for dy in (-1, 0, 1)}

    # The head is in front of the portal and has no outline over it, where ink on ink or on the rim would blur it: it
    # has one only where it is out in the open.
    def outline(layer: Layer) -> None:
        for cell in edge_of(head) - edge_of(ring):
            layer.dot(cell[0], cell[1], p.ink)

    picture.part(ring)
    picture.part(outline, outline=False)
    picture.part(head, outline=False)
    return picture


def hotpotato(p: Palette) -> Picture:
    """The bomb food with an open padlock."""
    picture = Picture(p)
    picture.part(lambda layer: bomb(layer, p, 5, 9, 10))
    padlock_open(picture, 13, 12)
    return picture


def shed(p: Palette) -> Picture:
    """The live snake, head up, going away at the right; the dropped end, a bomb on a segment, left behind at the left.
    The game just takes the dropped cells off the board, so the dropped end is drawn as it was, with nothing between."""
    picture = Picture(p)

    def live(layer: Layer) -> None:
        snake_head(layer, p, 15, 4, "up")
        segment(layer, p, 15, 9, 5, 1)
        segment(layer, p, 15, 14, 5, 0)

    picture.part(live)
    picture.part(lambda layer: segment(layer, p, 3, 14, 5, 1))
    picture.part(lambda layer: bomb(layer, p, 1, 5, 9))
    return picture


def allmodes(p: Palette) -> Picture:
    """Four small tiles, one for each mode: Easy, Classic, Portals and Hot potato."""
    picture = Picture(p)
    size = 9
    spots = ((6, 6), (16, 6), (6, 16), (16, 16))
    tiles = (p.leaf, p.board_light, p.berry, p.coral)

    for index, ((x, y), colour) in enumerate(zip(spots, tiles)):

        def tile(layer: Layer, x=x, y=y, colour=colour, index=index) -> None:
            layer.block(x, y, size, size, colour, blend(colour, p.ink, 28))
            if index == 0:
                berry(layer, p, x + 2, y + 2, 5)
            elif index == 1:
                snake_head(layer, p, x + 2, y + 2, "right", tongue=False)
            elif index == 2:
                portal(layer, p, x + 1, y + 1, 7, p.pink)
            else:
                bomb(layer, p, x + 2, y + 3, 5, fuse=False)
                layer.dot(x + 5, y + 2, p.cream)
                layer.dot(x + 5, y + 1, p.gold)

        picture.part(tile)
    return picture


PICTURES = {
    "wrap": wrap,
    "bonus": bonus,
    "combo": combo,
    "best": best,
    "speed": speed,
    "berries100": berries100,
    "berries1000": berries1000,
    "portals": portals,
    "portal": portal_through,
    "hotpotato": hotpotato,
    "shed": shed,
    "allmodes": allmodes,
}


# PNG -----------------------------------------------------------------------------------------------------------------


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def png_bytes(raw: bytes) -> bytes:
    """A PNG of 1024 by 1024 pixels, RGB, 8 bits, 72 pixels per inch, from the raw rows of pixels.

    Every row is stored with the filter "Up", so the 32 identical rows of a cell compress to almost nothing."""
    stride = SIDE * 3
    lines = bytearray()
    previous = bytes(stride)
    for y in range(SIDE):
        row = raw[y * stride : (y + 1) * stride]
        lines.append(2)
        lines += bytes((a - b) & 0xFF for a, b in zip(row, previous))
        previous = row
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", SIDE, SIDE, 8, 2, 0, 0, 0))
        + chunk(b"pHYs", struct.pack(">IIB", PIXELS_PER_METRE, PIXELS_PER_METRE, 1))
        + chunk(b"IDAT", zlib.compress(bytes(lines), 9))
        + chunk(b"IEND", b"")
    )


def make_all(out: Path, tokens: Path = TOKENS) -> list[Path]:
    palette = Palette(tokens)
    out.mkdir(parents=True, exist_ok=True)
    written = []
    for slug, draw in PICTURES.items():
        path = out / f"{slug}.png"
        try:
            raw = draw(palette).render()
        except ValueError as error:
            raise ValueError(f"{slug}: {error}") from error
        path.write_bytes(png_bytes(raw))
        written.append(path)
    return written


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="where to write the images")
    args = parser.parse_args()
    written = make_all(args.out)
    print(f"wrote {len(written)} images to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
