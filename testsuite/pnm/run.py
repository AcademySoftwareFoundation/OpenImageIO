#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = ' >> out.txt 2>&1 '

imagedir = OIIO_TESTSUITE_IMAGEDIR + "/pnm"

for f in [ "bw-ascii.pbm", "bw-binary.pbm",
           "grey-ascii.pgm", "grey-binary.pgm",
           "rgb-ascii.ppm", "rgb-binary.ppm" ] :
    command += rw_command ("src", f)

# Get the hashes of a few reference PFM files
files = [ "test-1.pfm", "test-2.pfm", "test-3.pfm" ]
for f in files:
    command += info_command (imagedir + "/" + f,
                             safematch=True, hash=True)

# Damaged files
files = [ "src/bad-4552.pgm", "src/bad-4553.pgm" ]
for f in files:
    command += info_command (f, extraargs="--oiioattrib try_all_readers 0 --printstats", failureok=True)

# Decompression bomb: a 19-byte header declaring a ~4 GB image (65000x65000).
# The compression-ratio guard must reject it before the caller allocates the
# full pixel buffer.
command += info_command ("src/bomb-65000.pgm", failureok=True, safematch=True)

# Write a float PFM taller than one write_image() chunk (a 3840-wide RGB
# float image is written in 64-row chunks), then read it back and compare
# it against the same pattern. This catches per-chunk row flipping (#5471).
command += oiiotool ("--pattern fill:top=0,0,0:bottom=1,1,1 3840x2160 3 -d float -o tall-ramp.pfm")
command += oiiotool ("tall-ramp.pfm --pattern fill:top=0,0,0:bottom=1,1,1 3840x2160 3 -d float --diff")
