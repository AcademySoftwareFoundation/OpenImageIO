// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


///////////////////////////////////////////////////////////////////////////
// This file contains code examples from the ImageBufAlgo chapter of the
// main OpenImageIO documentation.
//
// To add an additional test, replicate the section below. Change
// "example1" to a helpful short name that identifies the example.

// BEGIN-imagebufalgo-example1
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
using namespace OIIO;

void example1()
{
    //
    // Example code fragment from the docs goes here.
    //
    // It probably should generate either some text output (which will show up
    // in "out.txt" that captures each test's output), or it should produce a
    // (small) image file that can be compared against a reference image that
    // goes in the ref/ subdirectory of this test.
    //
}
// END-imagebufalgo-example1

//
///////////////////////////////////////////////////////////////////////////


// Section: ImageBufAlgo common principles


// Section: Pattern Generation


// Section: Image transformation and data movement

void example_circular_shift()
{
// BEGIN-imagebufalgo-cshift
    ImageBuf A("grid.exr");
    ImageBuf B = ImageBufAlgo::circular_shift(A, 70, 30);
    B.write("cshift.exr");
// END-imagebufalgo-cshift
}



// Section: Image Arithmetic


// Section: Image comparison and statistics


// Section: Convolution and frequency-space algorithms


// Section: Image enhancement / restoration


// Section: Morphological filters


// Section: Color space conversion


// Section: Import / export





int main(int /*argc*/, char** /*argv*/)
{
    // Each example function needs to get called here, or it won't execute
    // as part of the test.
    example1();

    // Section: ImageBufAlgo common principles

    // Section: Pattern Generation

    // Section: Image transformation and data movement
    example_circular_shift();

    // Section: Image Arithmetic

    // Section: Image comparison and statistics

    // Section: Convolution and frequency-space algorithms

    // Section: Image enhancement / restoration

    // Section: Morphological filters

    // Section: Color space conversion

    // Section: Import / export

    return 0;
}
