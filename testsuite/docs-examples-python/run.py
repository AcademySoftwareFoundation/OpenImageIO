#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = " >> out.txt 2>&1 "

# To avoid duplicating example images between the C++ and Python tests,
# they all live with the C++ ones.
refdirlist += [ "../docs-examples-cpp/ref" ]

# Prep:
command += run_app("cmake -E copy " + test_source_dir + "/../common/grid-small.exr grid.exr")
command += run_app("cmake -E copy " + test_source_dir + "/../common/tahoe-small.tif tahoe.tif")
command += run_app("cmake -E copy " + test_source_dir + "/../common/grid-small.exr A.exr")
command += run_app("cmake -E copy " + test_source_dir + "/../common/grid-small.exr B.exr")
command += run_app("cmake -E copy " + test_source_dir + "/../common/with_nans.tif with_nans.tif")
command += run_app("cmake -E copy " + test_source_dir + "/../common/checker_with_alpha.exr checker_with_alpha.exr")
command += run_app("cmake -E copy " + test_source_dir + "/../common/unpremult.tif unpremult.tif")
command += run_app("cmake -E copy " + test_source_dir + "/../common/bayer.png bayer.png")

command += oiio_app("oiiotool") +  "--pattern fill:top=0:bottom=1 256x256 1 -o mono.exr > out.txt ;"

# Copy the grid to both a tiled and scanline version
command += oiio_app("iconvert") + "../common/grid.tif --scanline scanline.tif > out.txt ;" 
command += oiio_app("iconvert") + "../common/grid.tif --tile 64 64 tiled.tif > out.txt ;" 

# Run the examples for each chapter
for chapter in [ "imageioapi", "imageoutput", "imageinput", "writingplugins",
                 "imagecache", "texturesys", "imagebuf", "imagebufalgo" ] :
    command += pythonbin + " src/docs-examples-" + chapter + ".py " + redirect + " ;"

# hashes merely check that the images don't change, but saves us the space
# of checking in a full copy of the image if it's not needed. This is not
# suitable if the image may change slightly from platform to platform or
# with different versions of dependencies, for that we should use the
# full reference image comparison with appropriate thresholds.
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
    "channels-rgba.exr",
    "channels-rgb.exr",
    "channels-brga.exr",
    "channels-alpha.exr",
    "channel-append.exr",
    "copy.exr",
    "crop.exr",
    "cut.exr",
    "paste.exr",
    "rotate-90.exr",
    "rotate-180.exr",
    "rotate-270.exr",
    "flip.exr",
    "flop.exr",
    "rotate-45.tif",
    "resize.tif",
    "resample.exr",
    "fit.tif",
    "warp.exr",
    "transpose.exr",
    "reorient.exr",
    "cshift.exr",
    "texture.exr",
    "add.exr",
    "add_cspan.exr",
    "sub.exr",
    "absdiff.exr",
    "abs.exr",
    "mul.exr",
    "div.exr",
    "checker_with_alpha_filled.exr",
    "tahoe_median_filter.tif",
    "tahoe_unsharp_mask.tif"
]
for file in hashes :
    command += info_command(file, verbose=False)

# outputs should contain all the images that need to be checked directly
# and need the images checked into the ref directory.
outputs = [
    # Outputs from the ImageOutput chapter:
    "simple.tif", "scanlines.tif",
    # Outputs from the ImageInput chapter:

    # Outputs from the ImageBuf chapter:

    # Outputs from the ImageBufAlgo chapter:

    # ... etc ... other chapters ...

    # Last, we have the out.txt that captures console output of the test
    # programs.
    "out.txt"
    ]
