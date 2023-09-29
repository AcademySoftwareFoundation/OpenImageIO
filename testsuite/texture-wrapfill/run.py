#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# This tests a particular tricky case: the interplay of "black" wrap mode
# with fill color.  Outside the s,t [0,1] range, it should be black, NOT
# fill color.  

# Make an RGB grid for our test
command += oiiotool("../common/grid.tif -ch R,G,B -o grid3.tif")

# And a 1-channel grid for our test
command += oiiotool("../common/grid.tif -ch R -o grid1.tif")

command += testtex_command ("grid3.tif",
                            " -res 256 256 -automip " +
                            " -wrap black -fill 0.5 -d uint8 -o out3.tif")

command += testtex_command ("grid1.tif",
                            " -res 200 200 -automip --graytorgb " +
                            " -wrap black -fill 0.5 -d uint8 -o out1.tif")

outputs = [ "out3.tif", "out1.tif" ]
