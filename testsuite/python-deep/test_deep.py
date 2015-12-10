#!/usr/bin/env python 

import OpenImageIO as oiio

test_xres = 3
test_yres = 3
test_nchannels = 4
test_chantypes = (oiio.TypeDesc.TypeHalf, oiio.TypeDesc.TypeHalf,
                  oiio.TypeDesc.TypeHalf, oiio.TypeDesc.TypeFloat)
print "test_chantypes ", str(test_chantypes[0]), str(test_chantypes[1]), str(test_chantypes[2]), str(test_chantypes[3])

# Make a simple deep image
# Only odd pixel indes have samples, and they have #samples = pixel index.
def make_test_deep_image () :
    dd = oiio.DeepData()
    dd.init (test_xres*test_yres, test_nchannels, test_chantypes)
    for p in range(dd.pixels) :
        if p&1 :
            dd.set_samples (p, p)
    dd.alloc()
    for p in range(dd.pixels) :
        ns = dd.samples(p)
        for s in range(ns) :
            for c in range(dd.channels) :
                dd.set_deep_value (p, c, s, c*10+s+p/10.0)
    return dd



def print_deep_image (dd) :
    print "After init, dd has", dd.pixels, "pixels,", dd.channels, "channels."
    for p in range(dd.pixels) :
        ns = dd.samples(p)
        if ns > 0 :
            print "  Nsamples[", p, "] =", ns, "samples:"
            for s in range(ns) :
                print "  sample", s, ": ",
                for c in range(dd.channels) :
                    print "[%d] %.2f / " % (c, dd.deep_value (p, c, s)),
                print




######################################################################
# main test starts here

try:
    # Make a deep test image and print info about it (tests DeepData)
    dd = make_test_deep_image ()
    print_deep_image (dd)

    # Try to write the test image to an exr file
    print "\nWriting image..."
    spec = oiio.ImageSpec (test_xres, test_yres, test_nchannels, oiio.TypeDesc.TypeFloat)
    spec.channelformats = test_chantypes
    spec.deep = True
    output = oiio.ImageOutput.create ("deeptest.exr")
    output.open ("deeptest.exr", spec, oiio.Create)
    output.write_deep_image (dd)
    output.close ()

    # read the exr file and double check it
    print "\nReading image..."
    input = oiio.ImageInput.open ("deeptest.exr")
    ddr = input.read_native_deep_image ()
    if ddr != None :
        print_deep_image (ddr)

    print "\nDone."

except Exception as detail:
    print "Unknown exception:", detail

