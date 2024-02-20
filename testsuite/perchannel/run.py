#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Test reading and writing of files with per-channel data formats.


# Few image formats support per-channel formats, but OpenEXR is one
# of them, and the OpenEXR test images contains one such image:
image = OIIO_TESTSUITE_IMAGEDIR + "/ScanLines/Blobbies.exr"

# iconvert reads and writes native format, test that.
command += (oiio_app("iconvert") + image + " blob1.exr ;\n")
command += diff_command (image, "blob1.exr")

# oiiotool uses a different code path involving ImageCache and ImageBuf,
# and will convert to float along the way.
command += (oiio_app("oiiotool") + image + " -o blob2.exr ;\n")
command += diff_command (image, "blob2.exr")


# Now we do a similar test for a tiled OpenEXR file with per-channel formats
image = OIIO_TESTSUITE_IMAGEDIR + "/Tiles/Spirals.exr"

# iconvert reads and writes native format, test that.
command += (oiio_app("iconvert") + image + " spiral1.exr ;\n")
command += diff_command (image, "spiral1.exr")

# oiiotool uses a different code path involving ImageCache and ImageBuf,
# and will convert to float along the way.
command += (oiio_app("oiiotool") + image + " -o spiral2.exr ;\n")
command += diff_command (image, "spiral2.exr")

# Create a file with per-channel data types, make sure that works
command += oiiotool("--create 64x64 4 --d R=half,G=float,B=half,A=float -o hfhf.exr")
command += info_command("hfhf.exr", verbose=False)

# Make sure that read/write unmodified will preserve the channel types
command += oiiotool("hfhf.exr -o hfhf_copy.exr")
command += info_command("hfhf_copy.exr", verbose=False)

# Make sure that read/modify/write preserves the channel types of the input
command += oiiotool("hfhf.exr -mulc 0.5,0.5,0.5,0.5 -o hfhf_mod.exr")
command += info_command("hfhf_mod.exr", verbose=False)

#print "Running this command:\n" + command + "\n"

