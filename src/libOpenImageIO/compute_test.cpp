/*
  Copyright 2016 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

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


//
// Task: take "images" A and B, and compute R = A*A + B.
//
// Do this a whole bunch of different ways and benchmark.
//


#include <iostream>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/benchmark.h>

using namespace OIIO;

static int iterations = 0;
static int numthreads = Sysutil::hardware_concurrency();
static int ntrials = 5;
static bool verbose = false;
static bool wedge = false;
static bool allgpus = false;

static spin_mutex print_mutex;  // make the prints not clobber each other

static int xres = 1920, yres = 1080, channels = 3;
static int npixels = xres * yres;
static int size = npixels * channels;
static ImageBuf imgA, imgB, imgR;



static void
test_arrays (ROI roi)
{
    const float *a = (const float *)imgA.localpixels(); ASSERT(a);
    const float *b = (const float *)imgB.localpixels(); ASSERT(b);
    float *r = (float *)imgR.localpixels(); ASSERT(r);
    for (int x = 0; x < size; ++x)
        r[x] = a[x] * a[x] + b[x];
}



static void
test_arrays_like_image (ROI roi)
{
    const float *a = (const float *)imgA.localpixels(); ASSERT(a);
    const float *b = (const float *)imgB.localpixels(); ASSERT(b);
    float *r = (float *)imgR.localpixels(); ASSERT(r);
    int nchannels = imgA.nchannels();
    for (int y = roi.ybegin; y < roi.yend; ++y) {
        for (int x = roi.xbegin; x < roi.xend; ++x) {
            int i = (y*xres + x) * nchannels;
            for (int c = 0; c < nchannels; ++c)
                r[i+c] = a[i+c] * a[i+c] + b[i+c];
        }
    }
}



static void
test_arrays_simd4 (ROI roi)
{
    const float *a = (const float *)imgA.localpixels(); ASSERT(a);
    const float *b = (const float *)imgB.localpixels(); ASSERT(b);
    float *r = (float *)imgR.localpixels(); ASSERT(r);
    int x, end4 = size - (size&3);
    for (x = 0; x < end4; x += 4, a += 4, b += 4, r += 4) {
        simd::vfloat4 a_simd(a), b_simd(b);
        *(simd::vfloat4 *)r = a_simd * a_simd + b_simd;
    }
    for ( ; x < size; ++x, ++a, ++b, ++r) {
        *r = a[0]*a[0] + b[0];
    }
}



static void
test_arrays_like_image_simd (ROI roi)
{
    const float *a = (const float *)imgA.localpixels(); ASSERT(a);
    const float *b = (const float *)imgB.localpixels(); ASSERT(b);
    float *r = (float *)imgR.localpixels(); ASSERT(r);
    int nchannels = imgA.nchannels();
    for (int y = roi.ybegin; y < roi.yend; ++y) {
        for (int x = roi.xbegin; x < roi.xend; ++x) {
            int i = (y*xres + x) * nchannels;
            simd::vfloat4 a_simd, b_simd, r_simd;
            a_simd.load (a+i, 3);
            b_simd.load (b+i, 3);
            r_simd = a_simd * a_simd + b_simd;
            r_simd.store (r+i, 3);
        }
    }
}



static void
test_IBA (ROI roi, int threads)
{
    ImageBufAlgo::mad (imgR, imgA, imgA, imgB, roi, threads);
}





void
test_compute ()
{
    Benchmarker bench;
    bench.iterations (iterations);
    bench.trials (ntrials);
    bench.work (xres*yres*channels);
    bench.units (Benchmarker::Unit::ms);

    ROI roi (0, xres, 0, yres, 0, 1, 0, channels);

    ImageBufAlgo::zero (imgR);
    bench ("1D array loop", test_arrays, roi);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,0), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,1), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,2), 0.50, 0.001);
    // imgR.write ("ref.exr");

    ImageBufAlgo::zero (imgR);
    bench ("iterated as image", test_arrays_like_image, roi);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,0), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,1), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,2), 0.50, 0.001);

    ImageBufAlgo::zero (imgR);
    bench ("iterated as image, threaded",
           [&](){ ImageBufAlgo::parallel_image (roi, test_arrays_like_image); });
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,0), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,1), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,2), 0.50, 0.001);

    ImageBufAlgo::zero (imgR);
    bench ("1D array loop, SIMD", test_arrays_simd4, roi);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,0), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,1), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,2), 0.50, 0.001);

    ImageBufAlgo::zero (imgR);
    bench ("iterated as image, SIMD", test_arrays_like_image_simd, roi);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,0), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,1), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,2), 0.50, 0.001);

    ImageBufAlgo::zero (imgR);
    bench ("iterated as image, SIMD, threaded",
           [&](){ ImageBufAlgo::parallel_image (roi, test_arrays_like_image_simd); });
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,0), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,1), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,2), 0.50, 0.001);

    ImageBufAlgo::zero (imgR);
    bench ("IBA::mad 1 thread", test_IBA, roi, 1);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,0), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,1), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,2), 0.50, 0.001);

    ImageBufAlgo::zero (imgR);
    bench ("IBA::mad threaded", test_IBA, roi, numthreads);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,0), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,1), 0.25, 0.001);
    OIIO_CHECK_EQUAL_THRESH (imgR.getchannel(xres/2,yres/2,0,2), 0.50, 0.001);
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("compute_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  compute_test [options]",
                // "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose mode",
                "--threads %d", &numthreads,
                    ustring::format("Number of threads (default: %d)", numthreads).c_str(),
                "--iterations %d", &iterations,
                    ustring::format("Number of iterations (default: %d)", iterations).c_str(),
                "--trials %d", &ntrials, "Number of trials",
                "--allgpus", &allgpus, "Run OpenCL tests on all devices, not just default",
                "--wedge", &wedge, "Do a wedge test",
                NULL);
    if (ap.parse (argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }
}



int main (int argc, char *argv[])
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs (argc, argv);

    // Initialize
    imgA.reset (ImageSpec (xres, yres, channels, TypeDesc::FLOAT));
    imgB.reset (ImageSpec (xres, yres, channels, TypeDesc::FLOAT));
    imgR.reset (ImageSpec (xres, yres, channels, TypeDesc::FLOAT));
    float red[3]  = { 1, 0, 0 };
    float green[3] = { 0, 1, 0 };
    float blue[3]  = { 0, 0, 1 };
    float black[3] = { 0, 0, 0 };
    ImageBufAlgo::fill (imgA, red, green, red, green);
    ImageBufAlgo::fill (imgB, blue, blue, black, black);
    // imgA.write ("A.exr");
    // imgB.write ("B.exr");

    test_compute ();

    return unit_test_failures;
}
