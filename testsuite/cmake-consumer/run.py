#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio


command += run_app("cmake " + test_source_dir + " -DCMAKE_BUILD_TYPE=Release >> build.txt 2>&1", silent=True)
command += run_app("cmake --build . --config Release >> build.txt 2>&1", silent=True)
if platform.system() == 'Windows' :
    command += run_app("Release\\consumer")
else :
    command += run_app("./consumer")
