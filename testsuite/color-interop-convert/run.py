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
    command += oiiotool ("--colorconfig src/interop.ocio probe.exr "
                         + "--colorconvert ap0 lin_ap1_scene "
                         + "-echo \"cross-config convert: {TOP.AVGCOLOR}\"")

    # (2) Cross-config display success: the INPUT is the foreign, well-known
    # identity; "disp"/"view1" are local. The foreign source is bridged
    # through the shared identities into the local display/view.
    command += oiiotool ("--colorconfig src/interop.ocio probe.exr "
                         + "--iscolorspace lin_ap1_scene "
                         + "--ociodisplay disp view1 "
                         + "-echo \"cross-config display: {TOP.AVGCOLOR}\"")

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
    command += oiiotool ("--colorconfig src/noninterop.ocio probe.exr "
                         + "--colorconvert enc lin_ap1_scene "
                         + "-echo \"strict-off fallback: {TOP.AVGCOLOR}\"")

    # (5) Untouched local-name conversion: both spaces are defined by this
    # config directly, so none of the cross-config machinery above is
    # involved -- ordinary local resolution behaves exactly as before.
    command += oiiotool ("--colorconfig src/interop.ocio probe.exr "
                         + "--colorconvert ap0 g22 "
                         + "-echo \"local-only convert: {TOP.AVGCOLOR}\"")

outputs = [ "out.txt" ]
