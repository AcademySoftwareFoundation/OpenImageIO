#!/usr/bin/python 

imagedir = parent + "/oiio-images"
files = [ "dpx_nuke_10bits_rgb.dpx", "dpx_nuke_16bits_rgba.dpx" ]
for f in files:
    command += rw_command (imagedir, f)


# Additionally, test for regressions for endian issues with 16 bit DPX output
# (related to issue #354)
command += oiio_app("oiiotool") + " input_rgb_mattes.tif -o output_rgb_mattes.dpx >> out.txt;"
command += oiio_app("idiff") + " input_rgb_mattes.tif output_rgb_mattes.dpx >> out.txt;"

