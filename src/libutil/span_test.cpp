// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <array>
#include <iostream>
#include <vector>

#include <OpenImageIO/array_view.h>
#include <OpenImageIO/image_view.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/strided_ptr.h>
#include <OpenImageIO/unittest.h>

using namespace OIIO;



void
test_span()
{
    static float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 0 };
    cspan<float> a(A);
    OIIO_CHECK_EQUAL(a.size(), 12);
    OIIO_CHECK_EQUAL(std::size(a), size_t(12));
    OIIO_CHECK_EQUAL(std::ssize(a), int(12));
    OIIO_CHECK_EQUAL(a[0], 0.0f);
    OIIO_CHECK_EQUAL(a[1], 1.0f);
    OIIO_CHECK_EQUAL(a[2], 0.0f);
    OIIO_CHECK_EQUAL(a[3], 2.0f);

    OIIO_CHECK_EQUAL(&a.front(), &a[0]);
    OIIO_CHECK_EQUAL(&a.back(), &a[a.size() - 1]);

    OIIO_CHECK_EQUAL(a.begin(), &a[0]);
    OIIO_CHECK_EQUAL(a.end(), &a[a.size()]);
    OIIO_CHECK_EQUAL(a.cbegin(), &a[0]);
    OIIO_CHECK_EQUAL(a.cend(), &a[a.size()]);

    span<float>::const_iterator i = a.begin();
    OIIO_CHECK_EQUAL(*i, 0.0f);
    ++i;
    OIIO_CHECK_EQUAL(*i, 1.0f);

    // Test == and !=
    float v12[]  = { 1, 2 };
    float v123[] = { 1, 2, 3 };
    float v124[] = { 1, 2, 4 };
    OIIO_CHECK_ASSERT(cspan<float>(v123) == cspan<float>(v123));
    OIIO_CHECK_ASSERT(false == (cspan<float>(v123) != cspan<float>(v123)));
    OIIO_CHECK_ASSERT(cspan<float>(v123) != cspan<float>(v12));
    OIIO_CHECK_ASSERT(false == (cspan<float>(v123) == cspan<float>(v12)));
    OIIO_CHECK_ASSERT(cspan<float>(v123) != cspan<float>(v124));
}



void
test_span_mutable()
{
    float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 0 };
    span<float> a(A);
    OIIO_CHECK_EQUAL(a.size(), 12);
    OIIO_CHECK_EQUAL(std::size(a), size_t(12));
    OIIO_CHECK_EQUAL(std::ssize(a), int(12));
    OIIO_CHECK_EQUAL(a[0], 0.0f);
    OIIO_CHECK_EQUAL(a[1], 1.0f);
    OIIO_CHECK_EQUAL(a[2], 0.0f);
    OIIO_CHECK_EQUAL(a[3], 2.0f);

    OIIO_CHECK_EQUAL(&a.front(), &a[0]);
    OIIO_CHECK_EQUAL(&a.back(), &a[a.size() - 1]);

    a[2] = 42.0f;
    OIIO_CHECK_EQUAL(a[2], 42.0f);
    // span<float>::const_iterator i = a.begin();
    // OIIO_CHECK_EQUAL (*i, 0.0f);
    // ++i;  OIIO_CHECK_EQUAL (*i, 1.0f);
}



void
test_span_initlist_called(cspan<float> a)
{
    OIIO_CHECK_EQUAL(a.size(), 12);
    OIIO_CHECK_EQUAL(a[0], 0.0f);
    OIIO_CHECK_EQUAL(a[1], 1.0f);
    OIIO_CHECK_EQUAL(a[2], 0.0f);
    OIIO_CHECK_EQUAL(a[3], 2.0f);
}



void
test_span_initlist()
{
    // Exercise the span syntax with initializer_list.
    test_span_initlist_called({ 0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f, 4.0f,
                                0.0f, 5.0f, 0.0f, 0.0f });
}



void
test_span_vector()
{
    // Try the span syntax with a view of a std::vector
    std::vector<float> arr { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 0 };

    span<float> a(arr);
    OIIO_CHECK_EQUAL(a.size(), 12);
    OIIO_CHECK_EQUAL(a[0], 0.0f);
    OIIO_CHECK_EQUAL(a[1], 1.0f);
    OIIO_CHECK_EQUAL(a[2], 0.0f);
    OIIO_CHECK_EQUAL(a[3], 2.0f);
}



void
test_span_stdarray()
{
    // Try the span syntax with a view of a std::vector
    std::array<float, 12> arr { { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 0 } };

    span<float> a(arr);
    OIIO_CHECK_EQUAL(a.size(), 12);
    OIIO_CHECK_EQUAL(a[0], 0.0f);
    OIIO_CHECK_EQUAL(a[1], 1.0f);
    OIIO_CHECK_EQUAL(a[2], 0.0f);
    OIIO_CHECK_EQUAL(a[3], 2.0f);
}



void
test_const_strided_ptr()
{
    static const float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5 };

    // Make sure it works with unit stride
    strided_ptr<const float> a(A);
    OIIO_CHECK_EQUAL(*a, 0.0f);
    OIIO_CHECK_EQUAL(a[0], 0.0f);
    OIIO_CHECK_EQUAL(a[1], 1.0f);
    OIIO_CHECK_EQUAL(a[2], 0.0f);
    OIIO_CHECK_EQUAL(a[3], 2.0f);

    // All the other tests are with stride of 2 elements
    a = strided_ptr<const float>(&A[1], 2);
    OIIO_CHECK_EQUAL(*a, 1.0f);
    OIIO_CHECK_EQUAL(a[0], 1.0f);
    OIIO_CHECK_EQUAL(a[1], 2.0f);
    OIIO_CHECK_EQUAL(a[2], 3.0f);
    OIIO_CHECK_EQUAL(a[3], 4.0f);

    ++a;
    OIIO_CHECK_EQUAL(*a, 2.0f);
    a++;
    OIIO_CHECK_EQUAL(*a, 3.0f);
    ++a;
    OIIO_CHECK_EQUAL(*a, 4.0f);
    --a;
    OIIO_CHECK_EQUAL(*a, 3.0f);
    a--;
    OIIO_CHECK_EQUAL(*a, 2.0f);
    a += 2;
    OIIO_CHECK_EQUAL(*a, 4.0f);
    a -= 2;
    OIIO_CHECK_EQUAL(*a, 2.0f);
    a = a + 2;
    OIIO_CHECK_EQUAL(*a, 4.0f);
    a = a - 2;
    OIIO_CHECK_EQUAL(*a, 2.0f);
}



void
test_strided_ptr()
{
    static float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5 };

    // Make sure it works with unit stride
    strided_ptr<float> a(A);
    OIIO_CHECK_EQUAL(*a, 0.0f);
    OIIO_CHECK_EQUAL(a[0], 0.0f);
    OIIO_CHECK_EQUAL(a[1], 1.0f);
    OIIO_CHECK_EQUAL(a[2], 0.0f);
    OIIO_CHECK_EQUAL(a[3], 2.0f);

    // All the other tests are with stride of 2 elements
    a = strided_ptr<float>(&A[1], 2);
    OIIO_CHECK_EQUAL(*a, 1.0f);
    OIIO_CHECK_EQUAL(a[0], 1.0f);
    OIIO_CHECK_EQUAL(a[1], 2.0f);
    OIIO_CHECK_EQUAL(a[2], 3.0f);
    OIIO_CHECK_EQUAL(a[3], 4.0f);

    ++a;
    OIIO_CHECK_EQUAL(*a, 2.0f);
    a++;
    OIIO_CHECK_EQUAL(*a, 3.0f);
    ++a;
    OIIO_CHECK_EQUAL(*a, 4.0f);
    --a;
    OIIO_CHECK_EQUAL(*a, 3.0f);
    a--;
    OIIO_CHECK_EQUAL(*a, 2.0f);
    a += 2;
    OIIO_CHECK_EQUAL(*a, 4.0f);
    a -= 2;
    OIIO_CHECK_EQUAL(*a, 2.0f);
    a = a + 2;
    OIIO_CHECK_EQUAL(*a, 4.0f);
    a = a - 2;
    OIIO_CHECK_EQUAL(*a, 2.0f);

    *a = 14.0;
    OIIO_CHECK_EQUAL(*a, 14.0f);
}



void
test_span_strided()
{
    static const float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5 };
    span_strided<const float> a(&A[1], 5, 2);
    OIIO_CHECK_EQUAL(a.size(), 5);
    OIIO_CHECK_EQUAL(a[0], 1.0f);
    OIIO_CHECK_EQUAL(a[1], 2.0f);
    OIIO_CHECK_EQUAL(a[2], 3.0f);
    OIIO_CHECK_EQUAL(a[3], 4.0f);
    // span_strided<const float>::const_iterator i = a.begin();
    // OIIO_CHECK_EQUAL (*i, 1.0f);
    // ++i;  OIIO_CHECK_EQUAL (*i, 2.0f);
}



void
test_span_strided_mutable()
{
    static float A[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5 };
    span_strided<float> a(&A[1], 5, 2);
    OIIO_CHECK_EQUAL(a.size(), 5);
    OIIO_CHECK_EQUAL(a[0], 1.0f);
    OIIO_CHECK_EQUAL(a[1], 2.0f);
    OIIO_CHECK_EQUAL(a[2], 3.0f);
    OIIO_CHECK_EQUAL(a[3], 4.0f);
    // span_strided<float>::iterator i = a.begin();
    // OIIO_CHECK_EQUAL (*i, 1.0f);
    // ++i;  OIIO_CHECK_EQUAL (*i, 2.0f);
}



void
test_image_view()
{
    const int X = 4, Y = 3, C = 3, Z = 1;
    static const float IMG[Z][Y][X][C] = {
        // 4x3 2D image with 3 channels
        { { { 0, 0, 0 }, { 1, 0, 1 }, { 2, 0, 2 }, { 3, 0, 3 } },
          { { 0, 1, 4 }, { 1, 1, 5 }, { 2, 1, 6 }, { 3, 1, 7 } },
          { { 0, 2, 8 }, { 1, 2, 9 }, { 2, 2, 10 }, { 3, 2, 11 } } }
    };

    image_view<const float> I((const float*)IMG, 3, 4, 3);
    for (int y = 0, i = 0; y < Y; ++y) {
        for (int x = 0; x < X; ++x, ++i) {
            OIIO_CHECK_EQUAL(I(x, y)[0], x);
            OIIO_CHECK_EQUAL(I(x, y)[1], y);
            OIIO_CHECK_EQUAL(I(x, y)[2], i);
        }
    }
}



void
test_image_view_mutable()
{
    const int X = 4, Y = 3, C = 3, Z = 1;
    static float IMG[Z][Y][X][C] = {
        // 4x3 2D image with 3 channels
        { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
          { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
          { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } } }
    };

    image_view<float> I((float*)IMG, 3, 4, 3);
    for (int y = 0, i = 0; y < Y; ++y) {
        for (int x = 0; x < X; ++x, ++i) {
            I(x, y)[0] = x;
            I(x, y)[1] = y;
            I(x, y)[2] = i;
        }
    }

    for (int y = 0, i = 0; y < Y; ++y) {
        for (int x = 0; x < X; ++x, ++i) {
            OIIO_CHECK_EQUAL(I(x, y)[0], x);
            OIIO_CHECK_EQUAL(I(x, y)[1], y);
            OIIO_CHECK_EQUAL(I(x, y)[2], i);
        }
    }
}



int
main(int /*argc*/, char* /*argv*/[])
{
    test_span();
    test_span_mutable();
    test_span_initlist();
    test_span_vector();
    test_span_stdarray();
    test_const_strided_ptr();
    test_strided_ptr();
    test_span_strided();
    test_span_strided_mutable();
    test_image_view();
    test_image_view_mutable();

    // array_view and span should be synonyms
    OIIO_CHECK_ASSERT((
        std::is_same<OIIO::cspan<float>, OIIO::array_view<const float>>::value));

    return unit_test_failures;
}
