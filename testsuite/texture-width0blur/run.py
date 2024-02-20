#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


command = testtex_command ("../common/textures/grid.tx",
                           "-res 256 256 -nowarp --width 0 --blur 0.1 --wrap clamp -d uint8 -o out.tif")
outputs = [ "out.tif" ]
