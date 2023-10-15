#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


imagedir = "ref/"
files = [ "norle-8.sgi", "rle-8.sgi", "norle-16.sgi", "rle-16.sgi" ]
for f in files:
    command = command + rw_command (imagedir, f)
