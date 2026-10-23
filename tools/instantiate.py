#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Turn a copy of this template into a new project.

    python3 tools/instantiate.py --name my_project [--description "One line"] [--dry-run]

--name is lower snake case. It replaces icl_template in namespaces, targets, directories and
include paths; MY_PROJECT replaces ICL_TEMPLATE (CMake options and the variable prefix); my-project
replaces icl-template (the vcpkg name and archive names). The changelog is reset and the
script removes itself unless --keep is given.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKIP_DIRS = {".git", "build", ".icl-infra", "stage", "dist", "__pycache__", ".venv"}
NAME = re.compile(r"^[a-z][a-z0-9]*(_[a-z0-9]+)*$")


def is_text(path: Path) -> bool:
    try:
        path.read_text(encoding="utf-8")
        return True
    except (UnicodeDecodeError, OSError):
        return False


def files() -> list[Path]:
    out = []
    for path in sorted(ROOT.rglob("*")):
        rel = path.relative_to(ROOT)
        if any(part in SKIP_DIRS for part in rel.parts):
            continue
        if path.is_file() and path != Path(__file__).resolve():
            out.append(path)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", required=True)
    ap.add_argument("--description", default="")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--keep", action="store_true", help="keep this script")
    args = ap.parse_args()
    if not NAME.match(args.name):
        print("--name must be lower snake case, for example my_project", file=sys.stderr)
        return 2
    snake, upper, kebab = args.name, args.name.upper(), args.name.replace("_", "-")
    pairs = [("icl_template", snake), ("ICL_TEMPLATE", upper), ("icl-template", kebab)]

    changed = 0
    for path in files():
        if not is_text(path):
            continue
        text = path.read_text(encoding="utf-8")
        new = text
        for old, repl in pairs:
            new = new.replace(old, repl)
        if new != text:
            changed += 1
            print(f"edit   {path.relative_to(ROOT)}")
            if not args.dry_run:
                path.write_text(new, encoding="utf-8", newline="")

    # Rename deepest paths first so a parent rename does not invalidate a child path.
    renames = sorted((p for p in ROOT.rglob("*") if "icl_template" in p.name
                      and not any(part in SKIP_DIRS for part in p.relative_to(ROOT).parts)),
                     key=lambda p: len(p.parts), reverse=True)
    for path in renames:
        target = path.with_name(path.name.replace("icl_template", snake))
        print(f"rename {path.relative_to(ROOT)} -> {target.relative_to(ROOT)}")
        if not args.dry_run:
            path.rename(target)

    if not args.dry_run:
        for fragment in (ROOT / "changelog.d").glob("*.md"):
            if fragment.name != "README.md":
                fragment.unlink()
        changelog = ROOT / "CHANGELOG.md"
        if changelog.exists():
            changelog.write_text("# Changelog\n\nAll notable changes are listed here. Each release section is assembled from the\n"
                                 "fragments in `changelog.d/`.\n", encoding="utf-8")
        if not args.keep:
            Path(__file__).resolve().unlink()
    print(f"{changed} files edited, {len(renames)} paths renamed")
    print("Next: set the description and topics in .github/repo-settings.json, update .github/CODEOWNERS, "
          "then run the steps in docs/new-project.md.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
