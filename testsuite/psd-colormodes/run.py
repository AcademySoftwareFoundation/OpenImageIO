#!/usr/bin/env python

failthresh = 0.008
anymatch = True
imagedir = "src/"
outputs = []
files = [
          "pattern2-8-rgb.psd", "pattern2-16-rgb.psd",
          #"pattern2-32-rgb.psd",
          "pattern2-8-cmyk.psd", "pattern2-16-cmyk.psd",
          "pattern2-8-multichannel.psd", "pattern2-16-multichannel.psd",
          "pattern2-8-grayscale.psd", "pattern2-16-grayscale.psd",
          #"pattern2-32-grayscale.psd",
          "pattern2-8-indexed.psd",
        ]
for f in files:
    outfile = f+".tif"
    command += rw_command (imagedir, f, testwrite=True,
                           printinfo=False, output_filename=outfile)
    outputs += [ outfile ]

outputs += [ "out.txt" ]
