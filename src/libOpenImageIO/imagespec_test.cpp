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


#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/unittest.h>

using namespace OIIO;


void test_imagespec_pixels ()
{
    std::cout << "test_imagespec_pixels\n";
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
    std::cout << "test_imagespec_metadata_val\n";
    std::string ret;

    int imatrix[] = {100, 200, 300, 400};
    metadata_val_test (&imatrix[0], 1, TypeDesc::TypeInt, ret);
    OIIO_CHECK_EQUAL (ret, "100");
    metadata_val_test (imatrix, sizeof (imatrix)/sizeof(int), TypeDesc::TypeInt, ret);
    OIIO_CHECK_EQUAL (ret, "100, 200, 300, 400");
    OIIO_CHECK_NE (ret, "100, 200, 300, 400,");

    float fmatrix[] = {10.12f, 200.34f, 300.11f, 400.9f};
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
    OIIO_CHECK_EQUAL (ret, "\"this is \\\"a test\\\"\"");
    OIIO_CHECK_NE (ret, smatrix[0]);
    OIIO_CHECK_NE (ret, "\"this is \"a test\"\",");
    metadata_val_test (smatrix, sizeof (smatrix) / sizeof (char *), TypeDesc::TypeString, ret);
    OIIO_CHECK_EQUAL (ret, "\"this is \\\"a test\\\"\", \"this is another test\"");

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
    std::cout << "test_imagespec_attribute_from_string\n";
    TypeDesc type = TypeDesc::TypeInt;
    std::string ret, data, invalid_data;

    data = "142";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, data);

    type = TypeDesc::TypeFloat;
    data = "1.23";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, data);

    type = TypeDesc(TypeDesc::FLOAT, 5);
    data = "1.23, 34.23, 35.11, 99.99, 1999.99";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, data);

    type = TypeDesc::UINT64;
    data = "18446744073709551615";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, data);

    type = TypeDesc::TypeMatrix;
    data = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, data);

    type = TypeDesc::TypeString;
    data = "foo";
    attribute_test (data, type, ret);
    OIIO_CHECK_EQUAL (ret, "\"foo\"");
}



static void
test_get_attribute ()
{
    std::cout << "test_get_attribute\n";
    ImageSpec spec (640, 480, 4, TypeDesc::FLOAT);
    spec.x = 10; spec.y = 12;
    spec.full_x = -5; spec.full_y = -8;
    spec.full_width = 1024; spec.full_height = 800;
    spec.tile_width = 64; spec.tile_height = 32;
    spec.attribute ("foo", int(42));
    spec.attribute ("pi", float(M_PI));
    spec.attribute ("bar", "barbarbar?");

    OIIO_CHECK_EQUAL (spec.get_int_attribute("width"), 640);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("height"), 480);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("nchannels"), 4);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("x"), 10);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("y"), 12);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("full_x"), -5);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("full_y"), -8);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("full_width"), 1024);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("full_height"), 800);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("tile_width"), 64);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("tile_height"), 32);
    OIIO_CHECK_EQUAL (spec.get_string_attribute("geom"), "640x480+10+12");
    OIIO_CHECK_EQUAL (spec.get_string_attribute("full_geom"), "1024x800-5-8");
    OIIO_CHECK_EQUAL (spec.get_int_attribute("foo"), 42);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("pi",4), 4);  // should fail int
    OIIO_CHECK_EQUAL (spec.get_float_attribute("pi"), float(M_PI));
    OIIO_CHECK_EQUAL (spec.get_int_attribute("bar"), 0);
    OIIO_CHECK_EQUAL (spec.get_int_attribute("bar"), 0);
    OIIO_CHECK_EQUAL (spec.get_string_attribute("bar"), "barbarbar?");
    OIIO_CHECK_ASSERT(spec.find_attribute("foo") != NULL);
    OIIO_CHECK_ASSERT(spec.find_attribute("Foo") != NULL);
    OIIO_CHECK_ASSERT(spec.find_attribute("Foo", TypeDesc::UNKNOWN, false) != NULL);
    OIIO_CHECK_ASSERT(spec.find_attribute("Foo", TypeDesc::UNKNOWN, true) == NULL);
    OIIO_CHECK_ASSERT(spec.find_attribute("foo", TypeDesc::INT) != NULL);
    OIIO_CHECK_ASSERT(spec.find_attribute("foo", TypeDesc::FLOAT) == NULL);
}


static void
test_imagespec_from_ROI ()
{
    ROI roi (0, 640, 0, 480, 0, 1, 0, 3);
    ImageSpec spec (roi, TypeDesc::FLOAT);
    OIIO_CHECK_EQUAL (spec.nchannels, 3);
    OIIO_CHECK_EQUAL (spec.width, 640);
    OIIO_CHECK_EQUAL (spec.height, 480);
    OIIO_CHECK_EQUAL (spec.depth, 1);
    OIIO_CHECK_EQUAL (spec.full_width, 640);
    OIIO_CHECK_EQUAL (spec.full_height, 480);
    OIIO_CHECK_EQUAL (spec.full_depth, 1);
}



int main (int argc, char *argv[])
{
    test_imagespec_pixels ();
    test_imagespec_metadata_val ();
    test_imagespec_attribute_from_string ();
    test_get_attribute ();
    test_imagespec_from_ROI ();

    return unit_test_failures;
}
