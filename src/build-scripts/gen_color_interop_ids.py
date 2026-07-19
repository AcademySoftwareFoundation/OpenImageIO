#!/usr/bin/env python3
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

"""
Regenerate src/include/OpenImageIO/color_interop_ids.h from the canonical
`interop_id:` entries declared in src/libOpenImageIO/interop-identities-config.ocio
(OIIO's built-in Color Interop Forum identities registry -- the single
source of truth for the canonical CIID set).

The registry is a small, hand-authored OCIO config, not machine-generated
YAML with exotic scalar styles, so a line-oriented `interop_id: <token>`
scan is sufficient (and avoids adding a PyYAML build dependency for a
one-shot dev-time script). Run this after any registry edit that adds,
removes, or renames an `interop_id:` entry, and commit the regenerated
header alongside it -- src/libOpenImageIO/color_test.cpp asserts the
checked-in header stays in sync with the registry at test time.

Usage:
    python src/build-scripts/gen_color_interop_ids.py
"""

from __future__ import annotations

import pathlib
import re
import shutil
import subprocess

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
REGISTRY_PATH = (REPO_ROOT / "src" / "libOpenImageIO"
                  / "interop-identities-config.ocio")
HEADER_PATH = (REPO_ROOT / "src" / "include" / "OpenImageIO"
               / "color_interop_ids.h")

INTEROP_ID_RE = re.compile(r"^\s*interop_id:\s*(\S+)\s*$")


def canonical_ids() -> list[str]:
    ids = set()
    for line in REGISTRY_PATH.read_text().splitlines():
        m = INTEROP_ID_RE.match(line)
        if m:
            ids.add(m.group(1))
    return sorted(ids)


def constant_name(interop_id: str) -> str:
    # Namespaced ids ("ocio:foo", "oiio:foo") use ':' as a separator, which
    # isn't legal in a C++ identifier; '_' is never used inside a namespace
    # or base token by the CIF grammar, so this substitution is unambiguous
    # and collision-free over the current registry (checked at generation
    # time below).
    return interop_id.replace(":", "_")


def generate(ids: list[str]) -> str:
    names = [constant_name(i) for i in ids]
    if len(set(names)) != len(names):
        raise SystemExit(
            "gen_color_interop_ids: sanitizing ':' to '_' produced a "
            "duplicate constant name -- registry has grown a colliding id; "
            "pick a different sanitization scheme before regenerating.")

    lines = [
        "// Copyright Contributors to the OpenImageIO project.",
        "// SPDX-License-Identifier: Apache-2.0",
        "// https://github.com/AcademySoftwareFoundation/OpenImageIO",
        "",
        "// GENERATED FILE -- DO NOT EDIT BY HAND.",
        "// Regenerate with: python src/build-scripts/gen_color_interop_ids.py",
        "// Source of truth: src/libOpenImageIO/interop-identities-config.ocio",
        "//",
        "// One `inline constexpr string_view` per canonical Color Interop Forum",
        "// ID declared in OIIO's built-in interop identities registry (see the",
        "// CIF recommendation \"An ID for Color Interop\",",
        "// https://github.com/AcademySoftwareFoundation/ColorInterop/wiki).",
        "// Values are the canonical ID strings -- pass them anywhere a",
        "// string_view CIID is accepted (e.g. ColorConfig::get_color_interop_id);",
        "// no API signature changes. Raw strings remain first-class for ids this",
        "// finite set cannot enumerate (local/custom/icc/user-namespaced ids).",
        "",
        "#pragma once",
        "",
        "#include <OpenImageIO/oiioversion.h>",
        "#include <OpenImageIO/string_view.h>",
        "",
        "OIIO_NAMESPACE_BEGIN",
        "",
        "namespace ColorInteropIDs {",
        "",
    ]
    for ident, iid in zip(names, ids):
        lines.append(f'inline constexpr string_view {ident} = "{iid}";')
    lines += [
        "",
        "/// Every constant above, for iteration (e.g. the sync test in",
        "/// color_test.cpp). Kept in sync with the individual constants by",
        "/// construction: both are emitted from the same generator run.",
        "inline constexpr string_view all[] = {",
    ]
    for i in range(0, len(names), 4):
        lines.append("    " + ", ".join(names[i:i + 4]) + ",")
    lines += [
        "};",
        "",
        "}  // namespace ColorInteropIDs",
        "",
        "OIIO_NAMESPACE_END",
        "",
    ]
    return "\n".join(lines)


def main() -> None:
    ids = canonical_ids()
    if not ids:
        raise SystemExit(
            f"gen_color_interop_ids: found no `interop_id:` entries in "
            f"{REGISTRY_PATH}")
    HEADER_PATH.write_text(generate(ids))
    # Match repo style (aligned assignments etc.) rather than replicating
    # clang-format's layout rules in this script.
    if shutil.which("clang-format"):
        subprocess.run(["clang-format", "-i", str(HEADER_PATH)], check=True)
    else:
        print("warning: clang-format not found on PATH; run it on the "
              "output before committing")
    print(f"Wrote {len(ids)} canonical CIID constants to {HEADER_PATH}")


if __name__ == "__main__":
    main()
