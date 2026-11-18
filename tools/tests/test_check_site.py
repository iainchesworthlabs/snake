# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for check_site_kit.py, check_site_pages.py and site_shell_hook.py (CTest label: unit). The Snake pages.

Run with: python3 -m unittest discover -s tools/tests -v
The page checks run on small sites written to a temporary folder; the kit check runs on a copy of the kit.
"""

from __future__ import annotations

import importlib.util
import re
import shutil
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "ci"))

import check_site_kit as kit_check  # noqa: E402
import check_site_pages as pages_check  # noqa: E402


def load_hook():
    spec = importlib.util.spec_from_file_location("site_shell_hook", ROOT / "tools" / "docs" / "site_shell_hook.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def png(width: int, height: int) -> bytes:
    """The smallest PNG of the given size (a blank grey image), made by hand so the tests need no imaging library."""

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = b"".join(bytes([0]) + bytes([0x80]) * width for _ in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    return bytes([0x89]) + b"PNG" + bytes([0x0D, 0x0A, 0x1A, 0x0A]) + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


PAGE = """<!doctype html><html lang="en-AU" data-app="snake"><head><title>T</title>
<link rel="canonical" href="https://example.test/x/"><link rel="stylesheet" href="{css}"></head>
<body><main><h1>Heading</h1>{body}</main></body></html>"""


def page(body: str = "", css: str = "/assets/shell.css") -> str:
    return PAGE.format(body=body, css=css)


class KitCheck(unittest.TestCase):
    def setUp(self) -> None:
        self.dir = tempfile.TemporaryDirectory()
        self.root = Path(self.dir.name)
        shutil.copytree(ROOT / kit_check.KIT, self.root / kit_check.KIT)
        shutil.copyfile(ROOT / kit_check.THEME, self._theme())

    def _theme(self) -> Path:
        path = self.root / kit_check.THEME
        path.parent.mkdir(parents=True, exist_ok=True)
        return path

    def tearDown(self) -> None:
        self.dir.cleanup()

    def test_the_pinned_copy_and_palette_pass(self) -> None:
        self.assertEqual(kit_check.check_sums(self.root), [])
        self.assertEqual(kit_check.check_palette(self.root), [])

    def test_an_edited_kit_file_fails(self) -> None:
        with (self.root / kit_check.KIT / "icl.css").open("a", encoding="utf-8") as handle:
            handle.write("/* edited */\n")
        self.assertTrue(any("icl.css" in p and "edited" in p for p in kit_check.check_sums(self.root)))

    def test_a_missing_and_an_extra_kit_file_fail(self) -> None:
        (self.root / kit_check.KIT / "kit" / "theme.css").unlink()
        (self.root / kit_check.KIT / "extra.css").write_text("x", encoding="utf-8")
        problems = kit_check.check_sums(self.root)
        self.assertTrue(any("kit/theme.css" in p and "missing" in p for p in problems))
        self.assertTrue(any("extra.css" in p for p in problems))

    def test_line_endings_are_not_compared(self) -> None:
        path = self.root / kit_check.KIT / "icl.css"
        path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
        self.assertEqual(kit_check.check_sums(self.root), [])

    def test_a_palette_that_fails_contrast_is_reported(self) -> None:
        theme = self.root / kit_check.THEME
        theme.write_text(theme.read_text(encoding="utf-8").replace("light-dark(#2b1d52, #ffc93c)", "light-dark(#ffe08a, #ffc93c)", 1),
                         encoding="utf-8")
        problems = kit_check.check_palette(self.root)
        self.assertEqual(len(problems), 1)
        self.assertIn("FAIL", problems[0])

    def test_a_property_that_is_not_a_role_is_reported(self) -> None:
        theme = self.root / kit_check.THEME
        theme.write_text(theme.read_text(encoding="utf-8").replace("}", "  --app-extra: #000000;\n}"), encoding="utf-8")
        self.assertTrue(kit_check.check_palette(self.root))


class PagesCheck(unittest.TestCase):
    def setUp(self) -> None:
        self.dir = tempfile.TemporaryDirectory()
        self.site = Path(self.dir.name)
        for name in pages_check.FIXED:
            path = self.site / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(page() if name.endswith(".html") else "x", encoding="utf-8")

    def tearDown(self) -> None:
        self.dir.cleanup()

    def problems(self, body: str) -> list[str]:
        (self.site / "faq" / "index.html").write_text(page(body), encoding="utf-8")
        return pages_check.check_site(self.site, ROOT)

    def test_a_clean_site_passes(self) -> None:
        self.assertEqual(self.problems('<a href="../privacy/">p</a> <a href="https://github.com/x">g</a> <a href="/support/">shell</a>'), [])

    def test_a_missing_fixed_address_is_reported(self) -> None:
        (self.site / "privacy" / "index.html").unlink()
        self.assertTrue(any("privacy/index.html" in p for p in pages_check.check_site(self.site, ROOT)))

    def test_a_script_is_reported_but_json_ld_is_not(self) -> None:
        self.assertTrue(any("script" in p for p in self.problems("<script>x</script>")))
        self.assertEqual(self.problems('<script type="application/ld+json">{}</script>'), [])

    def test_a_load_from_another_host_is_reported(self) -> None:
        self.assertTrue(self.problems('<img src="https://example.org/a.png">'))
        self.assertTrue(self.problems('<a href="https://example.org/">x</a>'))

    def test_a_style_attribute_is_reported(self) -> None:
        self.assertTrue(any("style" in p for p in self.problems('<p style="color:red">x</p>')))

    def test_a_broken_link_and_anchor_are_reported(self) -> None:
        self.assertTrue(any("no such file" in p for p in self.problems('<a href="../nope/">x</a>')))
        self.assertTrue(any("no anchor" in p for p in self.problems('<a href="../privacy/#missing">x</a>')))

    def test_an_anchor_that_exists_passes(self) -> None:
        (self.site / "privacy" / "index.html").write_text(page('<h2 id="there">x</h2>'), encoding="utf-8")
        self.assertEqual(self.problems('<a href="../privacy/#there">x</a>'), [])

    def test_a_link_that_names_the_section_is_reported(self) -> None:
        self.assertTrue(any("relative" in p for p in self.problems('<a href="/snake/faq/">x</a>')))

    def test_an_email_address_is_reported(self) -> None:
        self.assertTrue(any("email" in p for p in self.problems('<a href="mailto:a@b.test">x</a>')))

    def test_a_picture_of_the_design_mock_is_reported(self) -> None:
        picture = self.site / "assets" / "img" / "toybox-title.png"
        picture.parent.mkdir(parents=True, exist_ok=True)
        picture.write_bytes(b"x")
        self.assertTrue(any("design mock" in p for p in self.problems('<img src="../assets/img/toybox-title.png" width="8" height="8" alt="">')))
        self.assertEqual(self.problems('<img src="../assets/img/icon-1024.png" width="8" height="8" alt="">'), [])

    def test_a_picture_needs_alt_text_and_a_size(self) -> None:
        self.assertTrue(any("no alt" in p for p in self.problems('<img src="../assets/img/icon-1024.png" width="8" height="8">')))
        self.assertTrue(any("width" in p for p in self.problems('<img src="../assets/img/icon-1024.png" height="8" alt="">')))
        self.assertTrue(any("height" in p for p in self.problems('<img src="../assets/img/icon-1024.png" width="8" alt="">')))
        self.assertTrue(any("width" in p for p in self.problems('<img src="../assets/img/icon-1024.png" width="wide" height="8" alt="">')))

    def test_alt_text_that_is_a_file_name_or_too_short_is_reported(self) -> None:
        sized = 'src="../assets/img/icon-1024.png" width="8" height="8"'
        self.assertTrue(any("file name" in p for p in self.problems(f'<img {sized} alt="icon-1024.png">')))
        self.assertTrue(any("shorter" in p for p in self.problems(f'<img {sized} alt="Icon">')))
        self.assertTrue(any("decorative" in p for p in self.problems(f'<img {sized} alt=" ">')))
        self.assertEqual(self.problems(f'<img {sized} alt="The Snake icon: a coral snake on yellow.">'), [])

    def test_a_size_of_the_wrong_shape_for_a_png_is_reported(self) -> None:
        (self.site / "assets" / "img").mkdir(parents=True, exist_ok=True)
        (self.site / "assets" / "img" / "icon-1024.png").write_bytes(png(40, 20))
        self.assertEqual(self.problems('<img src="../assets/img/icon-1024.png" width="400" height="200" alt="">'), [])
        self.assertTrue(any("shape" in p for p in self.problems('<img src="../assets/img/icon-1024.png" width="200" height="200" alt="">')))

    def test_a_page_needs_one_heading(self) -> None:
        (self.site / "faq" / "index.html").write_text(page().replace("</main>", "<h1>Two</h1></main>"), encoding="utf-8")
        self.assertTrue(any("<h1>" in p for p in pages_check.check_site(self.site, ROOT)))

    def test_a_stylesheet_that_imports_is_reported(self) -> None:
        (self.site / "assets").mkdir(exist_ok=True)
        (self.site / "assets" / "snake.css").write_text('@import url("https://example.org/f.css");', encoding="utf-8")
        self.assertTrue(any("snake.css" in p for p in pages_check.check_site(self.site, ROOT)))


class TheRepository(unittest.TestCase):
    """The checks run on the files of this repository, as CI runs them."""

    def test_the_pictures_of_the_pages_are_png_files_with_the_sizes_the_pages_write(self) -> None:
        # The sizes in the markup of docs/*.md, against the files in docs/assets/img, without building the site.
        pictures = {path.name: pages_check.png_size(path) for path in (ROOT / "docs" / "assets" / "img").glob("*.png")}
        self.assertTrue(pictures)
        for name, size in pictures.items():
            self.assertIsNotNone(size, f"{name} is not a PNG file")
        for path in sorted((ROOT / "docs").glob("*.md")):
            for match in re.finditer(r'<img [^>]*src="(?:[.][.]/)?assets/img/([^"]+)"[^>]*>', path.read_text(encoding="utf-8")):
                tag = match.group(0)
                width = re.search(r'width="(\d+)"', tag)
                height = re.search(r'height="(\d+)"', tag)
                self.assertTrue(width and height, f"{path.name}: {match.group(1)} has no width and height")
                real = pictures.get(match.group(1))
                self.assertIsNotNone(real, f"{path.name}: {match.group(1)} is not in docs/assets/img")
                self.assertAlmostEqual(int(width.group(1)) / int(height.group(1)), real[0] / real[1],
                                       delta=0.02 * real[0] / real[1], msg=f"{path.name}: {match.group(1)}")

    def test_the_kit_and_palette_of_the_repository_pass(self) -> None:
        self.assertEqual(kit_check.check_sums(ROOT), [])
        self.assertEqual(kit_check.check_palette(ROOT), [])

    def test_the_palette_is_one_rule_with_the_eight_roles(self) -> None:
        text = (ROOT / kit_check.THEME).read_text(encoding="utf-8")
        body = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        self.assertEqual(body.count("{"), 1)
        self.assertIn('[data-app="snake"]', body)
        self.assertEqual(len(re.findall(r"--app-[a-z-]+:\s*light-dark\(#[0-9a-f]{6}, #[0-9a-f]{6}\);", body)), 8)

    def test_every_page_source_names_the_template_and_quotes_its_title(self) -> None:
        for path in sorted((ROOT / "docs").glob("*.md")):
            head = path.read_text(encoding="utf-8").split("---")[1] if path.read_text(encoding="utf-8").startswith("---") else ""
            if "template: marketing.html" not in head:
                continue
            for line in head.splitlines():
                key, _, value = line.partition(": ")
                if key in ("title_tag", "description") and ": " in value:
                    self.assertTrue(value.startswith('"') and value.endswith('"'), f"{path.name}: quote the {key}")


class ShellHook(unittest.TestCase):
    def test_data_app_is_added_once(self) -> None:
        hook = load_hook()
        once = hook.add_app('<!doctype html><html lang="en"><head></head></html>')
        self.assertIn('<html data-app="snake" lang="en">', once)
        self.assertEqual(hook.add_app(once), once)

    def test_a_page_with_no_html_tag_raises(self) -> None:
        with self.assertRaises(ValueError):
            load_hook().add_app("<p>no page</p>")


if __name__ == "__main__":
    unittest.main()
