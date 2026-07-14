#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Cross-config color conversion: when --colorconvert or --ociodisplay names a
# color space this OCIO config doesn't define, but the name is one OIIO
# recognizes from a small set of common, well-known identities (see
# src/libOpenImageIO/interop-identities-config.ocio), the conversion routes
# through those shared identities instead of failing outright -- as long as
# this config can itself be related to them via its "aces_interchange" role.
# OCIO's own "strictparsing" config setting opts out of this and restores the
# unconditional hard error.

import os

redirect = " >> out.txt 2>&1 "

# The two-config GetProcessorFromConfigs overloads and the interchange-role
# machinery this relies on need OCIO >= 2.3.
if float(ociover) < 2.3 :
    print("Skipping color-interop-convert tests: need OCIO >= 2.3, have", ociover)
else :
    # Debug-gated narration (which config, which route, why a fallback was
    # taken) goes to stderr; capture it in out.txt alongside everything else.
    os.environ["OPENIMAGEIO_DEBUG"] = "1"

    command += oiiotool ("--pattern constant:color=0.18,0.42,0.73 64x64 3 "
                         + "-d float -o probe.exr")

    # (1) Cross-config conversion success: "ap0" (this config's own,
    # transformless scene reference) -> "lin_ap1_scene" (a well-known ACEScg
    # identity this config never defines). The config is interoperable (it
    # declares an aces_interchange role), so the conversion is bridged through
    # the shared identities and produces a real, non-identity transform.
    # The actual pixel values depend on which built-in identities config OCIO
    # linked against provides (see build_interop_identities_config()), which
    # can shift slightly across OCIO versions -- so check the result as an
    # image (toleranced idiff), not as an exact-precision printed string.
    command += oiiotool ("--colorconfig src/interop.ocio probe.exr "
                         + "--colorconvert ap0 lin_ap1_scene "
                         + "-echo \"cross-config convert: wrote xconv-cross.exr\" "
                         + "-o xconv-cross.exr")

    # (2) Cross-config display success: the INPUT is the foreign, well-known
    # identity; "disp"/"view1" are local. The foreign source is bridged
    # through the shared identities into the local display/view. Same
    # version-dependent-precision reasoning as (1): compare as an image.
    command += oiiotool ("--colorconfig src/interop.ocio probe.exr "
                         + "--iscolorspace lin_ap1_scene "
                         + "--ociodisplay disp view1 "
                         + "-echo \"cross-config display: wrote xdisp-cross.exr\" "
                         + "-o xdisp-cross.exr")

    # (3) Strict-on hard error: the same conversion as (1), but this config
    # has OCIO strict parsing enabled, which opts out of the bridge and
    # restores today's hard error -- even though the config is otherwise
    # interoperable and the bridge could have resolved the name.
    command += oiiotool ("--colorconfig src/interop-strict.ocio probe.exr "
                         + "--colorconvert ap0 lin_ap1_scene "
                         + "-echo \"strict-on: should not print\"",
                         failureok=True)

    # (4) Strict-off fallback: this config is NOT interoperable at all (no
    # aces_interchange role, nothing to repair), so the bridge cannot apply --
    # but strict parsing is off, so the conversion falls back to a
    # pass-through (pixels unchanged) instead of failing, and narrates why.
    # The pass-through must not mistag the result with the requested (never
    # actually reached) destination space -- it should honestly keep
    # documenting the source space "enc", since no conversion happened. This
    # is checked as a color space NAME (not a float value), so it stays
    # stable across OCIO versions.
    command += oiiotool ("--colorconfig src/noninterop.ocio probe.exr "
                         + "--colorconvert enc lin_ap1_scene "
                         + "-echo \"strict-off fallback: {TOP.AVGCOLOR}\" "
                         + "-echo \"strict-off fallback colorspace tag: {TOP[\\\"oiio:ColorSpace\\\"]}\"")

    # (5) Untouched local-name conversion: both spaces are defined by this
    # config directly, so none of the cross-config machinery above is
    # involved -- ordinary local resolution behaves exactly as before.
    # Checked as an image for consistency with (1)/(2), even though this
    # route doesn't touch OCIO's built-in identities config.
    command += oiiotool ("--colorconfig src/interop.ocio probe.exr "
                         + "--colorconvert ap0 g22 "
                         + "-echo \"local-only convert: wrote xconv-local.exr\" "
                         + "-o xconv-local.exr")

    # (6) Strict-off inverse-display fallback: same non-interoperable config
    # as (4), but the INVERSE --ociodisplay direction. The input is
    # display-encoded; we ask to invert back to the foreign, well-known
    # identity "lin_ap1_scene" the config doesn't define. The bridge can't
    # apply (not interoperable) and strict parsing is off, so it falls back to
    # a pass-through -- the pixels never leave the "disp"/"view1" display
    # encoding. The honest tag is therefore that display/view's color space
    # ("enc"), NOT the "lin_ap1_scene" the inversion was reaching for and
    # never produced. Like (4), checked as a NAME so it stays version-stable.
    command += oiiotool ("--colorconfig src/noninterop.ocio probe.exr "
                         + "--ociodisplay:from=lin_ap1_scene:inverse=1 disp view1 "
                         + "-echo \"inverse strict-off fallback colorspace tag: {TOP[\\\"oiio:ColorSpace\\\"]}\"")

outputs = [ "xconv-cross.exr", "xdisp-cross.exr", "xconv-local.exr", "out.txt" ]
