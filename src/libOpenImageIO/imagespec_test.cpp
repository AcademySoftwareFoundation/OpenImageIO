/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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
#include "fmath.h"
#include "unittest.h"

OIIO_NAMESPACE_USING;


void test_imagespec_pixels ()
{
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
    std::cout << "sizeof (imagesize_t) = " << sizeof (imagesize_t) << std::endl;
    std::cout << "sizeof (stride_t) = " << sizeof (stride_t) << std::endl;
    std::cout << "sizeof (float) = " << sizeof (float) << std::endl;

    OIIO_CHECK_EQUAL ((size_t)BYTES_IN_FLOAT, sizeof (float));
    OIIO_CHECK_EQUAL (CHANNELS, spec.nchannels);
    OIIO_CHECK_EQUAL (WIDTH, spec.width);
    OIIO_CHECK_EQUAL (HEIGHT, spec.height);
    OIIO_CHECK_EQUAL (1, spec.depth);
    OIIO_CHECK_EQUAL (WIDTH, spec.full_width);
    OIIO_CHECK_EQUAL (HEIGHT, spec.full_height);
    OIIO_CHECK_EQUAL (1, spec.full_depth);
    // FIXME(nemec): uncomment after figuring out linking
    //   OIIO_CHECK_EQUAL (TypeDesc::UINT8, spec.format);
    OIIO_CHECK_EQUAL ((size_t)BYTES_IN_FLOAT, spec.channel_bytes ());
    OIIO_CHECK_EQUAL ((size_t)(BYTES_IN_FLOAT*CHANNELS), spec.pixel_bytes ());
    OIIO_CHECK_EQUAL ((imagesize_t)(BYTES_IN_FLOAT*CHANNELS*WIDTH), spec.scanline_bytes ());
    OIIO_CHECK_EQUAL ((imagesize_t)(WIDTH*HEIGHT), spec.image_pixels ());

    // check that the magnitude is right (not clamped) -- should be about > 2^40
    long long expected_bytes = BYTES_IN_FLOAT*CHANNELS*WIDTH*HEIGHT;
    // log (x) / log (2) = log2 (x)
    // log (2^32) / log (2) = log2 (2^32) = 32
    // log (2^32) * M_LOG2E = 32
    double log2_result = log ((double)expected_bytes) * M_LOG2E;
    OIIO_CHECK_LT (40, log2_result);
    OIIO_CHECK_EQUAL ((imagesize_t)expected_bytes, spec.image_bytes ());

    std::cout << "expected_bytes = " << expected_bytes << ", log "
              << log ((double)expected_bytes) << std::endl;
}



static void
metadata_val_test (void *data, int num_elements, TypeDesc type, std::string& val)
{
    static ImageSpec spec;
    ImageIOParameter p;

    p.init ("name", type, num_elements, data);
    val = spec.metadata_val (p);
}



void test_imagespec_metadata_val ()
{
    std::string ret;

    int imatrix[] = {100, 200, 300, 400};
    metadata_val_test (&imatrix[0], 1, TypeDesc::TypeInt, ret);
    OIIO_CHECK_EQUAL (ret, "100");
    metadata_val_test (imatrix, sizeof (imatrix)/sizeof(int), TypeDesc::TypeInt, ret);
    OIIO_CHECK_EQUAL (ret, "100, 200, 300, 400");
    OIIO_CHECK_NE (ret, "100, 200, 300, 400,");

    float fmatrix[] = {10.12, 200.34, 300.11, 400.9};
    metadata_val_test (&fmatrix[0], 1, TypeDesc::TypeFloat, ret);
    OIIO_CHECK_EQUAL (ret, "10.12");
    metadata_val_test (fmatrix, sizeof (fmatrix) / sizeof (float), TypeDesc::TypeFloat, ret);
    OIIO_CHECK_EQUAL (ret, "10.12, 200.34, 300.11, 400.9");
    OIIO_CHECK_NE (ret, "10, 200, 300, 400");
    OIIO_CHECK_NE (ret, "10.12, 200.34, 300.11, 400.9,");

    unsigned long long ullmatrix[] = {0xffffffffffffffffLL, 0xffffffffffffffffLL};
    metadata_val_test (&ullmatrix, 1, TypeDesc::UINT64, ret);
    OIIO_CHECK_EQUAL (ret, "18446744073709551615");
    metadata_val_test (&ullmatrix, sizeof (ullmatrix) / sizeof (unsigned long long), TypeDesc::UINT64, ret);
    OIIO_CHECK_EQUAL (ret, "18446744073709551615, 18446744073709551615");
    OIIO_CHECK_NE (ret, "-1, -1");
    OIIO_CHECK_NE (ret, "18446744073709551615, 18446744073709551615,");

    const char* smatrix[] = {"this is \"a test\"", "this is another test"};
    metadata_val_test (smatrix, 1, TypeDesc::TypeString, ret);
    OIIO_CHECK_EQUAL (ret, "\"this is \"a test\"\"");
    OIIO_CHECK_NE (ret, smatrix[0]);
    OIIO_CHECK_NE (ret, "\"this is \"a test\"\",");
    metadata_val_test (smatrix, sizeof (smatrix) / sizeof (char *), TypeDesc::TypeString, ret);
    OIIO_CHECK_EQUAL (ret, "\"this is \"a test\"\", \"this is another test\"");

    float matrix16[2][16] = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16},
                        {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25}};
    metadata_val_test (&matrix16[0], 1, TypeDesc::TypeMatrix, ret);
    OIIO_CHECK_EQUAL (ret, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16");
    OIIO_CHECK_NE (ret, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16,");
    metadata_val_test (matrix16, sizeof (matrix16) / (16 * sizeof (float)), TypeDesc::TypeMatrix, ret);
    OIIO_CHECK_EQUAL (ret, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16, 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25");
}



static void
attribute_test (const std::string &data, TypeDesc type, std::string &ret)
{
    ImageSpec spec;
    spec.attribute ("name", type, data);
    ret = spec.metadata_val (spec.extra_attribs[0]);
}



void test_imagespec_attribute_from_string ()
{
    TypeDesc type = TypeDesc::TypeInt;
    std::string ret, data, invalid_data;

    data = "1, 2, 3, 4, 5, 6";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, data);

    type = TypeDesc::TypeFloat;
    data = "1.23, 34.23, 35.11, 99.99, 1999.99";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, data);

    type = TypeDesc::UINT64;
    data = "18446744073709551615, 18446744073709551615";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, data);
    invalid_data = "18446744073709551615";
    OIIO_CHECK_NE (ret, invalid_data);
    invalid_data = "18446744073709551614, 18446744073709551615";
    OIIO_CHECK_NE (ret, invalid_data);

    type = TypeDesc::INT64;
    data = "-1, 9223372036854775807";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, data);
    invalid_data = "-1";
    OIIO_CHECK_NE (ret, invalid_data);

    type = TypeDesc::TypeMatrix;
    data = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16, 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, data);
    invalid_data = data = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36";
    OIIO_CHECK_NE (ret, invalid_data);

    type = TypeDesc::TypeString;
    data = "\"imageParameter:param\"";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, data);
}




int main (int argc, char *argv[])
{
    test_imagespec_pixels ();
    test_imagespec_metadata_val ();
    test_imagespec_attribute_from_string ();

    return unit_test_failures;
}
