// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


///////////////////////////////////////////////////////////////////////////
// This file contains code examples from the ImageInput chapter of the
// main OpenImageIO documentation.
//
// To add an additional test, replicate the section below. Change
// "example1" to a helpful short name that identifies the example.

// BEGIN-imageinput-example1
#include <OpenImageIO/imageio.h>
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
// END-imageinput-example1

//
///////////////////////////////////////////////////////////////////////////



// BEGIN-imageinput-simple
#include <OpenImageIO/imageio.h>
using namespace OIIO;

void simple_read()
{
    const char* filename = "tahoe.tif";

    auto inp = ImageInput::open(filename);
    if (! inp)
        return;
    const ImageSpec &spec = inp->spec();
    int xres = spec.width;
    int yres = spec.height;
    int nchannels = spec.nchannels;
    auto pixels = std::unique_ptr<unsigned char[]>(new unsigned char[xres * yres * nchannels]);
    inp->read_image(0, 0, 0, nchannels, TypeDesc::UINT8, &pixels[0]);
    inp->close();
}

// END-imageinput-simple



int main(int /*argc*/, char** /*argv*/)
{
    // Each example function needs to get called here, or it won't execute
    // as part of the test.
    simple_read();
    return 0;
}
