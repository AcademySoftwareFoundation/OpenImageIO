// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


///////////////////////////////////////////////////////////////////////////
// This file contains code examples from the ImageBuf chapter of the
// main OpenImageIO documentation.
//
// To add an additional test, replicate the section below. Change
// "example1" to a helpful short name that identifies the example.

// BEGIN-imagebuf-example1
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
// END-imagebuf-example1

//
///////////////////////////////////////////////////////////////////////////

#include <OpenImageIO/imagebuf.h>

// BEGIN-imagebuf-get-pixel-avg
void print_channel_averages(const std::string& filename)
{
    // Set up the ImageBuf and read the file
    ImageBuf buf(filename);
    bool ok = buf.read(0, 0, true, TypeDesc::FLOAT);  // Force a float buffer
    if (!ok)
        return;

    // Initialize a vector to contain the running total
    int nc = buf.nchannels();
    std::vector<float> total(nc, 0.0f);

    // Iterate over all pixels of the image, summing channels separately
    for (ImageBuf::ConstIterator<float> it(buf); !it.done(); ++it)
        for (int c = 0; c < nc; ++c)
            total[c] += it[c];

    // Print the averages
    imagesize_t npixels = buf.spec().image_pixels();
    for (int c = 0; c < nc; ++c)
        std::cout << "Channel " << c << " avg = " << (total[c] / npixels)
                  << "\n";
}

// END-imagebuf-get-pixel-avg

void print_channel_averages_example()
{
    const std::string filename = "findaverages.exr";
    int x_sz                   = 640;
    int y_sz                   = 480;
    ImageBuf A(ImageSpec(x_sz, y_sz, 3, TypeDesc::FLOAT));
    for (int i = 0; i < x_sz; ++i)
        for (int j = 0; j < y_sz; ++j) {
            // Create a square RGB gradient so determining an average is interesting
            A.setpixel(i, j, 0,
                       cspan<float>(
                           { powf(float(i) / (x_sz - 1), 2.0f),
                             powf(float(j) / (y_sz - 1), 2.0f),
                             powf(float(i * j) / (x_sz * y_sz - 1), 2.0f) }));
        }
    if (!A.write(filename)) {
        std::cout << "error: " << A.geterror() << "\n";
    } else {
        print_channel_averages(filename);
    }
}

// BEGIN-imagebuf-set-region-black
bool make_black(ImageBuf& buf, ROI region)
{
    if (buf.spec().format != TypeDesc::FLOAT)
        return false;  // Assume it's a float buffer

    // Clamp the region's channel range to the channels in the image
    region.chend = std::min(region.chend, buf.nchannels());
    // Iterate over all pixels in the region...
    for (ImageBuf::Iterator<float> it(buf, region); !it.done(); ++it) {
        if (!it.exists())  // Make sure the iterator is pointing
            continue;      //   to a pixel in the data window
        for (int c = region.chbegin; c < region.chend; ++c)
            it[c] = 0.0f;  // clear the value
    }
    return true;
}
// END-imagebuf-set-region-black

void make_black_example()
{
    int x_sz = 640;
    int y_sz = 480;
    ImageBuf A(ImageSpec(x_sz, y_sz, 3, TypeDesc::FLOAT));
    for (int i = 0; i < x_sz; ++i)
        for (int j = 0; j < y_sz; ++j) {
            // Create RGB gradient so region changing is easy to see
            A.setpixel(i, j, 0,
                       cspan<float>({ float(i) / (x_sz - 1),
                                      float(j) / (y_sz - 1),
                                      float(i * j) / (x_sz * y_sz - 1) }));
        }
    // A rectangular region straddling the middle of the image above
    ROI region(x_sz / 4, x_sz * 3 / 4, y_sz / 4, y_sz * 3 / 4, 0, 1, 0, 3);
    if (make_black(A, region)) {
        A.write("set-region-black.exr");
    } else {
        std::cout << "error: buffer is not a float buffer\n";
    }
}



// BEGIN-imagebuf-iterator-template
#include <OpenImageIO/half.h>
template<typename BUFT>
static bool make_black_impl(ImageBuf& buf, ROI region)
{
    // Clamp the region's channel range to the channels in the image
    region.chend = std::min(region.chend, buf.nchannels());

    // Iterate over all pixels in the region...
    for (ImageBuf::Iterator<BUFT> it(buf, region); !it.done(); ++it) {
        if (!it.exists())  // Make sure the iterator is pointing
            continue;      //   to a pixel in the data window
        for (int c = region.chbegin; c < region.chend; ++c)
            it[c] = 0.0f;  // clear the value
    }
    return true;
}

bool make_black_templated(ImageBuf& buf, ROI region)
{
    if (buf.spec().format == TypeDesc::FLOAT)
        return make_black_impl<float>(buf, region);
    else if (buf.spec().format == TypeDesc::HALF)
        return make_black_impl<half>(buf, region);
    else if (buf.spec().format == TypeDesc::UINT8)
        return make_black_impl<unsigned char>(buf, region);
    else if (buf.spec().format == TypeDesc::UINT16)
        return make_black_impl<unsigned short>(buf, region);
    else {
        buf.errorf("Unsupported pixel data format %s",
                   buf.spec().format.c_str());
        return false;
    }
}
// END-imagebuf-iterator-template

void make_black_template_example()
{
    int x_sz = 640;
    int y_sz = 480;
    ImageBuf A(ImageSpec(x_sz, y_sz, 3, TypeDesc::FLOAT));
    for (int i = 0; i < x_sz; ++i)
        for (int j = 0; j < y_sz; ++j) {
            // Create RGB gradient so region changing is easy to see
            A.setpixel(i, j, 0,
                       cspan<float>({ float(i) / (x_sz - 1),
                                      float(j) / (y_sz - 1),
                                      float(i * j) / (x_sz * y_sz - 1) }));
        }
    // A rectangular region straddling the middle of the image above
    ROI region(x_sz / 4, x_sz * 3 / 4, y_sz / 4, y_sz * 3 / 4, 0, 1, 0, 3);
    if (make_black_templated(A, region)) {
        A.write("set-region-black-template-float.exr");
    } else {
        std::cout << "error: " << A.geterror() << "\n";
    }

    ImageBuf B(ImageSpec(x_sz, y_sz, 3, TypeDesc::UINT8));
    for (int i = 0; i < x_sz; ++i)
        for (int j = 0; j < y_sz; ++j) {
            // Create RGB gradient so region changing is easy to see
            B.setpixel(i, j, 0,
                       cspan<float>({ float(i) / (x_sz - 1),
                                      float(j) / (y_sz - 1),
                                      float(i * j) / (x_sz * y_sz - 1) }));
        }
    // A rectangular region straddling the middle of the image above
    if (make_black_templated(B, region)) {
        B.write("set-region-black-template-uint8.exr");
    } else {
        std::cout << "error: " << B.geterror() << "\n";
    }
}

// BEGIN-imagebuf-dispatch
#include <OpenImageIO/imagebufalgo_util.h>
bool make_black_dispatch(ImageBuf& buf, ROI region)
{
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES(ok, "make_black_dispatch", make_black_impl,
                               buf.spec().format, buf, region);
    return ok;
}
// END-imagebuf-dispatch

void make_black_dispatch_example()
{
    int x_sz = 640;
    int y_sz = 480;
    ImageBuf A(ImageSpec(x_sz, y_sz, 3, TypeDesc::UINT16));
    for (int i = 0; i < x_sz; ++i)
        for (int j = 0; j < y_sz; ++j) {
            // Create RGB gradient so region changing is easy to see
            A.setpixel(i, j, 0,
                       cspan<float>({ float(i) / (x_sz - 1),
                                      float(j) / (y_sz - 1),
                                      float(i * j) / (x_sz * y_sz - 1) }));
        }
    // A rectangular region straddling the middle of the image above
    ROI region(x_sz / 4, x_sz * 3 / 4, y_sz / 4, y_sz * 3 / 4, 0, 1, 0, 3);
    if (make_black_templated(A, region)) {
        A.write("set-region-black-template-dispatch.exr");
    } else {
        std::cout << "error: " << A.geterror() << "\n";
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    // Each example function needs to get called here, or it won't execute
    // as part of the test.
    // example1();
    print_channel_averages_example();
    make_black_example();
    make_black_template_example();
    make_black_dispatch_example();
    return 0;
}
