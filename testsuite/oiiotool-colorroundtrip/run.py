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


# Spec 09 Feature B (reconciler write-shape): write-canonical CONVERSION in
# oiio:default. The config's oiio:default profile declares
# oiio:colorpolicy:write:canonicalize, so a g26_p3d65_display source is CONVERTED
# (not merely relabeled) to g26_xyzd65_display on write -- a real P3->XYZ
# primaries + DCI white headroom pixel conversion (the XYZ DCDM form, gamma 2.6 +
# 48/52.37 white scaling, alias dcdm_xyzd65), applied through OIIO's embedded
# interop registry. No attribute is set anywhere -- the mapping is config-declared
# (layer 2). We prove BOTH halves: (a) EXR carries the canonical id in its native
# slot; (b) the written pixels actually changed vs a no-conversion baseline (same
# source under the plain config, which declares no canonicalize) -- a
# metadata-only relabel would leave the pixels identical.
bcfg = "src/broadcast.ocio"
g26src = ("--pattern constant:color=0.5,0.5,0.5 8x8 3 "
          "--attrib oiio:ColorSpace g26_p3d65_display ")

# (a) tag proof.
command += "echo '=== g26 default: canonicalized to g26_xyzd65_display on write ==='" + redirect + " ;\n"
command += ("env OCIO=" + bcfg + " " + ot + " " + g26src + "-o g26def.exr"
            + " >/dev/null 2>&1 ;\n")
command += ("( env OCIO=" + bcfg + " " + ot + " --info -v g26def.exr"
            + " 2>/dev/null | grep -E 'colorInteropID' || true )" + redirect + " ;\n")

# (b) pixel proof: the canonicalized pixels DIFFER from the un-converted baseline
# (same source written under the plain config). --diff prints FAILURE when the
# images differ -> the conversion really touched the pixels. (A metadata-only
# relabel would print PASS.)
command += "echo '=== g26 default: pixels CONVERTED not relabeled (differ from baseline) ==='" + redirect + " ;\n"
command += ("env OCIO=" + cfg + " " + ot + " " + g26src + "-o g26plain.exr"
            + " >/dev/null 2>&1 ;\n")
command += ("( env OCIO=" + cfg + " " + ot + " g26def.exr g26plain.exr --diff"
            + " 2>/dev/null | grep -E 'PASS|FAILURE' || true )" + redirect + " ;\n")


# Spec 09 Feature A: the oiio:broadcast profile. Selecting it (env-var layer 3)
# routes P3 display content into the broadcast delivery container -- Rec.2020
# encoding primaries SIGNALED (CICP primaries code 9), narrow (limited) range
# (video_full_range_flag 0), and the true P3(D65) gamut carried in the MDCV
# mastering-display volume (not re-gamut'd). This supersedes the oiio:default
# canonicalize mapping for P3 content. PNG carries the tuple as a cICP chunk, so
# the primaries + range read back; the MDCV volume is shown via --colorwriteplan
# (no format in this build carries mDCV to a file yet).
bcast = "env OCIO=" + bcfg + " OPENIMAGEIO_COLORPOLICY=oiio:broadcast "

command += "echo '=== broadcast: P3 -> Rec.2020 signal + narrow range (PNG cICP readback) ==='" + redirect + " ;\n"
command += (bcast + ot + " " + g26src + "-o bcast.png >/dev/null 2>&1 ;\n")
command += ("( env OCIO=" + bcfg + " " + ot + " --info -v bcast.png"
            + " 2>/dev/null | grep -E '^    CICP:' || true )" + redirect + " ;\n")

command += "echo '=== broadcast: write plan for png -- Rec.2020 cICP + P3 MDCV volume ==='" + redirect + " ;\n"
command += (bcast + ot + " " + g26src + "--colorwriteplan png"
            + " 2>/dev/null | grep -E 'cicp|mdcv'" + redirect + " ;\n")


# oicio spec 34 + RFC 0006: the derived P3(D65) mastering-display volume now
# lands in the PNG file itself, as an SMPTE ST 2086 mDCV chunk, and reads back.
# The written bcast.png above carries it. We adopt oicio's EXACT wire keys
# ("Wire metadata keys", spec 34 lines 84-89): mdcv_{red,green,blue,white}_{x,y}
# = chromaticity xy scaled x50000, mdcv_max/min_luminance = cd/m2 x10000 (min
# floored at 1). The mDCV chunk needs libpng >= 1.6.50 (PNG_mDCV_SUPPORTED);
# on older libpng the writer/reader silently no-op, so this section is gated on
# the build's libpng version and skipped (not failed) when unsupported.
import re
libdeps = subprocess.check_output(
    [oiio_app('oiiotool').strip(), '--echo',
     '{getattribute(build:dependencies)}']).decode('utf-8')
_m = re.search(r'[Pp][Nn][Gg][^0-9]*([0-9]+)\.([0-9]+)\.([0-9]+)', libdeps)
png_has_mdcv = bool(_m) and (int(_m.group(1)), int(_m.group(2)),
                             int(_m.group(3))) >= (1, 6, 50)

if png_has_mdcv:
    command += ("echo '=== broadcast: PNG mDCV chunk round-trip"
                " (ST 2086 P3-D65 volume, xy x50000 / luminance x10000) ==='"
                + redirect + " ;\n")
    command += ("( env OCIO=" + bcfg + " " + ot + " --info -v bcast.png"
                + " 2>/dev/null | grep -E 'mdcv_' || true )" + redirect + " ;\n")

outputs = ["out.txt"]
