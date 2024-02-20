#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Test that appending channels fixes duplicate channel names


# Test 1: Make two images, both with R,G,B channel names, so duplicated.
# When mashing them together, they should end up with channels
# R, G, B, channel3, channel4, channel5.
command += oiiotool ("--create 64x64 3 -d uint8 -o a.exr")
command += oiiotool ("--create 64x64 3 -d uint8 -o b.exr")
command += oiiotool ("a.exr b.exr -chappend -o out.exr")
command += info_command ("out.exr", safematch=True)


# Test 2: Start with a multi-image file with different subimage names,
# then split subimages and append channels (convert multi-image file to
# single image / many-channel). The new channel names should be
# de-duplicated by using the subimage names. The resulting image should
# have channels R, G, B, Bimg.R, Bimg.G, Bimg.B.
command += oiiotool ("a.exr --attrib oiio:subimagename Aimg " +
                     "b.exr --attrib oiio:subimagename Bimg " +
                     "--siappend -o multi.exr")
command += oiiotool ("multi.exr -sisplit --chappend -o out2.exr")
command += info_command ("out2.exr", safematch=True)
