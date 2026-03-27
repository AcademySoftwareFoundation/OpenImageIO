#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = ' >> out.txt 2>&1 '

files = [ "psd_123.psd", "psd_123_nomaxcompat.psd", "psd_bitmap.psd",
          "psd_indexed_trans.psd", "psd_rgb_8.psd", "psd_rgb_16.psd",
          "psd_rgb_32.psd", "psd_rgba_8.psd", "psd_rgb_16_rle.psd" ]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)
    # command += oiiotool (OIIO_TESTSUITE_IMAGEDIR + f"/{f} -o ./{f}.tif")

# Test unassociated alpha metadata
command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/psd_123.psd", extraargs="--no-autopremult")

command += info_command ("src/different-mask-size.psd")
command += info_command ("src/layer-mask.psd")

# Test more modern (Photoshop 2023 files) with 16- and 32-bit files containing multiple sublayers
command += info_command ("src/Layers_8bit_RGB.psd")
command += info_command ("src/Layers_16bit_RGB.psd")
command += info_command ("src/Layers_32bit_RGB.psd")

# This file has a corrupted Exif block
command += info_command ("src/crash-psd-exif-1632.psd", failureok = 1)
# Corrupted thumbnail clobbered memory
command += info_command ("src/crash-thumb-1626.psd", failureok = 1)
# Corruption caused an integer overflow
command += info_command ("src/crash-005c.psd", failureok=True)
# Corruption where the file didn't have enough channels for its color mode
command += info_command ("src/crash-8a15.psd", failureok=True)
# Corruption where bad zip compression data caused a buffer overrun
command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/corrupt_20260312a.psd", failureok=True)

