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
#include <gtest/gtest.h>

#include <string>

using namespace OpenImageIO;

namespace {  // make an anon namespace

// The fixture for testing class ImageSpec.
class ImageSpecTest : public testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    ImageSpecTest () {
        // You can do set-up work for each test here.
    }
    virtual ~ImageSpecTest () {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp () {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown () {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Foo.
};



// Tests that ImageSpec handles huge images.
TEST_F (ImageSpecTest, image_pixels) {
    // images with dimensions > 2^16 (65536) on a side have > 2^32 pixels
    const long long WIDTH = 456789;
    const long long HEIGHT = 345678;
    const long long CHANNELS=3;
    const long long BYTES_IN_FLOAT = 4;
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);

    std::cout << "sizeof (int) = " << sizeof (int) << std::endl;
    std::cout << "sizeof (long) = " << sizeof (long) << std::endl;
    std::cout << "sizeof (long long) = " << sizeof (long long) << std::endl;
    std::cout << "sizeof (size_t) = " << sizeof (size_t) << std::endl;
    std::cout << "sizeof (stride_t) = " << sizeof (stride_t) << std::endl;
    std::cout << "sizeof (float) = " << sizeof (float) << std::endl;

    EXPECT_EQ (BYTES_IN_FLOAT, sizeof (float));
    EXPECT_EQ (CHANNELS, spec.nchannels);
    EXPECT_EQ (WIDTH, spec.width);
    EXPECT_EQ (HEIGHT, spec.height);
    EXPECT_EQ (1, spec.depth);
    EXPECT_EQ (WIDTH, spec.full_width);
    EXPECT_EQ (HEIGHT, spec.full_height);
    EXPECT_EQ (1, spec.full_depth);
    // FIXME(nemec): uncomment after figuring out linking
    //   EXPECT_EQ (TypeDesc::UINT8, spec.format);
    EXPECT_EQ (BYTES_IN_FLOAT, spec.channel_bytes ());
    EXPECT_EQ (BYTES_IN_FLOAT*CHANNELS, spec.pixel_bytes ());
    EXPECT_EQ (BYTES_IN_FLOAT*CHANNELS*WIDTH, spec.scanline_bytes ());
    EXPECT_EQ (WIDTH*HEIGHT, spec.image_pixels ());

    // check that the magnitude is right (not clamped) -- should be about > 2^40
    long long expected_bytes = BYTES_IN_FLOAT*CHANNELS*WIDTH*HEIGHT;
    // log (x) / log (2) = log2 (x)
    // log (2^32) / log (2) = log2 (2^32) = 32
    // log (2^32) * M_LOG2E = 32
    double log2_result = log (expected_bytes) * M_LOG2E;
    EXPECT_LT (40, log2_result);
    EXPECT_EQ (expected_bytes, spec.image_bytes ());

    std::cout << "expected_bytes = " << expected_bytes << ", log "
              << log (expected_bytes) << std::endl;
}



// The fixture for testing class ImageBuf.
class ImageBufTest : public testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.
    ImageBufTest () {
        // You can do set-up work for each test here.
    }
    virtual ~ImageBufTest () {
        // You can do clean-up work that doesn't throw exceptions here.
    }
    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:
    virtual void SetUp () {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }
    virtual void TearDown () {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }
    // Objects declared here can be used by all tests in the test case for Foo.
};



// Test ImageBuf::zero and ImageBuf::fill
TEST_F (ImageBufTest, ImageBuf_zero_fill)
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
        EXPECT_EQ (pixel[c], arbitrary1[c]) << "bad ImageBuf::setpixel";

    // Zero out and test that it worked
    A.zero ();
    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            A.getpixel (i, j, pixel);
            for (int c = 0;  c < CHANNELS;  ++c)
                EXPECT_EQ (pixel[c], 0.0f) << "bad ImageBuf::zero";
        }
    }

    // Test fill of whole image
    const float arbitrary2[CHANNELS] = { 0.6, 0.7, 0.3, 0.9 };
    A.fill (arbitrary2);
    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            A.getpixel (i, j, pixel);
            for (int c = 0;  c < CHANNELS;  ++c)
                EXPECT_EQ (pixel[c], arbitrary2[c]) << "bad ImageBuf::fill";
        }
    }

    // Test fill of partial image
    const float arbitrary3[CHANNELS] = { 0.42, 0.43, 0.44, 0.45 };
    {
        const int xbegin = 3, xend = 5, ybegin = 0, yend = 4;
        A.fill (arbitrary3, xbegin, xend, ybegin, yend);
        for (int j = 0;  j < HEIGHT;  ++j) {
            for (int i = 0;  i < WIDTH;  ++i) {
                float pixel[CHANNELS];
                A.getpixel (i, j, pixel);
                if (j >= ybegin && j < yend && i >= xbegin && i < xend) {
                    for (int c = 0;  c < CHANNELS;  ++c)
                        EXPECT_EQ (pixel[c], arbitrary3[c]) << "bad ImageBuf::fill";
                } else {
                    for (int c = 0;  c < CHANNELS;  ++c)
                        EXPECT_EQ (pixel[c], arbitrary2[c]) << "bad ImageBuf::fill";
                }
            }
        }
    }
}



// The fixture for testing class ImageBufAlgo::Crop.
class CropTest : public testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.
    CropTest () {
        // You can do set-up work for each test here.
        WIDTH = 8;
        HEIGHT = 6;
        CHANNELS = 4;
        // Crop region we'll work with
        xbegin = 3, xend = 5, ybegin = 0, yend = 4;
        ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
        spec.alpha_channel = 3;
        A.reset ("A", spec);
        B.reset ("B", spec);
        arbitrary1[0] = 0.2;
        arbitrary1[1] = 0.3;
        arbitrary1[2] = 0.4;
        arbitrary1[3] = 0.5;
    }
    virtual ~CropTest () {
        // You can do clean-up work that doesn't throw exceptions here.
    }
    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:
    virtual void SetUp () {
        // Code here will be called immediately after the constructor (right
        // before each test).
        A.fill (arbitrary1);
    }
    virtual void TearDown () {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }
    // Objects declared here can be used by all tests in the test case for Foo.
    int WIDTH, HEIGHT, CHANNELS;
    // Crop region we'll work with
    int xbegin, xend, ybegin, yend;
    ImageSpec spec;
    ImageBuf A, B;
    float arbitrary1[4];
};



// Test ImageBuf::crop
TEST_F (CropTest, crop_cut)
{
    // Test CUT crop
    ImageBufAlgo::crop (B, A, xbegin, xend, ybegin, yend,
                        ImageBufAlgo::CROP_CUT);
    // Should have changed the data window (origin and width/height)
    ASSERT_EQ (B.spec().x, 0);
    ASSERT_EQ (B.spec().width, xend-xbegin);
    ASSERT_EQ (B.spec().full_x, 0);
    ASSERT_EQ (B.spec().full_width, xend-xbegin);
    ASSERT_EQ (B.spec().y, 0);
    ASSERT_EQ (B.spec().height, yend-ybegin);
    ASSERT_EQ (B.spec().full_y, 0);
    ASSERT_EQ (B.spec().full_height, yend-ybegin);
    for (int j = 0;  j < B.spec().height;  ++j) {
        for (int i = 0;  i < B.spec().width;  ++i) {
            float pixel[CHANNELS];
            B.getpixel (i, j, pixel);
            // Inside the crop region should match what it always was
            for (int c = 0;  c < CHANNELS;  ++c)
                EXPECT_EQ (pixel[c], arbitrary1[c]) << "bad ImageBuf::crop BLACK";
        }
    }
}


TEST_F (CropTest, crop_window)
{
    // Test WINDOW crop
    ImageBufAlgo::crop (B, A, xbegin, xend, ybegin, yend,
                        ImageBufAlgo::CROP_WINDOW);
    // Should have changed the data window (origin and width/height)
    ASSERT_EQ (B.spec().x, xbegin);
    ASSERT_EQ (B.spec().width, xend-xbegin);
    ASSERT_EQ (B.spec().full_x, 0);
    ASSERT_EQ (B.spec().full_width, WIDTH);
    ASSERT_EQ (B.spec().y, ybegin);
    ASSERT_EQ (B.spec().height, yend-ybegin);
    ASSERT_EQ (B.spec().full_y, 0);
    ASSERT_EQ (B.spec().full_height, HEIGHT);
    for (int j = ybegin;  j < yend;  ++j) {
        for (int i = xbegin;  i < xend;  ++i) {
            float pixel[CHANNELS];
            B.getpixel (i, j, pixel);
            // Inside the crop region should match what it always was
            for (int c = 0;  c < CHANNELS;  ++c)
                EXPECT_EQ (pixel[c], arbitrary1[c]) << "bad ImageBuf::crop BLACK";
        }
    }
}


TEST_F (CropTest, crop_black)
{
    // Test BLACK crop
    ImageBufAlgo::crop (B, A, xbegin, xend, ybegin, yend,
                        ImageBufAlgo::CROP_BLACK);
    // Should be full size
    ASSERT_EQ (B.spec().x, 0);
    ASSERT_EQ (B.spec().width, WIDTH);
    ASSERT_EQ (B.spec().full_x, 0);
    ASSERT_EQ (B.spec().full_width, WIDTH);
    ASSERT_EQ (B.spec().y, 0);
    ASSERT_EQ (B.spec().height, HEIGHT);
    ASSERT_EQ (B.spec().full_y, 0);
    ASSERT_EQ (B.spec().full_height, HEIGHT);
    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            B.getpixel (i, j, pixel);
            if (j >= ybegin && j < yend && i >= xbegin && i < xend) {
                // Inside the crop region should match what it always was
                for (int c = 0;  c < CHANNELS;  ++c)
                    EXPECT_EQ (pixel[c], arbitrary1[c]) << "bad ImageBuf::crop BLACK";
            } else {
                // Outside the crop region should be black
                EXPECT_EQ (pixel[0], 0) << "bad ImageBuf::crop BLACK";
                EXPECT_EQ (pixel[1], 0) << "bad ImageBuf::crop BLACK";
                EXPECT_EQ (pixel[2], 0) << "bad ImageBuf::crop BLACK";
                EXPECT_EQ (pixel[3], 1) << "bad ImageBuf::crop BLACK";
            }
        }
    }
}


TEST_F (CropTest, crop_white)
{
    // Test WHITE crop
    ImageBufAlgo::crop (B, A, xbegin, xend, ybegin, yend,
                        ImageBufAlgo::CROP_WHITE);
    // Should be full size
    ASSERT_EQ (B.spec().x, 0);
    ASSERT_EQ (B.spec().width, WIDTH);
    ASSERT_EQ (B.spec().full_x, 0);
    ASSERT_EQ (B.spec().full_width, WIDTH);
    ASSERT_EQ (B.spec().y, 0);
    ASSERT_EQ (B.spec().height, HEIGHT);
    ASSERT_EQ (B.spec().full_y, 0);
    ASSERT_EQ (B.spec().full_height, HEIGHT);
    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            B.getpixel (i, j, pixel);
            if (j >= ybegin && j < yend && i >= xbegin && i < xend) {
                // Inside the crop region should match what it always was
                for (int c = 0;  c < CHANNELS;  ++c)
                    EXPECT_EQ (pixel[c], arbitrary1[c]) << "bad ImageBuf::crop WHITE";
            } else {
                // Outside the crop region should be black
                EXPECT_EQ (pixel[0], 1) << "bad ImageBuf::crop WHITE";
                EXPECT_EQ (pixel[1], 1) << "bad ImageBuf::crop WHITE";
                EXPECT_EQ (pixel[2], 1) << "bad ImageBuf::crop WHITE";
                EXPECT_EQ (pixel[3], 1) << "bad ImageBuf::crop WHITE";
            }
        }
    }
}


TEST_F (CropTest, crop_trans)
{
    // Test TRANS crop
    ImageBufAlgo::crop (B, A, xbegin, xend, ybegin, yend,
                        ImageBufAlgo::CROP_TRANS);
    // Should be ful size
    ASSERT_EQ (B.spec().x, 0);
    ASSERT_EQ (B.spec().width, WIDTH);
    ASSERT_EQ (B.spec().full_x, 0);
    ASSERT_EQ (B.spec().full_width, WIDTH);
    ASSERT_EQ (B.spec().y, 0);
    ASSERT_EQ (B.spec().height, HEIGHT);
    ASSERT_EQ (B.spec().full_y, 0);
    ASSERT_EQ (B.spec().full_height, HEIGHT);
    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            B.getpixel (i, j, pixel);
            if (j >= ybegin && j < yend && i >= xbegin && i < xend) {
                // Inside the crop region should match what it always was
                for (int c = 0;  c < CHANNELS;  ++c)
                    EXPECT_EQ (pixel[c], arbitrary1[c]) << "bad ImageBuf::crop TRANS";
            } else {
                // Outside the crop region should be black
                EXPECT_EQ (pixel[0], 0) << "bad ImageBuf::crop TRANS";
                EXPECT_EQ (pixel[1], 0) << "bad ImageBuf::crop TRANS";
                EXPECT_EQ (pixel[2], 0) << "bad ImageBuf::crop TRANS";
                EXPECT_EQ (pixel[3], 0) << "bad ImageBuf::crop TRANS";
            }
        }
    }
}



// Tests ImageBufAlgo::add
TEST_F (ImageBufTest, ImageBuf_add)
{
    const int WIDTH = 8;
    const int HEIGHT = 8;
    const int CHANNELS = 4;
    float pixel[CHANNELS];   // test pixel
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    spec.alpha_channel = 3;

    // Create buffers
    ImageBuf A ("A", spec);
    const float Aval[CHANNELS] = { 0.1, 0.2, 0.3, 0.4 };
    A.fill (Aval);
    ImageBuf B ("B", spec);
    const float Bval[CHANNELS] = { 0.01, 0.02, 0.03, 0.04 };
    B.fill (Bval);

    ImageBuf C ("C", spec);
    ImageBufAlgo::add (C, A, B);

    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            C.getpixel (i, j, pixel);
            for (int c = 0;  c < CHANNELS;  ++c)
                EXPECT_EQ (pixel[c], Aval[c]+Bval[c]) << "bad ImageBufAlgo::add";
        }
    }
}


}; // end anon namespace



int
main (int argc, char **argv)
{
    testing::InitGoogleTest (&argc, argv);
    return RUN_ALL_TESTS ();
}
