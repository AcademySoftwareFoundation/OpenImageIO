#!/usr/bin/env python 

import OpenImageIO as oiio


# Read the one subimage from input then write it to output using
# write_image, write_scanlines, write_scanline, write_tile, or write_tiles,
# depending on the 'method' argument).  (Just copy one subimage, one MIP
# level.)
def copy_subimage (input, output, method="image",
                   memformat=oiio.TypeFloat) :
    spec = input.spec ()
    if method == "image" :
        pixels = input.read_image (memformat)
        if pixels == None :
            print "Error reading input pixels in", in_filename
            return False
        output.write_image (pixels)
    elif method == "scanlines" and spec.tile_width == 0 :
        pixels = input.read_image (memformat)
        if pixels == None :
            print "Error reading input pixels in", in_filename
            return False
        output.write_scanlines (spec.y, spec.y+spec.height, spec.z,
                                pixels)
    elif method == "scanline" and spec.tile_width == 0 :
        for z in range(spec.z, spec.z+spec.depth) :
            for y in range(spec.y, spec.y+spec.height) :
                pixels = input.read_scanline (y, z, memformat)
                if pixels == None :
                    print "Error reading input pixels in", in_filename
                    return False
                output.write_scanline (y, z, pixels)
    elif method == "tiles" and spec.tile_width != 0 :
        pixels = input.read_image (memformat)
        if pixels == None :
            print "Error reading input pixels in", in_filename
            return False
        output.write_tiles (spec.x, spec.x+spec.width,
                            spec.y, spec.y+spec.height,
                            spec.z, spec.z+spec.depth,
                            pixels)
    elif method == "tile" and spec.tile_width != 0 :
        for z in range(spec.z, spec.z+spec.depth, spec.tile_depth) :
            for y in range(spec.y, spec.y+spec.height, spec.tile_height) :
                for x in range(spec.x, spec.x+spec.width, spec.tile_width) :
                    pixels = input.read_tile (x, y, z, memformat)
                    if pixels == None :
                        print "Error reading input pixels in", in_filename
                        return False
                    output.write_tile (x, y, z, pixels)
    else :
        print "Unknown method:", method
        return False
    return True


# Read the whole image then write using write_image, write_scanlines,
# write_scanline, write_tile, or write_tiles, depending on the 'method'
# argument).  (Just copy one subimage, one MIP level.)
def copy_image (in_filename, out_filename, method="image",
                memformat=oiio.TypeFloat, outformat=oiio.TypeUnknown) :
    input = oiio.ImageInput.open (in_filename)
    if not input :
        print 'Could not open "' + filename + '"'
        print "\tError: ", oiio.geterror()
        print
        return
    outspec = input.spec()
    if outformat != oiio.TypeUnknown :
        outspec.format = outformat
    output = oiio.ImageOutput.create (out_filename)
    if not output :
        print "Could not create ImageOutput for", out_filename
        return
    ok = output.open (out_filename, outspec)
    if not ok :
        print "Could not open", out_filename
        return
    ok = copy_subimage (input, output, method, memformat)
    input.close ()
    output.close ()
    if ok :
        print "Copied", in_filename, "to", out_filename, "as", method


######################################################################
# main test starts here

try:

    copy_image ("scanline.tif", "grid-image.tif", method="image")
    copy_image ("scanline.tif", "grid-scanline.tif", method="scanline")
    copy_image ("scanline.tif", "grid-scanlines.tif", method="scanlines")
    copy_image ("tiled.tif", "grid-timage.tif", method="image")
    copy_image ("tiled.tif", "grid-tile.tif", method="tile")
    copy_image ("tiled.tif", "grid-tiles.tif", method="tiles")

    # Regression test for crash when changing formats
    copy_image ("scanline.tif", "grid-image.tif",
                memformat=oiio.TypeUInt8, outformat=oiio.TypeUInt16)

    print "Done."
except Exception as detail:
    print "Unknown exception:", detail

