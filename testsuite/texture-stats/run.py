#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


redirect = " >> stats.txt"
command = testtex_command ("../common/textures/grid.tx -res 64 48 -v --teststatquery --runstats")
outputs = [ "out.exr" ]
