#!/usr/bin/env python


# Some tests to verify that we are transferring data formats properly.
#
command += oiiotool ("-pattern checker 128x128 3 -d uint8 -tile 16 16 -o uint8.tif " +
                     "-echo '\nexplicit -d uint save result: ' -metamatch \"width|tile\" -i:info=2 uint8.tif")
# Un-modified copy should preserve data type and tiling
command += oiiotool ("uint8.tif -o tmp.tif " +
                     "-echo '\nunmodified copy result: ' -metamatch \"width|tile\" -i:info=2 tmp.tif")
# Copy with explicit data request should change data type
command += oiiotool ("uint8.tif -d uint16 -o copy_uint16.tif " +
                     "-echo '\ncopy with explicit -d uint16 result: ' -metamatch \"width|tile\" -i:info=2 copy_uint16.tif")
# Subimage concatenation should preserve data type
command += oiiotool ("uint8.tif copy_uint16.tif -siappend -o tmp.tif " +
                     "-echo '\nsiappend result: ' -metamatch \"width|tile\" -i:info=2 tmp.tif")
# Combining two images preserves the format of the first read input, if
# there are not any other hints:
command += oiiotool ("-pattern checker 128x128 3 uint8.tif -add -o tmp.tif " +
                     "-echo '\ncombining images result: ' -metamatch \"width|tile\" -i:info=2 tmp.tif")





# Outputs to check against references
outputs = [ "out.txt" ]

#print "Running this command:\n" + command + "\n"
