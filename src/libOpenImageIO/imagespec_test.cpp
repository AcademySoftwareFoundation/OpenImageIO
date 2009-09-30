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

#define _USE_MATH_DEFINES
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

    EXPECT_EQ ((size_t)BYTES_IN_FLOAT, sizeof (float));
    EXPECT_EQ (CHANNELS, spec.nchannels);
    EXPECT_EQ (WIDTH, spec.width);
    EXPECT_EQ (HEIGHT, spec.height);
    EXPECT_EQ (1, spec.depth);
    EXPECT_EQ (WIDTH, spec.full_width);
    EXPECT_EQ (HEIGHT, spec.full_height);
    EXPECT_EQ (1, spec.full_depth);
    // FIXME(nemec): uncomment after figuring out linking
    //   EXPECT_EQ (TypeDesc::UINT8, spec.format);
    EXPECT_EQ ((size_t)BYTES_IN_FLOAT, spec.channel_bytes ());
    EXPECT_EQ ((size_t)(BYTES_IN_FLOAT*CHANNELS), spec.pixel_bytes ());
    EXPECT_EQ ((size_t)(BYTES_IN_FLOAT*CHANNELS*WIDTH), spec.scanline_bytes ());
    EXPECT_EQ ((size_t)(WIDTH*HEIGHT), spec.image_pixels ());

    // check that the magnitude is right (not clamped) -- should be about > 2^40
    long long expected_bytes = BYTES_IN_FLOAT*CHANNELS*WIDTH*HEIGHT;
    // log (x) / log (2) = log2 (x)
    // log (2^32) / log (2) = log2 (2^32) = 32
    // log (2^32) * M_LOG2E = 32
    double log2_result = log ((double)expected_bytes) * M_LOG2E;
    EXPECT_LT (40, log2_result);
    EXPECT_EQ ((size_t)expected_bytes, spec.image_bytes ());

    std::cout << "expected_bytes = " << expected_bytes << ", log "
              << log ((double)expected_bytes) << std::endl;
}



}; // end anon namespace



int
main (int argc, char **argv)
{
    testing::InitGoogleTest (&argc, argv);
    return RUN_ALL_TESTS ();
}
