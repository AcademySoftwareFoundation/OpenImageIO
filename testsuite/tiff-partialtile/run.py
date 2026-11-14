#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Regression test for a heap out-of-bounds write when reading partial edge
# tiles of a tiled TIFF whose dimensions are not a multiple of the tile size.
# We build a small C++ program that calls the low-level read_native_tiles()
# API, and run it against a 127x129 TIFF with 64x64 tiles. Deflate with the
# horizontal predictor lets the reader use its parallel decompression path
# as well as the serial one.

if platform.system() == 'Windows' :
    prefix = ".\\build\\Release\\"
else :
    prefix = "./build/"

command += oiio_app("oiiotool") + \
    "--pattern noise:type=uniform:min=0:max=1 127x129 3 -d uint8 " + \
    "--tile 64 64 --compression zip -o partial.tif > out.txt ;"

# Build and run the C++ regression test.
command += run_app("cmake -S " + test_source_dir + " -B build -DCMAKE_BUILD_TYPE=Release >> build.txt 2>&1", silent=True)
command += run_app("cmake --build build --config Release >> build.txt 2>&1", silent=True)
command += run_app(prefix + "read-partial-tiles partial.tif")
