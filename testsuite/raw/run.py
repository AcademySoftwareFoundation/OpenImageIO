#!/usr/bin/env python

imagedir = parent + "oiio-images/raw"
files = [ "RAW_CANON_EOS_7D.CR2", "RAW_NIKON_D3X.NEF" ]
outputs = []

# For each test image, read it and print all metadata, resize it (to make
# the ref images small) and compared to the reference.
for f in files:
    outputname = f+".tif"
    command += oiiotool ("-i:info=2 " + imagedir+"/"+f
                         + " -resample '5%' -d uint8 "
                         + "-o " + outputname)
    outputs += [ outputname ]

outputs += [ "out.txt" ]
