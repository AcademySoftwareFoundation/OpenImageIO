#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# texture-device is built by top-level CMake via add_subdirectory, so
# tests only need to run the already-built executable.
command += run_app(oiio_app("texture-device").strip()
				   + " --output out-non-unified.exr")
command += run_app(oiio_app("texture-device").strip()
				   + " --unified --output out-unified.exr")

command += diff_command("out-non-unified.exr", "ref/out.exr")
command += diff_command("out-unified.exr", "ref/out.exr")

outputs = [ "out.txt" ]
