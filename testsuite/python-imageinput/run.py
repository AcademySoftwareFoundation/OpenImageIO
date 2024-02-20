#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


shutil.copyfile ("../common/textures/grid.tx", "grid.tx")

command += pythonbin + " src/test_imageinput.py > out.txt"

