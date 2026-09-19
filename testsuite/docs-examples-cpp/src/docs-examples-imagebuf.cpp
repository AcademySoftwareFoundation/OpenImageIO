// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


///////////////////////////////////////////////////////////////////////////
// This file contains code examples from the ImageBuf chapter of the
// main OpenImageIO documentation.

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/imageio.h>

using namespace OIIO;


// BEGIN-imagebuf-perpixel-op-function
// Approach 1: using a standalone function to add two images
bool
my_add(span<float> r, cspan<float> a, cspan<float> b)
{
    for (size_t c = 0, nc = size_t(r.size()); c < nc; ++c)
        r[c] = a[c] + b[c];
    return true;
}
// END-imagebuf-perpixel-op-function


// BEGIN-imagebuf-perpixel-op-functor
// Approach 2: using a "functor" class to add two images
struct Adder {
    bool operator()(span<float> r, cspan<float> a, cspan<float> b)
    {
        for (size_t c = 0, nc = size_t(r.size()); c < nc; ++c)
            r[c] = a[c] + b[c];
        return true;
    }
};
// END-imagebuf-perpixel-op-functor


void
example_perpixel_op()
{
    print("example_perpixel_op\n");

    // Test setup. These lines are intentionally outside the documentation
    // snippets because the documentation assumes that A and B already exist.
    ImageSpec spec(1, 1, 3, TypeDesc::FLOAT);
    ImageBuf A(spec);
    ImageBuf B(spec);

    float a[3] = { 1.0f, 2.0f, 3.0f };
    float b[3] = { 4.0f, 5.0f, 6.0f };

    A.setpixel(0, 0, make_span(a));
    B.setpixel(0, 0, make_span(b));

    // BEGIN-imagebuf-perpixel-op-function-call
    ImageBuf R = ImageBufAlgo::perpixel_op(A, B, my_add);
    // END-imagebuf-perpixel-op-function-call

    ImageBuf::ConstIterator<float> p1(R);
    print("perpixel_op standalone: {} {} {}\n", p1[0], p1[1], p1[2]);

    // BEGIN-imagebuf-perpixel-op-functor-call
    Adder adder;
    R = ImageBufAlgo::perpixel_op(A, B, adder);
    // END-imagebuf-perpixel-op-functor-call

    ImageBuf::ConstIterator<float> p2(R);
    print("perpixel_op functor: {} {} {}\n", p2[0], p2[1], p2[2]);

    // BEGIN-imagebuf-perpixel-op-lambda
    // Approach 3: using a lambda to add two images
    R = ImageBufAlgo::perpixel_op(
        A, B, [](span<float> r, cspan<float> a, cspan<float> b) {
            for (size_t c = 0, nc = size_t(r.size()); c < nc; ++c)
                r[c] = a[c] + b[c];
            return true;
        });
    // END-imagebuf-perpixel-op-lambda

    ImageBuf::ConstIterator<float> p3(R);
    print("perpixel_op lambda: {} {} {}\n", p3[0], p3[1], p3[2]);
}


//
///////////////////////////////////////////////////////////////////////////


int
main(int /*argc*/, char** /*argv*/)
{
    // Each example function needs to get called here, or it won't execute
    // as part of the test.
    example_perpixel_op();
    return 0;
}
