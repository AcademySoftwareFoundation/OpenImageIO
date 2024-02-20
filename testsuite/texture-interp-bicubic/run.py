#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


command = testtex_command ("../common/textures/grid.tx",
                           extraargs = "-interpmode 2  -d uint8 -o out.tif")
outputs = [ "out.tif" ]
