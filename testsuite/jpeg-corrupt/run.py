#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


failureok = 1
redirect = ' >> out.txt 2>&1 '

# This file has a corrupted Exif block in the metadata. It used to
# crash on some platforms, on others would be caught by address sanitizer.
# Fixed by #1635. This test serves to guard against regressions.
command += info_command ("src/corrupt-exif.jpg", safematch=True)

# This file has a corrupted Exif block that makes it look like one item has a
# nonsensical length, that before being fixed, caused a buffer overrun.
command += info_command ("src/corrupt-exif-1626.jpg", safematch=True)

