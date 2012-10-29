/*
  Copyright 2012 Larry Gritz and the other authors and contributors.
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


#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "sysutil.h"
#include "unittest.h"

#include <iostream>

OIIO_NAMESPACE_USING;


// Tests ImageBuf construction from application buffer
void ImageBuf_test_appbuffer ()
{
    const int WIDTH = 8;
    const int HEIGHT = 8;
    const int CHANNELS = 1;
    static float buf[HEIGHT][WIDTH] = {
        { 0, 0, 0, 0, 1, 0, 0, 0 }, 
        { 0, 0, 0, 1, 0, 1, 0, 0 }, 
        { 0, 0, 1, 0, 0, 0, 1, 0 }, 
        { 0, 1, 0, 0, 0, 0, 0, 1 }, 
        { 0, 0, 1, 0, 0, 0, 1, 0 }, 
        { 0, 0, 0, 1, 0, 1, 0, 0 }, 
        { 0, 0, 0, 0, 1, 0, 0, 0 }, 
        { 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A ("A", spec, buf);

    // Make sure A now points to the buffer
    OIIO_CHECK_EQUAL ((void *)A.pixeladdr (0, 0, 0), (void *)buf);

    // write it
    A.save ("A.tif");

    // Read it back and make sure it matches the original
    ImageBuf B ("A.tif");
    B.read ();
    for (int y = 0;  y < HEIGHT;  ++y)
        for (int x = 0;  x < WIDTH;  ++x)
            OIIO_CHECK_EQUAL (A.getchannel (x, y, 0),
                              B.getchannel (x, y, 0));
}



// Tests histogram computation.
void histogram_computation_test ()
{
    const int INPUT_WIDTH   = 64;
    const int INPUT_HEIGHT  = 64;
    const int INPUT_CHANNEL = 0;

    const int HISTOGRAM_BINS = 256;

    const int SPIKE1 = 51;  // 0.2f in range 0->1 maps to 51 in range 0->255
    const int SPIKE2 = 128; // 0.5f in range 0->1 maps to 128 in range 0->255
    const int SPIKE3 = 204; // 0.8f in range 0->1 maps to 204 in range 0->255

    const int SPIKE1_COUNT = INPUT_WIDTH * 8;
    const int SPIKE2_COUNT = INPUT_WIDTH * 16;
    const int SPIKE3_COUNT = INPUT_WIDTH * 40;

    // Create input image with three regions with different pixel values.
    ImageSpec spec (INPUT_WIDTH, INPUT_HEIGHT, 1, TypeDesc::FLOAT);
    ImageBuf A ("A", spec);

    float value[] = {0.2f};
    ImageBufAlgo::fill (A, value, ROI(0, INPUT_WIDTH, 0, 8));

    value[0] = 0.5f;
    ImageBufAlgo::fill (A, value, ROI(0, INPUT_WIDTH, 8, 24));

    value[0] = 0.8f;
    ImageBufAlgo::fill (A, value, ROI(0, INPUT_WIDTH, 24, 64));

    // Compute A's histogram.
    std::vector<imagesize_t> hist;
    ImageBufAlgo::histogram (A, INPUT_CHANNEL, hist, HISTOGRAM_BINS);

    // Does the histogram size equal the number of bins?
    OIIO_CHECK_EQUAL (hist.size(), (imagesize_t)HISTOGRAM_BINS);

    // Are the histogram values as expected?
    OIIO_CHECK_EQUAL (hist[SPIKE1], (imagesize_t)SPIKE1_COUNT);
    OIIO_CHECK_EQUAL (hist[SPIKE2], (imagesize_t)SPIKE2_COUNT);
    OIIO_CHECK_EQUAL (hist[SPIKE3], (imagesize_t)SPIKE3_COUNT);
    for (int i = 0; i < HISTOGRAM_BINS; i++)
        if (i!=SPIKE1 && i!=SPIKE2 && i!=SPIKE3)
            OIIO_CHECK_EQUAL (hist[i], 0);
}



int
main (int argc, char **argv)
{
    ImageBuf_test_appbuffer ();
    histogram_computation_test ();

    return unit_test_failures;
}
