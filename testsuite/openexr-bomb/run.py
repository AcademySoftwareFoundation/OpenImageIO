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
