#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Read and write null scanline and a tiled mipmap image
command += oiiotool ('-v -info -stats ' +
                     '"foo.null?RES=640x480&CHANNELS=3&TYPE=uint8&PIXEL=0.25,0.5,1&a=1&b=2.5&c=foo&string d=bar&e=float 3&f=\\\"baz\\\"" ' +
                     '-o out.null ' +
                     '"bar.null?RES=128x128&CHANNELS=3&TILE=64x64&TEX=1&TYPE=uint16&PIXEL=0.25,0.5,1" ' +
                     '-o:tile=64x64 outtile.null'
                     )

# The null reader synthesizes pixels but still parses the query arguments.
# These used to abort on an uncaught length_error (negative CHANNELS), hang
# building channel names (huge CHANNELS), or hand back a negative-sized spec.
redirect = " >> out.txt 2>&1 "
for args in [ "CHANNELS=-1", "CHANNELS=2000000000", "RES=-4x-4", "RES=0x0",
              "RES=64x64&TILE=-4x-4" ]:
    command += oiiotool ('--info "bad.null?' + args + '"', failureok=True)
