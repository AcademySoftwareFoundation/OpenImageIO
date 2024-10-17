#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import array
import numpy
import OpenImageIO as oiio


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
    for attrib in spec.extra_attribs :
        if type(attrib.value) == str :
            print (" ", attrib.name, "= \"" + attrib.value + "\"")
        else :
            print (" ", attrib.name, "=", attrib.value)
    # Equivalent, using indexing rather than iterating:
    #for i in range(len(spec.extra_attribs)) :
    #    if type(spec.extra_attribs[i].value) == str :
    #        print (" ", spec.extra_attribs[i].name, "= \"" + spec.extra_attribs[i].value + "\"")
    #    else :
    #        print (" ", spec.extra_attribs[i].name, "=", spec.extra_attribs[i].value)


def write (image, filename, format=oiio.UNKNOWN) :
    if not image.has_error :
        image.write (filename, format)
    if image.has_error :
        print ("Error writing", filename, ":", image.geterror())



def test_perchannel_formats () :
    # Test writing per-channel formats with an ImageBuf
    b = oiio.ImageBuf(oiio.ImageSpec(2,2,4,"float"))
    oiio.ImageBufAlgo.fill(b, (0.1, 0.2, 0.3, 0.4))
    b.set_write_format (("half", "half", "half", "float"))
    b.write ("perchan.exr")



def test_deep () :
    # Test write and read of deep data
    # Let's try writing one
    print ("\nWriting deep buffer...")
    deepbufout_spec = oiio.ImageSpec (2, 2, 5, oiio.FLOAT)
    deepbufout_spec.channelnames = ("R", "G", "B", "A", "Z")
    deepbufout_spec.deep = True
    deepbufout = oiio.ImageBuf(deepbufout_spec)
    deepbufout.set_deep_samples (x=1, y=0, z=0, nsamples=2)
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=0, sample=0, value=0.42)
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=4, sample=0, value=42.0)
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=0, sample=1, value=0.47)
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=4, sample=1, value=43.0)
    # Also insert some new samples
    deepbufout.deep_insert_samples (x=1, y=0, z=0, samplepos=1, nsamples=2);
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=0, sample=1, value=1.1);
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=1, sample=1, value=2.2);
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=2, sample=1, value=2.3);
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=3, sample=1, value=1.0);
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=3, sample=1, value=42.25);
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=0, sample=2, value=0.1);
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=1, sample=2, value=0.2);
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=2, sample=2, value=0.3);
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=3, sample=2, value=1.0);
    deepbufout.set_deep_value (x=1, y=0, z=0, channel=3, sample=2, value=42.5);
    # But delete the first one
    deepbufout.deep_erase_samples (x=1, y=0, z=0, samplepos=1, nsamples=1);
    # Save
    deepbufout.write ("deepbuf.exr")
    # And read it back
    print ("\nReading back deep buffer:")
    deepbufin = oiio.ImageBuf ("deepbuf.exr")
    deepbufin_spec = deepbufin.spec()
    dd = deepbufin.deepdata()
    for p in range(dd.pixels) :
        ns = dd.samples(p)
        if ns > 1 :
            print ("Pixel", p//deepbufin_spec.width, p%deepbufin_spec.width, "had", ns, "samples")
            for s in range(ns) :
                print ("Sample", s)
                for c in range(dd.channels) :
                    print ("\tc {0} : {1:.3f}".format(c, dd.deep_value(p,c,s)))



# Test ability to write multiple subimages to a file.
# Also tests ImageBuf.write(ImageOutput)
def test_multiimage () :
    print ("Writing multi-image file")
    spec = oiio.ImageSpec (128, 64, 3, "float")
    out = oiio.ImageOutput.create ("multipart.exr")
    # Open with intent to write two subimages, each with same spec
    if not out.open ("multipart.exr", (spec, spec)) :
        print ("Error on initial open:", out.geterror())
        return
    img = oiio.ImageBufAlgo.fill ((0.5, 0.0, 0.0), spec.roi)
    if not img.write (out) :
        print ("Error on write:", img.geterror())
        return
    if not out.open ("multipart.exr", spec, "AppendSubimage") :
        print ("Error on open for append:", out.geterror())
        return
    img = oiio.ImageBufAlgo.fill ((0.0, 0.5, 0.0), spec.roi)
    if not img.write (out) :
        print ("Error on write:", img.geterror())
        return
    out.close ()


# Test the functionality of uninitialized ImageBufs
def test_uninitialized () :
    print ("Testing uninitialized bufs")
    empty = oiio.ImageBuf()
    print ("  empty nchannels:", empty.nchannels)


# Print floating point tuple contents with slightly less than full precision
# in order to mask LSB differences between platforms.
def ftupstr(tup) :
    return "(" + ", ".join(["{:.5}".format(x) for x in tup]) + ")"



######################################################################
# main test starts here

try:

    print ("Constructing to be a writable 320x240,4 UINT16:")
    b = oiio.ImageBuf (oiio.ImageSpec(320,240,4,oiio.UINT16))
    print_imagespec (b.spec())
    print ("Resetting to be a writable 640x480,3 Float:")
    b.reset (oiio.ImageSpec(640,480,3,oiio.FLOAT))
    print_imagespec (b.spec())

    print ("Constructing from a bare numpy array:")
    b = oiio.ImageBuf(numpy.array([[[0.1,0.0,0.9,1.0], [0.2,0.0,0.7,1.0]],
                                   [[0.3,0.0,0.8,1.0], [0.4,0.0,0.6,1.0]],
                                   [[0.5,0.0,0.7,1.0], [0.6,0.0,0.5,1.0]]], dtype="f"))
    print (" from 3D, shape is", b.spec().format, b.roi)
    # should be width=2, height=3, channels=4, format=FLOAT
    print_imagespec (b.spec())
    print ("  pixel (0,1) = {:.3g} {:.3g} {:.3g} {:.3g}".format(b.getpixel (0,1)[0],
           b.getpixel (0,1)[1], b.getpixel (0,1)[2], b.getpixel(0,1)[3]))
    print ("")
    b = oiio.ImageBuf(numpy.array([[1.0, 0.5],
                                   [0.25, 0.125],
                                   [1.0, 0.5]], dtype="uint8"))
    print (" from 2D uint8, shape is", b.spec().format, b.roi)
    print ("")
    b = oiio.ImageBuf(numpy.array([[[[0.1,0.0,0.9,1.0], [0.2,0.0,0.7,1.0]],
                                   [[0.3,0.0,0.8,1.0], [0.4,0.0,0.6,1.0]] ],
                                  [[[0.3,0.0,0.8,1.0], [0.4,0.0,0.6,1.0]],
                                   [[0.5,0.0,0.7,1.0], [0.6,0.0,0.5,1.0]]]], dtype="f"))
    print (" from 4D, shape is", b.spec().format, b.roi)
    print ("")

    # Test reading from disk
    print ("Testing read of ../common/textures/grid.tx:")
    b = oiio.ImageBuf ("../common/textures/grid.tx")
    b.init_spec ("../common/textures/grid.tx")
    b.read ()
    if b.nsubimages != 0:
        print ("subimage:", b.subimage, " / ", b.nsubimages)
    if b.nmiplevels != 0:
        print ("miplevel:", b.miplevel, " / ", b.nmiplevels)
    print ("channels:", b.nchannels)
    print ("name:", b.name)
    print ("file_format_name:", b.file_format_name)
    print ("deep:", b.deep)
    print ("orientation:", b.orientation)
    print ("oriented x,y,width,height:", b.oriented_x, b.oriented_y, b.oriented_width, b.oriented_height)
    print ("oriented full x,y,width,height:", b.oriented_full_x, b.oriented_full_y, b.oriented_full_width, b.oriented_full_height)
    print ("xyz beg/end:", b.xbegin, b.xend, b.ybegin, b.yend, b.zbegin, b.zend)
    print ("xyz min/max:", b.xmin, b.xmax, b.ymin, b.ymax, b.zmin, b.zmax)
    print ("setting full res...")
    b.set_full (0, 2048, 0, 2048, 0, 1)
    print ("roi =", b.roi)
    print ("full roi =", b.roi_full)
    print ("setting full roi again, as ROI...")
    b.roi_full = oiio.ROI(0, 1024, 0, 1024, 0, 1, 0, b.nchannels)
    print ("Changing origin...")
    b.set_origin (15, 20);
    print ("Printing the whole spec to be sure:")
    print_imagespec (b.spec())
    print ("")
    print ("Resetting to a different MIP level:")
    b.reset ("../common/textures/grid.tx", 0, 2)
    print_imagespec (b.spec())
    print ("")

    # Create a small buffer, do various pixel reads and writes
    print ("Making 2x2 RGB image:")
    b = oiio.ImageBuf (oiio.ImageSpec(2,2,3,oiio.UINT8))
    print_imagespec (b.spec())
    b.setpixel (0, 0, 0, (1.0, 0.0, 0.0))
    b.setpixel (1, 0, 0, (0.0, 1.0, 0.0))
    b.setpixel (0, 1, 0, (0.0, 0.0, 1.0))
    b.setpixel (1, 1, 0, (0.0, 0.0, 0.0))
    print ("Pixel 0,0 is", b.getpixel(0,0,0))
    print ("Pixel 1,0 is", b.getpixel(1,0))   # test 2D lookup
    print ("Pixel 0,1 is", b.getpixel(0,1))
    print ("Interpolating 1,0.5 ->", ftupstr(b.interppixel(1,0.5)))
    print ("Interpolating NDC 0.25,0.5 ->", ftupstr(b.interppixel_NDC(0.25,0.5)))
    print ("Interpolating bicubic 0.25,0.5 ->", ftupstr(b.interppixel_bicubic(1.0,0.5)))
    print ("Interpolating NDC bicubic 0.25,0.5 ->", ftupstr(b.interppixel_bicubic_NDC(0.25,0.5)))
    print ("The whole image is: ", b.get_pixels(oiio.TypeFloat))
    print ("")
    print ("Saving file...")
    b.write ("out.tif")

    # test set_pixels, too
    b.set_pixels (oiio.ROI(0, 2, 0, 2, 0, 1, 0, 3),
                  numpy.array([[[0.1,0.0,0.9], [0.2,0.0,0.7]],
                               [[0.3,0.0,0.8], [0.4,0.0,0.6]]], dtype="f"))
    b.write ("outtuple.tif")
    b.set_pixels (oiio.ROI(0, 2, 0, 2, 0, 1, 0, 3),
                  numpy.array([0.1,0.5,0.9, 0.2,0.5,0.7, 0.3,0.5,0.8, 0.4,0.5,0.6], dtype='f'))
    b.write ("outarray.tif")
    b.set_pixels (oiio.ROI(0, 2, 0, 2, 0, 1, 0, 3),
                  numpy.array([26,128,230, 51,128,178, 76,128,204, 102,128,153], dtype='B'))
    write (b, "outarrayB.tif", oiio.UINT8)
    b.set_pixels (oiio.ROI(0, 2, 0, 2, 0, 1, 0, 3),
                  numpy.array([6554,32767,58982, 13107,32767,45874,
                                   19660,32767,52428, 26214,32767,39321], dtype='H'))
    write (b, "outarrayH.tif", oiio.UINT16)

    test_perchannel_formats ()
    test_deep ()
    test_multiimage ()
    test_uninitialized ()

    print ("\nDone.")
except Exception as detail:
    print ("Unknown exception:", detail)

