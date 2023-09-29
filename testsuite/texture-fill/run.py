#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


command = testtex_command ("src/gray.png", "-fill 0.05 --res 128 128 --nowarp")
outputs = [ "out.exr" ]
