#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Tests input image which is partial crop, partial overscan!

command += oiiotool("../common/grid.tif " +
                    "--crop 500x1000+250+0 --fullsize 1000x800+0+100 -o grid-cropover.exr")
command += maketx_command ("grid-cropover.exr", "grid-cropover.tx.exr")
command += testtex_command ("grid-cropover.tx.exr", "--wrap black")

outputs = [ "out.exr" ]
