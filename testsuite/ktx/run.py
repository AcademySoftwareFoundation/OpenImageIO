#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# save the error output
redirect = ' >> out.txt 2>&1 '
files = [
    # Raw, uncompressed formats (sRGB)
    "2d_rgb8.ktx2",
    "2d_rgba8.ktx2",
    # Basis Universal formats
    "2d_uastc.ktx2",
    "2d_etc1s.ktx2",
    # GPU-block-compressed formats
    "2d_astc4x4.ktx2",
    "2d_bc1.ktx2",
    "2d_bc3.ktx2",
    "2d_bc4.ktx2",
    "2d_bc5.ktx2",
    "2d_bc7.ktx2",
    "2d_etc1.ktx2",
    "2d_etc2.ktx2",
]

for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)
