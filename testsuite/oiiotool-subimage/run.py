#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Slightly bump allowable failures, for slightly shifting font rendering
# with changed freetype versions.
failpercent *= 2.0

redirect = " >> out.txt 2>&1 "
failureok = 1

# test subimages
command += oiiotool ("--pattern constant:color=0.5,0.0,0.0 64x64 3 " +
                     "--pattern constant:color=0.0,0.5,0.0 64x64 3 " +
                     "--siappend -d half -o subimages-2.exr")
command += oiiotool ("--pattern constant:color=0.5,0.0,0.0 64x64 3 --text A -attrib oiio:subimagename layerA " +
                     "--pattern constant:color=0.0,0.5,0.0 64x64 3 --text B -attrib oiio:subimagename layerB " +
                     "--pattern constant:color=0.0,0.0,0.5 64x64 3 --text C -attrib oiio:subimagename layerC " +
                     "--pattern constant:color=0.5,0.5,0.0 64x64 3 --text D -attrib oiio:subimagename layerD " +
                     "--siappendall -d half -o subimages-4.exr")
command += oiiotool ("subimages-4.exr --subimage 3 -o subimageD3.exr")
command += oiiotool ("subimages-4.exr --subimage layerB -o subimageB1.exr")
command += oiiotool ("subimages-2.exr --sisplit -o:all=1 subimage%d.exr")

# test that we can set attributes on individual subimages
command += oiiotool ("subimages-4.exr --attrib:subimages=0 Beatle John --attrib:subimages=1 Beatle Paul --attrib:subimages=2 Beatle George --attrib:subimages=3 Beatle Ringo -o gpgr.exr")
command += info_command ("-a -v gpgr.exr", safematch=1)

# Test extraction of MIP levels
command += oiiotool ("../common/textures/grid.tx --selectmip 4 -o mip4.tif")
command += info_command ("mip4.tif", safematch=True)
command += oiiotool ("../common/textures/grid.tx --unmip -o unmip.tif")
command += info_command ("../common/textures/grid.tx", verbose=False)
command += info_command ("unmip.tif", verbose=False)

# Error cases
command += oiiotool ("-echo \"Select nonexistent subimage\""
                     + " subimages-4.exr --subimage 13 -o subimage13.exr")
command += oiiotool ("-echo \"Select nonexistent MIP level\""
                     + " ../common/textures/grid.tx --selectmip 14 -o mip14.tif")

# Outputs to check against references
outputs = [ 
            "subimages-2.exr", "subimages-4.exr",
            "subimage1.exr", "subimage2.exr",
            "subimageD3.exr", "subimageB1.exr",
            "mip4.tif",
            "out.txt"
          ]

#print "Running this command:\n" + command + "\n"
