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
#include <OpenImageIO/filesystem.h>
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
test_isData()
{
    // A data space answers by its name, an alias or a role, in any case,
    // whether or not the config's "data" role names it.
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        filename,
        "ocio_profile_version: 2.3\n"
        "roles: {default: linear, scene_linear: linear, data: Raw}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n"
        "  - !<ColorSpace> {name: linear}\n"
        "  - !<ColorSpace> {name: Raw, aliases: [\"Utility - Raw\"], "
        "isdata: true}\n"
        "  - !<ColorSpace> {name: Normals, isdata: true}\n"));
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        for (auto name :
             { "Raw", "RAW", "Utility - Raw", "data", "Normals", "normals" })
            OIIO_CHECK_ASSERT(config.isData(name));
        for (auto name : { "linear", "default", "missing" })
            OIIO_CHECK_FALSE(config.isData(name));
    }
    Filesystem::remove(filename);
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



static void
test_linear_display_cicp()
{
    // The linear display identities carry their scene identities' codes,
    // which still read back as the scene identities.
    const ColorConfig& config(ColorConfig::default_colorconfig());
    const struct {
        const char* display;
        const char* scene;
        int cicp[4];
    } cases[] = {
        { "lin_rec709_display", "lin_rec709_scene", { 1, 8, 1, 1 } },
        { "lin_p3d65_display", "lin_p3d65_scene", { 12, 8, 1, 1 } },
        { "lin_rec2020_display", "lin_rec2020_scene", { 9, 8, 10, 1 } },
    };
    for (const auto& c : cases) {
        cspan<int> cicp = config.get_cicp(c.display);
        OIIO_CHECK_ASSERT(cicp == cspan<int>(c.cicp));
        OIIO_CHECK_EQUAL(config.get_color_interop_id(c.cicp), c.scene);
    }
}



static void
test_gamma_pair_conversion()
{
    // OCIO < 2.5 composes back-to-back exponents in the wrong direction under
    // its default optimization, giving v^(1.8/2.2) instead of v^(2.2/1.8).
    ColorConfig config("ocio://cg-config-v2.1.0_aces-v1.3_ocio-v2.3");
    auto proc = config.createColorProcessor("Gamma 2.2 Rec.709 - Texture",
                                            "Gamma 1.8 Rec.709 - Texture");
    OIIO_CHECK_ASSERT(proc);
    if (!proc)
        return;
    for (float v : { 0.001f, 0.18f, 0.5f }) {
        float rgb[3] = { v, v, v };
        proc->apply(rgb);
        OIIO_CHECK_EQUAL_THRESH(rgb[0], std::pow(v, 2.2f / 1.8f), 1.0e-4f);
    }
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
    test_isData();
    test_Rec709_conversion();
    test_linear_display_cicp();
    test_gamma_pair_conversion();

    return unit_test_failures != 0;
}
