#!/usr/bin/python 

imagedir = parent + "/libtiffpic"

# caspian.tif	279x220 64-bit floating point (deflate) Caspian Sea from space
#   I can't get this to work with OIIO, but I can't get it to read with 
#     ImageMagick or OSX preview, either.
#   FIXME?

# cramps.tif	800x607 8-bit b&w (packbits) "cramps poster"
#    This tests 1-bit images, and packbits compression
# cramps-tile.tif	256x256 tiled version of cramps.tif (no compression)
#    Tests tiled images (especially tiled 1-bit) -- compare it to cramps
command += rw_command (imagedir, "cramps.tif")
command += rw_command (imagedir, "cramps-tile.tif")
command += diff_command (imagedir+"/cramps-tile.tif",
                                          imagedir+"/cramps.tif")

# dscf0013.tif	640x480 YCbCr digital camera image which lacks Reference
# 		Black/White values. Contains EXIF SubIFD. No compression.
# FIXME - we don't support YCbCr yet.  

# fax2d.tif	1728x1082 1-bit b&w (G3/2D) facsimile
# FIXME - we read the pixel data fine, but we fail to recognize that
#   differing XResolution and YResolution imply a non-square pixel
#   aspect ratio, and iv fails to display it well for this reason.
command += rw_command (imagedir, "fax2d.tif")

# g3test.tif	TIFF equivalent of g3test.g3 created by fax2tiff
command += rw_command (imagedir, "g3test.tif")
# FIXME - same aspect ratio issue as fax2d.tif

# jello.tif	256x192 8-bit RGB (packbits palette) Paul Heckbert "jello"
command += rw_command (imagedir, "jello.tif")

# ladoga.tif	158x118 16-bit unsigned, single band, deflate
# NOTE -- I have no idea if we read this correctly.  Neither ImageMagick
#    nor OSX preview display a meaningful image.

# off_l16.tif	333x225 8-bit CIE LogL (SGILog) office from Greg Larson
# off_luv24.tif	333x225 8-bit CIE LogLuv (SGILog24) office from " "
# off_luv32.tif	333x225	8-bit CIE LogLuv (SGILog) office from " "
#  FIXME -- we just don't handle LogL or LogLuv yet

# pc260001.tif	640x480 8-bit RGB digital camera image. Contains EXIF SubIFD.
# 		No compression.
# FIXME? - we don't seem to recognize additional Exif data that's in the
#    'Maker Note', which includes GainControl
command += rw_command (imagedir, "pc260001.tif")

# quad-jpeg.tif	512x384 8-bit YCbCr (jpeg) version of quad-lzw.tif
#  FIXME -- we don't handle this (YCbCr? jpeg?)
#  NOTE -- OSX preview doesn't handle this either (but ImageMagick does)

# quad-lzw.tif	512x384 8-bit RGB (lzw) "quadric surfaces"
# quad-tile.tif	512x384 tiled version of quad-lzw.tif (lzw)
command += rw_command (imagedir, "quad-lzw.tif")
command += rw_command (imagedir, "quad-tile.tif")
command += diff_command (imagedir+"/quad-tile.tif",
                                          imagedir+"/quad-lzw.tif")

# strike.tif	256x200 8-bit RGBA (lzw) "bowling pins" from Pixar
command += rw_command (imagedir, "strike.tif")

# text.tif	1512x359 4-bit b&w (thunderscan) am-express credit card
#  FIXME -- we don't get this right

# ycbcr-cat.tif	250x325 8-bit YCbCr (lzw) "kitty" created by rgb2ycbcr
#  FIXME -- we don't get this right

# smallliz.tif	160x160 8-bit YCbCr (OLD jpeg) lizard from HP**
# zackthecat.tif 234x213 8-bit YCbCr (OLD jpeg) tiled "ZackTheCat" from NeXT**
#   considered a deprecated format, not supported by libtiff

# oxford.tif	601x81 8-bit RGB (lzw) screendump off oxford
command += rw_command (imagedir, "oxford.tif", 0)

# The other images are from Hewlett Packard and exemplify the use of the
# HalftoneHints tag (in their words):
# The images are all the same subject, and should all appear the same
# after rendering.  Each of the images is slightly different as outlined
# by the following table:
#
#   FileName	   ToneRange  HalftoneHints
# jim___cg.tif      	A	    Y
# jim___dg.tif      	B	    N
# jim___gg.tif      	B	    Y
#
# NOTE -- OIIO appears to read this fine, but I'm really not sure how to
#    judge if it's "correct"
