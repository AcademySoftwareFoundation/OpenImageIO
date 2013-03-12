/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

// Based on the sample at:
// http://code.google.com/p/googletest/wiki/GoogleTestPrimer#Writing_the_main()_Function

#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "sysutil.h"
#include "unittest.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <cstdio>

OIIO_NAMESPACE_USING;


// Test ImageBuf::zero and ImageBuf::fill
void test_zero_fill ()
{
    std::cout << "test zero_fill\n";
    const int WIDTH = 8;
    const int HEIGHT = 6;
    const int CHANNELS = 4;
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    spec.alpha_channel = 3;

    // Create a buffer -- pixels should be undefined
    ImageBuf A ("A", spec);
    
    // Set a pixel to an odd value, make sure it takes
    const float arbitrary1[CHANNELS] = { 0.2, 0.3, 0.4, 0.5 };
    A.setpixel (1, 1, arbitrary1);
    float pixel[CHANNELS];   // test pixel
    A.getpixel (1, 1, pixel);
    for (int c = 0;  c < CHANNELS;  ++c)
        OIIO_CHECK_EQUAL (pixel[c], arbitrary1[c]);

    // Zero out and test that it worked
    ImageBufAlgo::zero (A);
    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            A.getpixel (i, j, pixel);
            for (int c = 0;  c < CHANNELS;  ++c)
                OIIO_CHECK_EQUAL (pixel[c], 0.0f);
        }
    }

    // Test fill of whole image
    const float arbitrary2[CHANNELS] = { 0.6, 0.7, 0.3, 0.9 };
    ImageBufAlgo::fill (A, arbitrary2);
    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            A.getpixel (i, j, pixel);
            for (int c = 0;  c < CHANNELS;  ++c)
                OIIO_CHECK_EQUAL (pixel[c], arbitrary2[c]);
        }
    }

    // Test fill of partial image
    const float arbitrary3[CHANNELS] = { 0.42, 0.43, 0.44, 0.45 };
    {
        const int xbegin = 3, xend = 5, ybegin = 0, yend = 4;
        ImageBufAlgo::fill (A, arbitrary3, ROI(xbegin, xend, ybegin, yend));
        for (int j = 0;  j < HEIGHT;  ++j) {
            for (int i = 0;  i < WIDTH;  ++i) {
                float pixel[CHANNELS];
                A.getpixel (i, j, pixel);
                if (j >= ybegin && j < yend && i >= xbegin && i < xend) {
                    for (int c = 0;  c < CHANNELS;  ++c)
                        OIIO_CHECK_EQUAL (pixel[c], arbitrary3[c]);
                } else {
                    for (int c = 0;  c < CHANNELS;  ++c)
                        OIIO_CHECK_EQUAL (pixel[c], arbitrary2[c]);
                }
            }
        }
    }
}



// Test ImageBuf::crop
void test_crop ()
{
    std::cout << "test crop\n";
    int WIDTH = 8, HEIGHT = 6, CHANNELS = 4;
    // Crop region we'll work with
    int xbegin = 3, xend = 5, ybegin = 0, yend = 4;
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    spec.alpha_channel = 3;
    ImageBuf A, B;
    A.reset ("A", spec);
    B.reset ("B", spec);
    float arbitrary1[4];
    arbitrary1[0] = 0.2;
    arbitrary1[1] = 0.3;
    arbitrary1[2] = 0.4;
    arbitrary1[3] = 0.5;
    ImageBufAlgo::fill (A, arbitrary1);

    // Test CUT crop
    ImageBufAlgo::crop (B, A, xbegin, xend, ybegin, yend);

    // Should have changed the data window (origin and width/height)
    OIIO_CHECK_EQUAL (B.spec().x, xbegin);
    OIIO_CHECK_EQUAL (B.spec().width, xend-xbegin);
    OIIO_CHECK_EQUAL (B.spec().y, ybegin);
    OIIO_CHECK_EQUAL (B.spec().height, yend-ybegin);
    float *pixel = ALLOCA(float, CHANNELS);
    for (int j = 0;  j < B.spec().height;  ++j) {
        for (int i = 0;  i < B.spec().width;  ++i) {
            B.getpixel (i+B.xbegin(), j+B.ybegin(), pixel);
            // Inside the crop region should match what it always was
            for (int c = 0;  c < CHANNELS;  ++c)
                OIIO_CHECK_EQUAL (pixel[c], arbitrary1[c]);
        }
    }
}



void test_paste ()
{
    std::cout << "test paste\n";
    // Create the source image, make it a gradient
    ImageSpec Aspec (4, 4, 3, TypeDesc::FLOAT);
    ImageBuf A ("A", Aspec);
    for (ImageBuf::Iterator<float> it (A);  !it.done();  ++it) {
        it[0] = float(it.x()) / float(Aspec.width-1);
        it[1] = float(it.y()) / float(Aspec.height-1);
        it[2] = 0.1f;
    }

    // Create destination image -- black it out
    ImageSpec Bspec (8, 8, 3, TypeDesc::FLOAT);
    ImageBuf B ("B", Bspec);
    float gray[3] = { .1, .1, .1 };
    ImageBufAlgo::fill (B, gray);

    // Paste a few pixels from A into B -- include offsets
    ImageBufAlgo::paste (B, 2, 2, 0, 1 /* chan offset */,
                         A, ROI(1, 4, 1, 4));

    // Spot check
    float a[3], b[3];
    B.getpixel (1, 1, 0, b);
    OIIO_CHECK_EQUAL (b[0], gray[0]);
    OIIO_CHECK_EQUAL (b[1], gray[1]);
    OIIO_CHECK_EQUAL (b[2], gray[2]);

    B.getpixel (2, 2, 0, b);
    A.getpixel (1, 1, 0, a);
    OIIO_CHECK_EQUAL (b[0], gray[0]);
    OIIO_CHECK_EQUAL (b[1], a[0]);
    OIIO_CHECK_EQUAL (b[2], a[1]);

    B.getpixel (3, 4, 0, b);
    A.getpixel (2, 3, 0, a);
    OIIO_CHECK_EQUAL (b[0], gray[0]);
    OIIO_CHECK_EQUAL (b[1], a[0]);
    OIIO_CHECK_EQUAL (b[2], a[1]);
}



void test_channel_append ()
{
    std::cout << "test channel_append\n";
    ImageSpec spec (2, 2, 1, TypeDesc::FLOAT);
    ImageBuf A ("A", spec);
    ImageBuf B ("B", spec);
    float Acolor = 0.1, Bcolor = 0.2;
    ImageBufAlgo::fill (A, &Acolor);
    ImageBufAlgo::fill (B, &Bcolor);

    ImageBuf R ("R");
    ImageBufAlgo::channel_append (R, A, B);
    OIIO_CHECK_EQUAL (R.spec().width, spec.width);
    OIIO_CHECK_EQUAL (R.spec().height, spec.height);
    OIIO_CHECK_EQUAL (R.nchannels(), 2);
    for (ImageBuf::ConstIterator<float> r(R); !r.done(); ++r) {
        OIIO_CHECK_EQUAL (r[0], Acolor);
        OIIO_CHECK_EQUAL (r[1], Bcolor);
    }
}



// Tests ImageBufAlgo::add
void test_add ()
{
    std::cout << "test add\n";
    const int WIDTH = 8;
    const int HEIGHT = 8;
    const int CHANNELS = 4;
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    spec.alpha_channel = 3;

    // Create buffers
    ImageBuf A ("A", spec);
    const float Aval[CHANNELS] = { 0.1, 0.2, 0.3, 0.4 };
    ImageBufAlgo::fill (A, Aval);
    ImageBuf B ("B", spec);
    const float Bval[CHANNELS] = { 0.01, 0.02, 0.03, 0.04 };
    ImageBufAlgo::fill (B, Bval);

    ImageBuf C ("C", spec);
    ImageBufAlgo::add (C, A, B);

    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            C.getpixel (i, j, pixel);
            for (int c = 0;  c < CHANNELS;  ++c)
                OIIO_CHECK_EQUAL (pixel[c], Aval[c]+Bval[c]);
        }
    }
}



// Tests ImageBufAlgo::compare
void test_compare ()
{
    std::cout << "test compare\n";
    // Construct two identical 50% grey images
    const int WIDTH = 10, HEIGHT = 10, CHANNELS = 3;
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A ("A", spec);
    ImageBuf B ("B", spec);
    const float grey[CHANNELS] = { 0.5, 0.5, 0.5 };
    ImageBufAlgo::fill (A, grey);
    ImageBufAlgo::fill (B, grey);

    // Introduce some minor differences
    const int NDIFFS = 10;
    ImageBuf::Iterator<float> a (A);
    for (int i = 0;  i < NDIFFS && a.valid();  ++i, ++a) {
        for (int c = 0;  c < CHANNELS;  ++c)
            a[c] = a[c] + 0.01f * i;
    }
    // We expect the differences to be { 0, 0.01, 0.02, 0.03, 0.04, 0.05,
    // 0.06, 0.07, 0.08, 0.09, 0, 0, ...}.
    const float failthresh = 0.05;
    const float warnthresh = 0.025;
    ImageBufAlgo::CompareResults comp;
    ImageBufAlgo::compare (A, B, failthresh, warnthresh, comp);
    // We expect 5 pixels to exceed the fail threshold, 7 pixels to
    // exceed the warn threshold, the maximum difference to be 0.09,
    // and the maximally different pixel to be (9,0).
    // The total error should be 3 chans * sum{0.01,...,0.09} / (pixels*chans)
    //   = 3 * 0.45 / (100*3) = 0.0045
    std::cout << "Testing comparison: " << comp.nfail << " failed, "
              << comp.nwarn << " warned, max diff = " << comp.maxerror
              << " @ (" << comp.maxx << ',' << comp.maxy << ")\n";
    std::cout << "   mean err " << comp.meanerror << ", RMS err " 
              << comp.rms_error << ", PSNR = " << comp.PSNR <<  "\n";
    OIIO_CHECK_EQUAL (comp.nfail, 5);
    OIIO_CHECK_EQUAL (comp.nwarn, 7);
    OIIO_CHECK_EQUAL_THRESH (comp.maxerror, 0.09, 1e-6);
    OIIO_CHECK_EQUAL (comp.maxx, 9);
    OIIO_CHECK_EQUAL (comp.maxy, 0);
    OIIO_CHECK_EQUAL_THRESH (comp.meanerror, 0.0045, 1.0e-8);
}



// Test ability to do a maketx directly from an ImageBuf
void
test_maketx_from_imagebuf()
{
    std::cout << "test make_texture from ImageBuf\n";
    // Make a checkerboard
    const int WIDTH = 16, HEIGHT = 16, CHANNELS = 3;
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A ("A", spec);
    float pink[] = { .5, .3, .3 }, green[] = { .1, .5, .1 };
    ImageBufAlgo::checker (A, 4, pink, green, 0, WIDTH, 0, HEIGHT, 0, 1);

    // Write it
    const char *pgname = "oiio-pgcheck.tx";
    remove (pgname);  // Remove it first
    ImageSpec configspec;
    ImageBufAlgo::make_texture (ImageBufAlgo::MakeTxTexture, A,
                                pgname, configspec);

    // Read it back and compare it
    ImageBuf B (pgname);
    B.read ();
    ImageBufAlgo::CompareResults comparison;
    ImageBufAlgo::compare (A, B, 0, 0, comparison);
    OIIO_CHECK_EQUAL (comparison.nwarn, 0);
    OIIO_CHECK_EQUAL (comparison.nfail, 0);
    remove (pgname);  // clean up
}




int
main (int argc, char **argv)
{
    test_zero_fill ();
    test_crop ();
    test_paste ();
    test_channel_append ();
    test_add ();
    test_compare ();
    test_maketx_from_imagebuf ();
    
    return unit_test_failures;
}
