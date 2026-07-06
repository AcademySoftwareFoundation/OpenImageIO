#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


command += info_command ("-s --stats ../common/textures/grid.tx", info_program="iinfo")

# command += oiiotool ("-pattern constant:color=0.25,0.5,0.75 4x4 3 --origin +2+2 --fullsize 20x20+1+1 -o tiny-az.exr")
command += info_command ("-f -v --hash --stats src/tiny-az.exr", info_program="iinfo")
command += info_command ("--echo \"info from oiiotool:\" --hash --stats --dumpdata src/tiny-az.exr", info_program="oiiotool")

# Get iinfo coverage of a deep file
# command += oiiotool ("-pattern constant:color=1e38,0 4x4 2 --chnames Z,A"
#                      " --point:color=10.0,1.0 2,2 --deepen -o tinydeep.exr")
command += info_command ("-v --hash --stats src/tinydeep.exr", info_program="iinfo")
command += info_command ("--echo \"info from oiiotool:\" --hash --stats --dumpdata src/tinydeep.exr", info_program="oiiotool")

# Dump of a flat integer file
command += oiiotool ("-pattern constant:color=0.25,0.5,0.75 2x2 3 -d uint8 -o tmp.tif")
command += oiiotool ("-echo \"uint8 file\" --hash --stats --dumpdata tmp.tif")

# Dump data in C style
command += oiiotool ("-echo \"int16 file, C format dump\" --dumpdata:C=foo tmp.tif")

# info in xml
command += info_command ("--info:format=xml tmp.tif", safematch=True)

# Info for subimages and mips
command += info_command ("--stats src/subimage.tif", info_program="iinfo")
command += info_command ("--stats src/mip.tif", info_program="iinfo")
