#!/usr/bin/env python

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

# The version of libraw installed on CircleCI is slightly different, so
# accept just a bit more pixel difference, and eliminate the Panasonic
# one, which is nothing but trouble on Circle.
# FIXME -- return to this later
if os.environ.get('CIRCLECI') == 'true' :
    failthresh = 0.024
    files.remove ("RAW_PANASONIC_G1.RW2")


# For each test image, read it and print all metadata, resize it (to make
# the ref images small) and compared to the reference.
for f in files:
    outputname = f+".tif"
    command += oiiotool ("-i:info=2 " + OIIO_TESTSUITE_IMAGEDIR + "/" + f
                         + " -resample '5%' -d uint8 "
                         + "-o " + outputname)
    outputs += [ outputname ]

outputs += [ "out.txt" ]
