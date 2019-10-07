#!/usr/bin/env python

from __future__ import absolute_import
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
# Also has problems on Travis. I think there is something weird about
# this file, there seems to be unicode in the Software metadata, which
# sure doesn't help.
# FIXME -- return to this later
if (os.getenv('CIRCLECI') == 'true' or os.getenv('GITHUB_ACTIONS') == 'true' or
    (os.getenv('TRAVIS') == 'true' and os.getenv('TRAVIS_OS_NAME') == 'linux')):
    failthresh = 0.024
    files.remove ("RAW_PANASONIC_G1.RW2")


# For each test image, read it and print all metadata, resize it (to make
# the ref images small) and compared to the reference.
for f in files:
    outputname = f+".tif"
    command += oiiotool ("-iconfig raw:ColorSpace linear "
                         + "-i:info=2 " + OIIO_TESTSUITE_IMAGEDIR + "/" + f
                         + " -resample '5%' -d uint8 "
                         + "-o " + outputname)
    outputs += [ outputname ]

outputs += [ "out.txt" ]
