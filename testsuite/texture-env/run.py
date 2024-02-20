#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


redirect = ">> out.txt 2>&1"
# Create a ramped checker
command += oiiotool("-pattern fill:topleft=0,0,0,1:topright=1,0,0,1:bottomleft=0,1,0,1:bottomright=1,1,0,1 256x128 4 "
                    "-pattern checker:color1=1,1,1,1:color2=0.5,0.5,0.5,1:width=16:height=16 256x128 4 -mul "
                    "-attrib DateTime \"2022-11-11 11:11:11\" -attrib oiio:subimagename Fred "
                    "-d half -oenv rampenv.exr")

command += testtex_command("--res 64 32 -d half -o env.exr rampenv.exr")

# This time, use handles, exercise derivatives
command += testtex_command("--handle --derivs --no-gettextureinfo "
                           "--res 64 32 -d half -o env-handle.exr rampenv.exr")

# more channels
command += oiiotool("-pattern fill:topleft=0,0,0,1,1:topright=1,0,0,1,1:bottomleft=0,1,0,1,1:bottomright=1,1,0,1,1 256x128 5 "
                    "-pattern checker:color1=1,1,1,1,1:color2=0.5,0.5,0.5,1,0.5:width=16:height=16 256x128 5 -mul "
                    "-attrib DateTime \"2022-11-11 11:11:11\" -d half -oenv env5chan.exr")
command += testtex_command("--nchannels 5 --derivs --no-gettextureinfo --res 64 32 -d half -o 5chan.exr env5chan.exr")

# subimages
command += testtex_command("--nchannels 4 --no-gettextureinfo -subimage 0 --res 64 32 -d half -o subimage0.exr rampenv.exr")
command += testtex_command("--nchannels 4 --no-gettextureinfo -subimagename Fred --res 64 32 -d half -o subimagefred.exr rampenv.exr")
command += testtex_command("--nchannels 4 --no-gettextureinfo -subimagename missing --res 2 1 -d half -o subimagemissing.exr rampenv.exr")

# interp and mip modes
command += testtex_command("-interpmode 0 -mipmode 1 --no-gettextureinfo --res 64 32 -d half -o closest-nomip.exr rampenv.exr")
command += testtex_command("-interpmode 1 -mipmode 2 --no-gettextureinfo --res 64 32 -d half -o bilinear-onelevel.exr rampenv.exr")
command += testtex_command("-interpmode 2 -mipmode 3 --no-gettextureinfo --res 64 32 -d half -o bicubic-trilinear.exr rampenv.exr")

outputs = [ "env.exr",
            "env-handle.exr",
            "5chan.exr",
            "subimage0.exr",
            "subimagefred.exr",
            "subimagemissing.exr",
            "closest-nomip.exr",
            "bilinear-onelevel.exr",
            "bicubic-trilinear.exr",
            "out.txt" ]
