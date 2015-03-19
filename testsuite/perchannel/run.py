#!/usr/bin/env python

# Test reading and writing of files with per-channel data formats.


# Few image formats support per-channel formats, but OpenEXR is one
# of them, and the OpenEXR test images contains one such image:
image = parent + "/openexr-images/ScanLines/Blobbies.exr"

# iconvert reads and writes native format, test that.
command += (oiio_app("iconvert") + image + " blob1.exr ;\n")
command += diff_command (image, "blob1.exr")

# oiiotool uses a different code path involving ImageCache and ImageBuf,
# and will convert to float along the way.
command += (oiio_app("oiiotool") + image + " -o blob2.exr ;\n")
command += diff_command (image, "blob2.exr")


# Now we do a similar test for a tiled OpenEXR file with per-channel formats
image = parent + "/openexr-images/Tiles/Spirals.exr"

# iconvert reads and writes native format, test that.
command += (oiio_app("iconvert") + image + " spiral1.exr ;\n")
command += diff_command (image, "spiral1.exr")

# oiiotool uses a different code path involving ImageCache and ImageBuf,
# and will convert to float along the way.
command += (oiio_app("oiiotool") + image + " -o spiral2.exr ;\n")
command += diff_command (image, "spiral2.exr")


#print "Running this command:\n" + command + "\n"

