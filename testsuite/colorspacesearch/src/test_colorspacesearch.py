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
    # A bare operator or an unresolvable hint is reported through the
    # ColorConfig error convention (geterror()) and yields an empty result;
    # a bad element type is a Python-level argument error and still raises.
    for bad in ("-", "bogus_hint"):
        r = acc.find_color_spaces(encoding=bad)
        print("  encoding={!r} -> {}, error: {}".format(
              bad, r, "yes" if acc.geterror() else "no"))
    try:
        acc.find_color_spaces(encoding=["ok", 3])
        print("  encoding=['ok', 3] did not raise")
    except ValueError:
        print("  encoding=['ok', 3] raised ValueError")

    # Determinism: a repeated identical query is byte-identical and ordered.
    query = dict(chromaticities="lin_rec709_scene",
                 transfer_function="~lin_rec709_scene")
    first = acc.find_color_spaces(**query)
    second = acc.find_color_spaces(**query)
    assert second == first, "find_color_spaces is not deterministic"
    print("  repeated query deterministic:", second == first)

    esc = oiio.ColorConfig(str(SRC / "search_escapes.ocio"))
    print("")
    # authored_encoding_only=True throughout: the fixture spaces are identity
    # transforms whose authored encodings contradict what fingerprinting
    # infers, and this block tests the term grammar, not twin-encoding
    # inference.
    print("Python find_color_spaces: escape grammar")
    print(r"  encoding='\-foo'  =",
          esc.find_color_spaces(encoding=r"\-foo",
                                authored_encoding_only=True))
    print(r"  encoding='\~foo'  =",
          esc.find_color_spaces(encoding=r"\~foo",
                                authored_encoding_only=True))
    print(r"  encoding='~\~foo' =",
          esc.find_color_spaces(encoding=r"~\~foo",
                                authored_encoding_only=True))
    print(r"  encoding='\\foo'  =",
          esc.find_color_spaces(encoding=r"\\foo",
                                authored_encoding_only=True))
    for bad in ("\\", r"\foo"):
        r = esc.find_color_spaces(encoding=bad)
        print("  encoding={!r} -> {}, error: {}".format(
              bad, r, "yes" if esc.geterror() else "no"))

    # Twin-encoding inference and the authored-only gate: theatrical_output
    # authors sdr-video but is tagged interop_id g26_p3d65_display, whose
    # registry twin carries sdr-cinema -- it matches both values unless
    # authored_encoding_only.
    twin = oiio.ColorConfig(str(SRC / "search_twin.ocio"))
    print("")
    print("Python find_color_spaces: twin encoding and authored_encoding_only")
    print("  encoding='sdr-cinema'              =",
          twin.find_color_spaces(encoding="sdr-cinema"))
    print("  encoding='sdr-video'               =",
          twin.find_color_spaces(encoding="sdr-video"))
    print("  encoding='theatrical_output'       =",
          twin.find_color_spaces(encoding="theatrical_output"))
    print("  encoding='sdr-cinema', authored_encoding_only=True =",
          twin.find_color_spaces(encoding="sdr-cinema",
                                 authored_encoding_only=True))
    print("")
    print("Done.")

except Exception as detail:
    print("Unknown exception:", detail)
