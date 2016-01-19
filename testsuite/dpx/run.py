#!/usr/bin/env python

imagedir = parent + "/oiio-images"
files = [ "dpx_nuke_10bits_rgb.dpx", "dpx_nuke_16bits_rgba.dpx" ]
for f in files:
    command += rw_command (imagedir, f)


# Additionally, test for regressions for endian issues with 16 bit DPX output
# (related to issue #354)
command += oiio_app("oiiotool") + " src/input_rgb_mattes.tif -o output_rgb_mattes.dpx >> out.txt;"
command += oiio_app("idiff") + " src/input_rgb_mattes.tif output_rgb_mattes.dpx >> out.txt;"

# Test reading and writing of stereo DPX (multi-image)
#command += (oiio_app("oiiotool") + "--create 80x60 3 --text:x=10 Left "
#            + "--caption \"view angle: left\" -d uint10 -o L.dpx >> out.txt;")
#command += (oiio_app("oiiotool") + "--create 80x60 3 --text:x=10 Right "
#            + "--caption \"view angle: right\" -d uint10 -o R.dpx >> out.txt;")
command += (oiio_app("oiiotool") + "ref/L.dpx ref/R.dpx --siappend -o stereo.dpx >> out.txt;")
command += info_command("stereo.dpx", safematch=True, hash=False, extraargs="--stats")
command += oiio_app("idiff") + "-a stereo.dpx ref/stereo.dpx >> out.txt;"

