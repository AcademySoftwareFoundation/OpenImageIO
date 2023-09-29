#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


files = [ "checker.cin" ]
for f in files:
    command += info_command ("../oiio-images/cineon/" + f)
