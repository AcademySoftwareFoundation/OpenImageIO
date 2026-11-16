#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Regression test: the OpenEXR readers' flat read_native_* calls must reject
# deep files instead of overrunning the caller's buffer, and the deep calls
# must reject flat files. Builds a small C++ helper and runs it on the dense
# deep fixtures shipped with openexr-bomb plus small generated flat files.

if platform.system() == 'Windows' :
    prefix = ".\\build\\Release\\"
else :
    prefix = "./build/"

fixtures = os.path.join(test_source_dir, "..", "openexr-bomb", "src")

command += oiiotool("--pattern constant:color=0.5,0.5,0.5 4x4 3 -o flat-scan.exr")
command += oiiotool("--pattern constant:color=0.5,0.5,0.5 4x4 3 --tile 2 2 -o flat-tiled.exr")
command += run_app("cmake -S " + test_source_dir + " -B build -DCMAKE_BUILD_TYPE=Release >> build.txt 2>&1", silent=True)
command += run_app("cmake --build build --config Release >> build.txt 2>&1", silent=True)
command += run_app(prefix + "read-deep-flat "
                   + os.path.join(fixtures, "deep-dense-scan.exr") + " "
                   + os.path.join(fixtures, "deep-dense-tiled.exr")
                   + " flat-scan.exr flat-tiled.exr")
