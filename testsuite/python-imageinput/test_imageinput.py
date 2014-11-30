#!/usr/bin/env python 

import OpenImageIO as oiio


# Print the contents of an ImageSpec
def print_imagespec (spec, subimage=0, mip=0, msg="") :
    if msg != "" :
        print str(msg)
    if spec.depth <= 1 :
        print ("  resolution %dx%d%+d%+d" % (spec.width, spec.height, spec.x, spec.y))
    else :
        print ("  resolution %dx%d%x%d+d%+d%+d" % (spec.width, spec.height, spec.depth, spec.x, spec.y, spec.z))
    if (spec.width != spec.full_width or spec.height != spec.full_height
        or spec.depth != spec.full_depth) :
        if spec.full_depth <= 1 :
            print ("  full res   %dx%d%+d%+d" % (spec.full_width, spec.full_height, spec.full_x, spec.full_y))
        else :
            print ("  full res   %dx%d%x%d+d%+d%+d" % (spec.full_width, spec.full_height, spec.full_depth, spec.full_x, spec.full_y, spec.full_z))
    if spec.tile_width :
        print ("  tile size  %dx%dx%d" % (spec.tile_width, spec.tile_height, spec.tile_depth))
    else :
        print "  untiled"
    if mip >= 1 :
        return
    print "  " + str(spec.nchannels), "channels:", spec.channelnames
    print "  format = ", str(spec.format)
    if spec.channelformats :
        print "  channelformats = ", spec.channelformats
    print "  alpha channel = ", spec.alpha_channel
    print "  z channel = ", spec.z_channel
    print "  deep = ", spec.deep
    for i in range(len(spec.extra_attribs)) :
        if type(spec.extra_attribs[i].value) == str :
            print " ", spec.extra_attribs[i].name, "= \"" + spec.extra_attribs[i].value + "\""
        else :
            print " ", spec.extra_attribs[i].name, "=", spec.extra_attribs[i].value



def poor_mans_iinfo (filename) :
    input = oiio.ImageInput.open (filename)
    if not input :
        print 'Could not open "' + filename + '"'
        print "\tError: ", oiio.geterror()
        print
        return
    print 'Opened "' + filename + '" as a ' + input.format_name()
    sub = 0
    mip = 0
    while True :
        if sub > 0 or mip > 0 :
            print "Subimage", sub, "MIP level", mip, ":"
        print_imagespec (input.spec(), mip=mip)
        mip = mip + 1
        if input.seek_subimage (sub, mip) :
            continue    # proceed to next MIP level
        else :
            sub = sub + 1
            mip = 0
            if input.seek_subimage (sub, mip) :
                continue    # proceed to next subimage
        break  # no more MIP levels or subimages
    input.close ()
    print



# Read the whole image (using either read_image, read_scanlines, or
# read_tiles, depending on the 'method' argument) and print a few 
# pixel values ot prove that we have the right data. Read nchannels
# channels, if nonzero, otherwise read the full channel range in the
# file.
def test_readimage (filename, sub=0, mip=0, type=oiio.UNKNOWN,
                    method="image", nchannels=0) :
    input = oiio.ImageInput.open (filename)
    if not input :
        print 'Could not open "' + filename + '"'
        print "\tError: ", oiio.geterror()
        print
        return
    print 'Opened "' + filename + '" as a ' + input.format_name()
    input.seek_subimage (sub, mip)
    spec = input.spec ()
    if nchannels == 0 or method == "image" :
        nchannels = spec.nchannels
    if type == oiio.UNKNOWN :
        type = spec.format.basetype
    if method == "image" :
        data = input.read_image (type)
    elif method == "scanlines" :
        data = input.read_scanlines (spec.y, spec.y+spec.height, spec.z,
                                     0, nchannels, type)
    elif method == "tiles" :
        data = input.read_tiles (spec.x, spec.x+spec.width,
                                 spec.y, spec.y+spec.height,
                                 spec.z, spec.z+spec.depth,
                                 0, nchannels, type)
    else :
        print "Unknown method:", method
        return
    if data == None :
        print "read returned None"
        return
    # print the first, last, and middle pixel values
    (x,y) = (spec.x, spec.y)
    i = ((y-spec.y)*spec.width + (x-spec.x)) * nchannels
    print "@", (x,y), "=", data[i:i+nchannels]
    (x,y) = (spec.x+spec.width-1, spec.y+spec.height-1)
    i = ((y-spec.y)*spec.width + (x-spec.x)) * nchannels
    print "@", (x,y), "=", data[i:i+nchannels]
    (x,y) = (spec.x+spec.width/2, spec.y+spec.height/2)
    i = ((y-spec.y)*spec.width + (x-spec.x)) * nchannels
    print "@", (x,y), "=", data[i:i+nchannels]
    input.close ()
    print



# Read the image, one scanline at a time, print a couple values
# at particular locations to make sure we have the correct data.
def test_readscanline (filename, sub=0, mip=0, type=oiio.UNKNOWN) :
    input = oiio.ImageInput.open (filename)
    if not input :
        print 'Could not open "' + filename + '"'
        print "\tError: ", oiio.geterror()
        print
        return
    print 'Opened "' + filename + '" as a ' + input.format_name()
    input.seek_subimage (sub, mip)
    spec = input.spec ()
    if spec.tile_width != 0 :
        print "Error: tiled"
        return
    if type == oiio.UNKNOWN :
        type = spec.format.basetype
    for y in range(spec.height) :
        data = input.read_scanline (y+spec.y, spec.z, type)
        if data == None :
            print "read returned None"
            return
        # print the first pixel of the first and last scanline
        if y == 0 or y == (spec.height-1) :
            i = 0 * spec.nchannels
            print "@", (spec.x,y+spec.y), "=", data[i:i+spec.nchannels]
    input.close ()
    print



# Read the whole image, one tile at a time, print a couple values
# at particular locations to make sure we have the correct data.
def test_readtile (filename, sub=0, mip=0, type=oiio.UNKNOWN) :
    input = oiio.ImageInput.open (filename)
    if not input :
        print 'Could not open "' + filename + '"'
        print "\tError: ", oiio.geterror()
        print
        return
    print 'Opened "' + filename + '" as a ' + input.format_name()
    input.seek_subimage (sub, mip)
    spec = input.spec ()
    if spec.tile_width == 0 :
        print "Error: not tiled"
        return
    if type == oiio.UNKNOWN :
        type = spec.format.basetype
    # Randomly read a couple of tiles, print a pixel from within it
    (tx,ty) = (spec.x, spec.y)
    data = input.read_tile (tx+spec.x, ty+spec.y, spec.z, type)
    if data == None :
        print "read returned None"
        return
    (x,y) = (tx+spec.tile_width/2, ty+spec.tile_height/2)
    i = ((y-ty)*spec.tile_width + (x-tx)) * spec.nchannels
    print "@", (x,y), "=", data[i:i+spec.nchannels]
    (tx,ty) = (spec.x+2*spec.tile_width, spec.y+2*spec.tile_height)
    data = input.read_tile (tx+spec.x, ty+spec.y, spec.z, type)
    (x,y) = (tx+spec.tile_width/2, ty+spec.tile_height/2)
    i = ((y-ty)*spec.tile_width + (x-tx)) * spec.nchannels
    print "@", (x,y), "=", data[i:i+spec.nchannels]
    input.close ()
    print



######################################################################
# main test starts here

try:
    # test basic opening and being able to read the spec
    poor_mans_iinfo ("badname.tif")
    poor_mans_iinfo ("../../../../../oiio-images/tahoe-gps.jpg")
    poor_mans_iinfo ("../common/textures/grid.tx")

    # test readimage
    print "Testing read_image:"
    test_readimage ("../../../../../oiio-images/tahoe-gps.jpg")
    # again, force a float buffer
    test_readimage ("../../../../../oiio-images/tahoe-gps.jpg",
                    type=oiio.FLOAT)
    # Test read of partial channels
    test_readimage ("../../../../../oiio-images/tahoe-gps.jpg",
                    method="scanlines", nchannels=1)

    # test readscanline
    print "Testing read_scanline:"
    test_readscanline ("../../../../../oiio-images/tahoe-gps.jpg")

    # test readtile
    print "Testing read_tile:"
    test_readtile ("../common/textures/grid.tx")

    # test readscanlines
    print "Testing read_scanlines:"
    test_readimage ("../../../../../oiio-images/tahoe-gps.jpg",
                    method="scanlines")

    # test readtiles
    print "Testing read_tiles:"
    test_readimage ("../common/textures/grid.tx",
                    method="tiles")

    print "Done."
except Exception as detail:
    print "Unknown exception:", detail

