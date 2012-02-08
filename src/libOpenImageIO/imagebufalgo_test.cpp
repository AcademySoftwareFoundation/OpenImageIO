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

OIIO_NAMESPACE_USING;


// Test ImageBuf::zero and ImageBuf::fill
void ImageBuf_zero_fill ()
{
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
        ImageBufAlgo::fill (A, arbitrary3, xbegin, xend, ybegin, yend);
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



// Tests ImageBufAlgo::add
void ImageBuf_add ()
{
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



// check if two float values match
inline bool
is_equal (float x, float y) {
    return std::abs(x - y) <= 1e-05 * std::abs(x); // 1e-05
}



int
main (int argc, char **argv)
{
    ImageBuf_zero_fill ();
    test_crop ();
    ImageBuf_add ();
    
    return unit_test_failures;
}
