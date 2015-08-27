#!/usr/bin/env python

# FIXME: Overscan textures don't work right. It sounds fine conceptually
# (just let texture extend past the 0-1 range), but once you start thinking
# about how the MIP-map levels work, it gets really ugly. Because display
# and pixel data windows (that is, image origins and resolutions) are
# integers, once you start downsizing to generate MIP levels, you quickly
# get into situations where whole numbers can't accurately represent the
# sizes of the display and data windows (certainly not both at once). So
# punt for now, skip this test, we don't expect it to work properly.
exit ()


command = (oiio_app("maketx") + " --filter lanczos3 "
           + parent + "/oiio-images/grid-overscan.exr"
           + " -o grid-overscan.exr ;\n")
command = command + testtex_command ("grid-overscan.exr", "--wrap black")

outputs = [ "out.exr" ]


# FIXME: some day, remove grid-overscan.exr from oiio-images, and just
# generate an overscan image on the fly, like shown below. But don't bother
# for the moment, because overscan textures don't work right.
#
# command += (oiio_app("oiiotool") + parent+"/oiio-images/grid.tif"
#             + " -resize 512x512 "
#             + " -pattern checker:color1=1,0,0:color2=.25,0,0 640x640 3 "
#             + "-origin -64-64 -paste +0+0 -o overscan-src.exr ;")
# command += (oiio_app("maketx") + " --filter lanczos3 overscan-src.exr "
#            + " -o grid-overscan.exr ;\n")
