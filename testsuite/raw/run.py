#!/usr/bin/env python

files = [ "RAW_CANON_EOS_7D.CR2",
          "RAW_NIKON_D3X.NEF",
          "RAW_FUJI_F700.RAF",
          "RAW_NIKON_D3X.NEF",
          "RAW_OLYMPUS_E3.ORF",
          "RAW_PANASONIC_G1.RW2",
          "RAW_PENTAX_K200D.PEF",
          "RAW_SONY_A300.ARW" ]
outputs = []

# For each test image, read it and print all metadata, resize it (to make
# the ref images small) and compared to the reference.
for f in files:
    outputname = f+".tif"
    command += oiiotool ("-i:info=2 " + OIIO_TESTSUITE_IMAGEDIR + "/" + f
                         + " -resample '5%' -d uint8 "
                         + "-o " + outputname)
    outputs += [ outputname ]

outputs += [ "out.txt" ]
