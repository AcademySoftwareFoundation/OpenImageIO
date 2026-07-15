#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Python binding for ColorConfig.find_color_spaces: per-axis hint coercion
# (one string or a sequence), the term-grammar escapes, and the determinism
# guarantee (a repeated identical query returns a byte-identical ordered list).

from pathlib import Path
import OpenImageIO as oiio

SRC = Path(__file__).parent

try:
    acc = oiio.ColorConfig(str(SRC / "oiio_test_config.ocio"))

    print("Python find_color_spaces: hint coercion")
    # A str hint and a 1-element-list hint are equivalent.
    print("  str hint == list hint:",
          acc.find_color_spaces(encoding="scene-linear")
          == acc.find_color_spaces(encoding=["scene-linear"]))
    # A bare operator, an unresolvable hint, and a bad element type all raise.
    for bad in ("-", "bogus_hint", ["ok", 3]):
        try:
            acc.find_color_spaces(encoding=bad)
            print("  encoding={!r} did not raise".format(bad))
        except ValueError:
            print("  encoding={!r} raised ValueError".format(bad))

    # Determinism: a repeated identical query is byte-identical and ordered.
    query = dict(chromaticities="lin_rec709_scene",
                 transfer_function="~lin_rec709_scene")
    first = acc.find_color_spaces(**query)
    second = acc.find_color_spaces(**query)
    assert second == first, "find_color_spaces is not deterministic"
    print("  repeated query deterministic:", second == first)

    esc = oiio.ColorConfig(str(SRC / "search_escapes.ocio"))
    print("")
    print("Python find_color_spaces: escape grammar")
    print(r"  encoding='\-foo'  =", esc.find_color_spaces(encoding=r"\-foo"))
    print(r"  encoding='\~foo'  =", esc.find_color_spaces(encoding=r"\~foo"))
    print(r"  encoding='~\~foo' =", esc.find_color_spaces(encoding=r"~\~foo"))
    print(r"  encoding='\\foo'  =", esc.find_color_spaces(encoding=r"\\foo"))
    for bad in ("\\", r"\foo"):
        try:
            esc.find_color_spaces(encoding=bad)
            print("  encoding={!r} did not raise".format(bad))
        except ValueError:
            print("  encoding={!r} raised ValueError".format(bad))
    print("")
    print("Done.")

except Exception as detail:
    print("Unknown exception:", detail)
