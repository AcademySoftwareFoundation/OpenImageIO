#!/usr/bin/env python 

import OpenImageIO as oiio

test_xres = 3
test_yres = 3
test_nchannels = 6
test_chantypes = (oiio.TypeDesc.TypeHalf, oiio.TypeDesc.TypeHalf,
                  oiio.TypeDesc.TypeHalf, oiio.TypeDesc.TypeHalf,
                  oiio.TypeDesc.TypeFloat, oiio.TypeDesc.TypeFloat)
test_channames = ("R", "G", "B", "A", "Z", "Zback")
print "test_chantypes ", str(test_chantypes[0]), str(test_chantypes[1]), str(test_chantypes[2]), str(test_chantypes[3]), str(test_chantypes[4]), str(test_chantypes[5])

# Make a simple deep image
# Only odd pixel indes have samples, and they have #samples = pixel index.
def make_test_deep_image () :
    dd = oiio.DeepData()
    dd.init (test_xres*test_yres, test_nchannels, test_chantypes, test_channames)
    for p in range(dd.pixels) :
        if p&1 :
            dd.set_samples (p, p)
    for p in range(dd.pixels) :
        ns = dd.samples(p)
        for s in range(ns) :
            # pixels alternate R, G, B. Increasing alpha with each sample
            # in the pixel, with the last being opaque. Z increases with
            # each sample (staring at 10.0, + 1.0 for each sample), Zback
            # is 0.5 past Z.
            alpha = float(s+1)/ns
            r = 1.0 if (p % 3) == 0 else 0.0
            g = 1.0 if (p % 3) == 1 else 0.0
            b = 1.0 if (p % 3) == 2 else 0.0
            dd.set_deep_value (p, 0, s, alpha * r)   # R
            dd.set_deep_value (p, 1, s, alpha * g)   # G
            dd.set_deep_value (p, 2, s, alpha * b)   # B
            dd.set_deep_value (p, 3, s, alpha)       # A
            dd.set_deep_value (p, 4, s, 10.0+s)      # Z
            dd.set_deep_value (p, 5, s, 10.0+s+0.5)  # Zback
    return dd



def print_deep_image (dd, prefix="After init,") :
    print prefix, "dd has", dd.pixels, "pixels,", dd.channels, "channels."
    for p in range(dd.pixels) :
        ns = dd.samples(p)
        if ns > 0 or dd.capacity(p) > 0 :
            print "  Nsamples[", p, "] =", ns, " (capacity=", dd.capacity(p), ")", "samples:"
            for s in range(ns) :
                print "  sample", s, ": ",
                for c in range(dd.channels) :
                    print "[%d %s] %.2f / " % (c, dd.channelname(c), dd.deep_value (p, c, s)),
                print


def test_insert_erase () :
    print "\nTesting insert and erase..."
    dd = oiio.DeepData ()
    dd.init (3, test_nchannels, test_chantypes, test_channames)
    dd.set_samples (1, 1)
    dd.set_deep_value (1, 4, 0, 10.0)
    print_deep_image (dd, "After setting one sample:")
    dd.insert_samples (1, 0, 1)
    dd.set_deep_value (1, 4, 0, 9.0)
    dd.insert_samples (1, 2, 1)
    dd.set_deep_value (1, 4, 2, 11.0)
    print_deep_image (dd, "After inserting before and after:")
    dd.erase_samples (1, 1, 1)
    print_deep_image (dd, "After deleting the middle:")


def test_deep_copy () :
    print "\nTesting copy_deep_pixel..."
    # Set up an image
    src = make_test_deep_image ()
    dst = make_test_deep_image ()
    dst.copy_deep_pixel (3, src, 5)
    dst.copy_deep_pixel (5, src, 3)
    print_deep_image (dst, "test_deep_copy: should swap pixels 3 and 5,")


def test_sample_split () :
    print "\nTesting split..."
    # Set up a simple 3-pixel image
    dd = oiio.DeepData ()
    dd.init (2, test_nchannels, test_chantypes, test_channames)
    for p in range(dd.pixels) :
        dd.set_samples (p, 2)
        # first sample - reddish
        dd.set_deep_value (p, 0, 0, 0.5)   # R
        dd.set_deep_value (p, 1, 0, 0.1)   # G
        dd.set_deep_value (p, 2, 0, 0.1)   # B
        dd.set_deep_value (p, 3, 0, 0.5)   # A
        dd.set_deep_value (p, 4, 0, 10.0)  # Z
        dd.set_deep_value (p, 5, 0, 11.0)  # Zback
        # second sample - greenish
        dd.set_deep_value (p, 0, 1, 0.1)   # R
        dd.set_deep_value (p, 1, 1, 0.5)   # G
        dd.set_deep_value (p, 2, 1, 0.1)   # B
        dd.set_deep_value (p, 3, 1, 0.5)   # A
        dd.set_deep_value (p, 4, 1, 20.0)  # Z
        dd.set_deep_value (p, 5, 1, 21.0)  # Zback
    # Now do a few splits of the second pixel
    dd.split (1, 0.5)    # close, doesn't split - should have no effect
    dd.split (1, 100.5)  # far, doesn't split - should have no effect
    dd.split (1, 15.0)   # still doesn't split - should have no effect
    dd.split (1, 20.0)   # Right on an edge -- should have no effect
    dd.split (1, 20.5)   # THIS one should split
    print_deep_image (dd, "After split,")


def test_sample_sort () :
    print "\nTesting sort..."
    # Set up a simple 2-pixel image with 4 samples
    dd = oiio.DeepData ()
    dd.init (2, test_nchannels, test_chantypes, test_channames)
    for p in range(dd.pixels) :
        dd.set_samples (p, 4)
        for s in range(dd.samples(p)) :
            dd.set_deep_value (p, 0, s, 0.1*s) # R
            dd.set_deep_value (p, 1, s, 0.0)   # G
            dd.set_deep_value (p, 2, s, 0.0)   # B
            dd.set_deep_value (p, 3, s, 0.5)   # A
            dd.set_deep_value (p, 4, s, 20.0 - s)  # Z: decreasing!
            dd.set_deep_value (p, 5, s, 20.0 - s + 0.5)  # Zback
    print_deep_image (dd, "Before z sort,")
    dd.sort (1)
    print_deep_image (dd, "After z sort of pixel 1,")


def test_merge_overlaps () :
    print "\nTesting merge_overlaps..."
    # Set up a simple 2-pixel image with 4 samples
    dd = oiio.DeepData ()
    dd.init (2, test_nchannels, test_chantypes, test_channames)
    for p in range(dd.pixels) :
        dd.set_samples (p, 4)
        for s in range(dd.samples(p)) :
            dd.set_deep_value (p, 0, s, 0.1*s) # R
            dd.set_deep_value (p, 1, s, 0.0)   # G
            dd.set_deep_value (p, 2, s, 0.0)   # B
            dd.set_deep_value (p, 3, s, 0.5)   # A
            # Make adjacent pairs overlap exactly
            dd.set_deep_value (p, 4, s, 10.0 + int(s/2))  # Z
            dd.set_deep_value (p, 5, s, 10.0 + int(s/2) + 0.5)  # Zback
    print_deep_image (dd, "Before merge_overlaps,")
    dd.merge_overlaps (1)
    print_deep_image (dd, "After merge_overlaps of pixel 1,")


def test_merge_deep_pixels () :
    print "\nTesting merge_deep_pixels..."
    # Set up two simple 1-pixel images with overlapping samples
    Add = oiio.DeepData ()
    Add.init (1, test_nchannels, test_chantypes, test_channames)
    Add.set_samples (0, 1)
    Add.set_deep_value (0, 0, 0, 0.5) # R
    Add.set_deep_value (0, 1, 0, 0.0) # G
    Add.set_deep_value (0, 2, 0, 0.0) # B
    Add.set_deep_value (0, 3, 0, 0.5) # A
    Add.set_deep_value (0, 4, 0, 10.0) # Z
    Add.set_deep_value (0, 5, 0, 12.0) # Zback
    Bdd = oiio.DeepData ()
    Bdd.init (1, test_nchannels, test_chantypes, test_channames)
    Bdd.set_samples (0, 1)
    Bdd.set_deep_value (0, 0, 0, 0.5) # R
    Bdd.set_deep_value (0, 1, 0, 0.0) # G
    Bdd.set_deep_value (0, 2, 0, 0.0) # B
    Bdd.set_deep_value (0, 3, 0, 0.5) # A
    Bdd.set_deep_value (0, 4, 0, 11.0) # Z
    Bdd.set_deep_value (0, 5, 0, 13.0) # Zback
    print_deep_image (Add, "Before merge_deep_pixels,")
    print_deep_image (Bdd, "And the other image,")
    Add.merge_deep_pixels (0, Bdd, 0)
    print_deep_image (Add, "After merge_deep_pixels,")


def test_occlusion_cull () :
    print "\nTesting occlusion_cull..."
    dd = oiio.DeepData ()
    dd.init (1, test_nchannels, test_chantypes, test_channames)
    dd.set_samples (0, 3)
    for s in range(dd.samples(0)) :
        dd.set_deep_value (0, 0, s, 0.5) # R
        dd.set_deep_value (0, 1, s, 0.0) # G
        dd.set_deep_value (0, 2, s, 0.0) # B
        dd.set_deep_value (0, 3, s, (1.0 if s==1 else 0.5)) # A
        dd.set_deep_value (0, 4, s, 10.0+s) # Z
        dd.set_deep_value (0, 5, s, 10.5+s) # Zback
    print_deep_image (dd, "Before occlusion_cull,")
    dd.occlusion_cull (0)
    print_deep_image (dd, "After occlusion_cull,")



######################################################################
# main test starts here

try:
    # Make a deep test image and print info about it (tests DeepData)
    dd = make_test_deep_image ()
    print_deep_image (dd)

    # Try to write the test image to an exr file
    print "\nWriting image..."
    spec = oiio.ImageSpec (test_xres, test_yres, test_nchannels, oiio.TypeDesc.TypeFloat)
    spec.channelnames = test_channames
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

    test_insert_erase ()
    test_deep_copy ()
    test_sample_split ()
    test_sample_sort ()
    test_merge_overlaps ()
    test_merge_deep_pixels ()
    test_occlusion_cull ()

    print "\nDone."

except Exception as detail:
    print "Unknown exception:", detail

