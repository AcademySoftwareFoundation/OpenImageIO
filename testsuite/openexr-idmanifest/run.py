#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Multi-part, not deep
command += info_command ("src/manifest.exr", safematch = True)
command += oiiotool ("src/manifest.exr -o manifest.exr")
command += info_command ("./manifest.exr", safematch = True)

outputs += [ "out.txt" ]
