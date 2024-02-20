#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


command += maketx_command  ("../common/textures/grid.tx",
                            "grid-uint16.tx",
                            "-d uint16", showinfo=True)
command += testtex_command ("grid-uint16.tx")
outputs = [ "out.exr" ]
