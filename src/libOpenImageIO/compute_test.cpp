// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


//
// Task: take "images" A and B, and compute R = A*A + B.
//
// Do this a whole bunch of different ways and benchmark.
//


#include <iostream>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/unittest.h>

using namespace OIIO;

static int iterations = 0;
static int numthreads = Sysutil::hardware_concurrency();
static int ntrials    = 5;
static bool verbose   = false;
static bool wedge     = false;
static bool allgpus   = false;

static spin_mutex print_mutex;  // make the prints not clobber each other

static int xres = 1920, yres = 1080, channels = 3;
static int npixels = xres * yres;
static int size    = npixels * channels;
static ImageBuf imgA, imgB, imgR;



static void
test_arrays(ROI)
{
    const float* a = (const float*)imgA.localpixels();
    OIIO_DASSERT(a);
    const float* b = (const float*)imgB.localpixels();
    OIIO_DASSERT(b);
    float* r = (float*)imgR.localpixels();
    OIIO_DASSERT(r);
    for (int x = 0; x < size; ++x)
        r[x] = a[x] * a[x] + b[x];
}



static void
test_arrays_like_image(ROI roi)
{
    const float* a = (const float*)imgA.localpixels();
    OIIO_DASSERT(a);
    const float* b = (const float*)imgB.localpixels();
    OIIO_DASSERT(b);
    float* r = (float*)imgR.localpixels();
    OIIO_DASSERT(r);
    int nchannels = imgA.nchannels();
    for (int y = roi.ybegin; y < roi.yend; ++y) {
        for (int x = roi.xbegin; x < roi.xend; ++x) {
            int i = (y * xres + x) * nchannels;
            for (int c = 0; c < nchannels; ++c)
                r[i + c] = a[i + c] * a[i + c] + b[i + c];
        }
    }
}



static void
test_arrays_simd4(ROI)
{
    const float* a = (const float*)imgA.localpixels();
    OIIO_DASSERT(a);
    const float* b = (const float*)imgB.localpixels();
    OIIO_DASSERT(b);
    float* r = (float*)imgR.localpixels();
    OIIO_DASSERT(r);
    int x, end4 = size - (size & 3);
    for (x = 0; x < end4; x += 4, a += 4, b += 4, r += 4) {
        simd::vfloat4 a_simd(a), b_simd(b);
        *(simd::vfloat4*)r = a_simd * a_simd + b_simd;
    }
    for (; x < size; ++x, ++a, ++b, ++r) {
        *r = a[0] * a[0] + b[0];
    }
}



static void
test_arrays_like_image_simd(ROI roi)
{
    const float* a = (const float*)imgA.localpixels();
    OIIO_DASSERT(a);
    const float* b = (const float*)imgB.localpixels();
    OIIO_DASSERT(b);
    float* r = (float*)imgR.localpixels();
    OIIO_DASSERT(r);
    int nchannels = imgA.nchannels();
    for (int y = roi.ybegin; y < roi.yend; ++y) {
        for (int x = roi.xbegin; x < roi.xend; ++x) {
            int i = (y * xres + x) * nchannels;
            simd::vfloat4 a_simd, b_simd, r_simd;
            a_simd.load(a + i, 3);
            b_simd.load(b + i, 3);
            r_simd = a_simd * a_simd + b_simd;
            r_simd.store(r + i, 3);
        }
    }
}



static void
test_IBA(ROI roi, int threads)
{
    ImageBufAlgo::mad(imgR, imgA, imgA, imgB, roi, threads);
}



void
test_compute()
{
    Benchmarker bench;
    bench.iterations(iterations);
    bench.trials(ntrials);
    bench.work(xres * yres * channels);
    bench.units(Benchmarker::Unit::ms);

    ROI roi(0, xres, 0, yres, 0, 1, 0, channels);

    ImageBufAlgo::zero(imgR);
    bench("1D array loop", test_arrays, roi);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 0), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 1), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 2), 0.50,
                            0.001);
    // imgR.write ("ref.exr");

    ImageBufAlgo::zero(imgR);
    bench("iterated as image", test_arrays_like_image, roi);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 0), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 1), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 2), 0.50,
                            0.001);

    ImageBufAlgo::zero(imgR);
    bench("iterated as image, threaded",
          [&]() { ImageBufAlgo::parallel_image(roi, test_arrays_like_image); });
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 0), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 1), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 2), 0.50,
                            0.001);

    ImageBufAlgo::zero(imgR);
    bench("1D array loop, SIMD", test_arrays_simd4, roi);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 0), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 1), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 2), 0.50,
                            0.001);

    ImageBufAlgo::zero(imgR);
    bench("iterated as image, SIMD", test_arrays_like_image_simd, roi);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 0), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 1), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 2), 0.50,
                            0.001);

    ImageBufAlgo::zero(imgR);
    bench("iterated as image, SIMD, threaded", [&]() {
        ImageBufAlgo::parallel_image(roi, test_arrays_like_image_simd);
    });
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 0), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 1), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 2), 0.50,
                            0.001);

    ImageBufAlgo::zero(imgR);
    bench("IBA::mad 1 thread", test_IBA, roi, 1);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 0), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 1), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 2), 0.50,
                            0.001);

    ImageBufAlgo::zero(imgR);
    bench("IBA::mad threaded", test_IBA, roi, numthreads);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 0), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 1), 0.25,
                            0.001);
    OIIO_CHECK_EQUAL_THRESH(imgR.getchannel(xres / 2, yres / 2, 0, 2), 0.50,
                            0.001);
}



static void
getargs(int argc, char* argv[])
{
    ArgParse ap;
    // clang-format off
    ap.intro("compute_test\n" OIIO_INTRO_STRING)
      .usage("compute_test [options]");

    ap.arg("-v", &verbose)
      .help("Verbose mode");
    ap.arg("--threads %d", &numthreads)
      .help(Strutil::fmt::format("Number of threads (default: {})", numthreads));
    ap.arg("--iters %d", &iterations)
      .help(Strutil::fmt::format("Number of iterations (default: {})", iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");
    ap.arg("--allgpus", &allgpus)
      .help("Run OpenCL tests on all devices, not just default");
    ap.arg("--wedge", &wedge)
      .help("Do a wedge test");
    // clang-format on

    ap.parse(argc, (const char**)argv);
}



int
main(int argc, char* argv[])
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs(argc, argv);

    // Initialize
    imgA.reset(ImageSpec(xres, yres, channels, TypeDesc::FLOAT));
    imgB.reset(ImageSpec(xres, yres, channels, TypeDesc::FLOAT));
    imgR.reset(ImageSpec(xres, yres, channels, TypeDesc::FLOAT));
    float red[3]   = { 1, 0, 0 };
    float green[3] = { 0, 1, 0 };
    float blue[3]  = { 0, 0, 1 };
    float black[3] = { 0, 0, 0 };
    ImageBufAlgo::fill(imgA, cspan<float>(red), cspan<float>(green),
                       cspan<float>(red), cspan<float>(green));
    ImageBufAlgo::fill(imgB, cspan<float>(blue), cspan<float>(blue),
                       cspan<float>(black), cspan<float>(black));
    // imgA.write ("A.exr");
    // imgB.write ("B.exr");

    test_compute();

    return unit_test_failures;
}
