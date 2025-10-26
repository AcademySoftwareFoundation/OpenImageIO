#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

imagedir = "ref/"
files = [ "vp9_display_p3.mkv", "vp9_rec2100_pq.mkv" ]
for f in files:
    command = command + info_command (os.path.join(imagedir, f))
