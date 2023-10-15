#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Creates a texture with overscan -- it looks like the usual grid in the
# 0-1 (display) window, but is red checker for a while outside that window.
#
# We then make two images using that texture:
# 1. out-exact.exr just straight maps the [0,1] range, and should look like
#    the whole grid and nothing but the grid.
# 2. out-over.exr maps the [-0.5,1.5] range, so you should see the grid,
#    surrounded by the red check border, surrounded by black. The grid
#    itself should be the "middle half" of the image.

command += oiiotool("../common/grid.tif"
            + " -resize 512x512 "
            + " -pattern checker:color1=1,0,0:color2=.25,0,0 640x640 3 "
            + "-origin -64-64 -paste +0+0 -fullsize 512x512+0+0 -o overscan-src.exr")
command += maketx_command("overscan-src.exr", "grid-overscan.exr",
                          "--filter lanczos3", silent=True)
command += testtex_command ("grid-overscan.exr",
                            "--res 256 256 --wrap black --nowarp -o out-exact.exr",
                            silent=True)
command += testtex_command ("grid-overscan.exr",
                            "--res 256 256 --wrap black --nowarp " +
                            "--offset -0.5 -0.5 0 --scalest 2 2 " +
                            "-o out-over.exr", silent=True)
command += testtex_command ("grid-overscan.exr",
                            "--res 256 256 --wrap clamp --nowarp " +
                            "--offset -0.5 -0.5 0 --scalest 2 2 " +
                            "-o out-overclamp.exr", silent=True)

outputs = [ "out-exact.exr", "out-over.exr", "out-overclamp.exr" ]

