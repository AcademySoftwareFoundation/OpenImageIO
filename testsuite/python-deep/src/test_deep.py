#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import random
import OpenImageIO as oiio

test_xres = 3
test_yres = 3
test_nchannels = 6
test_chantypes = (oiio.TypeHalf, oiio.TypeHalf, oiio.TypeHalf, oiio.TypeHalf,
                  oiio.TypeFloat, oiio.TypeFloat)
test_channames = ("R", "G", "B", "A", "Z", "Zback")
print ("test_chantypes ", str(test_chantypes[0]), str(test_chantypes[1]), str(test_chantypes[2]), str(test_chantypes[3]), str(test_chantypes[4]), str(test_chantypes[5]))


# Make a simple deep image
# Only odd pixel indes have samples, and they have #samples = pixel index.
def make_test_deep_image () -> oiio.DeepData:
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



def print_deep_image (dd: oiio.DeepData, prefix: str = "After init,") :
    print (prefix, "dd has", dd.pixels, "pixels,", dd.channels, "channels.")
    print ("  Channel indices: Z=", dd.Z_channel, "Zback=", dd.Zback_channel, "A=", dd.A_channel, "AR=", dd.AR_channel, "AG=", dd.AG_channel, "AB=", dd.AB_channel)
    for p in range(dd.pixels) :
        ns = dd.samples(p)
        if ns > 0 or dd.capacity(p) > 0 :
            print ("  Nsamples[", p, "] =", ns, " (capacity=", dd.capacity(p), ")", "samples:")
            for s in range(ns) :
                print ("  sample", s, ": ", end='')
                for c in range(dd.channels) :
                    print (" [%d %s] %.2f / " % (c, dd.channelname(c), dd.deep_value (p, c, s)), end='')
                print ()


def print_deep_imagebuf (buf: oiio.ImageBuf, prefix: str) :
    print_deep_image (buf.deepdata(), prefix)


def test_insert_erase () :
    print ("\nTesting insert and erase...")
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
    print ("\nTesting copy_deep_pixel...")
    # Set up an image
    src = make_test_deep_image ()
    dst = make_test_deep_image ()
    dst.copy_deep_pixel (3, src, 5)
    dst.copy_deep_pixel (5, src, 3)
    print_deep_image (dst, "test_deep_copy: should swap pixels 3 and 5,")


def test_sample_split () :
    print ("\nTesting split...")
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
    print ("\nTesting sort...")
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
        # Make sample 0's Z tie with sample 1's so that sort falls back to Zback
        dd.set_deep_value (p, 4, 0, 19)     # sample 0, Z=19 (was 20)
    print_deep_image (dd, "Before z sort,")
    dd.sort (1)
    print_deep_image (dd, "After z sort of pixel 1,")


def test_merge_overlaps () :
    print ("\nTesting merge_overlaps...")
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
            dd.set_deep_value (p, 4, s, 10.0 + int(s//2))  # Z
            dd.set_deep_value (p, 5, s, 10.0 + int(s//2) + 0.5)  # Zback
    print_deep_image (dd, "Before merge_overlaps,")
    dd.merge_overlaps (1)
    print_deep_image (dd, "After merge_overlaps of pixel 1,")


def test_merge_deep_pixels () :
    print ("\nTesting merge_deep_pixels...")
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


def test_merge_mixed_depth_types () :
    print ("\nTesting merge_deep_pixels with half Z, float Zback...")
    # Splitting [1,2] at 1.0006 rounds the remainder's front past 1.0007, so
    # it must not then be split at 1.0007.
    types = (oiio.FLOAT, oiio.HALF, oiio.FLOAT)
    names = ("A", "Z", "Zback")
    dds = []
    for zbacks in ((2.0,), (1.0006, 1.0007)) :
        dd = oiio.DeepData ()
        dd.init (1, 3, types, names)
        dd.set_samples (0, len(zbacks))
        for s, zback in enumerate(zbacks) :
            dd.set_deep_value (0, 0, s, 0.5)
            dd.set_deep_value (0, 1, s, 1.0)
            dd.set_deep_value (0, 2, s, zback)
        dds.append (dd)
    dds[0].merge_deep_pixels (0, dds[1], 0)
    print ("nsamples =", dds[0].samples(0))
    for s in range(dds[0].samples(0)) :
        print ("  A={:.6f} Z={:.6f} Zback={:.6f}".format(
               *[dds[0].deep_value(0, c, s) for c in range(3)]))


def make_nested (zfront, zback, nsamples, fmt) :
    # One-pixel deep image with nsamples nested samples
    spec = oiio.ImageSpec (1, 1, 3, fmt)
    spec.channelnames = ("A", "Z", "Zback")
    spec.deep = True
    buf = oiio.ImageBuf (spec)
    buf.set_deep_samples (0, 0, 0, nsamples)
    eps = 0.5 * (zback - zfront) / (nsamples + 1)
    for s in range(nsamples) :
        buf.set_deep_value (0, 0, 0, 0, s, 0.5)
        buf.set_deep_value (0, 0, 0, 1, s, zfront + s * eps)
        buf.set_deep_value (0, 0, 0, 2, s, zback - s * eps)
    return buf


def test_deep_merge_limit () :
    print ("\nTesting deep_merge size limit...")
    oldlimit = oiio.get_int_attribute ("limits:imagesize_MB")
    oiio.attribute ("limits:imagesize_MB", 1)
    cascade = make_nested (0.0, 1.0, 300, "float")
    empty = make_nested (0.0, 1.0, 0, "float")
    for (label, A, B) in (("empty B", cascade, empty),
                          ("empty A", empty, cascade)) :
        dst = oiio.ImageBufAlgo.deep_merge (A, B)
        print (label, ":", dst.has_error, dst.deep_samples(0, 0, 0))
    # All depths become 1.0 when converted to half
    narrow = make_nested (1.0, 1.0004, 300, "float")
    dst = make_nested (0.0, 1.0, 0, "half")
    ok = oiio.ImageBufAlgo.deep_merge (dst, narrow, narrow)
    print ("float to half :", ok, dst.deep_samples(0, 0, 0))
    # ... but without conversion, it really is too big
    dst = oiio.ImageBufAlgo.deep_merge (narrow, narrow)
    print ("float to float : error", dst.has_error, dst.geterror()[:40])
    # Just under and just over the limit: n nested samples merged with
    # themselves make 2*n*n samples of 12 bytes each.
    for n in (209, 210) :
        A = make_nested (0.0, 1.0, n, "float")
        dst = oiio.ImageBufAlgo.deep_merge (A, A)
        print (n, "nested : error", dst.has_error)
        dst.geterror()
    # With a channel subset that excludes depth, A's depths are not copied
    A = make_nested (0.0, 1.0, 210, "float")
    dst = make_nested (0.0, 1.0, 0, "float")
    ok = oiio.ImageBufAlgo.deep_merge (dst, A, A,
                                       roi=oiio.ROI(0, 1, 0, 1, 0, 1, 0, 1))
    print ("channel subset :", ok, dst.geterror()[:40])
    oiio.attribute ("limits:imagesize_MB", oldlimit)


def make_random_deep (types, samples) :
    # One-pixel deep image with the given (alpha, z, zback) samples
    spec = oiio.ImageSpec (1, 1, 4, "float")
    spec.channelnames = ("R", "A", "Z", "Zback")
    spec.channelformats = types
    spec.deep = True
    buf = oiio.ImageBuf (spec)
    buf.set_deep_samples (0, 0, 0, len(samples))
    for s, (a, z, zback) in enumerate(samples) :
        for c, val in enumerate((0.5 * a, a, z, zback)) :
            buf.set_deep_value (0, 0, 0, c, s, val)
    return buf


def random_samples (rng, base, width) :
    samples = []
    for s in range(rng.choice((0, 1, 2, 5, 12))) :
        a = rng.choice((0.0, 0.1, 0.5, 1.0))
        z, zback = sorted((base + width * rng.random(),
                           base + width * rng.random()))
        kind = rng.randrange(5)
        if kind == 0 :               # zero thickness
            zback = z
        elif kind == 1 and samples : # same extent as another sample
            z, zback = samples[-1][1:]
        samples.append ((a, z, zback))
    return samples


def check_random_merge (rng, srctypes, dsttypes) :
    # Merge two random pixels, return a list of any problems found.
    base = rng.choice((0.25, 0.5))
    width = rng.choice((0.0002, 0.001, 0.01, 0.4))
    Asamps = random_samples (rng, base, width)
    Bsamps = random_samples (rng, base, width)
    A = make_random_deep (srctypes, Asamps)
    B = make_random_deep (srctypes, Bsamps)
    dst = make_random_deep (dsttypes, [])
    if not oiio.ImageBufAlgo.deep_merge (dst, A, B, occlusion_cull=False) :
        return ["failed: " + dst.geterror()]
    problems = []
    expected = 1.0
    for samp in Asamps + Bsamps :
        expected *= 1.0 - samp[0]
    transmittance = 1.0
    for s in range(dst.deep_samples(0, 0, 0)) :
        r = dst.deep_value (0, 0, 0, 0, s)
        a = dst.deep_value (0, 0, 0, 1, s)
        if not (0.0 <= a <= 1.0) :
            problems.append ("alpha {}".format(a))
        if not abs(r - 0.5 * a) < 1.0e-4 :
            problems.append ("color {} for alpha {}".format(r, a))
        transmittance *= 1.0 - a
    if not abs(transmittance - expected) < 1.0e-4 :
        problems.append ("transmittance {} vs {}".format(transmittance,
                                                         expected))
    # deep_merge's capacity estimate must match what the merge really needs.
    if Asamps and Bsamps :
        direct = oiio.DeepData ()
        direct.init (1, 4, dsttypes, ("R", "A", "Z", "Zback"))
        direct.copy_deep_pixel (0, A.deepdata(), 0)
        direct.merge_deep_pixels (0, B.deepdata(), 0)
        needed = direct.capacity(0)
    else :
        needed = len(Asamps) + len(Bsamps)
    if dst.deepdata().capacity(0) != needed :
        problems.append ("capacity {} vs {}".format(dst.deepdata().capacity(0),
                                                    needed))
    return problems


def test_deep_merge_random () :
    print ("\nTesting deep_merge of random pixels...")
    rng = random.Random (1)
    F, H, U = oiio.FLOAT, oiio.HALF, oiio.UINT16
    nproblems = 0
    for zt in (F, H, U) :
        for zbt in (F, H, U) :
            dsttypes = (F, F, zt, zbt)
            for srctypes in (dsttypes, (F, F, F, F)) :
                for i in range(100) :
                    for p in check_random_merge (rng, srctypes, dsttypes) :
                        nproblems += 1
                        if nproblems <= 10 :
                            print ("  ", srctypes, "->", dsttypes, ":", p)
    print ("problems:", nproblems)


def test_occlusion_cull () :
    print ("\nTesting occlusion_cull...")
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

def test_opaque_z () :
    print ("\nTesting opaque_z...")
    dd = oiio.DeepData ()
    # 3 test pixels
    dd.init (3, test_nchannels, test_chantypes, test_channames)
    # First pixel: has 3 samples, middle one is opaque
    dd.set_samples (0, 3)
    for s in range(dd.samples(0)) :
        dd.set_deep_value (0, 0, s, 0.5) # R
        dd.set_deep_value (0, 1, s, 0.0) # G
        dd.set_deep_value (0, 2, s, 0.0) # B
        dd.set_deep_value (0, 3, s, (1.0 if s==1 else 0.5)) # A
        dd.set_deep_value (0, 4, s, 10.0+s) # Z
        dd.set_deep_value (0, 5, s, 10.5+s) # Zback
    # Second pixel: 3 samples, none are opaque
    dd.set_samples (1, 3)
    for s in range(dd.samples(0)) :
        dd.set_deep_value (1, 0, s, 0.5) # R
        dd.set_deep_value (1, 1, s, 0.0) # G
        dd.set_deep_value (1, 2, s, 0.0) # B
        dd.set_deep_value (1, 3, s, 0.5) # A
        dd.set_deep_value (1, 4, s, 10.0+s) # Z
        dd.set_deep_value (1, 5, s, 10.5+s) # Zback
    # Third pixel is empty
    print_deep_image (dd, "Values")
    print ("Opaque z: ", end='')
    for p in range(dd.pixels) :
        print (' %.6g' % dd.opaque_z(p), end='')
    print ()


def set_ib_sample (ib: oiio.ImageBuf, x: int, y: int, sample: int, vals: tuple[float, ...]) :
    if ib.deep_samples(x, y) <= sample :
        ib.set_deep_samples (x, y, 0, sample+1)
    for c in range(ib.nchannels) :
        ib.set_deep_value (x, y, 0, c, sample, vals[c])

def add_ib_sample (ib: oiio.ImageBuf, x: int, y: int, vals: tuple[float, ...]) :
    set_ib_sample (ib, x, y, ib.deep_samples(x,y), vals)


def test_iba_deep_holdout () :
    print ("\nTesting ImageBufAlgo.deep_holdout...")
    spec = oiio.ImageSpec (6, 1, 6, oiio.FLOAT)
    spec.deep = True
    spec.channelformats = (oiio.TypeHalf, oiio.TypeHalf, oiio.TypeHalf,
                           oiio.TypeHalf, oiio.TypeFloat, oiio.TypeFloat)
    spec.channelnames = ("R", "G", "B", "A", "Z", "Zback")
    src = oiio.ImageBuf (spec)
    # Set up source image
    #   pixel 0: empty
    #   pixel 1: one sample close
    add_ib_sample (src, 1, 0, (0.5, 0.0, 0.0, 0.75, 10.0, 10.5))
    #   pixel 2: one sample far
    add_ib_sample (src, 2, 0, (0.5, 0.0, 0.0, 0.75, 20.0, 20.5))
    #   pixel 3: one sample close, one far
    add_ib_sample (src, 3, 0, (0.5, 0.0, 0.0, 0.75, 10.0, 10.5))
    add_ib_sample (src, 3, 0, (0.5, 0.0, 0.0, 0.75, 20.0, 20.5))
    #   pixel 4: three samples, one spans the threshold
    add_ib_sample (src, 4, 0, (0.5, 0.0, 0.0, 0.75, 10.0, 10.5))
    add_ib_sample (src, 4, 0, (0.5, 0.0, 0.0, 0.75, 15.0, 16.0))
    add_ib_sample (src, 4, 0, (0.5, 0.0, 0.0, 0.75, 20.0, 20.5))
    print_deep_imagebuf (src, "Input image")

    # Set up holdout image: depth increases left to right
    hold = oiio.ImageBuf (spec)
    for x in range(6) :
        add_ib_sample (hold, x, 0, (0, 0, 0, 0.5, 12.0, 12.5))
        add_ib_sample (hold, x, 0, (0, 0, 0, 1.0, 15.0+0.1*x, 15.0+0.1*x+0.1))
    print_deep_imagebuf (hold, "Holdout image")
    result = oiio.ImageBuf()
    oiio.ImageBufAlgo.deep_holdout (result, src, hold)
    print_deep_imagebuf (result, "Result after holdout")



######################################################################
# main test starts here

try:
    # Make a deep test image and print info about it (tests DeepData)
    dd = make_test_deep_image ()
    print_deep_image (dd)

    # Try to write the test image to an exr file
    print ("\nWriting image...")
    spec = oiio.ImageSpec (test_xres, test_yres, test_nchannels, oiio.TypeFloat)
    spec.channelnames = test_channames
    spec.channelformats = test_chantypes
    spec.deep = True
    output = oiio.ImageOutput.create ("deeptest.exr")
    assert output is not None
    output.open ("deeptest.exr", spec, "create")
    output.write_deep_image (dd)
    output.close ()

    # read the exr file and double check it
    print ("\nReading image...")
    input = oiio.ImageInput.open ("deeptest.exr")
    assert input is not None
    ddr = input.read_native_deep_image ()
    if ddr :
        print_deep_image (ddr)
    # Flat (non-deep) reads of a deep image must fail cleanly
    print ("flat read_image:", input.read_image(), input.geterror())
    print ("flat read_scanlines:", input.read_scanlines(0, 0, 0, 1, 0, 0, 6),
           input.geterror())
    ic = oiio.ImageCache()
    print ("ImageCache get_pixels:",
           ic.get_pixels("deeptest.exr", 0, 0, 0, 3, 0, 3, 0, 1, "float"),
           ic.geterror())

    test_insert_erase ()
    test_deep_copy ()
    test_sample_split ()
    test_sample_sort ()
    test_merge_overlaps ()
    test_merge_deep_pixels ()
    test_merge_mixed_depth_types ()
    test_deep_merge_limit ()
    test_deep_merge_random ()
    test_occlusion_cull ()
    test_opaque_z ()

    test_iba_deep_holdout ();

    print ("\nDone.")

except Exception as detail:
    print ("Unknown exception:", detail)

