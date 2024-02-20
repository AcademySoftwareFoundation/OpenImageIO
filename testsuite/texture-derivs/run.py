#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# OLD CODE: (result ramp.exr is now checked into src)
# import OpenImageIO as oiio
# 
# # Make a ramp image, left half is increating r at rate dr/ds = 0.5, dr/dt = 0,
# # right half is increasing r at rate dr/ds = 0, dr/dt = 0.5
# b = oiio.ImageBuf (oiio.ImageSpec(128,128,3,oiio.FLOAT))
# for y in range(128) :
#     for x in range(128) :
#         if x < 64 :
#             b.setpixel (x, y, 0, (float(0.5*x)/128.0, 0.0, 0.1))
#         else:
#             b.setpixel (x, y, 0, (float(0.5*y)/128.0, 0.0, 0.1))
# b.write ("ramp.exr")

command = testtex_command ("-res 128 128 -nowarp -wrap clamp -derivs src/ramp.exr")
outputs = [ "out.exr", "out.exr-ds.exr", "out.exr-dt.exr" ]
