#!/usr/bin/env python

import OpenImageIO as oiio
import os

redirect = " >> out.txt 2>> out.err.txt"

imagedir = OIIO_TESTSUITE_IMAGEDIR + "/targa"

files = [ "CBW8.TGA", "CCM8.TGA", "CTC16.TGA", "CTC24.TGA", "CTC32.TGA",
          "UBW8.TGA", "UCM8.TGA", "UTC16.TGA", "UTC24.TGA", "UTC32.TGA",
          "round_grill.tga" ]
for f in files:
    command += rw_command (imagedir, f)


# Test ability to extract thumbnails
outputs = [ ]
for f in files:
    fin = imagedir + "/" + f
    buf = oiio.ImageBuf(fin)
    if buf.has_thumbnail :
        thumbname = f + ".tif"
        command += run_app(pythonbin + " src/extractthumb.py " + fin + " " + thumbname)
        outputs += [ thumbname ]

# Test corrupted files
command += iconvert("-v src/crash1.tga crash1.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash2.tga -o crash2.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash3.tga -o crash3.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash4.tga -o crash4.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash5.tga -o crash5.exr", failureok = True)

outputs += [ 'out.txt', 'out.err.txt' ]
