#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/OpenImageIO/oiio

redirect = ' >> out.txt 2>&1 '

files = [ "psd_123.psd", "psd_123_nomaxcompat.psd", "psd_bitmap.psd",
          "psd_indexed_trans.psd", "psd_rgb_8.psd", "psd_rgb_16.psd",
          "psd_rgb_32.psd", "psd_rgba_8.psd" ]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)

# Test unassociated alpha metadata
command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/psd_123.psd", extraargs="--no-autopremult")

command += info_command ("src/different-mask-size.psd")
command += info_command ("src/layer-mask.psd")

# This file has a corrupted Exif block
command += info_command ("src/crash-psd-exif-1632.psd", failureok = 1)
# Corrupted thumbnail clobbered memory
command += info_command ("src/crash-thumb-1626.psd", failureok = 1)
