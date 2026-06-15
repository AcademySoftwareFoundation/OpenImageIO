#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Capture error output
redirect = " >> out.txt 2>&1 "

src = "../common/tahoe-small.tif"

# Test that a thumbnail embeds into TGA (currently the only format that can),
# is reported in the file's metadata, and leaves the main image on the stack.
command += oiiotool (src + " --dup --resize:filter=box 50x38 --set-thumbnail"
                     + " --echo \"after set-thumbnail: {TOP.width}x{TOP.height}\""
                     + " -o out.tga")
command += info_command ("out.tga", safematch=True)

# Test that an embed/extract round trip matches a directly resized image.
command += oiiotool ("out.tga --get-thumbnail -o thumb.tif")
command += oiiotool ("--warn 0.005 --fail 0.005 thumb.tif " + src
                     + " --resize:filter=box 50x38 --diff")

# Test that thumbnail metadata is omitted from formats without thumbnail
# support.
command += oiiotool (src + " --dup --resize:filter=box 50x38 --set-thumbnail -o no_thumb.tif")
command += info_command ("no_thumb.tif", safematch=True, hash=False)

# Test error cases: an empty thumbnail image, and too few images on the stack.
command += oiiotool (src + " --dup --get-thumbnail:fail=0 --set-thumbnail",
                     failureok=True)
command += oiiotool (src + " --set-thumbnail", failureok=True)

outputs = [ "out.txt" ]
