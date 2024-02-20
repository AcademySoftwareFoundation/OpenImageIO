#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


command = testtex_command ("--gettexels -res 128 128 -d uint8 ../common/textures/grid.tx -o postage.tif")
outputs = [ "postage.tif" ]
