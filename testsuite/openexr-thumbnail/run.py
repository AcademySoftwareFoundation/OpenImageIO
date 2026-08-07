#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Capture error output
redirect = " >> out.txt 2>&1 "

imagedir = OIIO_TESTSUITE_IMAGEDIR

# An EXR preview lives in the header, so verify that reading it does not 
# depend on how the pixels themselves are stored.
files = [ "ScanLines/Tree.exr",
          "Tiles/Spirals.exr",
          "MultiResolution/StageEnvCube.exr",
          "LuminanceChroma/Garden.exr" ]
for f in files:
    command += oiiotool ("--echo \"" + f + "\" " + imagedir + "/" + f
                         + " --thumbnail-get --eraseattrib \".*\" --printinfo")

command += oiiotool (imagedir + "/MultiResolution/StageEnvCube.exr"
                     + " --thumbnail-get -o thumb.tif")

# Test that an EXR with no preview reports no thumbnail.
command += oiiotool (imagedir + "/ScanLines/Desk.exr --thumbnail-get",
                     failureok=True)

# Test that a preview stays on the part it was stored on.
multi = "src/multipart-preview.exr"
command += oiiotool (multi + " --subimage 0 --thumbnail-get"
                     + " --echo \"subimage 0 thumbnail {TOP.width}x{TOP.height}\"")
command += oiiotool (multi + " --subimage 1 --thumbnail-get", failureok=True)

outputs = [ "thumb.tif", "out.txt" ]
