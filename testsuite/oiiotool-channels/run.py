#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# This test exercises the oiiotool command examples from the "Channel
# reordering and padding" section of the oiiotool documentation. The command
# lines below, between the BEGIN-docs/END-docs marker comments, are the very
# same lines that appear verbatim in the docs -- src/doc/oiiotool.md
# literalincludes this section of the file, so any change made here must be
# made in the docs as well, and vice versa.

redirect = " >> out.txt 2>&1 "

# Set up the images the documentation examples operate on: an RGBA .tif, an
# RGBA .exr, and a 5-channel .exr with conventional channel names.
command += run_commands("""
    oiiotool -pattern checker:color1=0.9,0.2,0.1,1:color2=0.1,0.4,0.9,1 64x64 4 -d uint8 -o rgba.tif
    oiiotool -pattern constant:color=0.25,0.5,0.75,0.8 64x64 4 -d half -o rgba.exr
    oiiotool -pattern constant:color=0.1,0.5,0.9,0.3,0.7 64x64 5 -d half --chnames R,G,B,A,Z -o manychannels.exr
    """)

command += run_commands("""
    # BEGIN-docs-channels-copy-color
    oiiotool rgba.tif --ch R,G,B -o rgb.tif
    # END-docs-channels-copy-color
    # BEGIN-docs-channels-zero-rg
    oiiotool rgb.tif --ch R=0,G=0,B -o justblue.tif
    # END-docs-channels-zero-rg
    # BEGIN-docs-channels-swap-rb
    oiiotool rgba.tif --ch R=B,G,B=R,A -o bgra.tif
    # END-docs-channels-swap-rb
    # BEGIN-docs-channels-extract
    oiiotool -i:ch=R,G,B manychannels.exr -o rgb.exr
    # END-docs-channels-extract
    # BEGIN-docs-channels-add-alpha-const
    oiiotool rgb.tif --ch R,G,B,A=1.0 -o rgba.tif
    # END-docs-channels-add-alpha-const
    # BEGIN-docs-channels-add-alpha-from-r
    oiiotool rgb.tif --ch R,G,B,A=R -o rgba.tif
    # END-docs-channels-add-alpha-from-r
    # BEGIN-docs-channels-add-z
    oiiotool rgba.exr --ch R,G,B,A,Z=3.0 -o rgbaz.exr
    # END-docs-channels-add-z
    """)

# Verify the results of the documented examples by content hash, so that any
# change in the documented behavior turns this test red.
command += info_command("rgb.tif", verbose=False, hash=True)
command += info_command("justblue.tif", verbose=False, hash=True)
command += info_command("bgra.tif", verbose=False, hash=True)
command += info_command("rgb.exr", verbose=False, hash=True)
command += info_command("rgba.tif", verbose=False, hash=True)
command += info_command("rgbaz.exr", verbose=False, hash=True)

# The hashes in the out.txt info lines pin the exact contents of every
# output image, so comparing out.txt is sufficient.
outputs = [ "out.txt" ]
