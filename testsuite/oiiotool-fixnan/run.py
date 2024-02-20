#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


redirect = " >> out.txt 2>&1"
failureok = True

command += oiiotool ("src/bad.exr --fixnan black -o black.exr")
command += oiiotool ("src/bad.exr --fixnan box3 -o box3.exr")
command += oiiotool ("src/bad.exr --fixnan error -o err.exr")
command += info_command ("src/bad.exr", "--stats", safematch=True)
command += info_command ("black.exr", "--stats", safematch=True)
command += info_command ("box3.exr", "--stats", safematch=True)

# test deep
command += oiiotool ("src/bad.exr --chnames R,A,Z --deepen -o deep.exr")
command += oiiotool ("deep.exr --echo \"Bad deep (black):\" --printstats" +
                     " --fixnan black --printstats --echo \" \"")
command += oiiotool ("deep.exr --echo \"Bad deep (box3):\" --printstats" +
                     " --fixnan box3 --printstats --echo \" \"")
command += oiiotool ("deep.exr --echo \"Bad deep (error):\" --printstats" +
                     " --fixnan error --printstats --echo \" \"")

# Outputs to check against references
outputs = [ "black.exr", "box3.exr", "out.txt" ]
