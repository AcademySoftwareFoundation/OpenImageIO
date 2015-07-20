#!/usr/bin/env python 

import array
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


def write (image, filename, format=oiio.UNKNOWN) :
    if not image.has_error :
        image.set_write_format (format)
        image.write (filename)
    if image.has_error :
        print "Error writing", filename, ":", image.geterror()



######################################################################
# main test starts here

try:

    print "Constructing to be a writeable 320x240,4 UINT16:"
    b = oiio.ImageBuf (oiio.ImageSpec(320,240,4,oiio.UINT16))
    print_imagespec (b.spec())
    print "Resetting to be a writeable 640x480,3 Float:"
    b.reset (oiio.ImageSpec(640,480,3,oiio.FLOAT))
    print_imagespec (b.spec())
    print ""

    # Test reading from disk
    print "Testing read of grid.tx:"
    b = oiio.ImageBuf ("../common/textures/grid.tx")
    print "subimage:", b.subimage, " / ", b.nsubimages
    print "miplevel:", b.miplevel, " / ", b.nmiplevels
    print "channels:", b.nchannels
    print "name:", b.name
    print "file_format_name:", b.file_format_name
    print "deep:", b.deep
    print "orientation:", b.orientation
    print "oriented x,y,width,height:", b.oriented_x, b.oriented_y, b.oriented_width, b.oriented_height
    print "oriented full x,y,width,height:", b.oriented_full_x, b.oriented_full_y, b.oriented_full_width, b.oriented_full_height
    print "xyz beg/end:", b.xbegin, b.xend, b.ybegin, b.yend, b.zbegin, b.zend
    print "xyz min/max:", b.xmin, b.xmax, b.ymin, b.ymax, b.zmin, b.zmax
    print "setting full res..."
    b.set_full (0, 2048, 0, 2048, 0, 1)
    print "roi =", b.roi
    print "full roi =", b.roi_full
    print "setting full roi again, as ROI..."
    b.roi_full = oiio.ROI(0, 1024, 0, 1024, 0, 1, 0, b.nchannels)
    print "Printing the whole spec to be sure:"
    print_imagespec (b.spec())
    print ""
    print "Resetting to a different MIP level:"
    b.reset ("../common/textures/grid.tx", 0, 2)
    print_imagespec (b.spec())
    print ""

    # Create a small buffer, do various pixel reads and writes
    print "Making 2x2 RGB image:"
    b = oiio.ImageBuf (oiio.ImageSpec(2,2,3,oiio.UINT8))
    print_imagespec (b.spec())
    b.setpixel (0, 0, 0, (1.0, 0.0, 0.0))
    b.setpixel (1, 0, 0, (0.0, 1.0, 0.0))
    b.setpixel (0, 1, 0, (0.0, 0.0, 1.0))
    b.setpixel (1, 1, 0, (0.0, 0.0, 0.0))
    print "Pixel 0,0 is", b.getpixel(0,0,0)
    print "Pixel 1,0 is", b.getpixel(1,0)   # test 2D lookup
    print "Pixel 0,1 is", b.getpixel(0,1)
    print "Interpolating 1,0.5 ->", b.interppixel(1,0.5)
    print "Interpolating NDC 0.25,0.5 ->", b.interppixel_NDC(0.25,0.5)
    print "Interpolating bicubic 0.25,0.5 ->", b.interppixel_bicubic(1.0,0.5)
    print "Interpolating NDC bicubic 0.25,0.5 ->", b.interppixel_bicubic_NDC(0.25,0.5)
    print "The whole image is: ", b.get_pixels(oiio.TypeDesc.TypeFloat)
    print ""
    print "Saving file..."
    b.write ("out.tif")

    # test set_pixels, too
    b.set_pixels (oiio.ROI(0, 2, 0, 2, 0, 1, 0, 3), (0.1,0.0,0.9, 0.2,0.0,0.7,
                                                     0.3,0.0,0.8, 0.4,0.0,0.6))
    b.write ("outtuple.tif")
    b.set_pixels (oiio.ROI(0, 2, 0, 2, 0, 1, 0, 3),
                  array.array('f',[0.1,0.5,0.9, 0.2,0.5,0.7, 0.3,0.5,0.8, 0.4,0.5,0.6]))
    b.write ("outarray.tif")
    b.set_pixels (oiio.ROI(0, 2, 0, 2, 0, 1, 0, 3),
                  array.array('B',[26,128,230, 51,128,178, 76,128,204, 102,128,153]))
    write (b, "outarrayB.tif", oiio.UINT8)
    b.set_pixels (oiio.ROI(0, 2, 0, 2, 0, 1, 0, 3),
                  array.array('H',[6554,32767,58982, 13107,32767,45874,
                                   19660,32767,52428, 26214,32767,39321]))
    write (b, "outarrayH.tif", oiio.UINT16)

    # Test write and read of deep data
    # Let's try writing one
    print "\nWriting deep buffer..."
    deepbufout_spec = oiio.ImageSpec (2, 2, 5, oiio.FLOAT)
    deepbufout_spec.channelnames = ("R", "G", "B", "A", "Z")
    deepbufout_spec.deep = True
    deepbufout = oiio.ImageBuf(deepbufout_spec)
    deepbufout.deepdata().set_samples (1, 2)
    deepbufout.deepdata().set_deep_value (1, 0, 0, 0.42)
    deepbufout.deepdata().set_deep_value (1, 4, 0, 42.0)
    deepbufout.deepdata().set_deep_value (1, 0, 1, 0.47)
    deepbufout.deepdata().set_deep_value (1, 4, 1, 43.0)
    deepbufout.write ("deepbuf.exr")
    # And read it back
    print "\nReading back deep buffer:"
    deepbufin = oiio.ImageBuf ("deepbuf.exr")
    deepbufin_spec = deepbufin.spec()
    dd = deepbufin.deepdata()
    for p in range(dd.pixels) :
        ns = dd.samples(p)
        if ns > 1 :
            print "Pixel", p/deepbufin_spec.width, p%deepbufin_spec.width, "had", ns, "samples"
            for s in range(ns) :
                print "Sample", s
                for c in range(dd.nchannels) :
                    print "\tc", c, ":", dd.deep_value(p,c,s)

    print "\nDone."
except Exception as detail:
    print "Unknown exception:", detail

