#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


failureok = True
redirect = ' >> out.txt 2>&1 '

# src/bomb.png is a tiny (353-byte) PNG whose IHDR declares a ~6 GB image
# (46340x46340x3): a decompression bomb. The reader must reject it with a
# bounded error instead of attempting the enormous allocation.
command += oiiotool("--info -v src/bomb.png", failureok=True)
