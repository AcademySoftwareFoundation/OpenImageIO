#!/usr/bin/env python 

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = " >> out.txt 2>&1 "

# Prep:
command += run_app("cmake -E copy " + test_source_dir + "/../common/grid-small.exr grid.exr")
command += run_app("cmake -E copy " + test_source_dir + "/../common/tahoe-small.tif tahoe.tif")

# Run the examples for each chapter
for chapter in [ "imageioapi", "imageoutput", "imageinput", "writingplugins",
                 "imagecache", "texturesys", "imagebuf", "imagebufalgo" ] :
    command += pythonbin + " src/docs-examples-" + chapter + ".py " + redirect + " ;"

# hashes merely check that the images don't change, but saves us the space
# of checking in a full copy of the image if it's not needed.
hashes = [
    # Outputs from the ImageBufAlgo chapter:
    "zero1.exr",
    "zero2.exr",
    "zero3.exr",
    "zero4.exr",
    "fill.exr",
    "checker.exr",
    "noise1.exr",
    "noise2.exr",
    "noise3.exr",
    "noise4.exr",
    "blue-noise.exr",
    "point.exr",
    "lines.exr",
    "box.exr",
    "text1.exr",
    "text2.exr",
    "cshift.exr",
    "texture.exr"
]
for file in hashes :
    command += info_command(file, verbose=False)

# outputs should contain all the images that need to be checked directly
# and need the images checked into the ref directory.
outputs = [
    # Outputs from the ImageOutput chapter:
    "simple.tif", "scanlines.tif",
    # Outputs from the ImageInput chapter:

    # ... etc ... other chapters ...

    # Last, we have the out.txt that captures console output of the test
    # programs.
    "out.txt"
    ]

