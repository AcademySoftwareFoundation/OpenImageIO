#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# This test just maps a 50% grey texture, once with default settings, and once
# declaring that it thinks the texture is in sRGB texture space.

# Note: we deliberately make the two output images different sizes so that
# they can't match against each others' ref images.

command += oiiotool ("-pattern constant:color=0.5,0.5,0.5 64x64 3 -d uint8 -otex grey.exr")
command += testtex_command ("-res 64 64 --no-gettextureinfo --nowarp grey.exr -o nocc.exr")
command += testtex_command ("-res 60 60 --no-gettextureinfo --nowarp --texcolorspace sRGB grey.exr -o cc.exr")
outputs = [ "nocc.exr", "cc.exr", "out.txt" ]
