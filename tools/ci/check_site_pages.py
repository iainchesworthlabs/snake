#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the built documentation site: the Snake pages are plain HTML, load nothing from other sites, and link to files that exist.

    python3 tools/ci/check_site_pages.py [--site site] [--root .]

Without --site it builds the site with `mkdocs build --strict` into a temporary folder (it needs the packages of
requirements/docs.txt) and checks that. With --site it checks a folder that is already built.

Every page except 404.html:
- has a <title>, exactly one <h1>, lang="en-AU", and a canonical address;
- links to a file that exists, with an anchor that exists in it. Links that start with "/" are the company shell's
  (/, /support/, /assets/v1/...) and are not followed, but none of them may start with /snake/ (the pages must work
  under a preview address, so the pages of this site use relative links). An absolute link is allowed to GitHub and to gnu.org only.

The pages (tools/docs/theme/marketing.html) run under a preview that blocks every script, so they also have

- no <script> except a JSON-LD data block, no <style> and no style= attribute;
- nothing loaded from another host: no src, srcset, stylesheet link or CSS url() or @import that starts with http(s):// or //;
- no mailto: link (support is through GitHub issues only), no picture that does not show the game as built;
- every <img> has an alt attribute (alt="" for a picture that only decorates; any other alt is a description of at least
  10 characters, not a file name), a width and a height in pixels, and, for a PNG, a width and height of the same shape as
  the file, so the page does not shift as the picture loads. Whether the alt text is true of the picture is for a person
  to check: look at each picture before writing its alt.

And the addresses that a store listing or the app will print exist: /, privacy/, support/ (they never move once printed),
and the other pages of the site.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlparse

ROOT = Path(__file__).resolve().parents[2]
FIXED = ["index.html", "privacy/index.html", "support/index.html", "how-to-play/index.html", "features/index.html",
         "faq/index.html", "whats-new/index.html", "accessibility/index.html", "press/index.html", "theme.css",
         "assets/icl.css", "assets/snake.css", "assets/img/icon-1024.png"]
ALLOWED_HOSTS = {"github.com", "www.gnu.org"}
PNG_SIGNATURE = bytes([0x89]) + b"PNG" + bytes([0x0D, 0x0A, 0x1A, 0x0A])
MIN_ALT = 10
FILE_NAME = re.compile(r"[.](png|jpe?g|gif|svg|webp|avif)", re.IGNORECASE)
# Pictures in the repository that do not show the game as built: docs/design/screenshots are renders of the design mock
# (docs/design/screens/*.html), not of the game. They stay off the pages until the owner re-renders them from the game.
# Remove a name here when its picture has been re-rendered.
STALE_PICTURES = {"toybox-desktop.png", "toybox-esp.png", "toybox-game-over.png", "toybox-gameplay.png",
                  "toybox-leaderboard.png", "toybox-name-entry.png", "toybox-paused.png", "toybox-title.png",
                  "screen-flow.png"}


class Page(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.title = ""
        self._in_title = False
        self.h1 = 0
        self.lang = ""
        self.canonical = ""
        self.ids: set[str] = set()
        self.refs: list[tuple[str, str, str]] = []  # (tag, attribute, value)
        self.scripts: list[str] = []  # the type of each <script>
        self.styles = 0
        self.style_attrs = 0
        self.images: list[dict[str, str | None]] = []  # src, alt (None when absent), width, height of each <img>

    def handle_starttag(self, tag, attrs):  # noqa: ANN001, ANN201
        a = {k: (v or "") for k, v in attrs}
        if tag == "html":
            self.lang = a.get("lang", "")
        if tag == "title":
            self._in_title = True
        if tag == "h1":
            self.h1 += 1
        if tag == "script":
            self.scripts.append(a.get("type", ""))
        if tag == "style":
            self.styles += 1
        if tag == "img":
            self.images.append({"src": a.get("src", ""), "alt": a["alt"] if "alt" in a else None,
                                "width": a.get("width"), "height": a.get("height")})
        if "style" in a:
            self.style_attrs += 1
        if "id" in a:
            self.ids.add(a["id"])
        if tag == "a" and a.get("name"):
            self.ids.add(a["name"])
        if tag == "link" and a.get("rel") == "canonical":
            self.canonical = a.get("href", "")
            return
        for attribute in ("href", "src"):
            if attribute in a:
                self.refs.append((tag, attribute, a[attribute]))
        if "srcset" in a:
            for part in a["srcset"].split(","):
                if part.strip():
                    self.refs.append((tag, "srcset", part.split()[0]))

    def handle_endtag(self, tag):  # noqa: ANN001, ANN201
        if tag == "title":
            self._in_title = False

    def handle_data(self, data):  # noqa: ANN001, ANN201
        if self._in_title:
            self.title += data


def parse(path: Path) -> Page:
    page = Page()
    page.feed(path.read_text(encoding="utf-8"))
    return page


def target_file(site: Path, page_file: Path, link: str) -> Path | None:
    """The file a relative link names, or None when it leaves the site."""
    path = unquote(urlparse(link).path)
    page_file = page_file.resolve()
    target = (page_file.parent / path).resolve() if path else page_file
    try:
        target.relative_to(site.resolve())
    except ValueError:
        return None
    if target.is_dir():
        target = target / "index.html"
    return target


def png_size(path: Path) -> tuple[int, int] | None:
    """The width and height of a PNG file, or None when the file is not a PNG."""
    try:
        with path.open("rb") as handle:
            head = handle.read(24)
    except OSError:
        return None
    if len(head) < 24 or not head.startswith(PNG_SIGNATURE) or head[12:16] != b"IHDR":
        return None
    return int.from_bytes(head[16:20], "big"), int.from_bytes(head[20:24], "big")


def check_images(site: Path, path: Path, page: Page) -> list[str]:
    """The picture rules of the module docstring, for the <img> elements of one page."""
    where = path.relative_to(site).as_posix()
    problems: list[str] = []
    for image in page.images:
        src = image["src"] or "(no src)"
        alt = image["alt"]
        if alt is None:
            problems.append(f'{where}: {src}: <img> has no alt attribute (alt="" if it only decorates)')
        elif alt.strip():
            if FILE_NAME.search(alt):
                problems.append(f"{where}: {src}: the alt text looks like a file name; describe what the picture shows")
            elif len(alt.strip()) < MIN_ALT:
                problems.append(f"{where}: {src}: the alt text is shorter than {MIN_ALT} characters; describe what the picture shows")
        elif alt != "":
            problems.append(f'{where}: {src}: write alt="" for a decorative picture, not a space')
        sizes = []
        for name in ("width", "height"):
            value = image[name]
            if value is None or not value.isdigit() or int(value) <= 0:
                problems.append(f"{where}: {src}: <img> needs a {name} in pixels, so the page does not shift as it loads")
            else:
                sizes.append(int(value))
        if len(sizes) == 2 and image["src"] and not urlparse(image["src"]).scheme and not image["src"].startswith(("/", "//")):
            target = target_file(site, path, image["src"])
            real = png_size(target) if target is not None and target.is_file() else None
            if real and abs(sizes[0] / sizes[1] - real[0] / real[1]) > 0.02 * real[0] / real[1]:
                problems.append(f"{where}: {src}: width {sizes[0]} and height {sizes[1]} are not the shape of the picture ({real[0]} by {real[1]})")
    return problems


def check_site(site: Path, root: Path) -> list[str]:
    problems: list[str] = []
    for name in FIXED:
        if not (site / name).is_file():
            problems.append(f"{name}: is not in the built site (a store listing or the app will print it, or it is a page of the site)")
    pages = {p: parse(p) for p in sorted(site.rglob("*.html"))}
    for path, page in pages.items():
        rel = path.relative_to(site).as_posix()
        if rel == "404.html":
            continue
        where = rel
        if not page.title.strip():
            problems.append(f"{where}: no <title>")
        if page.h1 != 1:
            problems.append(f"{where}: has {page.h1} <h1>, not one")
        if not page.canonical:
            problems.append(f"{where}: no canonical address")
        if page.lang != "en-AU":
            problems.append(f'{where}: <html> needs lang="en-AU"')
        for kind in page.scripts:
            if kind != "application/ld+json":
                problems.append(f"{where}: has a <script>; the pages are script-free (a preview blocks scripts)")
        if page.styles or page.style_attrs:
            problems.append(f"{where}: has <style> or a style= attribute; put it in docs/assets/snake.css")
        problems.extend(check_images(site, path, page))
        for tag, attribute, value in page.refs:
            value = value.strip()
            if not value or value.startswith(("data:", "javascript:")):
                continue
            if value.startswith("mailto:"):
                problems.append(f"{where}: {value}: support is through GitHub issues only, no email address")
                continue
            parts = urlparse(value)
            if parts.scheme in ("http", "https") or value.startswith("//"):
                if parts.scheme not in ("http", "https") or parts.netloc not in ALLOWED_HOSTS:
                    problems.append(f"{where}: {value}: not a link to a host this site may name ({', '.join(sorted(ALLOWED_HOSTS))})")
                elif attribute == "srcset" or (tag != "a" and tag != "link"):
                    problems.append(f"{where}: {value}: loads something from another host")
                elif tag == "link" and "icon" not in value:
                    problems.append(f"{where}: {value}: a <link> to another host")
                continue
            if value.startswith("/"):
                if value.startswith("/snake/"):
                    problems.append(f"{where}: {value}: write a relative link, so the page works under a preview address")
                continue
            if parts.scheme:
                problems.append(f"{where}: {value}: unknown link type")
                continue
            target = target_file(site, path, value)
            if target is None or not target.exists():
                problems.append(f"{where}: {value}: no such file in the built site")
                continue
            if target.name in STALE_PICTURES:
                problems.append(f"{where}: {target.name} is a render of the design mock, not of the game; leave it off the pages")
            if parts.fragment and target.suffix == ".html":
                other = pages.get(target) or parse(target)
                if unquote(parts.fragment) not in other.ids:
                    problems.append(f"{where}: {value}: no anchor #{parts.fragment} in {target.relative_to(site).as_posix()}")
    for name in ("theme.css", "assets/snake.css", "assets/icl.css"):
        css = site / name
        if css.is_file():
            text = re.sub(r"/\*.*?\*/", "", css.read_text(encoding="utf-8"), flags=re.S)
            if re.search(r"@import|url\(\s*['\"]?(https?:)?//", text):
                problems.append(f"{name}: loads something with @import or an address; the site loads nothing from other sites")
    return problems


def build(root: Path, out: Path) -> int:
    result = subprocess.run([sys.executable, "-m", "mkdocs", "build", "--strict", "--quiet", "--site-dir", str(out)],
                            cwd=root, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
    return result.returncode


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--site", type=Path, help="a site that is already built")
    args = parser.parse_args(argv)
    tmp = None
    site = args.site
    if site is None:
        if subprocess.run([sys.executable, "-c", "import mkdocs, material"], capture_output=True, check=False).returncode != 0:
            print("check_site_pages: mkdocs-material is not installed (pip install -r requirements/docs.txt): cannot build the site", file=sys.stderr)
            return 2
        tmp = tempfile.mkdtemp(prefix="snake-site-")
        site = Path(tmp)
        if build(args.root, site) != 0:
            shutil.rmtree(tmp, ignore_errors=True)
            return 1
    try:
        problems = check_site(site, args.root)
    finally:
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)
    for problem in problems:
        print(f"::error::{problem}")
    if problems:
        return 1
    print("Site pages: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
