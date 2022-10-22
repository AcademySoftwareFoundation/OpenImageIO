#!/usr/bin/env python

redirect = ' >> out.txt 2>&1 '

files = [ "psd_123.psd", "psd_123_nomaxcompat.psd", "psd_bitmap.psd",
          "psd_indexed_trans.psd", "psd_rgb_8.psd", "psd_rgb_16.psd",
          "psd_rgb_32.psd", "psd_rgba_8.psd" ]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)

command += info_command ("src/different-mask-size.psd")
command += info_command ("src/layer-mask.psd")

# This file has a corrupted Exif block
command += info_command ("src/crash-psd-exif-1632.psd", failureok = 1)
# Corrupted thumbnail clobbered memory
command += info_command ("src/crash-thumb-1626.psd", failureok = 1)
