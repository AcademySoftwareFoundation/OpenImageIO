#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/OpenImageIO/oiio


# command += "echo test_source_dir=" + test_source_dir + " >> build.txt ;"
command += run_app("cmake -S data -B build -DCMAKE_BUILD_TYPE=Release >> build.txt 2>&1", silent=True)
command += run_app("cmake --build build --config Release >> build.txt 2>&1", silent=True)
if platform.system() == 'Windows' :
    command += run_app("build\\Release\\docs-examples-imageoutput")
else :
    command += run_app("./build/docs-examples-imageoutput")

outputs = [ "simple.tif", "scanlines.tif",
            "out.txt" ]
