#!/usr/bin/env python

import OpenImageIO as oiio

# Read the one subimage from input then write it to output using
# copy_image, thus skipping both decompression and recompression steps
# (Just copy one subimage, one MIP level.)
def copy_image (in_filename, out_filename) :
    input = oiio.ImageInput.open (in_filename)
    if not input :
        print ('Could not open "' + in_filename + '"')
        print ("\tError: ", oiio.geterror())
        print ()
        return
    outspec = input.spec()
    output = oiio.ImageOutput.create (out_filename)
    if not output :
        print ("Could not create ImageOutput for", out_filename)
        return
    ok = output.open (out_filename, outspec)
    if not ok :
        print ("Could not open", out_filename)
        return
    ok = output.copy_image(input)
    input.close ()
    output.close ()
    if ok :
        print ("Copied", in_filename, "to", out_filename)

# Copy lossy compressed image, this should not recompress the image again (loss of data).
copy_image("ref/compressed-pxr24.exr", "compressed-pxr24.exr")
copy_image("ref/compressed-b44.exr", "compressed-b44.exr")
copy_image("ref/compressed-b44a.exr", "compressed-b44a.exr")
copy_image("ref/compressed-dwaa.exr", "compressed-dwaa.exr")
copy_image("ref/compressed-dwab.exr", "compressed-dwab.exr")