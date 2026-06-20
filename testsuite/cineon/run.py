#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# save the error output
redirect = " >> out.txt 2>&1 "

files = [ "checker.cin" ]
for f in files:
    command += info_command ("../oiio-images/cineon/" + f)


# Regression tests for broken files
command += info_command ("src/broken_bitdepth.cin", verbose=False, failureok=True)

outputs = [ "out.txt" ]
