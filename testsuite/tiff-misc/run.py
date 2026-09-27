#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Miscellaneous TIFF-related tests

# save the error output
redirect = " >> out.txt 2>&1 "

# Regression test -- we once had a bug where 'separate' planarconfig
# tiled float files would have data corrupted by a buffer overwrite.
command += oiiotool("--pattern checker 128x128 4 --tile 64 64 --planarconfig separate -d float -o check1.tif")

# Test bug we had until OIIO 2.3 when reading planarconfig=separate files
# (fixed by #2757) that was not detected by the uncompressed file. So copy
# to force compression in order to properly test:
command += rw_command ("src", "separate.tif")

# Test bugs we had until OIIO 2.4 for these corrupt file
command += oiiotool ("--oiioattrib try_all_readers 0 --info -v src/corrupt1.tif", failureok = True)
command += oiiotool ("--oiioattrib try_all_readers 0 --info -v src/crash-1633.tif", failureok = True)
command += oiiotool ("--oiioattrib try_all_readers 0 --info src/crash-1643.tif -o out.exr", failureok = True)
command += iconvert ("src/crash-1709.tif crash-1709.exr", failureok=True)

# Test reading and writing GPS tags
command += oiiotool ("src/gps.tif -o gps.tif")
command += info_command ("gps.tif", safematch=True, hash=False)

# Test bug with corrupt cmyk file
command += iconvert ("src/crash-cmyk-e12b.tif out.tif", failureok=True)

# Regression test: CMYK (photometric=separated) TIFF missing its
# BitsPerSample tag, which makes libtiff default it to 1 bit/sample. When
# unpacking the sub-8-bit samples for the CMYK->RGB conversion, the bit
# unpacker was writing all 4 input channels' worth of unpacked samples
# directly into the 3-channel RGB output buffer instead of scratch space,
# causing a heap buffer overflow.
command += info_command ("src/crash-cmyk-1bit.tif", safematch=True)

# Regression: a valid DEFLATE TIFF whose StripOffsets[0] points past EOF must
# be rejected cleanly during the pixel read (not read out of bounds). Placed
# last so its output appends at the tail of every libtiff-version ref variant.
command += oiiotool ("--oiioattrib try_all_readers 0 src/crash-rawstrip-offset-past-eof.tif -o out.exr", failureok = True)

# Regression test: the tiled reader did not do the channel shuffling and bit
# unpacking that the strip reader does. Sub-8-bit CMYK tiles unpacked all 4
# input channels straight into the 3-channel RGB buffer and overflowed it,
# "separate" CMYK tiles overflowed in separate_to_contig, CMYK tiles at native
# 8 and 16 bits were read into scratch and never copied out, and 17-31 bit
# tiles were never unpacked at all, both leaving the caller's buffer
# uninitialized. Each pair below holds identical pixels, stored tiled and in
# strips, so the two must decode the same. Between them they cover every
# unpacking case the tiled reader has: <8, 9-15, 17-31, and native 8 and 16
# bits, contig and "separate", with and without CMYK->RGB conversion. Also
# placed at the end so the output appends to the tail of every
# libtiff-version ref variant.
for base in [ "cmyk-1bit", "cmyk-4bit-planar", "cmyk-8bit", "cmyk-12bit",
              "cmyk-16bit", "rgb-24bit" ] :
    command += oiiotool ("src/{0}-tiled.tif src/{0}-strip.tif --diff".format(base))

# Regression test: a RowsPerStrip far larger than the image height was passed
# through unclamped as "oiio:RowsPerChunk", and reading with a data type
# conversion then sized its scanline buffer from it (a 42 GB allocation for
# this 32x32 image). Also at the end, to append to every ref variant.
command += oiiotool ("-i:type=float src/rowsperstrip-huge.tif --echo \"RowsPerChunk={TOP['oiio:RowsPerChunk']} avg={TOP.AVGCOLOR}\"")

outputs = [ "check1.tif", "out.txt" ]
