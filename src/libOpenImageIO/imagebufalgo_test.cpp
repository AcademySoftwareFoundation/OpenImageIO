// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>

// Must be first to ensure that half is defined before typedesc.h included
#include <OpenImageIO/half.h>

#include <OpenImageIO/platform.h>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/half.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/unittest.h>

#include "imagebufalgo_demosaic_prv.h"

#if USE_OPENCV
#    include <OpenImageIO/imagebufalgo_opencv.h>
#endif

using namespace OIIO;


static int iterations     = 1;
static int numthreads     = 16;
static int ntrials        = 1;
static bool verbose       = false;
static bool wedge         = false;
static int threadcounts[] = { 1,  2,  4,  8,  12,  16,   20,
                              24, 28, 32, 64, 128, 1024, 1 << 30 };


static void
getargs(int argc, char* argv[])
{
    ArgParse ap;
    // clang-format off
    ap.intro("imagebufalgo_test\n" OIIO_INTRO_STRING)
      .usage("imagebufalgo_test [options]");

    ap.arg("-v", &verbose)
      .help("Verbose mode");
    ap.arg("--threads %d", &numthreads)
      .help(Strutil::fmt::format("Number of threads (default: {})", numthreads));
    ap.arg("--iters %d", &iterations)
      .help(Strutil::fmt::format("Number of iterations (default: {})", iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");
    ap.arg("--wedge", &wedge)
      .help("Do a wedge test");
    // clang-format on

    ap.parse(argc, (const char**)argv);
}



void
test_type_merge()
{
    std::cout << "test type_merge\n";
    OIIO_CHECK_EQUAL(TypeDesc::basetype_merge(TypeDesc::UINT8, TypeDesc::UINT8),
                     TypeDesc::UINT8);
    OIIO_CHECK_EQUAL(TypeDesc::basetype_merge(TypeDesc::UINT8, TypeDesc::FLOAT),
                     TypeDesc::FLOAT);
    OIIO_CHECK_EQUAL(TypeDesc::basetype_merge(TypeDesc::FLOAT, TypeDesc::UINT8),
                     TypeDesc::FLOAT);
    OIIO_CHECK_EQUAL(TypeDesc::basetype_merge(TypeDesc::UINT8, TypeDesc::UINT16),
                     TypeDesc::UINT16);
    OIIO_CHECK_EQUAL(TypeDesc::basetype_merge(TypeDesc::UINT16, TypeDesc::FLOAT),
                     TypeDesc::FLOAT);
    OIIO_CHECK_EQUAL(TypeDesc::basetype_merge(TypeDesc::HALF, TypeDesc::FLOAT),
                     TypeDesc::FLOAT);
    OIIO_CHECK_EQUAL(TypeDesc::basetype_merge(TypeDesc::HALF, TypeDesc::UINT8),
                     TypeDesc::HALF);
    OIIO_CHECK_EQUAL(TypeDesc::basetype_merge(TypeDesc::HALF, TypeDesc::UNKNOWN),
                     TypeDesc::HALF);
    OIIO_CHECK_EQUAL(TypeDesc::basetype_merge(TypeDesc::FLOAT,
                                              TypeDesc::UNKNOWN),
                     TypeDesc::FLOAT);
    OIIO_CHECK_EQUAL(TypeDesc::basetype_merge(TypeDesc::UINT8,
                                              TypeDesc::UNKNOWN),
                     TypeDesc::UINT8);
}



// Helper: make an IB filled with a constant value, with a spec that
// describes the image shape.
static ImageBuf
filled_image(cspan<float> value, const ImageSpec& spec)
{
    ImageBuf buf(spec);
    ImageBufAlgo::fill(buf, value);
    return buf;
}

// Helper: make an IB filled with a constant value, with given resolution and
// data type (defaulting to 4x4 float), with number of channels determined by
// the size of the value array).
static ImageBuf
filled_image(cspan<float> value, int width = 4, int height = 4,
             TypeDesc dtype = TypeDesc::FLOAT)
{
    ImageSpec spec(width, height, std::ssize(value), dtype);
    return filled_image(value, spec);
}

// Helper: make a 4x4 IB filled with a constant value, with given data type
// (defaulting to float), with number of channels determined by the size of
// the value array).
inline ImageBuf
filled_image(cspan<float> value, TypeDesc dtype)
{
    return filled_image(value, 4, 4, dtype);
}



// Test ImageBuf::zero and ImageBuf::fill
void
test_zero_fill()
{
    std::cout << "test zero_fill\n";
    const int WIDTH    = 8;
    const int HEIGHT   = 6;
    const int CHANNELS = 4;
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    spec.alpha_channel = 3;

    // Create a buffer -- pixels should be undefined
    ImageBuf A(spec);

    // Set a pixel to an odd value, make sure it takes
    const float arbitrary1[CHANNELS] = { 0.2f, 0.3f, 0.4f, 0.5f };
    A.setpixel(1, 1, make_span(arbitrary1));
    float pixel[CHANNELS];  // test pixel
    A.getpixel(1, 1, make_span(pixel));
    for (int c = 0; c < CHANNELS; ++c)
        OIIO_CHECK_EQUAL(pixel[c], arbitrary1[c]);

    // Zero out and test that it worked
    ImageBufAlgo::zero(A);
    for (int j = 0; j < HEIGHT; ++j) {
        for (int i = 0; i < WIDTH; ++i) {
            float pixel[CHANNELS];
            A.getpixel(i, j, make_span(pixel));
            for (int c = 0; c < CHANNELS; ++c)
                OIIO_CHECK_EQUAL(pixel[c], 0.0f);
        }
    }

    // Test fill of whole image
    const float arbitrary2[CHANNELS] = { 0.6f, 0.7f, 0.3f, 0.9f };
    ImageBufAlgo::fill(A, cspan<float>(arbitrary2));
    for (int j = 0; j < HEIGHT; ++j) {
        for (int i = 0; i < WIDTH; ++i) {
            float pixel[CHANNELS];
            A.getpixel(i, j, make_span(pixel));
            for (int c = 0; c < CHANNELS; ++c)
                OIIO_CHECK_EQUAL(pixel[c], arbitrary2[c]);
        }
    }

    // Test fill of partial image
    const float arbitrary3[CHANNELS] = { 0.42f, 0.43f, 0.44f, 0.45f };
    {
        const int xbegin = 3, xend = 5, ybegin = 0, yend = 4;
        ImageBufAlgo::fill(A, cspan<float>(arbitrary3),
                           ROI(xbegin, xend, ybegin, yend));
        for (int j = 0; j < HEIGHT; ++j) {
            for (int i = 0; i < WIDTH; ++i) {
                float pixel[CHANNELS];
                A.getpixel(i, j, make_span(pixel));
                if (j >= ybegin && j < yend && i >= xbegin && i < xend) {
                    for (int c = 0; c < CHANNELS; ++c)
                        OIIO_CHECK_EQUAL(pixel[c], arbitrary3[c]);
                } else {
                    for (int c = 0; c < CHANNELS; ++c)
                        OIIO_CHECK_EQUAL(pixel[c], arbitrary2[c]);
                }
            }
        }
    }

    // Timing
    Benchmarker bench;
    ImageBuf buf_rgba_float(ImageSpec(1000, 1000, 4, TypeFloat));
    ImageBuf buf_rgba_uint8(ImageSpec(1000, 1000, 4, TypeUInt8));
    ImageBuf buf_rgba_half(ImageSpec(1000, 1000, 4, TypeHalf));
    ImageBuf buf_rgba_uint16(ImageSpec(1000, 1000, 4, TypeDesc::UINT16));
    float vals[] = { 0, 0, 0, 0 };
    bench("  IBA::fill float[4] ",
          [&]() { ImageBufAlgo::fill(buf_rgba_float, cspan<float>(vals)); });
    bench("  IBA::fill uint8[4] ",
          [&]() { ImageBufAlgo::fill(buf_rgba_uint8, cspan<float>(vals)); });
    bench("  IBA::fill uint16[4] ",
          [&]() { ImageBufAlgo::fill(buf_rgba_uint16, cspan<float>(vals)); });
    bench("  IBA::fill half[4] ",
          [&]() { ImageBufAlgo::fill(buf_rgba_half, cspan<float>(vals)); });
}



// Test ImageBuf::copy
void
test_copy()
{
    std::cout << "test copy\n";

    // Make image A red, image B green, copy part of B to A and check result
    const int WIDTH = 4, HEIGHT = 4, CHANNELS = 4;
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    // copy region we'll work with
    ROI roi(2, 4, 1, 3);
    ImageBuf A(spec), B(spec);
    float red[4]   = { 1, 0, 0, 1 };
    float green[4] = { 0, 0, 0.5, 0.5 };
    ImageBufAlgo::fill(A, cspan<float>(red));
    ImageBufAlgo::fill(B, cspan<float>(green));
    ImageBufAlgo::copy(A, B, TypeUnknown, roi);
    for (ImageBuf::ConstIterator<float> r(A); !r.done(); ++r) {
        if (roi.contains(r.x(), r.y())) {
            for (int c = 0; c < CHANNELS; ++c)
                OIIO_CHECK_EQUAL(r[c], green[c]);
        } else {
            for (int c = 0; c < CHANNELS; ++c)
                OIIO_CHECK_EQUAL(r[c], red[c]);
        }
    }

    // Test copying into a blank image
    A.clear();
    ImageBufAlgo::copy(A, B, TypeUnknown, roi);
    for (ImageBuf::ConstIterator<float> r(A); !r.done(); ++r) {
        if (roi.contains(r.x(), r.y())) {
            for (int c = 0; c < CHANNELS; ++c)
                OIIO_CHECK_EQUAL(r[c], green[c]);
        } else {
            for (int c = 0; c < CHANNELS; ++c)
                OIIO_CHECK_EQUAL(r[c], 0.0f);
        }
    }

    // Timing
    Benchmarker bench;
    ImageSpec spec_rgba_float(1000, 1000, 4, TypeFloat);
    ImageSpec spec_rgba_uint8(1000, 1000, 4, TypeUInt8);
    ImageSpec spec_rgba_half(1000, 1000, 4, TypeHalf);
    ImageSpec spec_rgba_int16(1000, 1000, 4, TypeDesc::INT16);
    ImageBuf buf_rgba_uint8(spec_rgba_uint8);
    ImageBuf buf_rgba_float(spec_rgba_float);
    ImageBuf buf_rgba_float2(spec_rgba_float);
    ImageBuf buf_rgba_half(spec_rgba_half);
    ImageBuf buf_rgba_half2(spec_rgba_half);
    ImageBuf empty;
    bench("  IBA::copy float[4] -> float[4] ",
          [&]() { ImageBufAlgo::copy(buf_rgba_float, buf_rgba_float2); });
    bench("  IBA::copy float[4] -> empty ", [&]() {
        empty.clear();
        ImageBufAlgo::copy(empty, buf_rgba_float2);
    });
    bench("  IBA::copy float[4] -> uint8[4] ",
          [&]() { ImageBufAlgo::copy(buf_rgba_uint8, buf_rgba_float2); });
    bench("  IBA::copy half[4] -> half[4] ",
          [&]() { ImageBufAlgo::copy(buf_rgba_half, buf_rgba_half2); });
    bench("  IBA::copy half[4] -> empty ", [&]() {
        empty.clear();
        ImageBufAlgo::copy(empty, buf_rgba_half2);
    });
}



// Test ImageBuf::crop
void
test_crop()
{
    std::cout << "test crop\n";
    int WIDTH = 8, HEIGHT = 6, CHANNELS = 4;
    // Crop region we'll work with
    int xbegin = 3, xend = 5, ybegin = 0, yend = 4;
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    spec.alpha_channel = 3;
    ImageBuf A, B;
    A.reset(spec);
    B.reset(spec);
    float arbitrary1[4];
    arbitrary1[0] = 0.2f;
    arbitrary1[1] = 0.3f;
    arbitrary1[2] = 0.4f;
    arbitrary1[3] = 0.5f;
    ImageBufAlgo::fill(A, cspan<float>(arbitrary1));

    // Test CUT crop
    ImageBufAlgo::crop(B, A, ROI(xbegin, xend, ybegin, yend));

    // Should have changed the data window (origin and width/height)
    OIIO_CHECK_EQUAL(B.spec().x, xbegin);
    OIIO_CHECK_EQUAL(B.spec().width, xend - xbegin);
    OIIO_CHECK_EQUAL(B.spec().y, ybegin);
    OIIO_CHECK_EQUAL(B.spec().height, yend - ybegin);
    span<float> pixel = OIIO_ALLOCA_SPAN(float, CHANNELS);
    for (int j = 0; j < B.spec().height; ++j) {
        for (int i = 0; i < B.spec().width; ++i) {
            B.getpixel(i + B.xbegin(), j + B.ybegin(), pixel);
            // Inside the crop region should match what it always was
            for (int c = 0; c < CHANNELS; ++c)
                OIIO_CHECK_EQUAL(pixel[c], arbitrary1[c]);
        }
    }
}



void
test_paste()
{
    std::cout << "test paste\n";
    // Create the source image, make it a color gradient
    ImageSpec Aspec(4, 4, 3, TypeDesc::FLOAT);
    ImageBuf A(Aspec);
    for (ImageBuf::Iterator<float> it(A); !it.done(); ++it) {
        it[0] = float(it.x()) / float(Aspec.width - 1);
        it[1] = float(it.y()) / float(Aspec.height - 1);
        it[2] = 0.1f;
    }

    // Create destination image -- fill with grey
    ImageSpec Bspec(8, 8, 3, TypeDesc::FLOAT);
    ImageBuf B(Bspec);
    float gray[3] = { 0.1f, 0.1f, 0.1f };
    ImageBufAlgo::fill(B, cspan<float>(gray));

    // Paste a few pixels from A into B -- include offsets
    ImageBufAlgo::paste(B, 2, 2, 0, 1 /* chan offset */,
                        ImageBufAlgo::cut(A, ROI(1, 4, 1, 4)));

    // Spot check
    float a[3], b[3];
    B.getpixel(1, 1, 0, make_span(b));
    OIIO_CHECK_EQUAL(b[0], gray[0]);
    OIIO_CHECK_EQUAL(b[1], gray[1]);
    OIIO_CHECK_EQUAL(b[2], gray[2]);

    B.getpixel(2, 2, 0, make_span(b));
    A.getpixel(1, 1, 0, make_span(a));
    OIIO_CHECK_EQUAL(b[0], gray[0]);
    OIIO_CHECK_EQUAL(b[1], a[0]);
    OIIO_CHECK_EQUAL(b[2], a[1]);

    B.getpixel(3, 4, 0, make_span(b));
    A.getpixel(2, 3, 0, make_span(a));
    OIIO_CHECK_EQUAL(b[0], gray[0]);
    OIIO_CHECK_EQUAL(b[1], a[0]);
    OIIO_CHECK_EQUAL(b[2], a[1]);
}



void
test_channel_append()
{
    std::cout << "test channel_append\n";
    ImageSpec spec(2, 2, 1, TypeDesc::FLOAT);
    ImageBuf A(spec);
    ImageBuf B(spec);
    float Acolor = 0.1, Bcolor = 0.2;
    ImageBufAlgo::fill(A, Acolor);
    ImageBufAlgo::fill(B, Bcolor);

    ImageBuf R = ImageBufAlgo::channel_append(A, B);
    OIIO_CHECK_EQUAL(R.spec().width, spec.width);
    OIIO_CHECK_EQUAL(R.spec().height, spec.height);
    OIIO_CHECK_EQUAL(R.nchannels(), 2);
    for (ImageBuf::ConstIterator<float> r(R); !r.done(); ++r) {
        OIIO_CHECK_EQUAL(r[0], Acolor);
        OIIO_CHECK_EQUAL(r[1], Bcolor);
    }
}



// Tests ImageBufAlgo::add
void
test_add()
{
    std::cout << "test add\n";

    // Create buffers
    const float Aval[] = { 0.1f, 0.2f, 0.3f, 0.4f };
    const float Bval[] = { 0.01f, 0.02f, 0.03f, 0.04f };
    ImageBuf A         = filled_image(Aval);
    ImageBuf B         = filled_image(Bval);

    // Test addition of images
    ImageBuf R = ImageBufAlgo::add(A, B);
    for (ImageBuf::ConstIterator<float> r(R); !r.done(); ++r)
        for (int c = 0, nc = R.nchannels(); c < nc; ++c)
            OIIO_CHECK_EQUAL(r[c], Aval[c] + Bval[c]);

    // Test addition of image and constant color
    ImageBuf D = ImageBufAlgo::add(A, Bval);
    auto comp  = ImageBufAlgo::compare(R, D, 1e-6f, 1e-6f);
    OIIO_CHECK_EQUAL(comp.maxerror, 0.0f);
}



// Tests ImageBufAlgo::sub
void
test_sub()
{
    std::cout << "test sub\n";

    // Create buffers
    const float Aval[] = { 0.1f, 0.2f, 0.3f, 0.4f };
    const float Bval[] = { 0.01f, 0.02f, 0.03f, 0.04f };
    ImageBuf A         = filled_image(Aval);
    ImageBuf B         = filled_image(Bval);

    // Test subtraction of images
    ImageBuf R = ImageBufAlgo::sub(A, B);
    for (ImageBuf::ConstIterator<float> r(R); !r.done(); ++r)
        for (int c = 0, nc = R.nchannels(); c < nc; ++c)
            OIIO_CHECK_EQUAL(r[c], Aval[c] - Bval[c]);

    // Test subtraction of image and constant color
    ImageBuf D = ImageBufAlgo::sub(A, Bval);
    auto comp  = ImageBufAlgo::compare(R, D, 1e-6f, 1e-6f);
    OIIO_CHECK_EQUAL(comp.maxerror, 0.0f);
}



// Tests ImageBufAlgo::mul
void
test_mul()
{
    std::cout << "test mul\n";

    // Create buffers
    // Create buffers
    const float Aval[] = { 0.1f, 0.2f, 0.3f, 0.4f };
    const float Bval[] = { 0.01f, 0.02f, 0.03f, 0.04f };
    ImageBuf A         = filled_image(Aval);
    ImageBuf B         = filled_image(Bval);

    // Test multiplication of images
    ImageBuf R = ImageBufAlgo::mul(A, B);
    for (ImageBuf::ConstIterator<float> r(R); !r.done(); ++r)
        for (int c = 0, nc = R.nchannels(); c < nc; ++c)
            OIIO_CHECK_EQUAL(r[c], Aval[c] * Bval[c]);

    // Test multiplication of image and constant color
    ImageBuf D = ImageBufAlgo::mul(A, Bval);
    auto comp  = ImageBufAlgo::compare(R, D, 1e-6f, 1e-6f);
    OIIO_CHECK_EQUAL(comp.maxerror, 0.0f);
}



// Tests ImageBufAlgo::mad
void
test_mad()
{
    std::cout << "test mad\n";
    const int WIDTH = 4, HEIGHT = 4, CHANNELS = 4;
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);

    // Create buffers
    ImageBuf A(spec);
    const float Aval[CHANNELS] = { 0.1f, 0.2f, 0.3f, 0.4f };
    ImageBufAlgo::fill(A, cspan<float>(Aval));
    ImageBuf B(spec);
    const float Bval[CHANNELS] = { 1, 2, 3, 4 };
    ImageBufAlgo::fill(B, cspan<float>(Bval));
    ImageBuf C(spec);
    const float Cval[CHANNELS] = { 0.01f, 0.02f, 0.03f, 0.04f };
    ImageBufAlgo::fill(C, cspan<float>(Cval));

    // Test multiplication of images
    ImageBuf R(spec);
    ImageBufAlgo::mad(R, A, B, C);
    for (int j = 0; j < spec.height; ++j)
        for (int i = 0; i < spec.width; ++i)
            for (int c = 0; c < spec.nchannels; ++c)
                OIIO_CHECK_EQUAL(R.getchannel(i, j, 0, c),
                                 Aval[c] * Bval[c] + Cval[c]);

    // Test multiplication of image and constant color
    ImageBuf D(spec);
    ImageBufAlgo::mad(D, A, cspan<float>(Bval), cspan<float>(Cval));
    auto comp = ImageBufAlgo::compare(R, D, 1e-6f, 1e-6f);
    OIIO_CHECK_EQUAL(comp.maxerror, 0.0f);
}



// Tests ImageBufAlgo::min
void
test_min()
{
    std::cout << "test min\n";

    // Create buffers
    const float Aval[] = { 0.1f, 0.02f, 0.3f, 0.04f };
    const float Bval[] = { 0.01f, 0.2f, 0.03f, 0.4f };
    ImageBuf A         = filled_image(Aval);
    ImageBuf B         = filled_image(Bval);

    // Test min of images
    ImageBuf R = ImageBufAlgo::min(A, B);
    for (ImageBuf::ConstIterator<float> r(R); !r.done(); ++r)
        for (int c = 0, nc = R.nchannels(); c < nc; ++c)
            OIIO_CHECK_EQUAL(r[c], std::min(Aval[c], Bval[c]));

    // Test min of image and constant color
    ImageBuf D = ImageBufAlgo::min(A, Bval);
    auto comp  = ImageBufAlgo::compare(R, D, 1e-6f, 1e-6f);
    OIIO_CHECK_EQUAL(comp.maxerror, 0.0f);
}



// Tests ImageBufAlgo::max
void
test_max()
{
    std::cout << "test max\n";

    // Create buffers
    const float Aval[] = { 0.1f, 0.02f, 0.3f, 0.04f };
    const float Bval[] = { 0.01f, 0.2f, 0.03f, 0.4f };
    ImageBuf A         = filled_image(Aval);
    ImageBuf B         = filled_image(Bval);

    // Test max of images
    ImageBuf R = ImageBufAlgo::max(A, B);
    for (ImageBuf::ConstIterator<float> r(R); !r.done(); ++r)
        for (int c = 0, nc = R.nchannels(); c < nc; ++c)
            OIIO_CHECK_EQUAL(r[c], std::max(Aval[c], Bval[c]));

    // Test max of image and constant color
    ImageBuf D = ImageBufAlgo::max(A, Bval);
    auto comp  = ImageBufAlgo::compare(R, D, 1e-6f, 1e-6f);
    OIIO_CHECK_EQUAL(comp.maxerror, 0.0f);
}



// Test ImageBuf::over
void
test_over(TypeDesc dtype = TypeFloat)
{
    std::cout << "test over " << dtype << "\n";

    ROI roi(2, 4, 1, 3);  // region with fg

    // Create buffers
    const float BGval[] = { 0.5f, 0.0f, 0.0f, 0.5f };
    ImageBuf BG         = filled_image(BGval, dtype);

    ImageBuf FG         = filled_image({ 0.0f, 0.0f, 0.0f, 0.0f }, dtype);
    const float FGval[] = { 0.0f, 0.5f, 0.0f, 0.5f };
    ImageBufAlgo::fill(FG, cspan<float>(FGval), roi);

    // value it should be where composited
    const float comp_val[] = { 0.25f, 0.5f, 0.0f, 0.75f };

    // Test over
    ImageBuf R = ImageBufAlgo::over(FG, BG);
    int nc     = R.nchannels();
    for (ImageBuf::ConstIterator<float> r(R); !r.done(); ++r) {
        if (roi.contains(r.x(), r.y()))
            for (int c = 0; c < nc; ++c)
                OIIO_CHECK_EQUAL(R.getchannel(r.x(), r.y(), 0, c), comp_val[c]);
        else
            for (int c = 0; c < nc; ++c)
                OIIO_CHECK_EQUAL(R.getchannel(r.x(), r.y(), 0, c), BGval[c]);
    }

    // Timing
    Benchmarker bench;
    ImageSpec onekfloat(1000, 1000, 4, TypeFloat);
    BG = filled_image(BGval, 1000, 1000);
    FG = filled_image({ 0.0f, 0.0f, 0.0f, 0.0f }, 1000, 1000);
    ImageBufAlgo::fill(FG, cspan<float>(FGval), ROI(250, 750, 100, 900));
    R.reset(onekfloat);
    bench("  IBA::over ", [&]() { ImageBufAlgo::over(R, FG, BG); });
}



// Test ImageBuf::zover
void
test_zover()
{
    std::cout << "test zover\n";

    ImageSpec spec(4, 4, 5, TypeFloat);
    spec.channelnames.assign({ "R", "G", "B", "A", "Z" });
    spec.z_channel = 4;

    ROI roi(2, 4, 1, 3);  // region with fg

    // Create buffers
    const float Aval[] = { 0.5f, 0.5, 0.5, 1.0f, 10.0f };  // z == 10
    ImageBuf A         = filled_image(Aval, spec);

    ImageBuf B         = filled_image({ 0.0f, 0.0f, 0.0f, 1.0f, 15.0f }, spec);
    const float Bval[] = { 1.0f, 1.0f, 1.0f, 1.0f, 5.0f };
    ImageBufAlgo::fill(B, cspan<float>(Bval), roi);

    // Test zover
    ImageBuf R = ImageBufAlgo::zover(A, B, true);
    int nc     = R.nchannels();
    for (ImageBuf::ConstIterator<float> r(R); !r.done(); ++r) {
        if (roi.contains(r.x(), r.y()))
            for (int c = 0; c < nc; ++c)
                OIIO_CHECK_EQUAL(R.getchannel(r.x(), r.y(), 0, c), Bval[c]);
        else
            for (int c = 0; c < nc; ++c)
                OIIO_CHECK_EQUAL(R.getchannel(r.x(), r.y(), 0, c), Aval[c]);
    }
}



// Tests ImageBufAlgo::compare
void
test_compare()
{
    std::cout << "test compare\n";
    // Construct two identical 50% grey images
    const int WIDTH = 10, HEIGHT = 10, CHANNELS = 3;
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A(spec);
    ImageBuf B(spec);
    const float grey[CHANNELS] = { 0.5f, 0.5f, 0.5f };
    ImageBufAlgo::fill(A, cspan<float>(grey));
    ImageBufAlgo::fill(B, cspan<float>(grey));

    // Introduce some minor differences
    const int NDIFFS = 10;
    ImageBuf::Iterator<float> a(A);
    for (int i = 0; i < NDIFFS && a.valid(); ++i, ++a) {
        for (int c = 0; c < CHANNELS; ++c)
            a[c] = a[c] + 0.01f * i;
    }
    // We expect the differences to be { 0, 0.01, 0.02, 0.03, 0.04, 0.05,
    // 0.06, 0.07, 0.08, 0.09, 0, 0, ...}.
    const float failthresh = 0.05;
    const float warnthresh = 0.025;
    auto comp = ImageBufAlgo::compare(A, B, failthresh, warnthresh);
    // We expect 5 pixels to exceed the fail threshold, 7 pixels to
    // exceed the warn threshold, the maximum difference to be 0.09,
    // and the maximally different pixel to be (9,0).
    // The total error should be 3 chans * sum{0.01,...,0.09} / (pixels*chans)
    //   = 3 * 0.45 / (100*3) = 0.0045
    std::cout << "Testing comparison: " << comp.nfail << " failed, "
              << comp.nwarn << " warned, max diff = " << comp.maxerror << " @ ("
              << comp.maxx << ',' << comp.maxy << ")\n";
    std::cout << "   mean err " << comp.meanerror << ", RMS err "
              << comp.rms_error << ", PSNR = " << comp.PSNR << "\n";
    OIIO_CHECK_EQUAL(comp.nfail, 5);
    OIIO_CHECK_EQUAL(comp.nwarn, 7);
    OIIO_CHECK_EQUAL_THRESH(comp.maxerror, 0.09f, 1e-6f);
    OIIO_CHECK_EQUAL(comp.maxx, 9);
    OIIO_CHECK_EQUAL(comp.maxy, 0);
    OIIO_CHECK_EQUAL_THRESH(comp.meanerror, 0.0045f, 1.0e-8f);

    // Relative comparison: warn at 5% of the difference, fail at 10% of the
    // difference. In row 0, we have:
    //    A:  0.50  0.50  0.50  0.50  0.50  0.50  0.50  0.50  0.50  0.50
    //    B:  0.50  0.51  0.52  0.53  0.54  0.55  0.56  0.57  0.58  0.59
    // mean:  0.50  0.505 0.51  0.515 0.52  0.525 0.53  0.535 0.54  0.545
    // diff:  0.0   0.01  0.02  0.03  0.04  0.05  0.06  0.07  0.08  0.09
    // fail?                                       x     x     x     x
    // warn?                     x     x     x     x     x     x     x
    comp = ImageBufAlgo::compare(A, B, 0.0f, 0.0f, 0.1f, 0.05f);
    // We expect 4 pixels to exceed the fail threshold, 7 pixels to
    // exceed the warn threshold, the maximum difference to be 0.09,
    // and the maximally different pixel to be (9,0).
    // The total error should be 3 chans * sum{0.01,...,0.09} / (pixels*chans)
    //   = 3 * 0.45 / (100*3) = 0.0045
    std::cout << "Testing relative comparison: " << comp.nfail << " failed, "
              << comp.nwarn << " warned, max diff = " << comp.maxerror << " @ ("
              << comp.maxx << ',' << comp.maxy << ")\n";
    std::cout << "   mean err " << comp.meanerror << ", RMS err "
              << comp.rms_error << ", PSNR = " << comp.PSNR << "\n";
    OIIO_CHECK_EQUAL(comp.nfail, 4);
    OIIO_CHECK_EQUAL(comp.nwarn, 7);
    OIIO_CHECK_EQUAL_THRESH(comp.maxerror, 0.09f, 1e-6f);
    OIIO_CHECK_EQUAL(comp.maxx, 9);
    OIIO_CHECK_EQUAL(comp.maxy, 0);
    OIIO_CHECK_EQUAL_THRESH(comp.meanerror, 0.0045f, 1.0e-8f);
}



// Tests ImageBufAlgo::isConstantColor
void
test_isConstantColor()
{
    std::cout << "test isConstantColor\n";
    const int WIDTH = 10, HEIGHT = 10, CHANNELS = 3;
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A(spec);
    const float col[CHANNELS] = { 0.25, 0.5, 0.75 };
    ImageBufAlgo::fill(A, cspan<float>(col));

    float thecolor[CHANNELS] = { 0, 0, 0 };
    OIIO_CHECK_EQUAL(ImageBufAlgo::isConstantColor(A), true);
    OIIO_CHECK_EQUAL(ImageBufAlgo::isConstantColor(A, 0.0f, thecolor), true);
    OIIO_CHECK_EQUAL(col[0], thecolor[0]);
    OIIO_CHECK_EQUAL(col[1], thecolor[1]);
    OIIO_CHECK_EQUAL(col[2], thecolor[2]);

    // Now introduce a difference
    A.setpixel(2, 2, 0, { 0.25f, 0.51f, 0.75f });
    OIIO_CHECK_EQUAL(ImageBufAlgo::isConstantColor(A), false);
    OIIO_CHECK_EQUAL(ImageBufAlgo::isConstantColor(A, 0.0f,
                                                   span<float>(thecolor)),
                     false);
    // But not with lower threshold
    OIIO_CHECK_EQUAL(ImageBufAlgo::isConstantColor(A, 0.015f), true);

    // Make sure ROI works
    ROI roi(0, WIDTH, 0, 2, 0, 1, 0, CHANNELS);  // should match for this ROI
    OIIO_CHECK_EQUAL(ImageBufAlgo::isConstantColor(A, 0.0f, span<float>(), roi),
                     true);
}



// Tests ImageBufAlgo::isConstantChannel
void
test_isConstantChannel()
{
    std::cout << "test isConstantChannel\n";
    const int WIDTH = 10, HEIGHT = 10, CHANNELS = 3;
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A(spec);
    ImageBufAlgo::fill(A, { 0.25f, 0.5f, 0.75f });

    OIIO_CHECK_EQUAL(ImageBufAlgo::isConstantChannel(A, 1, 0.5f), true);

    // Now introduce a difference
    A.setpixel(2, 2, 0, { 0.25f, 0.51f, 0.75f });
    // It should still pass if within the threshold
    OIIO_CHECK_EQUAL(ImageBufAlgo::isConstantChannel(A, 1, 0.5f, 0.015f), true);
    // But not with lower threshold
    OIIO_CHECK_EQUAL(ImageBufAlgo::isConstantChannel(A, 1, 0.5f, 0.005), false);
    // And certainly not with zero threshold
    OIIO_CHECK_EQUAL(ImageBufAlgo::isConstantChannel(A, 1, 0.5f), false);

    // Make sure ROI works
    ROI roi(0, WIDTH, 0, 2, 0, 1, 0, CHANNELS);  // should match for this ROI
    OIIO_CHECK_ASSERT(
        ImageBufAlgo::isConstantChannel(A, 1, 0.5f, 0.0f, roi = roi));
}



// Tests ImageBufAlgo::isMonochrome
void
test_isMonochrome()
{
    std::cout << "test isMonochrome\n";
    const int WIDTH = 10, HEIGHT = 10, CHANNELS = 3;
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A(spec);
    ImageBufAlgo::fill(A, { 0.25f, 0.25f, 0.25f });

    OIIO_CHECK_EQUAL(ImageBufAlgo::isMonochrome(A), true);

    // Now introduce a tiny difference
    const float another[CHANNELS] = { 0.25f, 0.25f, 0.26f };
    A.setpixel(2, 2, 0, make_span(another));
    // It should still pass if within the threshold
    OIIO_CHECK_EQUAL(ImageBufAlgo::isMonochrome(A, 0.015f), true);
    // But not with lower threshold
    OIIO_CHECK_EQUAL(ImageBufAlgo::isMonochrome(A, 0.005f), false);
    // And certainly not with zero threshold
    OIIO_CHECK_EQUAL(ImageBufAlgo::isMonochrome(A), false);


    // Make sure ROI works
    ROI roi(0, WIDTH, 0, 2, 0, 1, 0, CHANNELS);  // should match for this ROI
    OIIO_CHECK_EQUAL(ImageBufAlgo::isMonochrome(A, 0.0f, roi), true);
}



// Tests ImageBufAlgo::computePixelStats()
void
test_computePixelStats()
{
    std::cout << "test computePixelStats\n";
    ImageBuf img(ImageSpec(2, 2, 3, TypeDesc::FLOAT));
    float black[3] = { 0, 0, 0 }, white[3] = { 1, 1, 1 };
    img.setpixel(0, 0, make_span(black));
    img.setpixel(1, 0, make_span(white));
    img.setpixel(0, 1, make_span(black));
    img.setpixel(1, 1, make_span(white));
    auto stats = ImageBufAlgo::computePixelStats(img);
    for (int c = 0; c < 3; ++c) {
        OIIO_CHECK_EQUAL(stats.min[c], 0.0f);
        OIIO_CHECK_EQUAL(stats.max[c], 1.0f);
        OIIO_CHECK_EQUAL(stats.avg[c], 0.5f);
        OIIO_CHECK_EQUAL(stats.stddev[c], 0.5f);
        OIIO_CHECK_EQUAL(stats.nancount[c], 0);
        OIIO_CHECK_EQUAL(stats.infcount[c], 0);
        OIIO_CHECK_EQUAL(stats.finitecount[c], 4);
    }
}



// Tests histogram computation.
void
histogram_computation_test()
{
    const int INPUT_WIDTH   = 64;
    const int INPUT_HEIGHT  = 64;
    const int INPUT_CHANNEL = 0;

    const int HISTOGRAM_BINS = 256;

    const int SPIKE1 = 51;   // 0.2f in range 0->1 maps to 51 in range 0->255
    const int SPIKE2 = 128;  // 0.5f in range 0->1 maps to 128 in range 0->255
    const int SPIKE3 = 204;  // 0.8f in range 0->1 maps to 204 in range 0->255

    const int SPIKE1_COUNT = INPUT_WIDTH * 8;
    const int SPIKE2_COUNT = INPUT_WIDTH * 16;
    const int SPIKE3_COUNT = INPUT_WIDTH * 40;

    // Create input image with three regions with different pixel values.
    ImageSpec spec(INPUT_WIDTH, INPUT_HEIGHT, 1, TypeDesc::FLOAT);
    ImageBuf A(spec);

    float value[] = { 0.2f };
    ImageBufAlgo::fill(A, cspan<float>(value), ROI(0, INPUT_WIDTH, 0, 8));

    value[0] = 0.5f;
    ImageBufAlgo::fill(A, cspan<float>(value), ROI(0, INPUT_WIDTH, 8, 24));

    value[0] = 0.8f;
    ImageBufAlgo::fill(A, cspan<float>(value), ROI(0, INPUT_WIDTH, 24, 64));

    // Compute A's histogram.
    std::vector<imagesize_t> hist = ImageBufAlgo::histogram(A, INPUT_CHANNEL,
                                                            HISTOGRAM_BINS);

    // Does the histogram size equal the number of bins?
    OIIO_CHECK_EQUAL(hist.size(), (imagesize_t)HISTOGRAM_BINS);

    // Are the histogram values as expected?
    OIIO_CHECK_EQUAL(hist[SPIKE1], (imagesize_t)SPIKE1_COUNT);
    OIIO_CHECK_EQUAL(hist[SPIKE2], (imagesize_t)SPIKE2_COUNT);
    OIIO_CHECK_EQUAL(hist[SPIKE3], (imagesize_t)SPIKE3_COUNT);
    for (int i = 0; i < HISTOGRAM_BINS; i++)
        if (i != SPIKE1 && i != SPIKE2 && i != SPIKE3)
            OIIO_CHECK_EQUAL(hist[i], 0);
}



// Test ability to do a maketx directly from an ImageBuf
void
test_maketx_from_imagebuf()
{
    std::cout << "test make_texture from ImageBuf\n";
    // Make a checkerboard
    const int WIDTH = 16, HEIGHT = 16, CHANNELS = 3;
    ImageSpec spec(WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    ImageBuf A(spec);
    float pink[] = { 0.5f, 0.3f, 0.3f }, green[] = { 0.1f, 0.5f, 0.1f };
    ImageBufAlgo::checker(A, 4, 4, 4, cspan<float>(pink), cspan<float>(green));

    // Write it
    const char* pgname = "oiio-pgcheck.tx";
    remove(pgname);  // Remove it first
    ImageSpec configspec;
    ImageBufAlgo::make_texture(ImageBufAlgo::MakeTxTexture, A, pgname,
                               configspec);

    // Read it back and compare it
    ImageBuf B(pgname);
    B.read();
    auto comparison = ImageBufAlgo::compare(A, B, 0, 0);
    OIIO_CHECK_EQUAL(comparison.nwarn, 0);
    OIIO_CHECK_EQUAL(comparison.nfail, 0);
    remove(pgname);  // clean up
}



// Test various IBAprep features
void
test_IBAprep()
{
    std::cout << "test IBAprep\n";
    using namespace ImageBufAlgo;
    ImageBuf rgb(ImageSpec(256, 256, 3));   // Basic RGB uint8 image
    ImageBuf rgba(ImageSpec(256, 256, 4));  // Basic RGBA uint8 image

#define CHECK(...)                               \
    {                                            \
        ImageBuf dst;                            \
        ROI roi;                                 \
        OIIO_CHECK_ASSERT(IBAprep(__VA_ARGS__)); \
    }
#define CHECK0(...)                               \
    {                                             \
        ImageBuf dst;                             \
        ROI roi;                                  \
        OIIO_CHECK_ASSERT(!IBAprep(__VA_ARGS__)); \
    }

    // Test REQUIRE_ALPHA
    CHECK(roi, &dst, &rgba, IBAprep_REQUIRE_ALPHA);
    CHECK0(roi, &dst, &rgb, IBAprep_REQUIRE_ALPHA);

    // Test REQUIRE_Z
    ImageSpec rgbaz_spec(256, 256, 5);
    rgbaz_spec.channelnames[4] = std::string("Z");
    rgbaz_spec.z_channel       = 4;
    ImageBuf rgbaz(rgbaz_spec);
    CHECK(roi, &dst, &rgbaz, IBAprep_REQUIRE_Z);
    CHECK0(roi, &dst, &rgb, IBAprep_REQUIRE_Z);

    // Test REQUIRE_SAME_NCHANNELS
    CHECK(roi, &dst, &rgb, &rgb, NULL, NULL, IBAprep_REQUIRE_SAME_NCHANNELS);
    CHECK0(roi, &dst, &rgb, &rgba, NULL, NULL, IBAprep_REQUIRE_SAME_NCHANNELS);

    // Test NO_SUPPOERT_VOLUME
    ImageSpec volspec(256, 256, 3);
    volspec.depth = 256;
    ImageBuf vol(volspec);
    CHECK(roi, &dst, &rgb, IBAprep_NO_SUPPORT_VOLUME);
    CHECK0(roi, &dst, &vol, IBAprep_NO_SUPPORT_VOLUME);

    // Test SUPPORT_DEEP
    ImageSpec deepspec(256, 256, 3);
    deepspec.deep = true;
    ImageBuf deep(deepspec);
    CHECK(roi, &dst, &deep, IBAprep_SUPPORT_DEEP);
    CHECK0(roi, &dst, &deep);  // deep should be rejected

    // Test DEEP_MIXED
    CHECK(roi, &dst, &deep, &deep, NULL,
          IBAprep_SUPPORT_DEEP | IBAprep_DEEP_MIXED);
    CHECK(roi, &dst, &deep, &rgb, NULL,
          IBAprep_SUPPORT_DEEP | IBAprep_DEEP_MIXED);
    CHECK(roi, &dst, &deep, &deep, NULL, IBAprep_SUPPORT_DEEP);
    CHECK0(roi, &dst, &deep, &rgb, NULL, IBAprep_SUPPORT_DEEP);

    // Test DST_FLOAT_PIXELS
    {
        ROI roi1, roi2;
        ImageBuf dst1, dst2;
        OIIO_CHECK_ASSERT(IBAprep(roi1, &dst1, &rgb));
        OIIO_CHECK_EQUAL(dst1.spec().format, TypeDesc::UINT8);
        OIIO_CHECK_ASSERT(IBAprep(roi2, &dst2, &rgb, IBAprep_DST_FLOAT_PIXELS));
        OIIO_CHECK_EQUAL(dst2.spec().format, TypeDesc::FLOAT);
    }

    // Test MINIMIZE_NCHANNELS
    {
        ROI roi1, roi2;
        ImageBuf dst1, dst2;
        OIIO_CHECK_ASSERT(IBAprep(roi1, &dst1, &rgb, &rgba));
        OIIO_CHECK_EQUAL(dst1.nchannels(), 4);
        OIIO_CHECK_ASSERT(IBAprep(roi2, &dst2, &rgb, &rgba, NULL, NULL,
                                  IBAprep_MINIMIZE_NCHANNELS));
        OIIO_CHECK_EQUAL(dst2.nchannels(), 3);
    }
#undef CHECK
}



// Test extra validation checks done by `st_warp`
void
test_validate_st_warp_checks()
{
    // using namespace ImageBufAlgo;
    std::cout << "test st_warp validation checks" << std::endl;

    const int size = 16;
    ImageSpec srcSpec(size, size, 3, TypeDesc::FLOAT);
    ImageBuf SRC(srcSpec);
    ImageBuf ST;
    ImageBuf DST;

    ImageBufAlgo::zero(SRC);

    // Fail: Uninitialized ST buffer
    OIIO_CHECK_ASSERT(!ImageBufAlgo::st_warp(DST, SRC, ST));

    ROI disjointROI(size, size, size * 2, size * 2, 0, 1, 0, 2);
    ImageSpec stSpec(disjointROI, TypeDesc::HALF);
    ST.reset(stSpec);
    // Fail: Non-intersecting ST and output ROIs
    OIIO_CHECK_ASSERT(!ImageBufAlgo::st_warp(DST, SRC, ST));

    stSpec = ImageSpec(size, size, 2, TypeDesc::HALF);
    ST.reset(stSpec);

    DST.reset();
    // Fail: Out-of-range chan_s
    OIIO_CHECK_ASSERT(!ImageBufAlgo::st_warp(DST, SRC, ST, nullptr, 2));
    // Fail: Out-of-range chan_t
    OIIO_CHECK_ASSERT(!ImageBufAlgo::st_warp(DST, SRC, ST, nullptr, 0, 2));
    // Success
    OIIO_CHECK_ASSERT(ImageBufAlgo::st_warp(DST, SRC, ST, nullptr));
}


void
benchmark_parallel_image(int res, int iters)
{
    using namespace ImageBufAlgo;
    print("\nTime old parallel_image for {}x{}\n", res, res);

    print("  threads time    rate   (best of {})\n", ntrials);
    print("  ------- ------- -------\n");
    ImageSpec spec(res, res, 3, TypeDesc::FLOAT);
    ImageBuf X(spec), Y(spec);
    ImageBufAlgo::zero(Y);
    ImageBufAlgo::fill(X, { 1.0f, 1.0f, 1.0f });
    float a = 0.5f;

    // Lambda that does some exercise (a basic SAXPY)
    auto exercise = [&](ROI roi) {
        ImageBuf::Iterator<float> y(Y, roi);
        ImageBuf::ConstIterator<float> x(X, roi);
        for (; !y.done(); ++y, ++x)
            for (int c = roi.chbegin; c < roi.chend; ++c)
                y[c] = a * x[c] + y[c];
    };

    for (int i = 0; threadcounts[i] <= numthreads; ++i) {
        int nt = wedge ? threadcounts[i] : numthreads;
        ImageBufAlgo::zero(Y);
        auto func = [&]() {
            ImageBufAlgo::parallel_image(Y.roi(), nt, exercise);
        };
        double range;
        double t = time_trial(func, ntrials, iters, &range) / iters;
        print("  {:4}   {:7.3f} ms  {:5.1f} Mpels/s\n", nt, t * 1000,
              double(res * res) / t / 1.0e6);
        if (!wedge)
            break;  // don't loop if we're not wedging
    }

    print("\nTime new parallel_image for {}x{}\n", res, res);

    print("  threads time    rate   (best of {})\n", ntrials);
    print("  ------- ------- -------\n");
    for (int i = 0; threadcounts[i] <= numthreads; ++i) {
        int nt = wedge ? threadcounts[i] : numthreads;
        // default_thread_pool()->resize (nt);
        zero(Y);
        auto func = [&]() { parallel_image(Y.roi(), nt, exercise); };
        double range;
        double t = time_trial(func, ntrials, iters, &range) / iters;
        print("  {:4}   {:6.2f} ms  {:5.1f} Mpels/s\n", nt, t * 1000,
              double(res * res) / t / 1.0e6);
        if (!wedge)
            break;  // don't loop if we're not wedging
    }
}



void
test_opencv()
{
#if USE_OPENCV
    std::cout << "Testing OpenCV round trip\n";
    // Make a gradient RGB image, convert to OpenCV cv::Mat, then convert
    // that back to ImageBuf, make sure the round trip has the same pixels
    // as the original image.
    ImageBuf src
        = ImageBufAlgo::fill({ 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
                             { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f },
                             ROI(0, 64, 0, 64, 0, 1, 0, 3));
    cv::Mat mat;
    ImageBufAlgo::to_OpenCV(mat, src);
    OIIO_CHECK_ASSERT(!mat.empty());
    ImageBuf dst = ImageBufAlgo::from_OpenCV(mat);
    OIIO_CHECK_ASSERT(!dst.has_error());
    auto comp = ImageBufAlgo::compare(src, dst, 0.0f, 0.0f);
    OIIO_CHECK_EQUAL(comp.error, false);
    OIIO_CHECK_EQUAL(comp.maxerror, 0.0f);

    // Regression test: reading from ImageBuf-backed image to OpenCV
    std::string filename = "testsuite/common/tahoe-tiny.tif";
    if (!Filesystem::exists(filename))
        filename = "../../testsuite/common/tahoe-tiny.tif";
    auto loaded_image = OIIO::ImageBuf(filename, 0, 0, ImageCache::create());
    OIIO_CHECK_ASSERT(loaded_image.initialized());
    if (!loaded_image.initialized()) {
        std::cout << loaded_image.geterror() << 'n';
        return;
    }
    auto cv_image = cv::Mat {};
    try {
        bool ok = OIIO::ImageBufAlgo::to_OpenCV(cv_image, loaded_image, {}, 1);
        OIIO_CHECK_ASSERT(ok);
        if (!ok) {
            std::cout << "Error when converting: " << OIIO::geterror() << '\n';
            return;
        }
    } catch (const std::exception& e) {
        OIIO_CHECK_ASSERT(0);
        std::cout << "Error when converting: " << e.what() << '\n';
        return;
    }
#endif
}



void
test_color_management()
{
    ColorConfig config;
    auto processor = config.createColorProcessor("lin_srgb", "srgb");
    // These color spaces might not be found if the site running this test
    // has a weirdo OCIO config that doesn't contain those names. If we fail,
    // try again using the built-in config (OCIO 2.2+) and hope for the best.
    if (!processor)
        processor = ColorConfig("ocio://default")
                        .createColorProcessor("lin_srgb", "srgb");
    OIIO_CHECK_ASSERT(processor);

    // Test the IBA::colorconvert version that works on a color at a time
    {
        float rgb[3] = { 0.5f, 0.5f, 0.5f };
        bool r       = ImageBufAlgo::colorconvert(rgb, processor.get(), false);
        OIIO_CHECK_ASSERT(r);
        if (!r)
            OIIO::print("colorconvert error: {}\n", OIIO::geterror());
        OIIO_CHECK_EQUAL_THRESH(rgb[1], 0.735356983052449f, 1.0e-5);
    }
    {
        float rgba[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
        bool r        = ImageBufAlgo::colorconvert(rgba, processor.get(), true);
        OIIO_CHECK_ASSERT(r);
        if (!r)
            OIIO::print("colorconvert error: {}\n", OIIO::geterror());
        OIIO_CHECK_EQUAL_THRESH(rgba[1], 0.735356983052449f, 1.0e-5);
    }
}



static void
test_yee()
{
    print("Testing Yee comparison\n");
    ImageSpec spec(1, 1, 3, TypeDesc::FLOAT);
    ImageBuf img1(spec);
    ImageBufAlgo::fill(img1, { 0.1f, 0.1f, 0.1f });
    ImageBuf img2(spec);
    ImageBufAlgo::fill(img2, { 0.1f, 0.6f, 0.1f });
    ImageBufAlgo::CompareResults cr;
    int n = ImageBufAlgo::compare_Yee(img1, img2, cr);
    OIIO_CHECK_EQUAL(n, 1);
    OIIO_CHECK_EQUAL(cr.maxx, 0);
    OIIO_CHECK_EQUAL(cr.maxy, 0);
}



// Raw function to reverse channels
bool
chan_reverse(span<float> d, cspan<float> s)
{
    for (size_t c = 0, nc = size_t(d.size()); c < nc; ++c)
        d[c] = s[nc - 1 - c];
    return true;
}

// Functor to reverse channels
class ChannelReverser {
public:
    ChannelReverser() {}
    bool operator()(span<float> d, cspan<float> s)
    {
        for (size_t c = 0, nc = size_t(d.size()); c < nc; ++c)
            d[c] = s[nc - 1 - c];
        return true;
    }
};



template<typename T>
static void
test_simple_perpixel()
{
    TypeDesc td = TypeDescFromC<T>::value();
    print("test_simple_perpixel {}\n", td);
    {
        print("  unary op\n");
        ImageBuf src = filled_image({ 0.25f, 0.5f, 0.75f, 1.0f }, 4, 4, td);
        ImageBuf result;
        // Test with raw function pointer
        result = ImageBufAlgo::perpixel_op(src, chan_reverse);
        OIIO_CHECK_EQUAL(result.spec().format, td);
        for (ImageBuf::ConstIterator<T> r(result); !r.done(); ++r) {
            OIIO_CHECK_EQUAL(r[0], 1.0f);
            OIIO_CHECK_EQUAL(r[1], 0.75f);
            OIIO_CHECK_EQUAL(r[2], 0.5f);
            OIIO_CHECK_EQUAL(r[3], 0.25f);
        }
        // Test with functor
        result = ImageBufAlgo::perpixel_op(src, ChannelReverser());
        OIIO_CHECK_EQUAL(result.spec().format, td);
        for (ImageBuf::ConstIterator<T> r(result); !r.done(); ++r) {
            OIIO_CHECK_EQUAL(r[0], 1.0f);
            OIIO_CHECK_EQUAL(r[1], 0.75f);
            OIIO_CHECK_EQUAL(r[2], 0.5f);
            OIIO_CHECK_EQUAL(r[3], 0.25f);
        }
        // Test with lambda, including variable capture
        float bias = 0.0;  // Force capture of this variable
        result     = ImageBufAlgo::perpixel_op(src, [&](span<float> d,
                                                    cspan<float> s) {
            for (size_t c = 0, nc = size_t(d.size()); c < nc; ++c)
                d[c] = s[nc - 1 - c] + bias;
            return true;
        });
        OIIO_CHECK_EQUAL(result.spec().format, td);
        for (ImageBuf::ConstIterator<T> r(result); !r.done(); ++r) {
            OIIO_CHECK_EQUAL(r[0], 1.0f);
            OIIO_CHECK_EQUAL(r[1], 0.75f);
            OIIO_CHECK_EQUAL(r[2], 0.5f);
            OIIO_CHECK_EQUAL(r[3], 0.25f);
        }
    }
    {
        print("  binary op\n");
        ImageBuf srcA   = filled_image({ 0.25f, 0.5f, 0.75f, 1.0f }, 4, 4, td);
        ImageBuf srcB   = filled_image({ 1.0f, 2.0f, 3.0f, 4.0f }, 4, 4, td);
        ImageBuf result = ImageBufAlgo::perpixel_op(
            srcA, srcB, [&](span<float> d, cspan<float> a, cspan<float> b) {
                for (size_t c = 0, nc = size_t(d.size()); c < nc; ++c)
                    d[c] = a[c] + b[c];
                return true;
            });
        OIIO_CHECK_EQUAL(result.spec().format, td);
        for (ImageBuf::ConstIterator<T> r(result); !r.done(); ++r) {
            OIIO_CHECK_EQUAL(r[0], 1.25f);
            OIIO_CHECK_EQUAL(r[1], 2.5f);
            OIIO_CHECK_EQUAL(r[2], 3.75f);
            OIIO_CHECK_EQUAL(r[3], 5.0f);
        }
    }

    if (td == TypeFloat) {
        // Timing test: how much more expensive is the perpixel_op than the
        // fully optimized per-type version?
        Benchmarker bench;
        bench.units(Benchmarker::Unit::ms);
        ImageBuf af(ImageSpec(2048, 2048, 4, TypeFloat));
        ImageBuf bf(ImageSpec(2048, 2048, 4, TypeFloat));
        ImageBuf au8(ImageSpec(2048, 2048, 4, TypeUInt8));
        ImageBuf bu8(ImageSpec(2048, 2048, 4, TypeUInt8));
        bench("  IBA::add() float",
              [&]() { ImageBuf r = ImageBufAlgo::add(af, bf); });
        bench("  IBA::add() u8",
              [&]() { ImageBuf r = ImageBufAlgo::add(au8, bu8); });
        bench("  IBA::perpixel_op<float> add", [&]() {
            ImageBuf r = ImageBufAlgo::perpixel_op(
                af, bf,  //
                [](span<float> r, cspan<float> a, cspan<float> b) {
                    for (size_t c = 0, nc = size_t(r.size()); c < nc; ++c)
                        r[c] = a[c] + b[c];
                    return true;
                });
        });
        bench("  IBA::perpixel_op<u8> add", [&]() {
            ImageBuf r = ImageBufAlgo::perpixel_op(
                au8, bu8,  //
                [](span<float> r, cspan<float> a, cspan<float> b) {
                    for (size_t c = 0, nc = size_t(r.size()); c < nc; ++c)
                        r[c] = a[c] + b[c];
                    return true;
                });
        });
    }
}


template<class T>
std::string
mosaic(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
       const std::string& pattern, const float (&white_balance)[4],
       int nthreads);

template<>
std::string
mosaic<float>(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
              const std::string& pattern, const float (&white_balance)[4],
              int nthreads)
{
    return ImageBufAlgo::mosaic_float(dst, src, x_offset, y_offset, pattern,
                                      white_balance, nthreads);
}

template<>
std::string
mosaic<half>(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
             const std::string& pattern, const float (&white_balance)[4],
             int nthreads)
{
    return ImageBufAlgo::mosaic_half(dst, src, x_offset, y_offset, pattern,
                                     white_balance, nthreads);
}

template<>
std::string
mosaic<uint16_t>(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
                 const std::string& pattern, const float (&white_balance)[4],
                 int nthreads)
{
    return ImageBufAlgo::mosaic_uint16(dst, src, x_offset, y_offset, pattern,
                                       white_balance, nthreads);
}

template<>
std::string
mosaic<uint8_t>(ImageBuf& dst, const ImageBuf& src, int x_offset, int y_offset,
                const std::string& pattern, const float (&white_balance)[4],
                int nthreads)
{
    return ImageBufAlgo::mosaic_uint8(dst, src, x_offset, y_offset, pattern,
                                      white_balance, nthreads);
}

struct DemosaicTestConfig {
    const char* pattern;
    size_t size_x;
    size_t size_y;
    size_t algos_count;
};

struct DemosaicTestAlgo {
    const char* name;
    const int inset;
};

template<typename T, bool write_images>
static void
test_demosaic(const DemosaicTestConfig& config, const DemosaicTestAlgo* algos,
              const ImageBuf& src_image, const float (&wb)[4],
              const float thresholds[])
{
    for (size_t y = 0; y < config.size_y; y++) {
        for (size_t x = 0; x < config.size_x; x++) {
            auto type = TypeDescFromC<T>().value();

            ImageSpec src_spec = src_image.spec();
            ImageSpec dst_spec(src_spec.width, src_spec.height, 1, type);
            ImageBuf mosaiced_image(dst_spec);

            std::string layout = mosaic<T>(mosaiced_image, src_image, x, y,
                                           config.pattern, wb, 0);

            std::string pattern(config.pattern);
            std::string ext = type.is_floating_point() ? "exr" : "png";

            if (write_images) {
                std::string path = pattern + "_" + std::string(type.c_str())
                                   + "_" + std::to_string(y) + "_"
                                   + std::to_string(x) + "_src." + ext;

                auto imageOutput = ImageOutput::create(ext);
                imageOutput->open(path, mosaiced_image.spec());
                mosaiced_image.write(imageOutput.get());
            }

            for (size_t i = 0; i < config.algos_count; i++) {
                std::string algo(algos[i].name);

                ParamValueList list;
                list.push_back(ParamValue("pattern", pattern));
                list.push_back(ParamValue("algorithm", algo));
                list.push_back(ParamValue("layout", layout));
                list.push_back(
                    ParamValue("white_balance", TypeDesc::FLOAT, 4, wb));
                ImageBuf demosaiced_image
                    = OIIO::ImageBufAlgo::demosaic(mosaiced_image, list);

                int inset       = algos[i].inset;
                float threshold = thresholds[i];

                ROI roi = src_image.roi();
                roi.xbegin += inset;
                roi.ybegin += inset;
                roi.xend -= inset;
                roi.yend -= inset;

                ImageBufAlgo::CompareResults cr
                    = ImageBufAlgo::compare(src_image, demosaiced_image,
                                            threshold, threshold, roi);
                OIIO_CHECK_FALSE(cr.error);

                if (write_images) {
                    std::string path = pattern + "_" + std::string(type.c_str())
                                       + "_" + std::to_string(y) + "_"
                                       + std::to_string(x) + "_" + algo + "."
                                       + ext;
                    auto imageOutput = ImageOutput::create(ext);
                    imageOutput->open(path, demosaiced_image.spec());
                    demosaiced_image.write(imageOutput.get());
                }
            }
        }
    }
}


static void
test_demosaic()
{
    print("Testing Demosaicing\n");

    ImageSpec src_spec(256, 256, 3, TypeDesc::FLOAT);
    ImageBuf src_image(src_spec);
    ImageBufAlgo::fill(src_image, { 0.0f, 0.0f, 0.9f }, { 0.0f, 0.9f, 0.0f },
                       { 0.9f, 0.0f, 0.9f }, { 0.9f, 0.9f, 0.0f });

    float wb[4] = { 2.0, 1.1, 1.5, 0.9 };

    const DemosaicTestConfig bayerConfig = { "bayer", 2, 2, 2 };
    const DemosaicTestAlgo bayerAlgos[]  = { { "linear", 1 }, { "MHC", 2 } };

    // There are 6x6=36 possible permutations of the XTrans pattern,
    // of which only 18 are unique. It is sufficient to only test all variants of
    // the top 3 vertical offsets, the bottom half is the same, but somewhat
    // shuffled.
    const DemosaicTestConfig xtransConfig = { "xtrans", 6, 3, 1 };
    const DemosaicTestAlgo xtransAlgos[]  = { { "linear", 2 } };


    const float bayer_thresholds[4][2] = {
        { 1.8e-07, 2.4e-07 },  // float
        { 0.00049, 0.00049 },  // half
        { 3.1e-05, 4.6e-05 },  // int16
        { 0.0079, 0.012 }      // int8
    };

    const float xtrans_thresholds[4][1] = {
        { 0.00099 },  // float
        { 0.0015 },   // half
        { 0.0011 },   // int16
        { 0.0079 }    // int8
    };

    constexpr bool write_files = false;
    ImageBuf true_image;

    if (write_files) {
        auto imageOutput = OIIO::ImageOutput::create("exr");
        imageOutput->open("source.exr", src_image.spec());
        src_image.write(imageOutput.get());
    }

    true_image.copy(src_image, TypeDesc::FLOAT);
    test_demosaic<float, write_files>(bayerConfig, bayerAlgos, true_image, wb,
                                      bayer_thresholds[0]);
    test_demosaic<float, write_files>(xtransConfig, xtransAlgos, true_image, wb,
                                      xtrans_thresholds[0]);

    true_image.copy(src_image, TypeDesc::HALF);
    test_demosaic<half, write_files>(bayerConfig, bayerAlgos, true_image, wb,
                                     bayer_thresholds[1]);
    test_demosaic<half, write_files>(xtransConfig, xtransAlgos, true_image, wb,
                                     xtrans_thresholds[1]);

    true_image.copy(src_image, TypeDesc::UINT16);
    test_demosaic<uint16_t, write_files>(bayerConfig, bayerAlgos, true_image,
                                         wb, bayer_thresholds[2]);
    test_demosaic<uint16_t, write_files>(xtransConfig, xtransAlgos, true_image,
                                         wb, xtrans_thresholds[2]);

    true_image.copy(src_image, TypeDesc::UINT8);
    test_demosaic<uint8_t, write_files>(bayerConfig, bayerAlgos, true_image, wb,
                                        bayer_thresholds[3]);
    test_demosaic<uint8_t, write_files>(xtransConfig, xtransAlgos, true_image,
                                        wb, xtrans_thresholds[3]);
}


int
main(int argc, char** argv)
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs(argc, argv);

    test_type_merge();
    test_zero_fill();
    test_copy();
    test_crop();
    test_paste();
    test_channel_append();
    test_add();
    test_sub();
    test_mul();
    test_mad();
    test_min();
    test_max();
    test_over(TypeFloat);
    test_over(TypeHalf);
    test_zover();
    test_compare();
    test_isConstantColor();
    test_isConstantChannel();
    test_isMonochrome();
    test_computePixelStats();
    histogram_computation_test();
    test_maketx_from_imagebuf();
    test_IBAprep();
    test_validate_st_warp_checks();
    test_opencv();
    test_color_management();
    test_yee();
    test_demosaic();
    test_simple_perpixel<float>();
    test_simple_perpixel<half>();

    benchmark_parallel_image(64, iterations * 64);
    benchmark_parallel_image(512, iterations * 16);
    benchmark_parallel_image(1024, iterations * 4);
    benchmark_parallel_image(2048, iterations);

    return unit_test_failures;
}
