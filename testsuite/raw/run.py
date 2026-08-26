#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import os

files = [ "RAW_CANON_EOS_7D.CR2",
          "RAW_NIKON_D3X.NEF",
          "RAW_FUJI_F700.RAF",
          "RAW_NIKON_D3X.NEF",
          "RAW_OLYMPUS_E3.ORF",
          "RAW_PANASONIC_G1.RW2",
          "RAW_PENTAX_K200D.PEF",
          "RAW_SONY_A300.ARW" ]
outputs = []

# Things vary a lot with libraw versions.
# FIXME -- return to this later
if (os.getenv('GITHUB_ACTIONS') == 'true'):
    failthresh = 0.024
    files.remove ("RAW_PANASONIC_G1.RW2")

# Fairly high hard fail, since libraw seems to diddle with its debayering
# from version to version, it's hard to make a single reference image.
hardfail = 0.017

# For each test image, read it and print all metadata, resize it (to make
# the ref images small) and compared to the reference.
for f in files:
    outputname = f+".tif"
    command += oiiotool ("-iconfig raw:ColorSpace linear "
                         + "-i:info=2 " + OIIO_TESTSUITE_IMAGEDIR + "/" + f
                         + " -resample '5%' -d uint8 "
                         + "-o " + outputname)
    outputs += [ outputname ]

# Undebayered reads must present the sensor data in the orientation the
# spec advertises, for every one of LibRaw's 8 flip codes -- four of them
# used to leave the caller's buffer untouched, and a fifth was mirrored.
# Compare against the unflipped read put through the equivalent oiiotool
# transform, so the check doesn't depend on the LibRaw version.
flipsrc = OIIO_TESTSUITE_IMAGEDIR + "/RAW_SONY_A300.ARW"
fliptransform = [ (1, "--flop"), (2, "--flip"), (3, "--flip --flop"),
                  (4, "--transpose"), (5, "--transpose --flip"),
                  (6, "--transpose --flop"), (7, "--transpose --flip --flop") ]
undebayer = "-iconfig raw:Demosaic none -iconfig raw:user_flip "
corner = " --cut 64x64+0+0 -d uint16 -o "
cmd = undebayer + "0 -i " + flipsrc
for flip, transform in fliptransform :
    cmd += " --dup " + transform + corner + "flipref%d.tif --pop" % flip
command += oiiotool (cmd)
for flip, transform in fliptransform :
    command += oiiotool (undebayer + "%d -i %s%sflip%d.tif"
                         % (flip, flipsrc, corner, flip))
    command += oiiotool ("--diff flipref%d.tif flip%d.tif" % (flip, flip))

# Malformed and hostile headers must be rejected cleanly, and the valid
# control must still read. Read the pixels undebayered so the expected
# output doesn't move with LibRaw's demosaicing.
redirect = " >> out.txt 2>&1"
for f in [ "valid-32x32.dng", "bad-exif-type.dng", "truncated.dng",
           "bomb-32000x32000.dng" ] :
    command += oiiotool ("--info src/" + f, failureok = True)
    command += oiiotool ("-iconfig raw:Demosaic none --stats src/" + f,
                         failureok = True)

# Check the crop size and position in all 4 orientations.
for rotation in [0, 3, 5, 6]:
    command += oiiotool ("-echo \"rotation " + str(rotation) + "\""
        " -iconfig:type=int raw:user_flip " + str(rotation) +
        " -i src/" + "crop-36x32.dng   --eraseattrib \".*\" --printinfo")

outputs += [ "out.txt" ]
