#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import OpenImageIO as oiio

import os

OIIO_TESTSUITE_IMAGEDIR = os.getenv('OIIO_TESTSUITE_IMAGEDIR',
                                    '../oiio-images')

# Print the contents of an ImageSpec
def print_imagespec (spec, subimage=0, mip=0, msg="") :
    if msg != "" :
        print (str(msg))
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
        print ("  untiled")
    if mip >= 1 :
        return
    print ("  " + str(spec.nchannels), "channels:", spec.channelnames)
    print ("  format = ", str(spec.format))
    if spec.channelformats :
        print ("  channelformats = ", spec.channelformats)
    print ("  alpha channel = ", spec.alpha_channel)
    print ("  z channel = ", spec.z_channel)
    print ("  deep = ", spec.deep)
    for i in range(len(spec.extra_attribs)) :
        if type(spec.extra_attribs[i].value) == str :
            print (" ", spec.extra_attribs[i].name, "= \"" + spec.extra_attribs[i].value + "\"")
        else :
            print (" ", spec.extra_attribs[i].name, "=", spec.extra_attribs[i].value)



def poor_mans_iinfo (filename) :
    input = oiio.ImageInput.open (filename)
    if not input :
        print ('Could not open "' + filename + '"')
        print ("\tError: ", oiio.geterror())
        return
    print ('Opened "' + filename + '" as a ' + input.format_name())
    sub = 0
    mip = 0
    while True :
        if sub > 0 or mip > 0 :
            print ("Subimage", sub, "MIP level", mip, ":")
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
    print ()



# Read the whole image (using either read_image, read_scanlines, or
# read_tiles, depending on the 'method' argument) and print a few 
# pixel values ot prove that we have the right data. Read nchannels
# channels, if nonzero, otherwise read the full channel range in the
# file.
def test_readimage (filename, sub=0, mip=0, type=oiio.UNKNOWN,
                    method="image", nchannels=0,
                    print_pixels = True, keep_unknown=False) :
    input = oiio.ImageInput.open (filename)
    if not input :
        print ('Could not open "' + filename + '"')
        print ("\tError: ", oiio.geterror())
        print ()
        return
    print ('Opened "' + filename + '" as a ' + input.format_name())
    input.seek_subimage (sub, mip)
    spec = input.spec ()
    if nchannels == 0 or method == "image" :
        nchannels = spec.nchannels
    if str(type) == "unknown" and not keep_unknown :
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
        print ("Unknown method:", method)
        return
    if data is None :
        print ("read returned None")
        return
    if print_pixels :
        # print the first, last, and middle pixel values
        (x,y) = (spec.x, spec.y)
        print ("@", (x,y), "=", data[y,x])
        (x,y) = (spec.x+spec.width-1, spec.y+spec.height-1)
        print ("@", (x,y), "=", data[y,x])
        (x,y) = (spec.x+spec.width//2, spec.y+spec.height//2)
        print ("@", (x,y), "=", data[y,x])
    else :
        print ("Read array typecode", data.dtype, " [", data.size, "]")
    # Test the spec and spec_dimensions methods
    spec = input.spec_dimensions (0, 0)
    if len(spec.extra_attribs) > 0 :
        print ("wrong spec_dimensions(s,m) metadata items: ", len(spec.extra_attribs))
    spec = input.spec (0, 0)
    if len(spec.extra_attribs) == 0 :
        print ("wrong spec(s,m) metadata items: ", len(spec.extra_attribs))
    input.close ()
    print ()



# Read the image, one scanline at a time, print a couple values
# at particular locations to make sure we have the correct data.
def test_readscanline (filename, sub=0, mip=0, type=oiio.UNKNOWN) :
    input = oiio.ImageInput.open (filename)
    if not input :
        print ('Could not open "' + filename + '"')
        print ("\tError: ", oiio.geterror())
        print ()
        return
    print ('Opened "' + filename + '" as a ' + input.format_name())
    input.seek_subimage (sub, mip)
    spec = input.spec ()
    if spec.tile_width != 0 :
        print ("Error: tiled")
        return
    if type == oiio.UNKNOWN :
        type = spec.format.basetype
    for y in range(spec.height) :
        data = input.read_scanline (y+spec.y, spec.z, type)
        if data is None :
            print ("read returned None")
            return
        # print the first pixel of the first and last scanline
        if y == 0 or y == (spec.height-1) :
            print ("@", (spec.x,y+spec.y), "=", data[0])
    input.close ()
    print ()



# Read the whole image, one tile at a time, print a couple values
# at particular locations to make sure we have the correct data.
def test_readtile (filename, sub=0, mip=0, type=oiio.UNKNOWN) :
    input = oiio.ImageInput.open (filename)
    if not input :
        print ('Could not open "' + filename + '"')
        print ("\tError: ", oiio.geterror())
        print ()
        return
    print ('Opened "' + filename + '" as a ' + input.format_name())
    input.seek_subimage (sub, mip)
    spec = input.spec ()
    if spec.tile_width == 0 :
        print ("Error: not tiled")
        return
    if type == oiio.UNKNOWN :
        type = spec.format.basetype
    # Randomly read a couple of tiles, print a pixel from within it
    (tx,ty) = (spec.x, spec.y)
    data = input.read_tile (tx+spec.x, ty+spec.y, spec.z, type)
    if data is None :
        print ("read returned None")
        return
    (x,y) = (tx+spec.tile_width//2, ty+spec.tile_height//2)
    print ("@", (x,y), "=", data[y,x])
    (tx,ty) = (spec.x+2*spec.tile_width, spec.y+2*spec.tile_height)
    data = input.read_tile (tx+spec.x, ty+spec.y, spec.z, type)
    print ("@", (x,y), "=", data[y,x])
    input.close ()
    print ()


def write (image, filename, format=oiio.UNKNOWN) :
    if not image.has_error :
        image.set_write_format (format)
        image.write (filename)
    if image.has_error :
        print ("Error writing", filename, ":", image.geterror())


# Regression test for #2285: configuration settings were "forgotten" if the
# scanline read order necessitated closing and reopening the file.
def test_tiff_remembering_config() :
    # Create a file that has unassociated alpha
    print ("Testing write and read of unassociated:")
    spec = oiio.ImageSpec(2,2,4,"float")
    spec.attribute("oiio:UnassociatedAlpha", 1)
    wbuf = oiio.ImageBuf(spec)
    oiio.ImageBufAlgo.fill(wbuf, (0.5, 0.5, 0.5, 0.5))
    print ("  writing: ", wbuf.get_pixels())
    wbuf.write("test_unassoc.tif")
    rbuf = oiio.ImageBuf("test_unassoc.tif")
    print ("\n  default reading as IB: ", rbuf.get_pixels())
    config = oiio.ImageSpec()
    config.attribute("oiio:UnassociatedAlpha", 1)
    rbuf = oiio.ImageBuf("test_unassoc.tif", 0, 0, config)
    print ("\n  reading as IB with unassoc hint: ", rbuf.get_pixels())
    print ("\n  reading as II with hint, read scanlines backward: ")
    ii = oiio.ImageInput.open("test_unassoc.tif", config)
    print ("    [1] = ", ii.read_scanline(1))
    print ("    [0] = ", ii.read_scanline(0))
    print ("\n")

# Regression test for #2292: make sure that TIFF cmyk files with RGB
# translation, when read backwards, is ok.
def test_tiff_cmyk() :
    # Create a file that has unassociated alpha
    filename = "test_cmyk.tif"
    print ("Testing write and read of TIFF CMYK with auto RGB translation:")
    spec = oiio.ImageSpec(2,2,4,"uint8")
    spec.attribute("tiff:ColorSpace", "CMYK")
    spec.channelnames = ("C", "M", "Y", "K")
    wbuf = oiio.ImageBuf(spec)
    oiio.ImageBufAlgo.fill(wbuf, (0.5, 0.0, 0.0, 0.5))
    print ("  writing: ", wbuf.get_pixels())
    wbuf.write(filename)
    rbuf = oiio.ImageBuf(filename)
    print ("\n  default reading as IB: ", rbuf.get_pixels())
    config = oiio.ImageSpec()
    config.attribute("oiio:RawColor", 1)
    rbuf = oiio.ImageBuf(filename, 0, 0, config)
    print ("\n  reading as IB with rawcolor=1: ", rbuf.get_pixels())
    print ("\n  reading as II with rawcolor=0, read scanlines backward: ")
    ii = oiio.ImageInput.open(filename)
    print ("    [1] = ", ii.read_scanline(1))
    print ("    [0] = ", ii.read_scanline(0))
    print ("\n")




######################################################################
# main test starts here

try:
    # test basic opening and being able to read the spec
    poor_mans_iinfo ("badname.tif")
    poor_mans_iinfo (OIIO_TESTSUITE_IMAGEDIR + "/tahoe-gps.jpg")
    poor_mans_iinfo ("grid.tx")

    # test readimage
    print ("Testing read_image:")
    test_readimage (OIIO_TESTSUITE_IMAGEDIR + "/tahoe-gps.jpg")
    # again, force a float buffer
    test_readimage (OIIO_TESTSUITE_IMAGEDIR + "/tahoe-gps.jpg",
                    type=oiio.FLOAT)
    # Test read of partial channels
    test_readimage (OIIO_TESTSUITE_IMAGEDIR + "/tahoe-gps.jpg",
                    method="scanlines", nchannels=1)

    # test readscanline
    print ("Testing read_scanline:")
    test_readscanline (OIIO_TESTSUITE_IMAGEDIR + "/tahoe-gps.jpg")

    # test readtile
    print ("Testing read_tile:")
    test_readtile ("grid.tx")

    # test readscanlines
    print ("Testing read_scanlines:")
    test_readimage (OIIO_TESTSUITE_IMAGEDIR + "/tahoe-gps.jpg",
                    method="scanlines")

    # test readtiles
    print ("Testing read_tiles:")
    test_readimage ("grid.tx",
                    method="tiles")

    # test reading a raw buffer in native format, we should get back
    # an unsigned byte array.
    b = oiio.ImageBuf (oiio.ImageSpec(64, 64, 3, oiio.UINT16))
    oiio.ImageBufAlgo.fill (b, (1,0,0), (0,1,0), (0,0,1), (1,1,1))
    write (b, "testu16.tif", oiio.UINT16)
    b.set_write_tiles (32, 32)
    write (b, "testf16.exr", oiio.HALF)
    print ("Test read_image native u16:")
    test_readimage ("testu16.tif", method="image", type=oiio.UNKNOWN,
                    keep_unknown=True, print_pixels=False)
    print ("Test read_scanlines native u16:")
    test_readimage ("testu16.tif", method="scanlines", type=oiio.UNKNOWN,
                    keep_unknown=True, print_pixels=False)
    print ("Test read_tiles native half:")
    test_readimage ("testf16.exr", method="tiles", type=oiio.UNKNOWN,
                    keep_unknown=True, print_pixels=False)
    print ("Test read_image into half:")
    test_readimage ("testu16.tif", method="image", type=oiio.HALF,
                    keep_unknown=True, print_pixels=False)
    print ("Test read_image into FLOAT:")
    test_readimage ("testu16.tif", method="image", type="float",
                    keep_unknown=True, print_pixels=False)

    test_tiff_remembering_config()
    test_tiff_cmyk()

    # Test is_imageio_format_name
    print ("is_imageio_format_name('tiff') =", oiio.is_imageio_format_name('tiff'))
    print ("is_imageio_format_name('txff') =", oiio.is_imageio_format_name('txff'))

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)

