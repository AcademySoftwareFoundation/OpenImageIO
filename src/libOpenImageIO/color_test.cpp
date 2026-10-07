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



static void
test_color_space_properties()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;

    // Published IDs declared by name or alias, a linear encoding with no ID,
    // one contradicting its ID, and a data space.
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        filename,
        "ocio_profile_version: 2.3\n"
        "roles: {default: ACEScg, scene_linear: ACEScg}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n    name: ACEScg\n    encoding: scene-linear\n"
        "    aliases: [lin_ap1_scene]\n"
        "  - !<ColorSpace> {name: srgb_rec709_scene, encoding: sdr-video}\n"
        "  - !<ColorSpace> {name: Adobe, aliases: [g22_adobergb_scene]}\n"
        "  - !<ColorSpace> {name: Plain, encoding: scene-linear}\n"
        "  - !<ColorSpace>\n    name: Mislabeled\n    encoding: scene-linear\n"
        "    aliases: [g22_rec709_scene]\n"
        "  - !<ColorSpace> {name: Data, isdata: true}\n"
        "  - !<ColorSpace> {name: Texture, aliases: [srgb_texture]}\n"));
    const float ap1[] = { .713f, .293f, .165f,   .83f,
                          .128f, .044f, .32168f, .33767f };
    const float p3d65[] = { .68f, .32f, .265f, .69f, .15f, .06f, .3127f, .329f };
    cspan<float> saved;
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        saved = config.get_chromaticities("scene_linear");
        OIIO_CHECK_ASSERT(saved == cspan<float>(ap1));
        OIIO_CHECK_EQUAL(config.get_transfer_gamma("scene_linear"), 1.0f);
        OIIO_CHECK_EQUAL(config.get_transfer_gamma("srgb_rec709_scene"), 0.0f);
        OIIO_CHECK_EQUAL(config.get_chromaticities("srgb_rec709_scene").size(),
                         8);
        // OIIO's built-in names resolve as they do in the other queries.
        OIIO_CHECK_EQUAL(config.get_chromaticities("sRGB").size(), 8);
        OIIO_CHECK_EQUAL(config.get_transfer_gamma("Adobe"), 563.0f / 256.0f);
        for (auto name : { "Plain", "Mislabeled" }) {
            OIIO_CHECK_EQUAL(config.get_transfer_gamma(name), 1.0f);
            OIIO_CHECK_ASSERT(config.get_chromaticities(name).empty());
        }
        for (auto name : { "Data", "data", "missing" }) {
            OIIO_CHECK_EQUAL(config.get_transfer_gamma(name), 0.0f);
            OIIO_CHECK_ASSERT(config.get_chromaticities(name).empty());
        }
        // A legacy alias, and IDs this config doesn't define, answer as they
        // do for get_color_interop_id() and get_cicp().
        OIIO_CHECK_EQUAL(config.get_color_interop_id("Texture"),
                         "srgb_rec709_scene");
        OIIO_CHECK_EQUAL(config.get_transfer_gamma("Texture"), 0.0f);
        OIIO_CHECK_EQUAL(config.get_chromaticities("Texture").size(), 8);
        OIIO_CHECK_EQUAL(config.get_transfer_gamma("lin_p3d65_display"), 1.0f);
        OIIO_CHECK_ASSERT(config.get_chromaticities("lin_p3d65_display")
                          == cspan<float>(p3d65));
    }
    // The span stays valid after the config is gone.
    OIIO_CHECK_ASSERT(saved == cspan<float>(ap1));
    // OpenColorIO 2.3's built-in CG config gives its spaces only the legacy
    // aliases, which identify them here too.
    {
        ColorConfig config("ocio://cg-config-v2.1.0_aces-v1.3_ocio-v2.3");
        OIIO_CHECK_ASSERT(config.get_chromaticities("ACEScg")
                          == cspan<float>(ap1));
        OIIO_CHECK_EQUAL(config.get_chromaticities("sRGB - Texture").size(), 8);
        OIIO_CHECK_EQUAL(config.get_transfer_gamma("sRGB - Texture"), 0.0f);
    }
    Filesystem::remove(filename);

    // OpenColorIO 2.5 reads a declared interop_id, which outranks the name.
    if (ColorConfig::OpenColorIO_version_hex() >= 0x02050000) {
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(
            filename,
            "ocio_profile_version: 2.5\n"
            "roles: {default: g18_rec709_scene}\n"
            "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
            "colorspaces:\n  - !<ColorSpace>\n    name: g18_rec709_scene\n"
            "    interop_id: g22_ap1_scene\n"));
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_EQUAL(config.get_transfer_gamma("g18_rec709_scene"), 2.2f);
        OIIO_CHECK_ASSERT(config.get_chromaticities("g18_rec709_scene")
                          == cspan<float>(ap1));
        Filesystem::remove(filename);
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
    test_color_space_properties();

    return unit_test_failures != 0;
}
