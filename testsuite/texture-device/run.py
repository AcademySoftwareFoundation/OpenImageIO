#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# texture-device is built by top-level CMake via add_subdirectory, so
# tests only need to run the already-built executable.
command += run_app(oiio_app("texture-device").strip())

outputs = [ "out.txt", "out.exr" ]
