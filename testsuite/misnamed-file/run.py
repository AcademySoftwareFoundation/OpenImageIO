#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


from __future__ import absolute_import
import shutil

# Make a copy called "misnamed.exr" that's actually a TIFF file
shutil.copyfile ("../common/grid.tif", "misnamed.exr")

# Now see if it is read correctly
command = info_command ("misnamed.exr")
