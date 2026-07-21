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


# Spec 09 Feature 1: force_interop_id. The same slotless formats (TIFF, JPEG)
# now DO carry colorInteropID when the CONFIG declares
# oiio:colorpolicy:write:force_interop_id -- with NO attribute set (the policy
# is config-declared, layer 2). The forced id is emitted as an aux string
# attribute that round-trips through XMP and reads back. Contrast with the TIF
# section above (same source, plain config -> untagged).
forcecfg = "src/force_interop.ocio"


def forced_section(fmt, note):
    out = "force." + fmt
    lines = ""
    lines += "echo '=== " + fmt.upper() + " forced: " + note + " ==='" + redirect + " ;\n"
    lines += ("env OCIO=" + forcecfg + " " + ot + " " + mksrc + "-o " + out
              + " >/dev/null 2>&1 ;\n")
    lines += ("( env OCIO=" + forcecfg + " " + ot + " --info -v " + out
              + " 2>/dev/null | grep -E 'colorInteropID' || true )"
              + redirect + " ;\n")
    return lines


command += forced_section("tif", "config-declared force -- colorInteropID carried")
command += forced_section("jpg", "config-declared force -- colorInteropID carried")


# Spec 09 Feature B: write-canonical mapping in oiio:default. The config's
# oiio:default profile declares oiio:colorpolicy:write:canonicalize, so a
# g26_p3d65_display source is written with the CANONICAL id g26_xyzd65_display
# (the XYZ DCDM form -- gamma 2.6 + DCI white scaling, alias dcdm_xyzd65), a
# P3->XYZ primaries conversion within the DCI-white-scaled family. No attribute
# is set anywhere -- the mapping is config-declared (layer 2). EXR carries the
# id in its native slot, so it reads back as the mapped id.
bcfg = "src/broadcast.ocio"
g26src = "--create 8x8 3 --attrib oiio:ColorSpace g26_p3d65_display "

command += "echo '=== g26 default: canonicalized to g26_xyzd65_display on write ==='" + redirect + " ;\n"
command += ("env OCIO=" + bcfg + " " + ot + " " + g26src + "-o g26def.exr"
            + " >/dev/null 2>&1 ;\n")
command += ("( env OCIO=" + bcfg + " " + ot + " --info -v g26def.exr"
            + " 2>/dev/null | grep -E 'colorInteropID' || true )" + redirect + " ;\n")

outputs = ["out.txt"]
