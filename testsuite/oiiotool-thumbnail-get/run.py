#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Capture error output
redirect = " >> out.txt 2>&1 "

psd = "src/with-thumbnail.psd"
no_thumb = "../common/tahoe-small.tif"

# Test extracting a present thumbnail.
command += oiiotool (psd + " --thumbnail-get -o thumb.tif")

# Test valid modifiers and stack integrity.
command += oiiotool (psd + " --thumbnail-get:index=0:fail=0"
                     + " --echo \"modifiers {TOP.width}x{TOP.height}\"")
command += oiiotool (psd + " --dup --thumbnail-get"
                     + " --echo \"thumbnail {TOP.width}x{TOP.height}\""
                     + " --pop --echo \"full {TOP.width}x{TOP.height}\"")

# Test missing-thumbnail behavior.
command += oiiotool (no_thumb + " --thumbnail-get", failureok=True)

# With fail=0, a missing thumbnail is generated from the image itself.
command += oiiotool (no_thumb + " --thumbnail-get:fail=0"
                     + " --echo \"generated {TOP.width}x{TOP.height}\""
                     + " -o generated.tif")

# The aspect ratio is preserved regardless of orientation.
command += oiiotool (no_thumb + " --transpose -o portrait.tif")
command += oiiotool ("portrait.tif --thumbnail-get:fail=0"
                     + " --echo \"portrait {TOP.width}x{TOP.height}\"")

# A source already smaller than the box is never enlarged.
command += oiiotool ("--pattern constant:color=0.25,0.5,0.75 40x30 3 -o small.tif")
command += oiiotool ("small.tif --thumbnail-get:fail=0"
                     + " --echo \"small source {TOP.width}x{TOP.height}\"")

# Sizing follows the display window, not the data window.
command += oiiotool (no_thumb + " --fullsize 800x800 -o bigwindow.exr")
command += oiiotool ("bigwindow.exr --thumbnail-get:fail=0"
                     + " --echo \"display window {TOP.width}x{TOP.height}\"")

# Test that fail=0 does not suppress an invalid index.
command += oiiotool (psd + " --thumbnail-get:index=1:fail=0", failureok=True)

# Test that the -i:get_thumbnail=1 read modifier produces the same thumbnail
# as the equivalent --thumbnail-get command above.
command += oiiotool ("-i:get_thumbnail=1 " + psd + " -o thumb_i.tif")
command += oiiotool ("--diff thumb_i.tif thumb.tif")

# Test that autoorient/autocc act on the thumbnail, not the main image.
command += oiiotool ("--autoorient --autocc -i:get_thumbnail=1 " + psd
                     + " --echo \"autoorient/autocc {TOP.width}x{TOP.height}\"")

# Test that -i:get_thumbnail forwards modifiers.
command += oiiotool ("-i:get_thumbnail=1:fail=0 " + no_thumb
                     + " --echo \"input fail=0 generated {TOP.width}x{TOP.height}\"")
command += oiiotool ("-i:get_thumbnail=1:index=1 " + psd, failureok=True)

# Test that -i:get_thumbnail=0 returns the main image.
command += oiiotool ("-i:get_thumbnail=0 " + psd + " --echo \"get_thumbnail=0 {TOP.width}x{TOP.height}\"")

outputs = [ "thumb.tif", "generated.tif", "out.txt" ]
