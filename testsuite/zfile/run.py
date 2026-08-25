#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


imagedir = "ref/"
command += oiiotool ("-pattern fill:topleft=0.1:topright=0.5:bottomleft=1.0:bottomright=0.3 64x64 1 -chnames Z -d float -o out.zfile")
command += info_command ("out.zfile", extraargs="-stats")

# A 136-byte header declaring a 32767x32767 (~4 GB) image: the
# compression-ratio guard must reject it before the caller allocates.
command += oiiotool ("-nostderr -info src/bomb-32767.zfile", failureok=True)
# Truncated gzip pixel data: the scanline read must hard-error rather than
# return an uninitialized buffer.
command += oiiotool ("-nostderr src/truncated.zfile -o truncated.exr", failureok=True)

outputs = [ "out.zfile", "out.txt" ]
