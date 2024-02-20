#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


imagedir = "ref/"
command += oiiotool ("-pattern fill:topleft=0.1:topright=0.5:bottomleft=1.0:bottomright=0.3 64x64 1 -chnames Z -d float -o out.zfile")
command += info_command ("out.zfile", extraargs="-stats")

outputs = [ "out.zfile", "out.txt" ]
