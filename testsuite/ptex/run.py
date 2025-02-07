#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


imagedir = "src"
files = [ "triangle.ptx" ]
for f in files:
    command += info_command (imagedir + "/" + f, extraargs="--stats")
