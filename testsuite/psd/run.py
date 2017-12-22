#!/usr/bin/env python

imagedir = parent + "/oiio-images/"
files = [ "psd_123.psd", "psd_123_nomaxcompat.psd", "psd_bitmap.psd",
          "psd_indexed_trans.psd", "psd_rgb_8.psd", "psd_rgb_16.psd",
          "psd_rgb_32.psd", "psd_rgba_8.psd" ]
for f in files:
    command += info_command (imagedir + f)

command += info_command ("src/different-mask-size.psd")
command += info_command ("src/layer-mask.psd")
