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
# More channels than max_tile_channels, so the cache holds a channel subset
command += oiiotool ("-pattern constant:color=0.5,0.5,0.5,1,0,0,0,0 64x64 8 -d uint8 -otex grey8.exr")
command += testtex_command ("-res 56 56 --no-gettextureinfo --nowarp --texcolorspace sRGB grey8.exr -o cc8.exr")
outputs = [ "nocc.exr", "cc.exr", "cc8.exr", "out.txt" ]
