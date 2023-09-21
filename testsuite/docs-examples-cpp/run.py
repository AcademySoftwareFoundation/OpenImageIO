#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/OpenImageIO/oiio


if platform.system() == 'Windows' :
    prefix = "Release\\"
else :
    prefix = "./"

# command += "echo test_source_dir=" + test_source_dir + " >> build.txt ;"
command += run_app("cmake " + test_source_dir + " -DCMAKE_BUILD_TYPE=Release >> build.txt 2>&1", silent=True)
command += run_app("cmake --build . --config Release >> build.txt 2>&1", silent=True)

# Run the examples for each chapter
for chapter in [ "imageioapi", "imageoutput", "imageinput", "writingplugins",
                 "imagecache", "texturesys", "imagebuf", "imagebufalgo" ] :
    command += run_app(prefix + "docs-examples-" + chapter)

outputs = [
    # Outputs from the ImageOutput chapter:
    "simple.tif", "scanlines.tif",
    # Outputs from the ImageInput chapter:

    # ... etc ... other chapters ...

    # Last, we have the out.txt that captures console output of the test
    # programs.
    "out.txt"
    ]
