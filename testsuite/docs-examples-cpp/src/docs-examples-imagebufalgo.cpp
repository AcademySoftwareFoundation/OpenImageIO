// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


// TRICK: skip deprecated functions in IBA
#define DOXYGEN_SHOULD_SKIP_THIS

#include <Imath/ImathMatrix.h>



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
    print("example_output_error1\n");
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
    print("example_output_error2\n");
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
    print("example_zero\n");
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

    zero.write("zero1.exr", TypeHalf);
    A.write("zero2.exr", TypeHalf);
    B.write("zero3.exr", TypeHalf);
    C.write("zero4.exr", TypeHalf);
}


void example_fill()
{
    print("example_fill\n");
    // BEGIN-imagebufalgo-fill
    // Create a new 640x480 RGB image, with a top-to-bottom gradient
    // from red to pink
    float pink[3] = { 1, 0.7, 0.7 };
    float red[3]  = { 1, 0, 0 };
    ImageBuf A    = ImageBufAlgo::fill(red, cspan<float>(pink),
                                       ROI(0, 640, 0, 480, 0, 1, 0, 3));

    // Draw a filled red rectangle overtop existing image A.
    ImageBufAlgo::fill (A, cspan<float>(red), ROI(50, 100, 75, 175));
    // END-imagebufalgo-fill

    A.write("fill.exr", TypeHalf);
}


void example_checker()
{
    print("example_checker\n");
    // BEGIN-imagebufalgo-checker
    // Create a new 640x480 RGB image, fill it with a two-toned gray
    // checkerboard, the checkers being 64x64 pixels each.
    ImageBuf A(ImageSpec(640, 480, 3, TypeDesc::FLOAT));
    float dark[3]  = { 0.1, 0.1, 0.1 };
    float light[3] = { 0.4, 0.4, 0.4 };
    ImageBufAlgo::checker(A, 64, 64, 1, cspan<float>(dark), cspan<float>(light),
                          0, 0, 0);
    // END-imagebufalgo-checker

    A.write("checker.exr", TypeHalf);
}


void example_noise1()
{
    print("example_noise1\n");
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

    A.write("noise1.exr", TypeHalf);
    B.write("noise2.exr", TypeHalf);
    C.write("noise3.exr", TypeHalf);
    D.write("noise4.exr", TypeHalf);
}


void example_noise2()
{
    print("example_noise2\n");
    // BEGIN-imagebufalgo-noise2
    const ImageBuf& A = ImageBufAlgo::bluenoise_image();
    // END-imagebufalgo-noise2

    A.write("blue-noise.exr", TypeHalf);
}


void example_point()
{
    print("example_point\n");
    // BEGIN-imagebufalgo-point
    ImageBuf A (ImageSpec (640, 480, 4, TypeDesc::FLOAT));
    float red[4] = { 1, 0, 0, 1 };
    ImageBufAlgo::render_point (A, 50, 100, red);
    // END-imagebufalgo-point

    A.write("point.exr", TypeHalf);
}


void example_lines()
{
    print("example_lines\n");
    // BEGIN-imagebufalgo-lines
    ImageBuf A (ImageSpec (640, 480, 4, TypeDesc::FLOAT));
    float red[4] = { 1, 0, 0, 1 };
    ImageBufAlgo::render_line (A, 10, 60, 250, 20, red);
    ImageBufAlgo::render_line (A, 250, 20, 100, 190, red, true);
    // END-imagebufalgo-lines

    A.write("lines.exr", TypeHalf);
}


void example_box()
{
    print("example_box\n");
    // BEGIN-imagebufalgo-box
    ImageBuf A (ImageSpec (640, 480, 4, TypeDesc::FLOAT));
    float cyan[4] = { 0, 1, 1, 1 };
    float yellow_transparent[4] = { 0.5, 0.5, 0, 0.5 };
    ImageBufAlgo::render_box (A, 150, 100, 240, 180, cyan);
    ImageBufAlgo::render_box (A, 100, 50, 180, 140, yellow_transparent, true);
    // END-imagebufalgo-box

    A.write("box.exr", TypeHalf);
}


void example_text1()
{
    print("example_text1\n");
    ImageBuf ImgA = ImageBufAlgo::zero(ROI(0, 640, 0, 480, 0, 1, 0, 3));
    ImageBuf ImgB = ImageBufAlgo::zero(ROI(0, 640, 0, 480, 0, 1, 0, 3));

    // BEGIN-imagebufalgo-text1
    ImageBufAlgo::render_text(ImgA, 50, 100, "Hello, world");
    float red[] = { 1, 0, 0, 1 };
    ImageBufAlgo::render_text(ImgA, 100, 200, "Go Big Red!", 60,
                              "" /*font name*/, cspan<float>(red));

    float white[] = { 1, 1, 1, 1 };
    ImageBufAlgo::render_text(ImgB, 320, 240, "Centered", 60, "" /*font name*/,
                              cspan<float>(white),
                              ImageBufAlgo::TextAlignX::Center,
                              ImageBufAlgo::TextAlignY::Center);
    // END-imagebufalgo-text1

    ImgA.write("text1.exr", TypeHalf);
    ImgB.write("text2.exr", TypeHalf);
}


void example_text2()
{
    print("example_text2\n");
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

void example_channels()
{
    print("example_channels\n");
    ImageBuf RGBA("grid.exr");

    // BEGIN-imagebufalgo-channels
    // Copy the first 3 channels of an RGBA, drop the alpha
    ImageBuf RGB = ImageBufAlgo::channels(RGBA, 3, {} /*default ordering*/);

    // Copy just the alpha channel, making a 1-channel image
    ImageBuf Alpha = ImageBufAlgo::channels(RGBA, 1, 3 /*alpha_channel*/);

    // Swap the R and B channels
    ImageBuf BRGA;
    bool success = ImageBufAlgo::channels(BRGA, RGBA, 4, { 2, 1, 0, 3 }, {},
                                          { "R", "G", "B", "A" });

    // Add an alpha channel with value 1.0 everywhere to an RGB image,
    // keep the other channels with their old ordering, values, and
    // names.
    RGBA = ImageBufAlgo::channels(RGB, 4, { 0, 1, 2, -1 },
                                  { 0 /*ignore*/, 0 /*ignore*/, 0 /*ignore*/,
                                    1.0 },
                                  { "", "", "", "A" });
    // END-imagebufalgo-channels

    RGBA.write("channels-rgba.exr");
    RGB.write("channels-rgb.exr");
    Alpha.write("channels-alpha.exr");
    BRGA.write("channels-brga.exr");
}


void example_channel_append()
{
    print("example_channel_append\n");
    ImageBuf Z(ImageSpec(640, 480, 1, TypeDesc::FLOAT));

    // BEGIN-imagebufalgo-channel-append
    ImageBuf RGBA("grid.exr");
    ImageBuf RGBAZ = ImageBufAlgo::channel_append(RGBA, Z);
    // END-imagebufalgo-channel-append

    RGBAZ.write("channel-append.exr", TypeHalf);
}


void example_copy()
{
    print("example_copy\n");
    // BEGIN-imagebufalgo-copy
    // Set B to be a copy of A, but converted to float
    ImageBuf A("grid.exr");
    ImageBuf B = ImageBufAlgo::copy(A, TypeDesc::FLOAT);
    // END-imagebufalgo-copy

    B.write("copy.exr");
}


void example_crop()
{
    print("example_crop\n");
    // BEGIN-imagebufalgo-crop
    // Set B to be a 200x100 region of A starting at (50,50), trimming
    // the exterior away but leaving that region in its original position.
    ImageBuf A("grid.exr");
    ImageBuf B = ImageBufAlgo::crop(A, ROI(50, 250, 50, 150));
    // END-imagebufalgo-crop

    B.write("crop.exr");
}


void example_cut()
{
    print("example_cut\n");
    // BEGIN-imagebufalgo-cut
    // Set B to be a 200x100 region of A starting at (50,50), but
    // moved to the upper left corner so its new origin is (0,0).
    ImageBuf A("grid.exr");
    ImageBuf B = ImageBufAlgo::cut(A, ROI(50, 250, 50, 150));
    // END-imagebufalgo-cut

    B.write("cut.exr");
}


void example_paste()
{
    print("example_paste\n");
    // BEGIN-imagebufalgo-paste
    // Paste Fg on top of Bg, offset by (100,100)
    ImageBuf Bg("grid.exr");
    ImageBuf Fg("tahoe.tif");
    ImageBufAlgo::paste(Bg, 100, 100, 0, 0, Fg);
    // END-imagebufalgo-paste

    Bg.write("paste.exr");
}


void example_rotate_n()
{
    print("example_rotate_n\n");
    // BEGIN-imagebufalgo-rotate-n
    ImageBuf A("grid.exr");
    ImageBuf R90  = ImageBufAlgo::rotate90(A);
    ImageBuf R180 = ImageBufAlgo::rotate180(A);
    ImageBuf R270 = ImageBufAlgo::rotate270(A);
    // END-imagebufalgo-rotate-n

    R90.write("rotate-90.exr");
    R180.write("rotate-180.exr");
    R270.write("rotate-270.exr");
}


void example_flip_flop_transpose()
{
    print("example_flip_flop_transpose\n");
    // BEGIN-imagebufalgo-flip-flop-transpose
    ImageBuf A("grid.exr");
    ImageBuf B1 = ImageBufAlgo::flip(A);
    ImageBuf B2 = ImageBufAlgo::flop(A);
    ImageBuf B3 = ImageBufAlgo::transpose(A);
    // END-imagebufalgo-flip-flop-transpose

    B1.write("flip.exr");
    B2.write("flop.exr");
    B3.write("transpose.exr");
}


void example_reorient()
{
    print("example_reorient\n");
    ImageBuf tmp("grid.exr");
    tmp.specmod().attribute("Orientation", 8);
    tmp.write("grid-vertical.exr");

    // BEGIN-imagebufalgo-reorient
    ImageBuf A("grid-vertical.exr");
    A = ImageBufAlgo::reorient(A);
    // END-imagebufalgo-reorient

    A.write("reorient.exr");
}


void example_circular_shift()
{
    print("example_circular_shift\n");
    // BEGIN-imagebufalgo-cshift
    ImageBuf A("grid.exr");
    ImageBuf B = ImageBufAlgo::circular_shift(A, 70, 30);
    // END-imagebufalgo-cshift
    B.write("cshift.exr");
}


void example_rotate()
{
    print("example_rotate\n");
    // BEGIN-imagebufalgo-rotate-angle
    ImageBuf Src ("grid.exr");
    ImageBuf Dst = ImageBufAlgo::rotate (Src, 45.0);
    // END-imagebufalgo-rotate-angle
    Dst.write("rotate-45.tif", TypeUInt8);
}


void example_resize()
{
    print("example_resize\n");
    // BEGIN-imagebufalgo-resize
    // Resize the image to 640x480, using the default filter
    ImageBuf Src("grid.exr");
    ROI roi(0, 320, 0, 240, 0, 1, /*chans:*/ 0, Src.nchannels());
    ImageBuf Dst = ImageBufAlgo::resize(Src, {}, roi);
    // END-imagebufalgo-resize
    Dst.write("resize.tif", TypeUInt8);
}


void example_resample()
{
    print("example_resample\n");
    // BEGIN-imagebufalgo-resample
    // Resample quickly to 320x240, with default interpolation
    ImageBuf Src("grid.exr");
    ROI roi(0, 320, 0, 240, 0, 1, /*chans:*/ 0, Src.nchannels());
    ImageBuf Dst = ImageBufAlgo::resample(Src, true, roi);
    // END-imagebufalgo-resample
    Dst.write("resample.exr");
}


void example_fit()
{
    print("example_fit\n");
    // BEGIN-imagebufalgo-fit
    // Resize to fit into a max of 640x480, preserving the aspect ratio
    ImageBuf Src("grid.exr");
    ROI roi(0, 320, 0, 240, 0, 1, /*chans:*/ 0, Src.nchannels());
    ImageBuf Dst = ImageBufAlgo::fit(Src, {}, roi);
    // END-imagebufalgo-fit
    Dst.write("fit.tif", TypeUInt8);
}


void example_warp()
{
    print("example_warp\n");
    // BEGIN-imagebufalgo-warp
    Imath::M33f M( 0.7071068, 0.7071068, 0,
                  -0.7071068, 0.7071068, 0,
                   20,       -8.284271,  1);
    ImageBuf Src("grid.exr");
    ImageBuf Dst = ImageBufAlgo::warp(Src, M, { { "filtername", "lanczos3" } });
    // END-imagebufalgo-warp
    Dst.write("warp.exr");
}

void example_demosaic()
{
    print("example_demosaic\n");
    // BEGIN-imagebufalgo-demosaic
    ImageBuf Src("bayer.png");
    float WB[3] = {2.0, 1.0, 1.5};
    ParamValue options[] = {
        { "layout", "BGGR" },
        ParamValue("white_balance", TypeFloat, 3, WB)
    };
    ImageBuf Dst = ImageBufAlgo::demosaic(Src, options);
    // END-imagebufalgo-demosaic
    Dst.write("demosaic.png");
}


// Section: Image Arithmetic
void example_add()
{
    print("example_add\n");
    // BEGIN-imagebufalgo-add
    // Add images A and B
    ImageBuf A ("A.exr");
    ImageBuf B ("B.exr");
    ImageBuf Sum = ImageBufAlgo::add(A, B);

    // Add 0.2 to channels 0-2, but not to channel 3
    ImageBuf SumCspan = ImageBufAlgo::add(A, { 0.2f, 0.2f, 0.2f, 0.0f });
    // END-imagebufalgo-add
    Sum.write("add.exr");
    SumCspan.write("add-cspan.exr");
}

void example_sub()
{
    print("example_sub\n");
    // BEGIN-imagebufalgo-sub
    ImageBuf A ("A.exr");
    ImageBuf B ("B.exr");
    ImageBuf Diff = ImageBufAlgo::sub(A, B);
    // END-imagebufalgo-sub
    Diff.write("sub.exr");
}

void example_absdiff()
{
    print("example_absdiff\n");
    // BEGIN-imagebufalgo-absdiff
    ImageBuf A ("A.exr");
    ImageBuf B ("B.exr");
    ImageBuf Diff = ImageBufAlgo::absdiff (A, B);
    // END-imagebufalgo-absdiff
    Diff.write("absdiff.exr");
}

void example_abs()
{
    print("example_abs\n");
    // BEGIN-imagebufalgo-absolute
    ImageBuf A("grid.exr");
    ImageBuf Abs = ImageBufAlgo::abs(A);
    // END-imagebufalgo-absolute
    Abs.write("abs.exr");
}

void example_scale()
{
    print("example_scale\n");
    // BEGIN-imagebufalgo-scale
    // Pixel-by-pixel multiplication of all channels of A by the single channel of B
    ImageBuf A("A.exr");
    ImageBuf B("mono.exr");
    ImageBuf Product = ImageBufAlgo::scale(A, B);

    // END-imagebufalgo-scale
    Product.write("scale.exr");
}

void example_mul()
{
    print("example_mul\n");
    // BEGIN-imagebufalgo-mul
    // Pixel-by-pixel, channel-by-channel multiplication of A and B
    ImageBuf A ("A.exr");
    ImageBuf B ("B.exr");
    ImageBuf Product = ImageBufAlgo::mul (A, B);

    // In-place reduce intensity of A's channels 0-2 by 50%
    ImageBufAlgo::mul (A, A, { 0.5f, 0.5f, 0.5f, 1.0f });
    // END-imagebufalgo-mul
    Product.write("mul.exr");
}


void example_div()
{
    print("example_div\n");
    // BEGIN-imagebufalgo-div
    // Pixel-by-pixel, channel-by-channel division of A by B
    ImageBuf A ("A.exr");
    ImageBuf B ("B.exr");
    ImageBuf Ratio = ImageBufAlgo::div (A, B);

    // In-place reduce intensity of A's channels 0-2 by 50%
    ImageBufAlgo::div (A, A, { 2.0f, 2.0f, 2.0f, 1.0f });
    // END-imagebufalgo-div
    Ratio.write("div.exr");
}

//TODO: mad and onwards

// Section: Image comparison and statistics


// Section: Convolution and frequency-space algorithms


// Section: Image enhancement / restoration

void example_fixNonFinite()
{
    print("example_fixNonFinite\n");
    // BEGIN-imagebufalgo-fixNonFinite
    ImageBuf Src ("with_nans.tif");
    int pixelsFixed = 0;
    ImageBufAlgo::fixNonFinite (Src, Src, ImageBufAlgo::NONFINITE_BOX3,
                            &pixelsFixed);
    std::cout << "Repaired " << pixelsFixed << " non-finite pixels\n";
    // END-imagebufalgo-fixNonFinite

    // fixing the nans seems nondeterministic - so not writing out the image
    // Src.write("with_nans_fixed.tif");

}


void example_fillholes_pushpull()
{
    print("example_fillholes_pushpull\n");
    // BEGIN-imagebufalgo-fillholes_pushpull

    ImageBuf Src ("checker_with_alpha.exr");
    ImageBuf Filled = ImageBufAlgo::fillholes_pushpull (Src);

    // END-imagebufalgo-fillholes_pushpull
    Filled.write("checker_with_alpha_filled.exr");

}


void example_median_filter()
{
    print("example_median_filter\n");
    // BEGIN-imagebufalgo-median_filter
    ImageBuf Noisy ("tahoe.tif");
    ImageBuf Clean = ImageBufAlgo::median_filter (Noisy, 3, 3);
    // END-imagebufalgo-median_filter
    Clean.write("tahoe_median_filter.tif");

}


void example_unsharp_mask()
{
    print("example_unsharp_mask\n");
    // BEGIN-imagebufalgo-unsharp_mask
    ImageBuf Blurry ("tahoe.tif");
    ImageBuf Sharp = ImageBufAlgo::unsharp_mask (Blurry, "gaussian", 5.0f);
    // END-imagebufalgo-unsharp_mask
    Sharp.write("tahoe_unsharp_mask.tif");

}


// Section: Morphological filters


// Section: Color space conversion


// Section: Import / export


void example_make_texture()
{
    print("example_make_texture\n");
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
    example_channels();
    example_channel_append();
    example_copy();
    example_crop();
    example_cut();
    example_paste();
    example_rotate_n();
    example_flip_flop_transpose();
    example_reorient();
    example_circular_shift();
    example_rotate();
    example_resize();
    example_resample();
    example_fit();
    example_warp();
    example_demosaic();

    // Section: Image Arithmetic
    example_add();
    example_sub();
    example_absdiff();
    example_abs();
    example_scale();
    example_mul();
    example_div();

    // Section: Image comparison and statistics

    // Section: Convolution and frequency-space algorithms

    // Section: Image enhancement / restoration

    example_fixNonFinite();
    example_fillholes_pushpull();
    example_median_filter();
    example_unsharp_mask();

    // Section: Morphological filters

    // Section: Color space conversion

    // Section: Import / export
    example_make_texture();

    return 0;
}
