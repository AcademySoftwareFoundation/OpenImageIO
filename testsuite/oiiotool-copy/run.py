#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


####################################################################
# This test exercises oiiotool functionality that is mostly about
# copying pixels from one image to another.
####################################################################

# Capture error output
redirect = " >> out.txt 2>&1 "

# Create some test images we need
command += oiiotool("-pattern constant:color=.25,.5,.75 64x64 3 -d half -o rgb64.exr")
command += oiiotool("-pattern constant:color=.25,.5,.75 64x64 3 -pattern constant:color=42 64x64 1 --chnames Z --siappend -d half -o rgb-z-parts64.exr")


# Test -i to read specific channels
# Note: this used to crash until PR #3513
command += oiiotool("-i:ch=R,G rgb64.exr -o rgonly.exr")
# Test -i to read nonexistent channels
command += oiiotool("-i:ch=Z rgb64.exr -d half -o ch-err.exr")
# Test -i to read nonexistent channels in one subimage of a multi-subimage file
command += oiiotool("-i:ch=R,G,B rgb-z-parts64.exr -d half -o ch-err2.exr")


# test -d to change data formats
command += oiiotool ("src/rgbaz.exr -d half -o allhalf.exr")
command += info_command ("allhalf.exr", safematch=1)

# test -d NAME=fmt to change data format of one channel, and to make
# sure oiiotool will output per-channel formats.
command += oiiotool ("src/rgbaz.exr -d half -d Z=float -o rgbahalf-zfloat.exr")
command += info_command ("rgbahalf-zfloat.exr", safematch=1)


# Some tests to verify that we are transferring data formats properly.
#
command += oiiotool ("-pattern checker 128x128 3 -d uint8 -tile 16 16 -o uint8.tif " +
                     "-echo \"explicit -d uint save result: \" -metamatch \"width|tile\" -i:info=2 uint8.tif -echo \"\"")
# Un-modified copy should preserve data type and tiling
command += oiiotool ("uint8.tif -o tmp.tif " +
                     "-echo \"unmodified copy result: \" -metamatch \"width|tile\" -i:info=2 tmp.tif -echo \"\"")
# Copy with explicit data request should change data type
command += oiiotool ("uint8.tif -d uint16 -o copy_uint16.tif " +
                     "-echo \"copy with explicit -d uint16 result: \" -metamatch \"width|tile\" -i:info=2 copy_uint16.tif -echo \"\"")
# Copy with data request in the -o
command += oiiotool ("uint8.tif -o:type=uint16 copy_uint16-o.tif " +
                     "-i copy_uint16-o.tif " +
                     "-echo \"copy with -o:type=uint16 result: \" -printinfo:native=1:verbose=0")
command += oiiotool ("uint8.tif -o:datatype=uint16 copy_uint16-o.tif " +
                     "-i copy_uint16-o.tif " +
                     "-echo \"copy with -o:datatype=uint16 result: \" -printinfo:native=1:verbose=0")
# Subimage concatenation should preserve data type
command += oiiotool ("uint8.tif copy_uint16.tif -siappend -o tmp.tif " +
                     "-echo \"siappend result: \" -metamatch \"width|tile\" -i:info=2 tmp.tif -echo \"\"")
# Combining two images preserves the format of the first read input, if
# there are not any other hints:
command += oiiotool ("-pattern checker 128x128 3 uint8.tif -add -o tmp.tif " +
                     "-echo \"combining images result: \" -metamatch \"width|tile\" -i:info=2 tmp.tif -echo \"\"")

# Try to copy extra channels to a file that doesn't support it -- we should get
# a warning message about only saving the first 3 channels.
command += oiiotool ("--pattern constant:color=0.1,0.2,0.3,0.4 64x64 4 --chnames R,G,B,X -d uint8 -o rgbx.png")


# test channel shuffling
command += oiiotool ("../common/grid.tif"
            + " --ch =0.25,B,G -o chanshuffle.tif")

# test --ch to separate RGBA from an RGBAZ file
command += oiiotool ("src/rgbaz.exr --ch R,G,B,A -o ch-rgba.exr")
command += oiiotool ("src/rgbaz.exr --ch Z -o ch-z.exr")

# test --chappend to merge RGBA and Z
command += oiiotool ("ch-rgba.exr ch-z.exr --chappend -o chappend-rgbaz.exr")

# test --chnames to rename channels
command += oiiotool ("src/rgbaz.exr --chnames Red,,,,Depth -o chname.exr")
command += info_command ("chname.exr", safematch=1)

# test --crop
command += oiiotool ("../common/grid.tif --crop 100x400+50+200 -o crop.tif")

# test --cut
command += oiiotool ("../common/grid.tif --cut 100x400+50+200 -o cut.tif")

# test --paste
command += oiiotool ("../common/grid.tif "
            + "--pattern checker 256x256 3 --paste +150+75 -o pasted.tif")

# test --pastemeta
command += oiiotool ("--pattern:type=half constant:color=0,1,0 64x64 3 -o green.exr")
command += oiiotool ("--pattern:type=half constant:color=1,0,0 64x64 3 -attrib hair brown -attrib eyes 2 -attrib weight 20.5 -o redmeta.exr")
command += oiiotool ("redmeta.exr green.exr --pastemeta -o greenmeta.exr")
command += info_command ("green.exr", safematch=True)
command += info_command ("greenmeta.exr", safematch=True)

# test mosaic
# Purposely test with fewer images than the mosaic array size
command += oiiotool ("--pattern constant:color=1,0,0 50x50 3 "
            + "--pattern constant:color=0,1,0 50x50 3 "
            + "--pattern constant:color=0,0,1 50x50 3 "
            + "--mosaic:pad=10 2x2 -d uint8 -o mosaic.tif")

# Test --mosaic with --fit
command += oiiotool ("--pattern constant:color=1,.1,.1 50x50 3 "
            + "--pattern constant:color=0.1,1,0.1 500x300 3 "
            + "--pattern constant:color=0.1,0.1,1 10x16 3 "
            + "--mosaic:pad=10:fit=64x64 2x2 -d uint8 -o mosaicfit.tif")


# test --metamerge, using chappend as an example
command += oiiotool ("--create 64x64 3 -chnames R,G,B -attrib a 3.0 -o aimg.exr")
command += oiiotool ("--create 64x64 3 -chnames A,Z -attrib b 1.0 -o bimg.exr")
command += oiiotool ("aimg.exr bimg.exr --chappend -o nometamerge.exr")
command += oiiotool ("--metamerge aimg.exr bimg.exr --chappend -o metamerge.exr")
command += info_command ("nometamerge.exr", safematch=True)
command += info_command ("metamerge.exr", safematch=True)

# test --chappend of multiple images
command += oiiotool ("--pattern constant:color=0.5 64x64 1 --text R --chnames R " +
                     "--pattern constant:color=0.25,0.75 64x64 2 --text G,B --chnames G,B " +
                     "--pattern constant:color=1.0 64x64 1 --text A --chnames A " +
                     "--chappend:n=3 -d half -o chappend-3images.exr")


# Interesting error cases
command += oiiotool ("-echo \"Testing -o with no image\" -o out.tif")


# Outputs to check against references
outputs = [
            "rgonly.exr", "ch-err.exr", "ch-err2.exr",
            "allhalf.exr", "rgbahalf-zfloat.exr",
            "chanshuffle.tif", "ch-rgba.exr", "ch-z.exr",
            "chappend-rgbaz.exr", "chname.exr",
            "crop.tif", "cut.tif", "pasted.tif",
            "mosaic.tif", "mosaicfit.tif",
            "greenmeta.exr",
            "chappend-3images.exr",
            "out.txt"
          ]

#print "Running this command:\n" + command + "\n"
