#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

hardfail = 0.013

command = testtex_command ("src/vertgrid.tx", " --scalest 4 1 ")
outputs = [ "out.exr" ]
