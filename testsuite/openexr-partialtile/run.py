#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Regression test for a heap out-of-bounds write when reading a partial edge
# tile of a tiled OpenEXR whose dimensions are not a multiple of the tile size.
# We build a small C++ program (which uses the low-level read_native_tiles()
# API for both the classic and OpenEXR-Core readers) and run it against a
# tiled EXR whose 127x129 size is deliberately not a multiple of the 64x64
# tile size.

if platform.system() == 'Windows' :
    prefix = ".\\build\\Release\\"
else :
    prefix = "./build/"

# Make a tiled EXR whose dimensions are not a multiple of the tile size.
command += oiio_app("oiiotool") + \
    "--pattern noise:type=uniform:min=0:max=1 127x129 3 -d half " + \
    "--tile 64 64 -o partial.exr > out.txt ;"

# Build and run the C++ regression test.
command += run_app("cmake -S " + test_source_dir + " -B build -DCMAKE_BUILD_TYPE=Release >> build.txt 2>&1", silent=True)
command += run_app("cmake --build build --config Release >> build.txt 2>&1", silent=True)
command += run_app(prefix + "read-partial-tiles partial.exr")
