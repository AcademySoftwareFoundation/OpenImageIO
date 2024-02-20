#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


command = oiiotool ("-pattern fill:topleft=0,0,0:topright=1,0,0:bottomleft=0,1,0:bottomright=1,1,1 "
                    + "64x64 3 -otex gradient.tx")
command += testtex_command ("gradient.tx",
                           "-flipt -nowarp -derivs -res 64 64 -d half -o out.exr")
outputs = [ "out.exr" ]
