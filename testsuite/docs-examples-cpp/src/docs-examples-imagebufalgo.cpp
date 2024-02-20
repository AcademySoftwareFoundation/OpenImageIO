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

void example_output_error1()
{
    ImageBuf fg, bg;

    // BEGIN-imagebufalgo-output-error1
    // Method 1: Return an image result
    ImageBuf dst = ImageBufAlgo::over (fg, bg);
    if (dst.has_error())
        std::cout << "error: " << dst.geterror() << "\n";
    // END-imagebufalgo-output-error1
}

void example_output_error2()
{
    ImageBuf fg, bg;

    // BEGIN-imagebufalgo-output-error2
    // Method 2: Write into an existing image
    ImageBuf dst;   // will be the output image
    bool ok = ImageBufAlgo::over (dst, fg, bg);
    if (! ok)
        std::cout << "error: " << dst.geterror() << "\n";
    // END-imagebufalgo-output-error2
}


// Section: Pattern Generation

void example_zero()
{
    ImageBuf A("grid.exr");
    ImageBuf B("grid.exr");
    ImageBuf C("grid.exr");

    // BEGIN-imagebufalgo-zero
    // Create a new 3-channel, 512x512 float image filled with 0.0 values.
    ImageBuf zero = ImageBufAlgo::zero (ROI(0,512,0,512,0,1,0,3));

    // Zero out an existing buffer, keeping it the same size and data type
    ImageBufAlgo::zero (A);

    // Zero out just the green channel, leave everything else the same
    ROI roi = B.roi ();
    roi.chbegin = 1; // green
    roi.chend = 2;   // one past the end of the channel region
    ImageBufAlgo::zero (B, roi);

    // Zero out a rectangular region of an existing buffer
    ImageBufAlgo::zero (C, ROI (0, 100, 0, 100));
    // END-imagebufalgo-zero

    zero.write("zero1.exr");
    A.write("zero2.exr");
    B.write("zero3.exr");
    C.write("zero4.exr");
}

void example_fill()
{
    // BEGIN-imagebufalgo-fill
    // Create a new 640x480 RGB image, with a top-to-bottom gradient
    // from red to pink
    float pink[3] = { 1, 0.7, 0.7 };
    float red[3] = { 1, 0, 0 };
    ImageBuf A = ImageBufAlgo::fill (red, pink, ROI(0, 640, 0, 480, 0, 1, 0, 3));

    // Draw a filled red rectangle overtop existing image A.
    ImageBufAlgo::fill (A, red, ROI(50, 100, 75, 175));
    // END-imagebufalgo-fill

    A.write("fill.exr");
}

void example_checker()
{
    // BEGIN-imagebufalgo-checker
    // Create a new 640x480 RGB image, fill it with a two-toned gray
    // checkerboard, the checkers being 64x64 pixels each.
    ImageBuf A (ImageSpec(640, 480, 3, TypeDesc::FLOAT));
    float dark[3] = { 0.1, 0.1, 0.1 };
    float light[3] = { 0.4, 0.4, 0.4 };
    ImageBufAlgo::checker (A, 64, 64, 1, dark, light, 0, 0, 0);
    // END-imagebufalgo-checker

    A.write("checker.exr");
}

void example_noise1()
{
    // BEGIN-imagebufalgo-noise1
    // Create a new 256x256 field of grayscale white noise on [0,1)
    ImageBuf A = ImageBufAlgo::noise ("uniform", 0.0f /*min*/, 1.0f /*max*/,
                                     true /*mono*/, 1 /*seed*/, ROI(0,256,0,256,0,1,0,3));

    // Create a new 256x256 field of grayscale white noise on [0,1)
    ImageBuf B = ImageBufAlgo::noise ("blue", 0.0f /*min*/, 1.0f /*max*/,
                                     true /*mono*/, 1 /*seed*/, ROI(0,256,0,256,0,1,0,3));

    // Add color Gaussian noise to an existing image
    ImageBuf C ("tahoe.tif");
    ImageBufAlgo::noise (C, "gaussian", 0.0f /*mean*/, 0.1f /*stddev*/,
                        false /*mono*/, 1 /*seed*/);

    // Use salt and pepper noise to make occasional random dropouts
    ImageBuf D ("tahoe.tif");
    ImageBufAlgo::noise (D, "salt", 0.0f /*value*/, 0.01f /*portion*/,
                        true /*mono*/, 1 /*seed*/);
    // END-imagebufalgo-noise1

    A.write("noise1.exr");
    B.write("noise2.exr");
    C.write("noise3.exr");
    D.write("noise4.exr");
}

void example_noise2()
{
    // BEGIN-imagebufalgo-noise2
    const ImageBuf& A = ImageBufAlgo::bluenoise_image();
    // END-imagebufalgo-noise2

    A.write("blue-noise.exr");
}

void example_point()
{
    // BEGIN-imagebufalgo-point
    ImageBuf A (ImageSpec (640, 480, 4, TypeDesc::FLOAT));
    float red[4] = { 1, 0, 0, 1 };
    ImageBufAlgo::render_point (A, 50, 100, red);
    // END-imagebufalgo-point

    A.write("point.exr");
}

void example_lines()
{
    // BEGIN-imagebufalgo-lines
    ImageBuf A (ImageSpec (640, 480, 4, TypeDesc::FLOAT));
    float red[4] = { 1, 0, 0, 1 };
    ImageBufAlgo::render_line (A, 10, 60, 250, 20, red);
    ImageBufAlgo::render_line (A, 250, 20, 100, 190, red, true);
    // END-imagebufalgo-lines

    A.write("lines.exr");
}

void example_box()
{
    // BEGIN-imagebufalgo-box
    ImageBuf A (ImageSpec (640, 480, 4, TypeDesc::FLOAT));
    float cyan[4] = { 0, 1, 1, 1 };
    float yellow_transparent[4] = { 0.5, 0.5, 0, 0.5 };
    ImageBufAlgo::render_box (A, 150, 100, 240, 180, cyan);
    ImageBufAlgo::render_box (A, 100, 50, 180, 140, yellow_transparent, true);
    // END-imagebufalgo-box

    A.write("box.exr");
}

void example_text1()
{
    ImageBuf ImgA = ImageBufAlgo::zero(ROI(0, 640, 0, 480, 0, 1, 0, 3));
    ImageBuf ImgB = ImageBufAlgo::zero(ROI(0, 640, 0, 480, 0, 1, 0, 3));

    // BEGIN-imagebufalgo-text1
    ImageBufAlgo::render_text (ImgA, 50, 100, "Hello, world");
    float red[] = { 1, 0, 0, 1 };
    ImageBufAlgo::render_text (ImgA, 100, 200, "Go Big Red!",
                              60, "" /*font name*/, red);

    float white[] = { 1, 1, 1, 1 };
    ImageBufAlgo::render_text (ImgB, 320, 240, "Centered",
                              60, "" /*font name*/, white,
                              ImageBufAlgo::TextAlignX::Center,
                              ImageBufAlgo::TextAlignY::Center);
    // END-imagebufalgo-text1

    ImgA.write("text1.exr");
    ImgB.write("text2.exr");
}

void example_text2()
{
    // BEGIN-imagebufalgo-text2
    // Render text centered in the image, using text_size to find out
    // the size we will need and adjusting the coordinates.
    ImageBuf A (ImageSpec (640, 480, 4, TypeDesc::FLOAT));
    ROI Aroi = A.roi();
    ROI size = ImageBufAlgo::text_size("Centered", 48, "Courier New");
    if (size.defined()) {
        int x = Aroi.xbegin + Aroi.width()/2  - (size.xbegin + size.width()/2);
        int y = Aroi.ybegin + Aroi.height()/2 - (size.ybegin + size.height()/2);
        ImageBufAlgo::render_text(A, x, y, "Centered", 48, "Courier New");
    }
    // END-imagebufalgo-text2
}

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


void example_make_texture()
{
    // BEGIN-imagebufalgo-make-texture
    ImageBuf Input ("grid.exr");
    ImageSpec config;
    config["maketx:highlightcomp"] = 1;
    config["maketx:filtername"] = "lanczos3";
    config["maketx:opaque_detect"] = 1;

    bool ok = ImageBufAlgo::make_texture (ImageBufAlgo::MakeTxTexture,
                                          Input, "texture.exr", config);
    if (! ok)
        std::cout << "make_texture error: " << OIIO::geterror() << "\n";
    // END-imagebufalgo-make-texture
}





int main(int /*argc*/, char** /*argv*/)
{
    // Each example function needs to get called here, or it won't execute
    // as part of the test.
    example1();

    // Section: ImageBufAlgo common principles
    example_output_error1();
    example_output_error2();

    // Section: Pattern Generation
    example_zero();
    example_fill();
    example_checker();
    example_noise1();
    example_noise2();
    example_point();
    example_lines();
    example_box();
    example_text1();
    example_text2();

    // Section: Image transformation and data movement
    example_circular_shift();

    // Section: Image Arithmetic

    // Section: Image comparison and statistics

    // Section: Convolution and frequency-space algorithms

    // Section: Image enhancement / restoration

    // Section: Morphological filters

    // Section: Color space conversion

    // Section: Import / export
    example_make_texture();

    return 0;
}
