#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


command += oiiotool("../common/grid.tif --crop 512x512+200+100 -o grid-crop.tif")
command += maketx_command ("grid-crop.tif", "grid-crop.tx")
command += testtex_command ("grid-crop.tx", "--wrap black")

outputs = [ "out.exr" ]
