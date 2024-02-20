#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Run the script 
command += pythonbin + " src/test_imagebuf.py > out.txt ;"

# compare the outputs
outputs = [ "out.tif", "outtuple.tif",
            "outarray.tif", "outarrayB.tif", "outarrayH.tif",
            "perchan.exr", "multipart.exr",
            "out.txt" ]

