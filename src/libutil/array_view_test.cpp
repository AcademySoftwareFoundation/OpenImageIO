/*
  Copyright 2014 Larry Gritz, et al. All Rights Reserved.

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

#include <iostream>

#include <OpenImageIO/strided_ptr.h>
#include <OpenImageIO/array_view.h>
#include <OpenImageIO/image_view.h>
#include <OpenImageIO/unittest.h>

OIIO_NAMESPACE_USING;



void test_offset ()
{
    // Test default constructor
    offset<1> off1_default;
    offset<2> off2_default;
    OIIO_CHECK_EQUAL (off1_default[0], 0);
    OIIO_CHECK_EQUAL (off2_default[0], 0);
    OIIO_CHECK_EQUAL (off2_default[0], 0);

    // Test explitit initializers
    offset<1> off1 (42), off1b(10);
    offset<2> off2 (14, 43), off2b(10,12);
    OIIO_CHECK_EQUAL (off1[0], 42);
    OIIO_CHECK_EQUAL (off2[0], 14);
    OIIO_CHECK_EQUAL (off2[1], 43);
    // test == and !=
    OIIO_CHECK_EQUAL (off1, off1);
    OIIO_CHECK_NE (off1, off1b);
    OIIO_CHECK_EQUAL (off2, off2);
    OIIO_CHECK_NE (off2, off2b);
    // test arithmetic
    OIIO_CHECK_EQUAL (off1+off1b, offset<1>(52));
    OIIO_CHECK_EQUAL (off1-off1b, offset<1>(32));
    OIIO_CHECK_EQUAL (-off1, offset<1>(-42));
    OIIO_CHECK_EQUAL (off1*2, offset<1>(84));
    OIIO_CHECK_EQUAL (off1/2, offset<1>(21));
    OIIO_CHECK_EQUAL (off2+off2b, offset<2>(24,55));
    OIIO_CHECK_EQUAL (off2-off2b, offset<2>(4,31));
    OIIO_CHECK_EQUAL (-off2, offset<2>(-14,-43));
    OIIO_CHECK_EQUAL (off2b*2, offset<2>(20,24));
    OIIO_CHECK_EQUAL (off2b/2, offset<2>(5,6));
    { offset<1> o = off1; ++o; OIIO_CHECK_EQUAL(o[0], 43); }
    { offset<1> o = off1; o++; OIIO_CHECK_EQUAL(o[0], 43); }
    { offset<1> o = off1; --o; OIIO_CHECK_EQUAL(o[0], 41); }
    { offset<1> o = off1; o--; OIIO_CHECK_EQUAL(o[0], 41); }
    { offset<1> o = off1; o += off1b; OIIO_CHECK_EQUAL(o[0], 52); }
    { offset<1> o = off1; o -= off1b; OIIO_CHECK_EQUAL(o[0], 32); }
    { offset<1> o = off1; o *= 2; OIIO_CHECK_EQUAL(o[0], 84); }
    { offset<1> o = off1; o /= 2; OIIO_CHECK_EQUAL(o[0], 21); }
    { offset<2> o = off2; o += off2b; OIIO_CHECK_EQUAL(o, offset<2>(24,55)); }
    { offset<2> o = off2; o -= off2b; OIIO_CHECK_EQUAL(o, offset<2>(4,31)); }
    { offset<2> o = off2b; o *= 2; OIIO_CHECK_EQUAL(o, offset<2>(20,24)); }
    { offset<2> o = off2b; o /= 2; OIIO_CHECK_EQUAL(o, offset<2>(5,6)); }

#if OIIO_CPLUSPLUS_VERSION >= 11
    {
        // test initializer list
        offset<1> off1 {42};
        offset<2> off2 {14, 43};
        OIIO_CHECK_EQUAL (off1[0], 42);
        OIIO_CHECK_EQUAL (off2[0], 14);
        OIIO_CHECK_EQUAL (off2[1], 43);
    }
#endif
}



void test_bounds ()
{
    // Test default constructor
    bounds<1> b1_default;
    bounds<2> b2_default;
    OIIO_CHECK_EQUAL (b1_default[0], 0);
    OIIO_CHECK_EQUAL (b2_default[0], 0);
    OIIO_CHECK_EQUAL (b2_default[0], 0);

    // Test explitit initializers
    bounds<1> b1 (42), b1b(10);
    bounds<2> b2 (14, 43), b2b(10,12);
    OIIO_CHECK_EQUAL (b1[0], 42);
    OIIO_CHECK_EQUAL (b2[0], 14);
    OIIO_CHECK_EQUAL (b2[1], 43);
    // test == and !=
    OIIO_CHECK_EQUAL (b1, b1);
    OIIO_CHECK_NE (b1, b1b);
    OIIO_CHECK_EQUAL (b2, b2);
    OIIO_CHECK_NE (b2, b2b);
    // test arithmetic
    offset<1> off1b(10);
    offset<2> off2b(10,12);
    OIIO_CHECK_EQUAL (b1+off1b, bounds<1>(52));
    OIIO_CHECK_EQUAL (b1-off1b, bounds<1>(32));
    OIIO_CHECK_EQUAL (b1*2, bounds<1>(84));
    OIIO_CHECK_EQUAL (b1/2, bounds<1>(21));
    OIIO_CHECK_EQUAL (b2+off2b, bounds<2>(24,55));
    OIIO_CHECK_EQUAL (b2-off2b, bounds<2>(4,31));
    OIIO_CHECK_EQUAL (b2b*2, bounds<2>(20,24));
    OIIO_CHECK_EQUAL (b2b/2, bounds<2>(5,6));
    { bounds<1> b = b1; b += off1b; OIIO_CHECK_EQUAL(b[0], 52); }
    { bounds<1> b = b1; b -= off1b; OIIO_CHECK_EQUAL(b[0], 32); }
    { bounds<1> b = b1; b *= 2; OIIO_CHECK_EQUAL(b[0], 84); }
    { bounds<1> b = b1; b /= 2; OIIO_CHECK_EQUAL(b[0], 21); }
    { bounds<2> b = b2; b += off2b; OIIO_CHECK_EQUAL(b, bounds<2>(24,55)); }
    { bounds<2> b = b2; b -= off2b; OIIO_CHECK_EQUAL(b, bounds<2>(4,31)); }
    { bounds<2> b = b2b; b *= 2; OIIO_CHECK_EQUAL(b, bounds<2>(20,24)); }
    { bounds<2> b = b2b; b /= 2; OIIO_CHECK_EQUAL(b, bounds<2>(5,6)); }

    // test iterators
    {
        bounds<1> b (3);
        bounds<1>::iterator i = b.begin();
        OIIO_CHECK_EQUAL (*i, 0); OIIO_CHECK_NE (i, b.end());
        ++i;
        OIIO_CHECK_EQUAL (*i, 1); OIIO_CHECK_NE (i, b.end());
        --i; OIIO_CHECK_EQUAL (*i, 0); ++i;
        i++;
        i--; OIIO_CHECK_EQUAL (*i, 1); ++i;
        OIIO_CHECK_EQUAL (*i, 2);
        i -= 1; OIIO_CHECK_EQUAL (*i, 1); i += 1;
        OIIO_CHECK_EQUAL (*i, 2); OIIO_CHECK_NE (i, b.end());
        i += 1;
        OIIO_CHECK_EQUAL (i, b.end());
    }
    {
        bounds<2> b (2, 2);
        bounds<2>::iterator i = b.begin();
             OIIO_CHECK_EQUAL (*i, offset<2>(0,0)); OIIO_CHECK_NE (i, b.end());
        ++i; OIIO_CHECK_EQUAL (*i, offset<2>(0,1)); OIIO_CHECK_NE (i, b.end());
        --i; OIIO_CHECK_EQUAL (*i, offset<2>(0,0)); ++i;
        ++i; OIIO_CHECK_EQUAL (*i, offset<2>(1,0)); OIIO_CHECK_NE (i, b.end());
        i--; OIIO_CHECK_EQUAL (*i, offset<2>(0,1)); i++;
        ++i; OIIO_CHECK_EQUAL (*i, offset<2>(1,1)); OIIO_CHECK_NE (i, b.end());
        i -= 1; OIIO_CHECK_EQUAL (*i, offset<2>(1,0)); i += 1;
        ++i; OIIO_CHECK_EQUAL (i, b.end());
    }

#if OIIO_CPLUSPLUS_VERSION >= 11
    {
        // test initializer list
        bounds<1> b1 {42};
        bounds<2> b2 {14, 43};
        OIIO_CHECK_EQUAL (b1[0], 42);
        OIIO_CHECK_EQUAL (b2[0], 14);
        OIIO_CHECK_EQUAL (b2[1], 43);
    }
#endif
}



void test_array_view ()
{
    static float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 0 };
    array_view<const float> a (A);
    OIIO_CHECK_EQUAL (a.size(), 12);
    OIIO_CHECK_EQUAL (a[0], 0.0f);
    OIIO_CHECK_EQUAL (a[1], 1.0f);
    OIIO_CHECK_EQUAL (a[2], 0.0f);
    OIIO_CHECK_EQUAL (a[3], 2.0f);
    // array_view<float>::const_iterator i = a.begin();
    // OIIO_CHECK_EQUAL (*i, 0.0f);
    // ++i;  OIIO_CHECK_EQUAL (*i, 1.0f);
}



void test_array_view_mutable ()
{
    float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 0 };
    array_view<float> a (A);
    OIIO_CHECK_EQUAL (a.size(), 12);
    OIIO_CHECK_EQUAL (a[0], 0.0f);
    OIIO_CHECK_EQUAL (a[1], 1.0f);
    OIIO_CHECK_EQUAL (a[2], 0.0f);
    OIIO_CHECK_EQUAL (a[3], 2.0f);

    a[2] = 42.0f;
    OIIO_CHECK_EQUAL (a[2], 42.0f);
    // array_view<float>::const_iterator i = a.begin();
    // OIIO_CHECK_EQUAL (*i, 0.0f);
    // ++i;  OIIO_CHECK_EQUAL (*i, 1.0f);
}



void test_array_view_initlist ()
{
#if OIIO_CPLUSPLUS_VERSION >= 11
    // Try the array_view syntax with initializer_list.
    array_view<const float> a { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 0 };
    OIIO_CHECK_EQUAL (a.size(), 12);
    OIIO_CHECK_EQUAL (a[0], 0.0f);
    OIIO_CHECK_EQUAL (a[1], 1.0f);
    OIIO_CHECK_EQUAL (a[2], 0.0f);
    OIIO_CHECK_EQUAL (a[3], 2.0f);
#endif
}




void test_array_view_2D ()
{
    float A[] = { 0, 1, 0,   2, 0, 3,   0, 4, 0,   5, 0, 0 };
    array_view<float,2> a (A, bounds<2>(4, 3));
    OIIO_CHECK_EQUAL (a.bounds()[0], 4);
    OIIO_CHECK_EQUAL (a.bounds()[1], 3);
    OIIO_CHECK_EQUAL (a.size(), 12);
    OIIO_CHECK_EQUAL (a[offset<2>(0,0)], 0.0f);
    OIIO_CHECK_EQUAL (a[offset<2>(0,1)], 1.0f);
    OIIO_CHECK_EQUAL (a[offset<2>(0,2)], 0.0f);
    OIIO_CHECK_EQUAL (a[offset<2>(1,0)], 2.0f);
    OIIO_CHECK_EQUAL (a[offset<2>(1,1)], 0.0f);
    OIIO_CHECK_EQUAL (a[offset<2>(1,2)], 3.0f);
#if 0
    // Test this after we add slicing
    OIIO_CHECK_EQUAL (a[0][0], 0.0f);
    OIIO_CHECK_EQUAL (a[0][1], 1.0f);
    OIIO_CHECK_EQUAL (a[0][2], 0.0f);
    OIIO_CHECK_EQUAL (a[1][0], 2.0f);
    OIIO_CHECK_EQUAL (a[1][1], 0.0f);
    OIIO_CHECK_EQUAL (a[1][2], 3.0f);
#endif
}



void test_const_strided_ptr ()
{
    static const float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5 };

    // Make sure it works with unit stride
    strided_ptr<const float> a (A);
    OIIO_CHECK_EQUAL (*a, 0.0f);
    OIIO_CHECK_EQUAL (a[0], 0.0f);
    OIIO_CHECK_EQUAL (a[1], 1.0f);
    OIIO_CHECK_EQUAL (a[2], 0.0f);
    OIIO_CHECK_EQUAL (a[3], 2.0f);

    // All the other tests are with stride of 2 elements
    a = strided_ptr<const float> (&A[1], 2*sizeof(A[0]));
    OIIO_CHECK_EQUAL (*a, 1.0f);
    OIIO_CHECK_EQUAL (a[0], 1.0f);
    OIIO_CHECK_EQUAL (a[1], 2.0f);
    OIIO_CHECK_EQUAL (a[2], 3.0f);
    OIIO_CHECK_EQUAL (a[3], 4.0f);

    ++a;  OIIO_CHECK_EQUAL (*a, 2.0f);
    a++;  OIIO_CHECK_EQUAL (*a, 3.0f);
    ++a;  OIIO_CHECK_EQUAL (*a, 4.0f);
    --a;  OIIO_CHECK_EQUAL (*a, 3.0f);
    a--;  OIIO_CHECK_EQUAL (*a, 2.0f);
    a += 2;  OIIO_CHECK_EQUAL (*a, 4.0f);
    a -= 2;  OIIO_CHECK_EQUAL (*a, 2.0f);
    a = a + 2;  OIIO_CHECK_EQUAL (*a, 4.0f);
    a = a - 2;  OIIO_CHECK_EQUAL (*a, 2.0f);
}



void test_strided_ptr ()
{
    static float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5 };

    // Make sure it works with unit stride
    strided_ptr<float> a (A);
    OIIO_CHECK_EQUAL (*a, 0.0f);
    OIIO_CHECK_EQUAL (a[0], 0.0f);
    OIIO_CHECK_EQUAL (a[1], 1.0f);
    OIIO_CHECK_EQUAL (a[2], 0.0f);
    OIIO_CHECK_EQUAL (a[3], 2.0f);

    // All the other tests are with stride of 2 elements
    a = strided_ptr<float> (&A[1], 2*sizeof(A[0]));
    OIIO_CHECK_EQUAL (*a, 1.0f);
    OIIO_CHECK_EQUAL (a[0], 1.0f);
    OIIO_CHECK_EQUAL (a[1], 2.0f);
    OIIO_CHECK_EQUAL (a[2], 3.0f);
    OIIO_CHECK_EQUAL (a[3], 4.0f);

    ++a;  OIIO_CHECK_EQUAL (*a, 2.0f);
    a++;  OIIO_CHECK_EQUAL (*a, 3.0f);
    ++a;  OIIO_CHECK_EQUAL (*a, 4.0f);
    --a;  OIIO_CHECK_EQUAL (*a, 3.0f);
    a--;  OIIO_CHECK_EQUAL (*a, 2.0f);
    a += 2;  OIIO_CHECK_EQUAL (*a, 4.0f);
    a -= 2;  OIIO_CHECK_EQUAL (*a, 2.0f);
    a = a + 2;  OIIO_CHECK_EQUAL (*a, 4.0f);
    a = a - 2;  OIIO_CHECK_EQUAL (*a, 2.0f);

    *a = 14.0; OIIO_CHECK_EQUAL (*a, 14.0f);
}



void test_array_view_strided ()
{
    static const float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5 };
    array_view_strided<const float> a (&A[1], 5, 2);
    OIIO_CHECK_EQUAL (a.size(), 5);
    OIIO_CHECK_EQUAL (a[0], 1.0f);
    OIIO_CHECK_EQUAL (a[1], 2.0f);
    OIIO_CHECK_EQUAL (a[2], 3.0f);
    OIIO_CHECK_EQUAL (a[3], 4.0f);
    // array_view_strided<const float>::const_iterator i = a.begin();
    // OIIO_CHECK_EQUAL (*i, 1.0f);
    // ++i;  OIIO_CHECK_EQUAL (*i, 2.0f);
}



void test_array_view_strided_mutable ()
{
    static float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5 };
    array_view_strided<float> a (&A[1], 5, 2);
    OIIO_CHECK_EQUAL (a.size(), 5);
    OIIO_CHECK_EQUAL (a[0], 1.0f);
    OIIO_CHECK_EQUAL (a[1], 2.0f);
    OIIO_CHECK_EQUAL (a[2], 3.0f);
    OIIO_CHECK_EQUAL (a[3], 4.0f);
    // array_view_strided<float>::iterator i = a.begin();
    // OIIO_CHECK_EQUAL (*i, 1.0f);
    // ++i;  OIIO_CHECK_EQUAL (*i, 2.0f);
}



void test_image_view ()
{
    const int X = 4, Y = 3, C = 3, Z = 1;
    static const float IMG[Z][Y][X][C] = {   // 4x3 2D image with 3 channels
        {{{0,0,0},  {1,0,1},  {2,0,2},  {3,0,3}},
         {{0,1,4},  {1,1,5},  {2,1,6},  {3,1,7}},
         {{0,2,8},  {1,2,9},  {2,2,10}, {3,2,11}}}
    };

    image_view<const float> I ((const float *)IMG, 3, 4, 3);
    for (int y = 0, i = 0;  y < Y;  ++y) {
        for (int x = 0;  x < X;  ++x, ++i) {
            OIIO_CHECK_EQUAL (I(x,y)[0], x);
            OIIO_CHECK_EQUAL (I(x,y)[1], y);
            OIIO_CHECK_EQUAL (I(x,y)[2], i);
        }
    }
}



void test_image_view_mutable ()
{
    const int X = 4, Y = 3, C = 3, Z = 1;
    static float IMG[Z][Y][X][C] = {   // 4x3 2D image with 3 channels
        {{{0,0,0},  {0,0,0},  {0,0,0},  {0,0,0}},
         {{0,0,0},  {0,0,0},  {0,0,0},  {0,0,0}},
         {{0,0,0},  {0,0,0},  {0,0,0},  {0,0,0}}}
    };

    image_view<float> I ((float *)IMG, 3, 4, 3);
    for (int y = 0, i = 0;  y < Y;  ++y) {
        for (int x = 0;  x < X;  ++x, ++i) {
            I(x,y)[0] = x;
            I(x,y)[1] = y;
            I(x,y)[2] = i;
        }
    }

    for (int y = 0, i = 0;  y < Y;  ++y) {
        for (int x = 0;  x < X;  ++x, ++i) {
            OIIO_CHECK_EQUAL (I(x,y)[0], x);
            OIIO_CHECK_EQUAL (I(x,y)[1], y);
            OIIO_CHECK_EQUAL (I(x,y)[2], i);
        }
    }
}



int main (int argc, char *argv[])
{
    test_offset ();
    test_bounds ();
    test_array_view ();
    test_array_view_mutable ();
    test_array_view_initlist ();
    test_array_view_2D ();
    test_const_strided_ptr ();
    test_strided_ptr ();
    test_array_view_strided ();
    test_array_view_strided_mutable ();
    test_image_view ();
    test_image_view_mutable ();

    return unit_test_failures;
}

