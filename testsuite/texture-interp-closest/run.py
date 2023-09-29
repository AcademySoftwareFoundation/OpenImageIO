#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Adjust error thresholds a tad to account for platform-to-platform variation
# in some math precision.
hardfail = 0.032
failpercent = 0.002

command = testtex_command ("../common/textures/grid.tx",
                           extraargs = "-interpmode 0  -d uint8 -o out.tif")
outputs = [ "out.tif" ]
