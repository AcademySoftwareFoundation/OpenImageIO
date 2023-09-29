#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Regression test for stride bugs for tiled images where resolution isn't
# a whole number of tiles.  This particular resolution mimics a particular
# failure case known to cause problems that I debugged.

# Create a file
command += (oiio_app("oiiotool") 
            + " --pattern checker 2220x1172 3 --tile 128 128 -d half -o bcheck.exr >> out.txt ;\n")


# Copy the file -- this conversion crashed when the bug was present
command += (oiio_app("iconvert") + " bcheck.exr -d float b2.exr ;\n")

# Make sure they match
command += diff_command ("bcheck.exr", "b2.exr")

#print "Running this command:\n" + command + "\n"

