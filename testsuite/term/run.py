#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


redirect = ' >> out.txt 2>&1 '

command += oiiotool ("-pattern fill:topleft=1,0,0:topright=0,1,0:bottomleft=0,0,1:bottomright=1,1,1 60x40 3 -o ramp.exr")
command += oiiotool ("-pattern fill:topleft=1,0,0:topright=0,1,0:bottomleft=0,0,1:bottomright=1,1,1 20x10 3 -o rampsmall.exr")

# Simplify by treating everything as sRGB already, so the tests don't include
# any uncertainty about color space conversion.
command += oiiotool ("-echo 24bit rampsmall.exr -iscolorspace srgb -attrib term:method 24bit -attrib term:fit 0 -attrib term:filename 24bit.out -o 24bit.term")
command += oiiotool ("-echo 24bit-space rampsmall.exr -iscolorspace srgb -attrib term:method 24bit-space -attrib term:fit 0 -attrib term:filename 24bit-space.out -o 24bit-space.term")
command += oiiotool ("-echo dither rampsmall.exr -iscolorspace srgb -attrib term:method dither -attrib term:fit 0 -attrib term:filename dither.out -o dither.term")
command += oiiotool ("-echo iterm2 ramp.exr -iscolorspace srgb -attrib term:method iterm2 -attrib term:fit 0 -attrib term:filename iterm2.out -o iterm2.term")

command += oiiotool ("-echo done")

outputs = [ "24bit.out", "24bit-space.out", "dither.out",
            "iterm2.out",
            "out.txt" ]
