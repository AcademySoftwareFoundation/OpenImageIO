#!/usr/bin/env python 

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Set up some udims
command += oiiotool ("-pattern constant:color=.5,.1,.1 256x256 3 -text:size=50:x=75:y=140 1001 --attrib Make pet --attrib Model dog -d uint8 -otex file.1001.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.1 256x256 3 -text:size=50:x=75:y=140 1002 --attrib Make pet --attrib Model dog -d uint8 -otex file.1002.tx")
command += oiiotool ("-pattern constant:color=.1,.1,.5 256x256 3 -text:size=50:x=75:y=140 1011 --attrib Make pet --attrib Model cat -d uint8 -otex file.1011.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.5 256x256 3 -text:size=50:x=75:y=140 1012 --attrib Make pet --attrib Model dog -d uint8 -otex file.1012.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.5 256x256 3 -text:size=50:x=75:y=140 1012 --attrib Make pet --attrib Model dog -d uint8 -otex file.1032.tx")

command += pythonbin + " src/test_texture_sys.py > out.txt"

