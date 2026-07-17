#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Minimal smoke test for imagebufalgo_opencv.h: build a tiny standalone
# program (linked against the already-installed OpenImageIO and OpenCV) that
# round-trips an ImageBuf through cv::Mat, and check that it compiles, links,
# and runs cleanly. Only enabled if OpenCV was found (see FOUNDVAR OpenCV_FOUND
# in src/cmake/testing.cmake).

redirect = " >> out.txt 2>&1 "

if platform.system() == 'Windows' :
    prefix = ".\\build\\Release\\"
else :
    prefix = "./build/"

# Build and run the standalone OpenCV round-trip test. The cmake
# configure/build steps are redirected to build.txt instead of out.txt since
# their output (paths, compiler banners, timings) isn't reproducible across
# machines and would break the ref comparison.
command += run_app("cmake -S " + test_source_dir + " -B build -DCMAKE_BUILD_TYPE=Release >> build.txt 2>&1", silent=True)
command += run_app("cmake --build build --config Release >> build.txt 2>&1", silent=True)
command += run_app(prefix + "opencv-roundtrip")
