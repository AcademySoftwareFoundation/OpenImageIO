#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


command = testtex_command ("missing.tx", "--missing 1 0 0 --res 8 8 missing.tx")
outputs = [ "out.exr" ]
