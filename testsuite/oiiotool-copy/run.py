#!/usr/bin/env python

####################################################################
# This test exercises oiiotool functionality that is mostly about
# copying pixels from one image to another.
####################################################################


# Some tests to verify that we are transferring data formats properly.
#
command += oiiotool ("-pattern checker 128x128 3 -d uint8 -tile 16 16 -o uint8.tif " +
                     "-echo \"explicit -d uint save result: \" -metamatch \"width|tile\" -i:info=2 uint8.tif -echo \"\"")
# Un-modified copy should preserve data type and tiling
command += oiiotool ("uint8.tif -o tmp.tif " +
                     "-echo \"unmodified copy result: \" -metamatch \"width|tile\" -i:info=2 tmp.tif -echo \"\"")
# Copy with explicit data request should change data type
command += oiiotool ("uint8.tif -d uint16 -o copy_uint16.tif " +
                     "-echo \"copy with explicit -d uint16 result: \" -metamatch \"width|tile\" -i:info=2 copy_uint16.tif -echo \"\"")
# Subimage concatenation should preserve data type
command += oiiotool ("uint8.tif copy_uint16.tif -siappend -o tmp.tif " +
                     "-echo \"siappend result: \" -metamatch \"width|tile\" -i:info=2 tmp.tif -echo \"\"")
# Combining two images preserves the format of the first read input, if
# there are not any other hints:
command += oiiotool ("-pattern checker 128x128 3 uint8.tif -add -o tmp.tif " +
                     "-echo \"combining images result: \" -metamatch \"width|tile\" -i:info=2 tmp.tif -echo \"\"")

# test --crop
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR + "/grid.tif --crop 100x400+50+200 -o crop.tif")

# test --cut
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR + "/grid.tif --cut 100x400+50+200 -o cut.tif")

# test --paste
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR + "/grid.tif "
            + "--pattern checker 256x256 3 --paste +150+75 -o pasted.tif")

# test --pastemeta
command += oiiotool ("--pattern:type=half constant:color=0,1,0 64x64 3 -o green.exr")
command += oiiotool ("--pattern:type=half constant:color=1,0,0 64x64 3 -attrib hair brown -attrib eyes 2 -attrib weight 20.5 -o redmeta.exr")
command += oiiotool ("redmeta.exr green.exr --pastemeta -o greenmeta.exr")
command += info_command ("green.exr", safematch=True)
command += info_command ("greenmeta.exr", safematch=True)

# test mosaic
# Purposely test with fewer images than the mosaic array size
command += oiiotool ("--pattern constant:color=1,0,0 50x50 3 "
            + "--pattern constant:color=0,1,0 50x50 3 "
            + "--pattern constant:color=0,0,1 50x50 3 "
            + "--mosaic:pad=10 2x2 -d uint8 -o mosaic.tif")


# test --metamerge, using chappend as an example
command += oiiotool ("--create 64x64 3 -chnames R,G,B -attrib a 3.0 -o aimg.exr")
command += oiiotool ("--create 64x64 3 -chnames A,Z -attrib b 1.0 -o bimg.exr")
command += oiiotool ("aimg.exr bimg.exr --chappend -o nometamerge.exr")
command += oiiotool ("--metamerge aimg.exr bimg.exr --chappend -o metamerge.exr")
command += info_command ("nometamerge.exr", safematch=True)
command += info_command ("metamerge.exr", safematch=True)

# test --chappend of multiple images
command += oiiotool ("--pattern constant:color=0.5 64x64 1 --text R --chnames R " +
                     "--pattern constant:color=0.25,0.75 64x64 2 --text G,B --chnames G,B " +
                     "--pattern constant:color=1.0 64x64 1 --text A --chnames A " +
                     "--chappend:n=3 -d half -o chappend-3images.exr")



# Outputs to check against references
outputs = [
            "crop.tif", "cut.tif", "pasted.tif", "mosaic.tif",
            "greenmeta.exr",
            "chappend-3images.exr",
            "out.txt"
          ]

#print "Running this command:\n" + command + "\n"
