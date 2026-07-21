#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Spec 09 -- per-format color-identity round-trip contract. Not every format
# round-trips color identity 1:1, but each format's behavior is PREDICTABLE and
# LOCKED here. We tag ONE source image (color space srgb_rec709_display, plus an
# explicit CICP tuple), write it to each format, read it back, and assert the
# DOCUMENTED contract for that format -- not byte-identity.
#
#   EXR  -- carries colorInteropID (native slot). CICP is never written, even
#           when the source has an explicit tuple (EXR has no CICP convention,
#           so it is stripped).
#   PNG  -- carries the display identity as a cICP chunk (H.273 tuple). It
#           round-trips as CICP, resolving back to the display space -- NOT as
#           colorInteropID.
#   TIFF -- carries NO color identity in this build (no native colorInteropID or
#           CICP slot wired): a round-tripped TIFF reads back untagged. Lossy but
#           predictable.
#   JPEG -- carries no color identity either. The JPEG reader fixed sRGB
#           assumption resolves every JPEG to srgb_rec709_scene regardless of the
#           source tag. Predictable, not a preserved identity.
#
# The ambient config is pinned to src/plain.ocio so the resolved names are
# deterministic (it defines srgb_rec709_display / _scene, lin_ap1_scene).
#
# NOTE: the test harness splits `command` on ';' and runs each fragment through
# its own shell, so note text below carries no ';' and no apostrophe.

redirect = " >> out.txt 2>&1 "
ot  = oiio_app("oiiotool")
cfg = "src/plain.ocio"

# One tagged source: a display identity plus an explicit CICP (1,13,0,1) tuple.
# The CICP is here to prove EXR strips it; PNG derives its own tuple regardless.
mksrc = ("--create 8x8 3 --attrib oiio:ColorSpace srgb_rec709_display "
         "'--attrib:type=int[4]' CICP 1,13,0,1 ")


def section(fmt, note):
    out = "rt." + fmt
    lines = ""
    # Header (self-describing contract).
    lines += "echo '=== " + fmt.upper() + ": " + note + " ==='" + redirect + " ;\n"
    # Write the tagged source to this format (muted -- the write itself is noise).
    lines += ("env OCIO=" + cfg + " " + ot + " " + mksrc + "-o " + out
              + " >/dev/null 2>&1 ;\n")
    # Read back and capture ONLY the color-identity lines (grepped, so the
    # varying build hash / timestamp of --info -v never reach the reference).
    # `|| true` so a format that carries nothing (TIFF) is not a shell failure.
    lines += ("( env OCIO=" + cfg + " " + ot + " --info -v " + out
              + " 2>/dev/null | grep -E "
              + "'colorInteropID|oiio:ColorSpace|^    CICP:' || true )"
              + redirect + " ;\n")
    return lines


command += section("exr", "colorInteropID preserved -- CICP stripped")
command += section("png", "round-trips as cICP chunk -- not colorInteropID")
command += section("tif", "color identity dropped -- reads back untagged")
command += section("jpg", "reader fixed sRGB assumption -- srgb_rec709_scene")

outputs = ["out.txt"]
