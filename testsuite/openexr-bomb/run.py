#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


failureok = True
redirect = ' >> out.txt 2>&1 '

# src/bomb.exr is a tiny (464-byte) EXR whose dataWindow declares a ~12 GB
# image: a decompression bomb. Both the C++ reader (openexr:core=0) and the
# C-API core reader (openexr:core=1) must reject it with a bounded error
# instead of attempting the enormous allocation.
command += oiiotool("-oiioattrib openexr:core 0 --info -v src/bomb.exr",
                    failureok=True)
command += oiiotool("-oiioattrib openexr:core 1 --info -v src/bomb.exr",
                    failureok=True)

# Headers with a sane data window but an absurd tile size or display window
# must also be rejected at open, not trigger an enormous allocation later
# when reading tiles or making a full-window buffer (#5506).
for f in ["src/bigtile.exr", "src/bigdisplay.exr", "src/bigattr.exr"]:
    command += oiiotool("-oiioattrib openexr:core 0 --info -v " + f,
                        failureok=True)
    command += oiiotool("-oiioattrib openexr:core 1 --info -v " + f,
                        failureok=True)

# Converting deep files with several samples in every pixel must go through
# the deep API (not overrun a flat buffer), including when iconvert can't
# use copy_image.
for f in ["src/deep-dense-scan.exr", "src/deep-dense-tiled.exr"]:
    command += iconvert("--no-copy-image " + f + " deep-out.exr")
    command += diff_command(f, "deep-out.exr")
