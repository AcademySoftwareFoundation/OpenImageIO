// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/unittest.h>


using namespace OIIO;
using namespace simd;


// Aid for things that are too short to benchmark accurately
#define REP10(x) x, x, x, x, x, x, x, x, x, x

static int iterations = 1000000;
static int ntrials    = 5;
static bool verbose   = false;



static void
getargs(int argc, char* argv[])
{
    ArgParse ap;
    // clang-format off
    ap.intro("color_test\n" OIIO_INTRO_STRING)
      .usage("color_test [options]");

    ap.arg("-v", &verbose)
      .help("Verbose mode");
    ap.arg("--iters %d", &iterations)
      .help(Strutil::fmt::format("Number of iterations (default: {})", iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");
    // clang-format on

    ap.parse(argc, (const char**)argv);
}



static void
test_sRGB_conversion()
{
    Benchmarker bench;

    OIIO_CHECK_EQUAL_THRESH(linear_to_sRGB(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_sRGB(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_sRGB(0.5f), 0.735356983052449f, 1.0e-6);

    OIIO_CHECK_EQUAL_THRESH(sRGB_to_linear(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(sRGB_to_linear(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(sRGB_to_linear(0.5f), 0.214041140482232f, 1.0e-6);

    // Check the SIMD versions, too
    OIIO_CHECK_SIMD_EQUAL_THRESH(linear_to_sRGB(vfloat4(0.0f)), vfloat4(0.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(linear_to_sRGB(vfloat4(1.0f)), vfloat4(1.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(linear_to_sRGB(vfloat4(0.5f)),
                                 vfloat4(0.735356983052449f), 1.0e-5);

    OIIO_CHECK_SIMD_EQUAL_THRESH(sRGB_to_linear(vfloat4(0.0f)), vfloat4(0.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(sRGB_to_linear(vfloat4(1.0f)), vfloat4(1.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(sRGB_to_linear(vfloat4(0.5f)),
                                 vfloat4(0.214041140482232f), 1.0e-5);

    float fval = 0.5f;
    clobber(fval);
    vfloat4 vfval(fval);
    clobber(vfval);
    bench("sRGB_to_linear",
          [&]() { return DoNotOptimize(sRGB_to_linear(fval)); });
    bench("linear_to_sRGB",
          [&]() { return DoNotOptimize(sRGB_to_linear(fval)); });
    bench.work(4);
    bench("sRGB_to_linear simd",
          [&]() { return DoNotOptimize(sRGB_to_linear(vfval)); });
    bench("linear_to_sRGB simd",
          [&]() { return DoNotOptimize(sRGB_to_linear(vfval)); });
}



static void
test_Rec709_conversion()
{
    Benchmarker bench;

    OIIO_CHECK_EQUAL_THRESH(linear_to_Rec709(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_Rec709(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_Rec709(0.5f), 0.705515089922121f, 1.0e-6);

    OIIO_CHECK_EQUAL_THRESH(Rec709_to_linear(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(Rec709_to_linear(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(Rec709_to_linear(0.5f), 0.259589400506286f, 1.0e-6);

    float fval = 0.5f;
    clobber(fval);
    bench("Rec709_to_linear",
          [&]() { return DoNotOptimize(Rec709_to_linear(fval)); });
    bench("linear_to_Rec709",
          [&]() { return DoNotOptimize(Rec709_to_linear(fval)); });
}



// Exercise the built-in color interop ID <-> CICP table via the public
// ColorConfig API (the table itself is a private, static array in
// color_ocio.cpp, so it can only be reached through get_color_interop_id()
// and get_cicp()).
static void
test_color_interop_ids()
{
    const ColorConfig& cc = ColorConfig::default_colorconfig();

    // A representative sample of built-in interop IDs should resolve to
    // themselves (case-insensitively) and, where the table records a CICP
    // correspondence, get_cicp() should return the expected 4 values.
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("srgb_rec709_scene"),
                     "srgb_rec709_scene");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("SRGB_REC709_SCENE"),
                     "srgb_rec709_scene");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("lin_ap1_scene"), "lin_ap1_scene");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("g24_rec709_display"),
                     "g24_rec709_display");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("data"), "data");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("unknown"), "unknown");

    // Unknown names (and the empty string) return an empty interop ID.
    OIIO_CHECK_EQUAL(cc.get_color_interop_id("not_a_real_interop_id"), "");
    OIIO_CHECK_EQUAL(cc.get_color_interop_id(""), "");

    // Entries that carry a CICP mapping: primaries (cicp[0]) and transfer
    // (cicp[1]) round-trip through get_cicp() / get_color_interop_id(cicp).
    cspan<int> cicp = cc.get_cicp("srgb_rec709_scene");
    OIIO_CHECK_EQUAL(cicp.size(), 4);
    if (cicp.size() == 4) {
        OIIO_CHECK_EQUAL(cicp[0], 1);   // CICPPrimaries::Rec709
        OIIO_CHECK_EQUAL(cicp[1], 13);  // CICPTransfer::sRGB
        OIIO_CHECK_EQUAL(cicp[2], 1);   // CICPMatrix::BT709
        OIIO_CHECK_EQUAL(cc.get_color_interop_id(cicp.data()),
                         "srgb_rec709_scene");
    }

    // Entries with no CICP mapping (e.g. AP1/AP0 scene-linear, "data",
    // "unknown") return an empty span from get_cicp().
    OIIO_CHECK_EQUAL(cc.get_cicp("lin_ap1_scene").size(), 0);
    OIIO_CHECK_EQUAL(cc.get_cicp("data").size(), 0);
    OIIO_CHECK_EQUAL(cc.get_cicp("unknown").size(), 0);

    // get_color_interop_id(cicp) picks the first table match by
    // (primaries, transfer) alone -- matrix and range are not part of the
    // lookup key. Scene-referred entries are listed first in the table, so
    // for CICP tuples shared with a later display-referred entry (e.g.
    // Rec709/sRGB), the scene-referred interop ID is returned.
    const int rec709_srgb_cicp[4] = { 1, 13, 1, 1 };
    OIIO_CHECK_EQUAL(cc.get_color_interop_id(rec709_srgb_cicp),
                     "srgb_rec709_scene");
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

    test_sRGB_conversion();
    test_Rec709_conversion();
    test_color_interop_ids();

    return unit_test_failures != 0;
}
