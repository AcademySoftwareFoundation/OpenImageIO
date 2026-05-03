#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Test FLIP perceptual image difference metric via oiiotool --flipdiff.

command += oiiotool("-pattern fill:color=0.1,0.1,0.1 64x64 3 -d half -o dark.exr")
command += oiiotool("-pattern fill:color=0.9,0.9,0.9 64x64 3 -d half -o light.exr")

# Same images shoudl report 0.0 difference
command += oiiotool("--experimental --echo \"Same image test\" dark.exr dark.exr --flipdiff:hdr=0")

# Two images that differ -- FLIP should report nonzero difference.
command += oiiotool("--experimental --echo \"Differing images, LDR\" dark.exr light.exr --flipdiff:hdr=0")
command += oiiotool("--experimental --echo \"Differing images, HDR\" dark.exr light.exr --flipdiff:hdr=1:maxluminance=0")

# Test against a small crop of the NVIDIA test image from the FLIP repo,
# just like NVIDIA with automatic selection of exposures.
command += oiiotool("--experimental src/reference-crop.exr src/test-crop.exr --flipdiff:hdr=1:maxluminance=0.0 -o reference-test-flip-auto.exr")
command += oiiotool("reference-test-flip-auto.exr --colormap magma -o reference-test-flip-colormapped.exr")

# Test against a small crop of the NVIDIA test image from the FLIP repo,
# the "OIIO way" of a standard max luminance.
command += oiiotool("--experimental src/reference-crop.exr src/test-crop.exr --flipdiff:hdr=1:maxluminance=2.0 -o reference-test-flip-oiio.exr")

# Test LDR, also suppress print, and setting variables
command += oiiotool("--experimental src/reference-crop.exr src/test-crop.exr " +
                    "--flipdiff:hdr=0:print=0 " +
                    "--echo \"LDR FLIP mean {TOP.'FLIP:meanerror'} max {TOP.'FLIP:maxerror'} max location ({TOP.'FLIP:maxx'}, {TOP.'FLIP:maxy'})\" " +
                    "-o reference-test-flip-ldr.exr")

# Outputs to check against references
outputs = [
    "reference-test-flip-auto.exr",
    "reference-test-flip-oiio.exr",
    "reference-test-flip-colormapped.exr",
    "out.txt",
    ]
