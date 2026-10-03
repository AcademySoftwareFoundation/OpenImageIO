// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"


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



// Loading a color config is logged as "ColorConfig::reset".
static bool
color_config_loaded()
{
    return Strutil::contains(OIIO::get_string_attribute("timing_report"),
                             "ColorConfig");
}



// Must run before anything else loads the default color config.
static void
test_colorspace_without_config()
{
    OIIO::attribute("log_times", 1);
    OIIO_CHECK_ASSERT(!color_config_loaded());

    // Setting a color space clears contradicting metadata without a config.
    ImageSpec spec;
    spec.attribute("tiff:ColorSpace", 1);
    spec.attribute("oiio:Gamma", 2.2f);
    spec.set_colorspace("lin_rec709_scene");
    OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                     "lin_rec709_scene");
    OIIO_CHECK_ASSERT(!spec.find_attribute("tiff:ColorSpace"));
    OIIO_CHECK_ASSERT(!spec.find_attribute("oiio:Gamma"));
    set_colorspace_rec709_gamma(spec, 2.2f);
    OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                     "g22_rec709_scene");
    OIIO_CHECK_EQUAL(spec.get_float_attribute("oiio:Gamma"), 2.2f);
    // An sRGB name keeps "Exif:ColorSpace" without asking a config.
    spec.attribute("Exif:ColorSpace", 1);
    set_colorspace(spec, "srgb_rec709_scene");
    OIIO_CHECK_ASSERT(spec.find_attribute("Exif:ColorSpace"));

    // CICP to color interop ID needs no config.
    OIIO_CHECK_EQUAL(get_color_interop_id({ 9, 16, 9, 1 }),
                     "pq_rec2020_display");
    OIIO_CHECK_EQUAL(get_color_interop_id({ 1, 13, 0, 1 }),
                     "srgb_rec709_scene");
    OIIO_CHECK_EQUAL(get_color_interop_id({ 2, 2, 2, 1 }), "");
    OIIO_CHECK_EQUAL(get_color_interop_id({}), "");
    OIIO_CHECK_ASSERT(!color_config_loaded());

    // Only a non-sRGB name with "Exif:ColorSpace" present asks the config
    // whether to erase it.
    set_colorspace(spec, "lin_rec709_scene");
    OIIO_CHECK_ASSERT(!spec.find_attribute("Exif:ColorSpace"));
    OIIO_CHECK_ASSERT(color_config_loaded());

    const int cicp[4] = { 9, 16, 9, 1 };
    OIIO_CHECK_EQUAL(ColorConfig::default_colorconfig().get_color_interop_id(
                         cicp),
                     get_color_interop_id(cicp));
    OIIO::attribute("log_times", 0);
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
test_declared_color_space_info()
{
    ColorSpaceInfo invalid;
    OIIO_CHECK_FALSE(invalid.valid());
    OIIO_CHECK_ASSERT(invalid.chromaticities().empty());
    OIIO_CHECK_EQUAL(invalid.transfer_function_gamma(), 0.0f);
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
        "  - !<ColorSpace> {name: Data, isdata: true}\n"));
    const float ap1[] = { .713f, .293f, .165f,   .83f,
                          .128f, .044f, .32168f, .33767f };
    ColorSpaceInfo saved;
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        saved = config.get_color_space_info("scene_linear");
        OIIO_CHECK_ASSERT(saved.valid());
        OIIO_CHECK_EQUAL(saved.transfer_function_gamma(), 1.0f);
        OIIO_CHECK_ASSERT(saved.chromaticities() == cspan<float>(ap1));
        auto srgb = config.get_color_space_info("srgb_rec709_scene");
        OIIO_CHECK_EQUAL(srgb.transfer_function_gamma(), 0.0f);
        OIIO_CHECK_EQUAL(srgb.chromaticities().size(), 8);
        // OIIO's built-in names resolve as they do in the other queries.
        OIIO_CHECK_EQUAL(
            config.get_color_space_info("sRGB").chromaticities().size(), 8);
        OIIO_CHECK_EQUAL(
            config.derive_color_space_info("Adobe").transfer_function_gamma(),
            563.0f / 256.0f);
        for (auto name : { "Plain", "Mislabeled" }) {
            auto info = config.get_color_space_info(name);
            OIIO_CHECK_EQUAL(info.transfer_function_gamma(), 1.0f);
            OIIO_CHECK_ASSERT(info.chromaticities().empty());
        }
        auto data = config.get_color_space_info("Data");
        OIIO_CHECK_ASSERT(data.valid());
        OIIO_CHECK_ASSERT(data.chromaticities().empty());
        OIIO_CHECK_EQUAL(data.transfer_function_gamma(), 0.0f);
        OIIO_CHECK_FALSE(config.get_color_space_info("missing").valid());
    }
    // The properties outlive the config; a move leaves its source invalid.
    auto copy  = saved;
    auto moved = std::move(saved);
    OIIO_CHECK_FALSE(saved.valid());
    OIIO_CHECK_ASSERT(copy.chromaticities() == cspan<float>(ap1));
    OIIO_CHECK_EQUAL(moved.transfer_function_gamma(), 1.0f);
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
        auto info = config.get_color_space_info("g18_rec709_scene");
        OIIO_CHECK_EQUAL(info.transfer_function_gamma(), 2.2f);
        OIIO_CHECK_ASSERT(info.chromaticities() == cspan<float>(ap1));
        Filesystem::remove(filename);
    }
}



static void
test_interop_id_memo()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;

    const char* test_config = OIIO_COLOR_TEST_CONFIG;

    // An older builtin with no native interop-ID declarations retains its
    // authored CIF alias answers without requiring other recognition work.
    {
        ColorConfig old_builtin("ocio://cg-config-v1.0.0_aces-v1.3_ocio-v2.1");
        OIIO_CHECK_EQUAL(old_builtin.get_color_interop_id("ACEScg"),
                         "lin_ap1_scene");
        OIIO_CHECK_EQUAL(old_builtin.get_color_interop_id("ACEScg"),
                         "lin_ap1_scene");
    }

    string_view saved;
    {
        ColorConfig first(test_config);
        OIIO_CHECK_FALSE(first.has_error());
        saved = first.get_color_interop_id("ACEScg");
        OIIO_CHECK_EQUAL(saved, "lin_ap1_scene");
    }
    OIIO_CHECK_EQUAL(saved, "lin_ap1_scene");

    constexpr int nthreads = 8;
    std::array<std::string, nthreads> results;
    std::array<std::unique_ptr<ColorConfig>, nthreads> configs;
    for (auto& concurrent : configs)
        concurrent.reset(new ColorConfig(test_config));
    for (const auto& concurrent : configs)
        OIIO_CHECK_FALSE(concurrent->has_error());
    OIIO_CHECK_ASSERT(configs[0]->getColorSpaceNames()
                      == configs[1]->getColorSpaceNames());
    OIIO_CHECK_EQUAL(configs[0]->resolve("scene_linear"),
                     "Linear Rec.709 (sRGB)");
    OIIO_CHECK_FALSE(configs[0]->isData("scene_linear"));

    // Race the first resolved-negative query, then repeat it warm. Gamma 2.6
    // Rec.709 is measurable but has no scene-referred reference counterpart,
    // so the sweep completes and the miss is what gets shared.
    const char* unmatched = "Gamma 2.6 Encoded Rec.709 (sRGB)";
    std::vector<std::thread> threads;
    for (int i = 0; i < nthreads; ++i) {
        threads.emplace_back([&, i] {
            results[i] = configs[i]->get_color_interop_id(unmatched);
        });
    }
    for (auto& thread : threads)
        thread.join();
    for (const auto& result : results)
        OIIO_CHECK_EQUAL(result, "");

    threads.clear();
    for (int i = 0; i < nthreads; ++i) {
        threads.emplace_back([&, i] {
            results[i] = configs[i]->get_color_interop_id(unmatched);
        });
    }
    for (auto& thread : threads)
        thread.join();
    for (const auto& result : results)
        OIIO_CHECK_EQUAL(result, "");

    // The same comparison identifies an undeclared log encoding the alias
    // table never listed, and the identity selects it back by name.
    OIIO_CHECK_EQUAL(configs[0]->get_color_interop_id("ACEScct"),
                     "ocio:acescct_ap1_scene");
    OIIO_CHECK_EQUAL(configs[0]->resolve("ocio:acescct_ap1_scene"), "ACEScct");
    OIIO_CHECK_ASSERT(configs[0]->equivalent("ACEScct", "acescct_ap1_scene"));

    OIIO_CHECK_EQUAL(configs[0]->get_color_interop_id("scene_linear"),
                     "lin_rec709_scene");

    // Growing the shared memo must not invalidate the first wrapper's view.
    ColorConfig config(test_config);
    OIIO_CHECK_FALSE(config.has_error());
    for (const auto& name : config.getColorSpaceNames())
        (void)config.get_color_interop_id(name);
    OIIO_CHECK_EQUAL(saved, "lin_ap1_scene");

    OIIO_CHECK_ASSERT(config.reset("ocio://default"));
    OIIO_CHECK_FALSE(config.has_error());
    OIIO_CHECK_EQUAL(config.get_color_interop_id("scene_linear"),
                     "lin_ap1_scene");
}



// The measured equality ID, as a derived snapshot reports it.
static std::string
equality_id(const ColorConfig& config, string_view colorspace,
            string_view context_key = "", string_view context_value = "")
{
    return std::string(ColorSpaceInfoAccess::equality_id(
        pvt::color_space_info(config, colorspace, true, context_key,
                              context_value)));
}



static void
test_recognized_id_equivalence()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    const char* text           = R"(ocio_profile_version: 2.3
roles: {default: Reference, aces_interchange: Reference}
file_rules:
  - !<Rule> {name: Default, colorspace: Reference}
colorspaces:
  - !<ColorSpace> {name: Reference}
  - !<ColorSpace> {name: Twin}
  - !<ColorSpace>
    name: Near
    to_scene_reference: !<MatrixTransform> {matrix: [1.001, 0, 0, 0, 0, 1.001, 0, 0, 0, 0, 1.001, 0, 0, 0, 0, 1]}
)";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(filename, text));
    ColorConfig config(filename);
    OIIO_CHECK_FALSE(config.has_error());
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Reference"), "lin_ap0_scene");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Near"), "lin_ap0_scene");
    OIIO_CHECK_FALSE(config.equivalent("Reference", "Near"));
    OIIO_CHECK_FALSE(config.equivalent("Near", "Reference"));
    OIIO_CHECK_ASSERT(config.equivalent("Reference", "Twin"));
    // Recognition is accepted within a response tolerance, so a gain too small
    // to move a probe is measured as the encoding it perturbs. Agreeing
    // measured identities nominate the pair and no more: the gain is real, the
    // conversion is not a no-op, and it is still performed.
    OIIO_CHECK_EQUAL(equality_id(config, "Reference"), "lin_ap0_scene");
    OIIO_CHECK_EQUAL(equality_id(config, "Near"), "lin_ap0_scene");
    OIIO_CHECK_FALSE(config.equivalent("Reference", "Near"));
    auto processor = config.createColorProcessor("Near", "Reference");
    OIIO_CHECK_ASSERT(processor);
    if (processor) {
        float pixel[3] = { 0.25f, 0.3f, 0.4f };
        processor->apply(pixel);
        OIIO_CHECK_EQUAL_THRESH(pixel[0], 0.25025f, 1.0e-7f);
        OIIO_CHECK_EQUAL_THRESH(pixel[1], 0.3003f, 1.0e-7f);
        OIIO_CHECK_EQUAL_THRESH(pixel[2], 0.4004f, 1.0e-7f);
    }
    Filesystem::remove(filename);
}



// A data space has no color to measure, so its own naming is the whole answer
// -- and that naming is read only after OpenColorIO has said the space is
// data, so a utility spelling on an ordinary color space asserts nothing.
// `bypass` and `data` can each be spelled once in a configuration, so the
// precedence rule is exercised by moving the one spelling that decides.
static void
test_measured_equality_id()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    auto fixture = [&](string_view data_name, string_view data_aliases,
                       string_view utility_aliases) {
        std::string text
            = "ocio_profile_version: 2.3\n"
              "roles: {default: Reference, aces_interchange: Reference}\n"
              "file_rules:\n  - !<Rule> {name: Default, colorspace: Reference}\n"
              "colorspaces:\n"
              "  - !<ColorSpace>\n    name: Reference\n"
              "  - !<ColorSpace>\n    name: Utility\n";
        if (!utility_aliases.empty())
            text += "    aliases: [" + std::string(utility_aliases) + "]\n";
        text += "  - !<ColorSpace>\n    name: Unique\n"
                "    categories: [is-unique]\n"
                "  - !<ColorSpace>\n    name: "
                + std::string(data_name) + "\n    isdata: true\n";
        if (!data_aliases.empty())
            text += "    aliases: [" + std::string(data_aliases) + "]\n";
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(filename, text));
    };

    fixture("Mask", "", "");
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_EQUAL(equality_id(config, "Mask"), "data");
        // A configuration that marks a space unique has said no shared
        // identity describes it, whatever its operations reproduce.
        OIIO_CHECK_EQUAL(equality_id(config, "Unique"), "");
        OIIO_CHECK_EQUAL(equality_id(config, "Reference"), "lin_ap0_scene");
        OIIO_CHECK_EQUAL(equality_id(config, "Utility"), "lin_ap0_scene");
        OIIO_CHECK_ASSERT(config.equivalent("Reference", "Utility"));
        // An identity that resolves to a local definition reports that
        // definition's measurement. One this configuration does not define
        // names no local definition, so no substituted reference is measured
        // for it.
        OIIO_CHECK_EQUAL(equality_id(config, "lin_ap0_scene"), "lin_ap0_scene");
        OIIO_CHECK_EQUAL(equality_id(config, "g22_rec709_scene"), "");
        OIIO_CHECK_EQUAL(equality_id(config, "not a color space"), "");
        OIIO_CHECK_EQUAL(equality_id(config, ""), "");
        // The two modes share one retained result map and cannot answer for
        // each other, warm or cold, in this wrapper or the next.
        const string_view interop = config.get_color_interop_id("Reference");
        OIIO_CHECK_EQUAL(equality_id(config, "Reference"), "lin_ap0_scene");
        OIIO_CHECK_EQUAL(config.get_color_interop_id("Reference"), interop);
        OIIO_CHECK_EQUAL(equality_id(config, "Mask"), "data");
        ColorConfig second(filename);
        OIIO_CHECK_FALSE(second.has_error());
        OIIO_CHECK_EQUAL(equality_id(second, "Utility"), "lin_ap0_scene");
        OIIO_CHECK_EQUAL(second.get_color_interop_id("Reference"), interop);
        OIIO_CHECK_EQUAL(equality_id(second, "Reference"), "lin_ap0_scene");
    }

    // Named `bypass` wins outright, without regard to case.
    fixture("bypass", "", "");
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_EQUAL(equality_id(config, "bypass"), "bypass");
        OIIO_CHECK_EQUAL(equality_id(config, "BYPASS"), "bypass");
    }
    // An alias of `bypass` wins only where no data identity is present.
    fixture("Mask", "bypass", "");
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_EQUAL(equality_id(config, "Mask"), "bypass");
        OIIO_CHECK_EQUAL(equality_id(config, "bypass"), "bypass");
    }
    fixture("data", "bypass", "");
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_EQUAL(equality_id(config, "data"), "data");
        OIIO_CHECK_EQUAL(equality_id(config, "bypass"), "data");
    }
    fixture("Mask", "bypass, data", "");
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_EQUAL(equality_id(config, "Mask"), "data");
    }
    // The same spelling on an ordinary color space claims no treatment: the
    // definition behind it is measured like any other.
    fixture("Mask", "", "bypass");
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_EQUAL(equality_id(config, "Utility"), "lin_ap0_scene");
        OIIO_CHECK_EQUAL(equality_id(config, "bypass"), "lin_ap0_scene");
        OIIO_CHECK_EQUAL(equality_id(config, "Mask"), "data");
    }
    Filesystem::remove(filename);
}



static void
test_declared_id_equivalence()
{
    if (!ColorConfig::supportsOpenColorIO()
        || ColorConfig::OpenColorIO_version_hex() < 0x02050000)
        return;

    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    const char* text           = R"(ocio_profile_version: 2.5
roles: {default: Actual, scene_linear: Actual, color_timing: Actual, compositing_log: Actual, aces_interchange: Actual}
file_rules:
  - !<Rule> {name: Default, colorspace: Actual}
displays:
  Test:
    - !<View> {name: Raw, colorspace: Actual}
colorspaces:
  - !<ColorSpace>
    name: Actual
    aliases: [lin_ap0_scene]
    interop_id: lin_ap0_scene
  - !<ColorSpace>
    name: Altered
    interop_id: lin_ap0_scene
    to_scene_reference: !<MatrixTransform> {matrix: [2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]}
  - !<ColorSpace>
    name: Twin
  - !<ColorSpace>
    name: Renamed
    interop_id: my-studio:working
  - !<ColorSpace>
    name: FakeData
    interop_id: data
)";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(filename, text));
    ColorConfig config(filename);
    OIIO_CHECK_FALSE(config.has_error());
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Actual"), "lin_ap0_scene");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Altered"), "lin_ap0_scene");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Twin"), "lin_ap0_scene");
    OIIO_CHECK_FALSE(config.equivalent("Actual", "Altered"));
    OIIO_CHECK_FALSE(config.equivalent("Altered", "Actual"));
    OIIO_CHECK_ASSERT(config.equivalent("Actual", "Twin"));

    // What the definitions measure as, with every declaration withheld. The
    // declaration a measurement contradicts is not withdrawn from the write
    // identity above: an author owns what is written to a file, and this owns
    // only what the arithmetic is.
    OIIO_CHECK_EQUAL(equality_id(config, "Actual"), "lin_ap0_scene");
    OIIO_CHECK_EQUAL(equality_id(config, "Twin"), "lin_ap0_scene");
    OIIO_CHECK_EQUAL(equality_id(config, "Altered"), "");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Altered"), "lin_ap0_scene");

    // An exact copy that declares a different identity. The two write
    // identities disagree and stay that way; the measured ones agree, so the
    // conversion OCIO itself reports as a no-op is no longer performed.
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Renamed"),
                     "my-studio:working");
    OIIO_CHECK_EQUAL(config.resolve("my-studio:working"), "Renamed");
    // The cheap query selects by an authored ID, as resolve() does.
    auto authored = config.get_color_space_info("my-studio:working");
    OIIO_CHECK_ASSERT(authored.valid());
    OIIO_CHECK_EQUAL(ColorSpaceInfoAccess::color_interop_id(authored),
                     "my-studio:working");
    // CIF Rec 03 strips the query, never the declaration.
    OIIO_CHECK_EQUAL(config.resolve("working"), "working");
    OIIO_CHECK_FALSE(config.isData("my-studio:working"));
    OIIO_CHECK_EQUAL(equality_id(config, "Renamed"), "lin_ap0_scene");
    OIIO_CHECK_ASSERT(config.equivalent("Actual", "Renamed"));
    OIIO_CHECK_ASSERT(config.equivalent("Renamed", "Actual"));
    OIIO_CHECK_FALSE(config.equivalent("Altered", "Renamed"));
    // A utility identity declared on a space OpenColorIO does not call data
    // asserts no treatment. Data-ness is native, read before any naming, and
    // never inferred from an attribute.
    OIIO_CHECK_FALSE(config.isData("FakeData"));
    OIIO_CHECK_EQUAL(config.get_color_interop_id("FakeData"), "data");
    OIIO_CHECK_EQUAL(equality_id(config, "FakeData"), "lin_ap0_scene");

    // Measuring changes nothing about what is written.
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Renamed"),
                     "my-studio:working");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Actual"), "lin_ap0_scene");

    auto processor = config.createColorProcessor("Actual", "Altered");
    OIIO_CHECK_ASSERT(processor);
    if (processor) {
        float pixel[3] = { 0.25f, 0.3f, 0.4f };
        processor->apply(pixel);
        OIIO_CHECK_EQUAL_THRESH(pixel[0], 0.125f, 1.0e-6f);
        OIIO_CHECK_EQUAL_THRESH(pixel[1], 0.3f, 1.0e-6f);
        OIIO_CHECK_EQUAL_THRESH(pixel[2], 0.4f, 1.0e-6f);
    }
    Filesystem::remove(filename);
}



static void
test_color_space_info()
{
    ColorSpaceInfo invalid;
    OIIO_CHECK_FALSE(invalid.valid());
    OIIO_CHECK_ASSERT(invalid.chromaticities().empty());
    OIIO_CHECK_EQUAL(invalid.transfer_function_gamma(), 0.0f);
    for (auto field : { ColorSpaceInfoField::Chromaticities,
                        ColorSpaceInfoField::TransferFunction }) {
        OIIO_CHECK_FALSE(ColorSpaceInfoAccess::computed(invalid, field));
        OIIO_CHECK_FALSE(ColorSpaceInfoAccess::available(invalid, field));
        OIIO_CHECK_FALSE(ColorSpaceInfoAccess::derived(invalid, field));
    }
    const auto invalid_field = static_cast<ColorSpaceInfoField>(255);
    OIIO_CHECK_FALSE(ColorSpaceInfoAccess::computed(invalid, invalid_field));
    OIIO_CHECK_FALSE(ColorSpaceInfoAccess::available(invalid, invalid_field));
    OIIO_CHECK_FALSE(ColorSpaceInfoAccess::derived(invalid, invalid_field));
    if (!ColorConfig::supportsOpenColorIO())
        return;
    // Writers ask about a missing oiio:ColorSpace attribute, whose default
    // string_view has no storage. It names no color space.
    const ColorConfig& default_config = ColorConfig::default_colorconfig();
    OIIO_CHECK_FALSE(
        default_config.get_color_space_info(string_view()).valid());
    OIIO_CHECK_FALSE(
        default_config.derive_color_space_info(string_view()).valid());
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        filename,
        "ocio_profile_version: 2.3\n"
        "roles: {default: Working, scene_linear: Working}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n  - !<ColorSpace>\n    name: Working\n"
        "    encoding: scene-linear\n    aliases: [working_alias, lin_rec709]\n"
        "  - !<ColorSpace>\n    name: Data\n    isdata: true\n"
        "    encoding: scene-linear\n    aliases: [data_alias]\n"
        "  - !<ColorSpace> {name: Plain}\n"
        "  - !<ColorSpace> {name: srgb_tx}\n"
        "display_colorspaces:\n"
        "  - !<ColorSpace> {name: Screen, encoding: display-linear}\n"));
    ColorSpaceInfo saved;
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        saved = config.get_color_space_info("scene_linear");
        OIIO_CHECK_ASSERT(saved.valid());
        OIIO_CHECK_EQUAL(saved.transfer_function_gamma(), 1.0f);
        OIIO_CHECK_ASSERT(saved.chromaticities().empty());
        OIIO_CHECK_ASSERT(ColorSpaceInfoAccess::computed(
            saved, ColorSpaceInfoField::TransferFunction));
        OIIO_CHECK_ASSERT(ColorSpaceInfoAccess::available(
            saved, ColorSpaceInfoField::TransferFunction));
        OIIO_CHECK_FALSE(ColorSpaceInfoAccess::derived(
            saved, ColorSpaceInfoField::TransferFunction));
        OIIO_CHECK_FALSE(ColorSpaceInfoAccess::computed(
            saved, ColorSpaceInfoField::Chromaticities));
        OIIO_CHECK_FALSE(ColorSpaceInfoAccess::computed(saved, invalid_field));
        OIIO_CHECK_FALSE(ColorSpaceInfoAccess::available(saved, invalid_field));
        OIIO_CHECK_FALSE(ColorSpaceInfoAccess::derived(saved, invalid_field));
        OIIO_CHECK_EQUAL(config.get_color_space_info("working_alias")
                             .transfer_function_gamma(),
                         1.0f);
        // It takes the steps of resolve() that need no measurement: a
        // stripped namespace and a legacy name, but not a space that only
        // measurement could select for OIIO's generic "sRGB".
        for (auto name : { "acme:working_alias", "linear" })
            OIIO_CHECK_EQUAL(
                config.get_color_space_info(name).transfer_function_gamma(),
                1.0f);
        OIIO_CHECK_FALSE(config.get_color_space_info("sRGB").valid());
        // A linear encoding names its image state without an ID.
        OIIO_CHECK_EQUAL(ColorSpaceInfoAccess::image_state(saved), "scene");
        OIIO_CHECK_EQUAL(ColorSpaceInfoAccess::image_state(
                             config.get_color_space_info("Screen")),
                         "display");
        // Before derivation, an undeclared ID and state are not computed.
        const auto cheap = config.get_color_space_info("Plain");
        const auto plain = config.derive_color_space_info("Plain");
        for (auto field : { ColorSpaceInfoField::ColorInteropID,
                            ColorSpaceInfoField::ImageState }) {
            OIIO_CHECK_FALSE(ColorSpaceInfoAccess::computed(cheap, field));
            OIIO_CHECK_ASSERT(ColorSpaceInfoAccess::computed(plain, field));
            OIIO_CHECK_FALSE(ColorSpaceInfoAccess::available(plain, field));
        }
        auto data = config.derive_color_space_info("data_alias");
        OIIO_CHECK_ASSERT(data.valid());
        OIIO_CHECK_EQUAL(data.transfer_function_gamma(), 0.0f);
        OIIO_CHECK_ASSERT(data.chromaticities().empty());
        for (auto field : { ColorSpaceInfoField::Chromaticities,
                            ColorSpaceInfoField::TransferFunction }) {
            OIIO_CHECK_ASSERT(ColorSpaceInfoAccess::computed(data, field));
            OIIO_CHECK_FALSE(ColorSpaceInfoAccess::available(data, field));
            OIIO_CHECK_FALSE(ColorSpaceInfoAccess::derived(data, field));
        }
        OIIO_CHECK_FALSE(config.get_color_space_info("missing").valid());
        OIIO_CHECK_ASSERT(config.reset("ocio://default"));
        OIIO_CHECK_EQUAL(saved.transfer_function_gamma(), 1.0f);
    }
    auto copy  = saved;
    auto moved = std::move(saved);
    OIIO_CHECK_FALSE(saved.valid());
    OIIO_CHECK_EQUAL(copy.transfer_function_gamma(), 1.0f);
    OIIO_CHECK_EQUAL(moved.transfer_function_gamma(), 1.0f);
    invalid = std::move(moved);
    OIIO_CHECK_FALSE(moved.valid());
    OIIO_CHECK_EQUAL(invalid.transfer_function_gamma(), 1.0f);
    Filesystem::remove(filename);

    // Writers report gamma 1.0 for any linear transfer function.
    for (const char* name :
         { "lin_rec709_scene", "scene_linear", "lin_ap1_scene",
           "lin_p3d65_scene", "lin_rec2020_scene" }) {
        ImageSpec spec;
        spec.attribute("oiio:ColorSpace", name);
        OIIO_CHECK_EQUAL(pvt::get_colorspace_rec709_gamma(spec), 1.0f);
    }
    ImageSpec srgb;
    srgb.attribute("oiio:ColorSpace", "srgb_rec709_scene");
    OIIO_CHECK_EQUAL(pvt::get_colorspace_rec709_gamma(srgb), 0.0f);

    // A name or ID that spells its Rec.709 gamma reports that gamma without
    // any further measurement, and with no oiio:Gamma attribute to fall back
    // on. This is what keeps a writer's tag when nothing else supplies one.
    for (auto&& [name, gamma] : { std::pair { "g18_rec709_scene", 1.8f },
                                  std::pair { "g22_rec709_display", 2.2f } }) {
        ImageSpec spelled;
        spelled.attribute("oiio:ColorSpace", name);
        OIIO_CHECK_EQUAL(pvt::get_colorspace_rec709_gamma(spelled), gamma);
    }
}



static void
test_color_space_info_concurrent()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    // A separate config keeps these first derivations cold, independently of
    // the other tests. An arbitrary studio name requires analytic recognition.
    // The unrelated missing LUT must not prevent sharing this file-free result.
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        filename,
        "ocio_profile_version: 2.3\n"
        "roles: {default: Unknown, aces_interchange: Unknown, "
        "cie_xyz_d65_interchange: XYZ}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n  - !<ColorSpace>\n    name: Unknown\n"
        "display_colorspaces:\n  - !<ColorSpace>\n    name: XYZ\n"
        "    encoding: display-linear\n"
        "  - !<ColorSpace>\n    name: StudioAdobe\n"
        "    encoding: sdr-video\n"
        "    from_display_reference: !<GroupTransform>\n"
        "      children:\n"
        "        - !<MatrixTransform> {matrix: [2.04158790381075, "
        "-0.56500697427886, -0.34473135077833, 0, -0.96924363628088, "
        "1.87596750150772, 0.0415550574071756, 0, 0.0134442806320311, "
        "-0.118362392231018, 1.01517499439121, 0, 0, 0, 0, 1]}\n"
        "        - !<ExponentTransform> {value: 2.19921875, style: mirror, "
        "direction: inverse}\n"
        "named_transforms:\n  - !<NamedTransform>\n    name: Unrelated\n"
        "    transform: !<FileTransform> {src: absent.cube}\n"));

    constexpr int nthreads = 8;
    std::array<ColorSpaceInfo, nthreads> positives, misses;
    {
        std::array<std::unique_ptr<ColorConfig>, nthreads> configs;
        std::atomic<int> ready { 0 };
        std::atomic<int> loaded { 0 };
        std::vector<std::thread> threads;
        for (int i = 0; i < nthreads; ++i) {
            threads.emplace_back([&, i] {
                ++ready;
                while (ready.load() != nthreads)
                    std::this_thread::yield();
                // Also race construction of the configs.
                configs[i].reset(new ColorConfig(filename));
                ++loaded;
                while (loaded.load() != nthreads)
                    std::this_thread::yield();
                // Opposite orders exercise concurrent publication of both keys.
                if (i % 2)
                    misses[i] = configs[i]->derive_color_space_info("XYZ");
                positives[i] = configs[i]->derive_color_space_info(
                    "StudioAdobe");
                if (!(i % 2))
                    misses[i] = configs[i]->derive_color_space_info("XYZ");
            });
        }
        for (auto& thread : threads)
            thread.join();
        for (const auto& config : configs)
            OIIO_CHECK_FALSE(config->has_error());
    }

    // All originating wrappers are gone; both kinds of snapshot must survive.
    const float expected[] = { .64f, .33f, .21f,   .71f,
                               .15f, .06f, .3127f, .3290f };
    auto check_positive    = [&](const ColorSpaceInfo& info) {
        OIIO_CHECK_ASSERT(info.valid());
        OIIO_CHECK_EQUAL(info.transfer_function_gamma(), 2.19921875f);
        auto xy = info.chromaticities();
        OIIO_CHECK_EQUAL(xy.size(), 8);
        if (xy.size() == 8)
            for (int j = 0; j < 8; ++j)
                OIIO_CHECK_EQUAL_THRESH(xy[j], expected[j], 1e-6f);
    };
    auto check_miss = [&](const ColorSpaceInfo& info) {
        OIIO_CHECK_ASSERT(info.valid());
        // Display-reference XYZ has a linear transfer but no RGB primaries.
        OIIO_CHECK_EQUAL(info.transfer_function_gamma(), 1.0f);
        OIIO_CHECK_ASSERT(info.chromaticities().empty());
    };
    for (int i = 0; i < nthreads; ++i) {
        check_positive(positives[i]);
        check_miss(misses[i]);
    }
    // A new wrapper's cheap query sees the process-shared derived properties.
    ColorConfig another(filename);
    OIIO_CHECK_FALSE(another.has_error());
    check_positive(another.get_color_space_info("StudioAdobe"));
    check_miss(another.derive_color_space_info("XYZ"));
    Filesystem::remove(filename);
}



static void
test_numeric_recognition()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    // Same authored Adobe RGB matrix used by the concurrent snapshot test.
    // Transfers vary independently; none of these studio names supplies an ID.
    const std::string matrix
        = "!<MatrixTransform> {matrix: [2.04158790381075, -0.56500697427886, "
          "-0.34473135077833, 0, -0.96924363628088, 1.87596750150772, "
          "0.0415550574071756, 0, 0.0134442806320311, -0.118362392231018, "
          "1.01517499439121, 0, 0, 0, 0, 1]}";
    std::string text
        = "ocio_profile_version: 2.3\n"
          "roles: {default: Scene, aces_interchange: Scene, "
          "cie_xyz_d65_interchange: XYZ}\n"
          "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
          "colorspaces:\n  - !<ColorSpace> {name: Scene}\n"
          "display_colorspaces:\n  - !<ColorSpace> {name: XYZ}\n";
    auto power = [](string_view value) {
        return std::string("!<ExponentTransform> {value: ") + std::string(value)
               + ", style: mirror, direction: inverse}";
    };
    auto add = [&](string_view name, const std::string& transfer,
                   const std::string& extra = "", bool nonreciprocal = false) {
        text += "  - !<ColorSpace>\n    name: " + std::string(name)
                + "\n    from_display_reference: !<GroupTransform>\n"
                  "      children:\n        - "
                + matrix + "\n";
        if (!extra.empty())
            text += "        - " + extra + "\n";
        if (!transfer.empty())
            text += "        - " + transfer + "\n";
        if (nonreciprocal)
            text += "    to_display_reference: !<MatrixTransform> {}\n";
    };
    add("CustomPower", power("[2.35, 2.35, 2.35, 1]"));
    add("NominalAdobe", power("[2.2, 2.2, 2.2, 1]"));
    add("ClampPower", "!<ExponentTransform> {value: 2.35, direction: inverse}");
    add("ExactAdobe", power("[2.19921875, 2.19921875, 2.19921875, 1]"));
    add("LinearAdobe", "");
    add("ToeAdobe",
        "!<ExponentWithLinearTransform> {gamma: [2.4, 2.4, 2.4, 1], "
        "offset: [0.055, 0.055, 0.055, 0], direction: inverse}");
    add("LogAdobe", "!<LogTransform> {base: 2}");
    add("LogAffineAdobe",
        "!<LogAffineTransform> {base: 2, log_side_slope: 0.5, "
        "log_side_offset: 0.1, lin_side_slope: 1, "
        "lin_side_offset: 0.01}");
    add("LogCameraAdobe",
        "!<LogCameraTransform> {base: 2, lin_side_break: 0.01}");
    add("UnequalLog", "!<LogAffineTransform> {base: 2, "
                      "log_side_slope: [0.5, 0.6, 0.5]}");
    // A linear RGB mixing matrix preserves unit white but changes the gamut.
    add("NovelGamut", power("[2.35, 2.35, 2.35, 1]"),
        "!<MatrixTransform> {matrix: [0.9, 0.1, 0, 0, 0, 1, 0, 0, "
        "0, 0, 1, 0, 0, 0, 0, 1]}");
    add("Gain", power("[2.35, 2.35, 2.35, 1]"),
        "!<MatrixTransform> {matrix: [2, 0, 0, 0, 0, 2, 0, 0, "
        "0, 0, 2, 0, 0, 0, 0, 1]}");
    add("ChangedWhite", power("[2.35, 2.35, 2.35, 1]"),
        "!<MatrixTransform> {matrix: [1.1, 0, 0, 0, 0, 1, 0, 0, "
        "0, 0, 0.9, 0, 0, 0, 0, 1]}");
    add("Offset", power("[2.35, 2.35, 2.35, 1]"),
        "!<MatrixTransform> {offset: [0.1, 0, 0, 0]}");
    add("UnequalPower", power("[2.35, 2.2, 2.35, 1]"));
    add("ChangedAlpha", power("[2.35, 2.35, 2.35, 2]"));
    // Forward, this is the reference's Adobe RGB exactly, so every comparison
    // that reads one direction reproduces it. Its independently authored
    // reverse says the space is the reference itself. The two cannot both be
    // true, so no encoding is named and neither the exponent nor the primaries
    // may be read off either direction.
    add("Nonreciprocal", power("[2.19921875, 2.19921875, 2.19921875, 1]"), "",
        true);
    // Curve alone, no gamut matrix: what the probe reads along the neutral
    // axis is the curve itself, so a published family either names it or none
    // does. The first is the sRGB curve exactly; the second is a toe nothing
    // publishes. A name is not evidence, so the third is spelled sRGB and
    // implements a 2.35 power. The fourth has no color to measure.
    auto plain = [&](string_view name, const std::string& transfer) {
        text += "  - !<ColorSpace>\n    name: " + std::string(name)
                + "\n    from_display_reference: " + transfer + "\n";
    };
    plain("PlainSRGB",
          "!<ExponentWithLinearTransform> {gamma: [2.4, 2.4, 2.4, 1], "
          "offset: [0.055, 0.055, 0.055, 0], direction: inverse}");
    plain("PlainOddToe", "!<ExponentWithLinearTransform> {gamma: [3, 3, 3, 1], "
                         "offset: [0.2, 0.2, 0.2, 0], direction: inverse}");
    add("MislabeledSRGB", power("[2.35, 2.35, 2.35, 1]"));
    text = Strutil::replace(text, "    name: MislabeledSRGB\n",
                            "    name: MislabeledSRGB\n"
                            "    aliases: [srgb_rec709_display]\n");
    text += "  - !<ColorSpace>\n    name: Bypass\n    isdata: true\n";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(filename, text));
    const float expected_xy[] = { .64f, .33f, .21f,   .71f,
                                  .15f, .06f, .3127f, .329f };
    auto check = [&](const ColorSpaceInfo& info, float gamma, bool has_xy) {
        OIIO_CHECK_ASSERT(info.valid());
        OIIO_CHECK_EQUAL(info.transfer_function_gamma(), gamma);
        auto xy = info.chromaticities();
        OIIO_CHECK_EQUAL(xy.size(), has_xy ? 8 : 0);
        if (has_xy && xy.size() == 8)
            for (int i = 0; i < 8; ++i)
                OIIO_CHECK_EQUAL_THRESH(xy[i], expected_xy[i], 1e-6f);
    };
    ColorSpaceInfo cold, saved, gamut_only;
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        cold = config.get_color_space_info("CustomPower");
        check(cold, 0.0f, false);
        OIIO_CHECK_FALSE(ColorSpaceInfoAccess::computed(
            cold, ColorSpaceInfoField::Chromaticities));
        OIIO_CHECK_FALSE(ColorSpaceInfoAccess::computed(
            cold, ColorSpaceInfoField::TransferFunction));
        saved = config.derive_color_space_info("CustomPower");
        check(saved, 2.35f, true);
        for (auto field : { ColorSpaceInfoField::Chromaticities,
                            ColorSpaceInfoField::TransferFunction }) {
            OIIO_CHECK_ASSERT(ColorSpaceInfoAccess::computed(saved, field));
            OIIO_CHECK_ASSERT(ColorSpaceInfoAccess::available(saved, field));
            OIIO_CHECK_ASSERT(ColorSpaceInfoAccess::derived(saved, field));
            OIIO_CHECK_FALSE(ColorSpaceInfoAccess::computed(cold, field));
        }
        check(config.get_color_space_info("CustomPower"), 2.35f, true);
        check(config.derive_color_space_info("NominalAdobe"), 2.2f, true);
        check(config.derive_color_space_info("ClampPower"), 2.35f, true);
        check(config.derive_color_space_info("ExactAdobe"), 2.19921875f, true);
        OIIO_CHECK_ASSERT(config.get_color_interop_id("NominalAdobe")
                          != "g22_adobergb_display");
        OIIO_CHECK_ASSERT(config.get_color_interop_id("NominalAdobe")
                          != "g22_adobergb_scene");
        check(config.derive_color_space_info("LinearAdobe"), 1.0f, true);
        gamut_only = config.derive_color_space_info("ToeAdobe");
        check(gamut_only, 0.0f, true);
        for (const char* name :
             { "LogAdobe", "LogAffineAdobe", "LogCameraAdobe" })
            check(config.derive_color_space_info(name), 0.0f, true);
        check(config.derive_color_space_info("NovelGamut"), 2.35f, false);
        const ColorSpaceInfo completed_negative
            = config.derive_color_space_info("NovelGamut");
        OIIO_CHECK_ASSERT(ColorSpaceInfoAccess::computed(
            completed_negative, ColorSpaceInfoField::Chromaticities));
        OIIO_CHECK_FALSE(ColorSpaceInfoAccess::available(
            completed_negative, ColorSpaceInfoField::Chromaticities));
        OIIO_CHECK_ASSERT(
            ColorSpaceInfoAccess::derived(completed_negative,
                                          ColorSpaceInfoField::Chromaticities));
        // A uniform gain is a statement about luminance, not about a gamut:
        // every chromaticity is a ratio and none of them moves, so the
        // primaries are still established. An unequal one moves the white and
        // a mixing matrix moves the primaries, and neither is reported.
        check(config.derive_color_space_info("Gain"), 2.35f, true);
        check(config.derive_color_space_info("ChangedWhite"), 2.35f, false);
        for (const char* name : { "Offset", "UnequalPower", "ChangedAlpha",
                                  "Nonreciprocal", "UnequalLog" })
            check(config.derive_color_space_info(name), 0.0f, false);
        OIIO_CHECK_ASSERT(config.get_color_interop_id("Nonreciprocal").empty());
        // Two authored directions that cannot both be true rule out every
        // reference encoding at once, so the measured query has nothing to
        // report either. Empty is "nothing reproduced this", not "unique".
        OIIO_CHECK_ASSERT(equality_id(config, "Nonreciprocal").empty());

        // What kind of curve was established, which is a separate question
        // from the exponent and is answered from measurement alone.
        using Kind = ColorTransferFunctionKind;
        static_assert(int(Kind::Transform) == int(Kind::Named) + 1);
        static_assert(int(Kind::Unrecognized) == int(Kind::Transform) + 1);
        auto kind = [](const ColorSpaceInfo& info) {
            return int(ColorSpaceInfoAccess::transfer_function_kind(info));
        };
        // The neutral-axis measurement is valid even though the three channel
        // exponents differ. Exact extraction is what then establishes that no
        // one shared RGB transfer function represents the definition.
        OIIO_CHECK_EQUAL(kind(config.derive_color_space_info("UnequalPower")),
                         int(Kind::Unrecognized));
        // The cheap query begins no measurement, so it has no family to
        // report until an explicit derivation has established one.
        const ColorSpaceInfo cold_curve = config.get_color_space_info(
            "PlainSRGB");
        OIIO_CHECK_EQUAL(kind(cold_curve), int(Kind::Undetermined));
        OIIO_CHECK_ASSERT(
            ColorSpaceInfoAccess::transfer_function_name(cold_curve).empty());
        // A piecewise sRGB curve is named by the family it measures as, and
        // no exponent is invented to describe it.
        const ColorSpaceInfo curve = config.derive_color_space_info(
            "PlainSRGB");
        OIIO_CHECK_EQUAL(kind(curve), int(Kind::Named));
        OIIO_CHECK_EQUAL(ColorSpaceInfoAccess::transfer_function_name(curve),
                         "srgb");
        OIIO_CHECK_EQUAL(curve.transfer_function_gamma(), 0.0f);
        // The snapshot handed out before is not strengthened behind the
        // caller's back; the cheap query afterwards reports what was retained.
        OIIO_CHECK_EQUAL(kind(cold_curve), int(Kind::Undetermined));
        OIIO_CHECK_EQUAL(ColorSpaceInfoAccess::transfer_function_name(
                             config.get_color_space_info("PlainSRGB")),
                         "srgb");
        // An established exponent answers first, and a name is never evidence
        // about a curve: this space is spelled sRGB and implements a power.
        const ColorSpaceInfo mislabeled = config.derive_color_space_info(
            "MislabeledSRGB");
        OIIO_CHECK_EQUAL(kind(mislabeled), int(Kind::Power));
        OIIO_CHECK_EQUAL(mislabeled.transfer_function_gamma(), 2.35f);
        OIIO_CHECK_ASSERT(
            ColorSpaceInfoAccess::transfer_function_name(mislabeled).empty());
        OIIO_CHECK_EQUAL(kind(config.derive_color_space_info("CustomPower")),
                         int(Kind::Power));
        OIIO_CHECK_EQUAL(kind(config.derive_color_space_info("LinearAdobe")),
                         int(Kind::Linear));
        // A measurement no published family describes is still an exact
        // native transfer when its operations separate into one RGB curve.
        const ColorSpaceInfo odd = config.derive_color_space_info(
            "PlainOddToe");
        OIIO_CHECK_EQUAL(kind(odd), int(Kind::Transform));
        OIIO_CHECK_ASSERT(
            ColorSpaceInfoAccess::transfer_function_name(odd).empty());
        // A log curve is never approximated by a power. Whether a published
        // family names one of these depends on the reference vocabulary; that
        // none of them is linear or a power does not.
        for (const char* name :
             { "LogAdobe", "LogAffineAdobe", "LogCameraAdobe" }) {
            const ColorSpaceInfo log = config.derive_color_space_info(name);
            OIIO_CHECK_ASSERT(kind(log) == int(Kind::Named)
                              || kind(log) == int(Kind::Transform));
            OIIO_CHECK_EQUAL(log.transfer_function_gamma(), 0.0f);
        }
        // Nothing to measure, and nothing to resolve.
        OIIO_CHECK_EQUAL(kind(config.derive_color_space_info("Bypass")),
                         int(Kind::Undetermined));
        OIIO_CHECK_EQUAL(kind(config.derive_color_space_info("NotASpace")),
                         int(Kind::Undetermined));
    }
    // Old empty and derived handles survive publication and wrapper lifetime.
    check(cold, 0.0f, false);
    check(saved, 2.35f, true);
    check(gamut_only, 0.0f, true);
    ColorConfig another(filename);
    OIIO_CHECK_FALSE(another.has_error());
    check(another.get_color_space_info("CustomPower"), 2.35f, true);
    check(another.get_color_space_info("ToeAdobe"), 0.0f, true);
    // A second wrapper's cheap query sees the family the first established.
    OIIO_CHECK_EQUAL(ColorSpaceInfoAccess::transfer_function_name(
                         another.get_color_space_info("PlainSRGB")),
                     "srgb");
    check(another.derive_color_space_info("NovelGamut"), 2.35f, false);
    Filesystem::remove(filename);
}



// Scene-referred gamut derivation and log recognition. Every matrix and every
// curve parameter below is copied verbatim from the built-in interop-identities
// config, so a difference in a result is a difference in recognition rather
// than in the fixture. That config reaches each of these gamuts by one
// chromatic adaptation; the differently adapted case, which needs primaries
// re-adapted numerically, lives in the Python properties test.
static void
test_gamut_recognition()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    const std::string bmdwg5
        = "!<MatrixTransform> {matrix: [0.647091325580708, "
          "0.242595385134207, 0.110313289285085, 0, 0.0651915997328519, "
          "1.02504756760476, -0.0902391673376125, 0, -0.0275570729194699, "
          "-0.0805887097177784, 1.10814578263725, 0, 0, 0, 0, 1]}";
    const std::string rec2020
        = "!<MatrixTransform> {matrix: [0.679085634706912, "
          "0.157700914643159, 0.163213450649929, 0, 0.0460020030800595, "
          "0.859054673002908, 0.0949433239170327, 0, -0.000573943187616196, "
          "0.0284677684080264, 0.97210617477959, 0, 0, 0, 0, 1]}";
    const std::string awg3
        = "!<MatrixTransform> {matrix: [0.680205505106279, "
          "0.236136601606481, 0.0836578932872398, 0, 0.0854149797421404, "
          "1.01747087860704, -0.102885858349182, 0, 0.00205652166929683, "
          "-0.0625625003847921, 1.06050597871549, 0, 0, 0, 0, 1]}";
    const std::string bmdfilm5
        = "!<LogCameraTransform> {base: 2.71828182845905, log_side_slope: "
          "0.0869287606549122, log_side_offset: 0.530013339229194, "
          "lin_side_offset: 0.00549407243225781, lin_side_break: 0.005, "
          "direction: inverse}";
    const std::string davinci
        = "!<LogCameraTransform> {log_side_slope: 0.07329248, log_side_offset: "
          "0.51304736, lin_side_offset: 0.0075, lin_side_break: 0.00262409, "
          "linear_slope: 10.44426855, direction: inverse}";
    const std::string logc3
        = "!<LogCameraTransform> {base: 10, log_side_slope: "
          "0.247189638318671, log_side_offset: 0.385536998692443, "
          "lin_side_slope: 5.55555555555556, lin_side_offset: "
          "0.0522722750251688, lin_side_break: 0.0105909904954696, "
          "direction: inverse}";
    std::string text
        = "ocio_profile_version: 2.3\n"
          "roles: {default: ACES, aces_interchange: ACES}\n"
          "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
          "colorspaces:\n  - !<ColorSpace>\n    name: ACES\n"
          "    encoding: scene-linear\n";
    auto add = [&](string_view name, const std::string& curve,
                   const std::string& matrix) {
        text += "  - !<ColorSpace>\n    name: " + std::string(name)
                + "\n    encoding: " + (curve.empty() ? "scene-linear" : "log")
                + "\n";
        if (curve.empty()) {
            text += "    to_scene_reference: " + matrix + "\n";
            return;
        }
        // Decode order: the curve linearizes, then the matrix reaches AP0.
        text += "    to_scene_reference: !<GroupTransform>\n"
                "      children:\n        - "
                + curve + "\n        - " + matrix + "\n";
    };
    // The reference's own curve with its linear segment removed. Every shared
    // parameter still agrees; the two disagree only below the break.
    const std::string bmdfilm5_no_break
        = "!<LogAffineTransform> {base: 2.71828182845905, log_side_slope: "
          "0.0869287606549122, log_side_offset: 0.530013339229194, "
          "lin_side_offset: 0.00549407243225781, direction: inverse}";
    add("CameraLinear", "", bmdwg5);
    add("AP1Linear", "", "!<BuiltinTransform> {style: ACEScg_to_ACES2065-1}");
    add("CameraLog", bmdfilm5, bmdwg5);
    add("WrongGamutSameCurve", bmdfilm5, rec2020);
    add("WrongCurveSameGamut", davinci, bmdwg5);
    add("NoLinearSegment", bmdfilm5_no_break, bmdwg5);
    add("UnpublishedGamut", logc3, awg3);
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(filename, text));

    const float bmdwg5_xy[]  = { .7177215f, .3171181f,  .228041f, .861569f,
                                 .1005841f, -.0820452f, .312717f, .3290312f };
    const float rec2020_xy[] = { .708f, .292f, .170f,  .797f,
                                 .131f, .046f, .3127f, .3290f };
    const float ap1_xy[]     = { .713f, .293f, .165f,   .830f,
                                 .128f, .044f, .32168f, .33767f };
    auto check_xy = [&](const ColorSpaceInfo& info, cspan<float> expected) {
        auto xy = info.chromaticities();
        OIIO_CHECK_EQUAL(xy.size(), expected.size());
        if (xy.size() == expected.size())
            for (size_t i = 0; i < expected.size(); ++i)
                OIIO_CHECK_EQUAL_THRESH(xy[i], expected[i], 1e-6f);
    };

    ColorConfig config(filename);
    OIIO_CHECK_FALSE(config.has_error());

    // A camera gamut with no reference transfer counterpart still reports its
    // published primaries, which the composed-matrix comparison alone cannot.
    auto camera_linear = config.derive_color_space_info("CameraLinear");
    OIIO_CHECK_ASSERT(camera_linear.valid());
    OIIO_CHECK_EQUAL(camera_linear.transfer_function_gamma(), 1.0f);
    check_xy(camera_linear, bmdwg5_xy);
    auto ap1 = config.derive_color_space_info("AP1Linear");
    OIIO_CHECK_EQUAL(ap1.transfer_function_gamma(), 1.0f);
    check_xy(ap1, ap1_xy);

    // Curve and gamut together identify the encoding, and the gamut remains a
    // usable partial fact whichever half fails.
    auto camera_log = config.derive_color_space_info("CameraLog");
    OIIO_CHECK_EQUAL(camera_log.transfer_function_gamma(), 0.0f);
    check_xy(camera_log, bmdwg5_xy);
    OIIO_CHECK_EQUAL(config.get_color_interop_id("CameraLog"),
                     "ocio:bmdfilm5_wg5_scene");

    // A matching curve on different primaries is not the encoding.
    auto wrong_gamut = config.derive_color_space_info("WrongGamutSameCurve");
    check_xy(wrong_gamut, rec2020_xy);
    OIIO_CHECK_EQUAL(config.get_color_interop_id("WrongGamutSameCurve"), "");

    // Matching primaries under a different curve are not the encoding either.
    auto wrong_curve = config.derive_color_space_info("WrongCurveSameGamut");
    check_xy(wrong_curve, bmdwg5_xy);
    OIIO_CHECK_EQUAL(config.get_color_interop_id("WrongCurveSameGamut"), "");

    // Agreement on every shared parameter is not agreement on the curve when
    // the behavior below the break differs. Numeric equivalence over the
    // nominal range does not establish equivalence outside it.
    auto no_break = config.derive_color_space_info("NoLinearSegment");
    check_xy(no_break, bmdwg5_xy);
    OIIO_CHECK_EQUAL(config.get_color_interop_id("NoLinearSegment"), "");

    // A gamut the published library does not hold is reconstructed from the
    // composed matrix rather than going unreported. The matrix reaches ACES
    // AP0, whose white and primaries are known, so un-adapting it recovers
    // the primaries the definition was built from -- here ARRI Wide Gamut 3,
    // to the digits ARRI publishes. The reconstruction is a reading of the
    // matrix, not of the name: nothing below consults either.
    const float awg3_xy[] = { .684f,  .313f,  .221f,  .848f,
                              .0861f, -.102f, .3127f, .3290f };
    auto unpublished      = config.derive_color_space_info("UnpublishedGamut");
    OIIO_CHECK_ASSERT(unpublished.valid());
    check_xy(unpublished, awg3_xy);
    Filesystem::remove(filename);
}



// What a color space is named and what it does are separate questions, and
// production configurations routinely disagree about them: a space called
// "sRGB" that is a pure 2.2 power, a space aliased for a display encoding that
// the configuration authors scene-referred, a state-less alias on a space the
// configuration places in its display section. Every fixture below is a
// reduction of one of those, and every one of them is a case where reading the
// name alone produces an identity the definition does not implement.
//
// The fixtures also differ from the built-in config in how their transfer
// functions treat negative input -- they clamp, where that config mirrors or
// passes through -- which is the ordinary case and which the operation-level
// comparison cannot see past. Recognition here comes from the measured
// comparison, whose probes stay inside the encoded gamut for that reason.
static void
test_naming_versus_measurement()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    // Matrices copied verbatim from the built-in config, so a
    // difference in a result is a difference in recognition, not in a fixture.
    const std::string rec709_to_ap0
        = "!<MatrixTransform> {matrix: [0.439632981919491, "
          "0.382988698151554, 0.177378319928955, 0, 0.0897764429588424, "
          "0.813439428748981, 0.0967841282921771, 0, 0.0175411703831727, "
          "0.111546553302387, 0.87091227631444, 0, 0, 0, 0, 1]}";
    const std::string p3d65_to_ap0
        = "!<MatrixTransform> {matrix: [0.518933487597981, 0.28625658638669, "
          "0.194809926015329, 0, 0.0738593830470598, 0.819845163936986, "
          "0.106295453015954, 0, -0.000307011368446647, 0.0438070502536223, "
          "0.956499961114824, 0, 0, 0, 0, 1]}";
    const std::string xyz_to_rec709
        = "!<MatrixTransform> {matrix: [3.24096994190452, -1.53738317757009, "
          "-0.498610760293003, 0, -0.96924363628088, 1.87596750150772, "
          "0.0415550574071756, 0, 0.0556300796969936, -0.203976958888976, "
          "1.05697151424288, 0, 0, 0, 0, 1]}";
    const std::string xyz_to_p3d65
        = "!<MatrixTransform> {matrix: [2.49349691194143, -0.931383617919124, "
          "-0.402710784450717, 0, -0.829488969561575, 1.76266406031835, "
          "0.0236246858419436, 0, 0.0358458302437845, -0.0761723892680418, "
          "0.956884524007688, 0, 0, 0, 0, 1]}";
    const std::string xyz_to_rec2020
        = "!<MatrixTransform> {matrix: [1.71665118797127, -0.355670783776392, "
          "-0.25336628137366, 0, -0.666684351832489, 1.61648123663494, "
          "0.0157685458139111, 0, 0.0176398574453108, -0.0427706132578085, "
          "0.942103121235474, 0, 0, 0, 0, 1]}";
    const std::string xyz_to_p3dci
        = "!<MatrixTransform> {matrix: [2.690225911625597, "
          "-1.094001937366136, -0.4250823476747523, 0, -0.820082184273491, "
          "1.750480908292057, 0.02660195421220572, 0, 0.03624575465400463, "
          "-0.07858083680558862, 0.9587469936609856, 0, 0, 0, 0, 1]}";

    std::string text
        = "ocio_profile_version: 2.3\n"
          "roles: {default: ACES, aces_interchange: ACES, "
          "cie_xyz_d65_interchange: XYZ}\n"
          "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
          "colorspaces:\n"
          "  - !<ColorSpace>\n    name: ACES\n    encoding: scene-linear\n";
    // Decode order for a scene space: the curve linearizes, the matrix reaches
    // AP0. Encode order for a display space: the matrix leaves CIE XYZ, the
    // curve encodes.
    auto scene = [&](string_view name, string_view alias, string_view curve,
                     const std::string& matrix) {
        text += "  - !<ColorSpace>\n    name: " + std::string(name) + "\n";
        if (!alias.empty())
            text += "    aliases: [" + std::string(alias) + "]\n";
        text += "    encoding: sdr-video\n"
                "    to_scene_reference: !<GroupTransform>\n"
                "      children:\n        - "
                + std::string(curve) + "\n        - " + matrix + "\n";
    };
    scene("Pure22", "srgb_texture", "!<ExponentTransform> {value: 2.2}",
          rec709_to_ap0);
    scene("Rec1886Scene", "rec1886_rec709_display",
          "!<ExponentTransform> {value: 2.4}", rec709_to_ap0);
    scene("WideGamutPure22", "p3d65_display",
          "!<ExponentTransform> {value: 2.2}", p3d65_to_ap0);
    // The same 2.2 power, declaring itself linear. The declared transfer is a
    // label like any other, and the identity query refuses to publish an
    // encoding that contradicts it rather than publishing the contradiction.
    text += "  - !<ColorSpace>\n    name: FalseLinear\n"
            "    encoding: scene-linear\n"
            "    to_scene_reference: !<GroupTransform>\n      children:\n"
            "        - !<ExponentTransform> {value: 2.2}\n        - "
            + rec709_to_ap0 + "\n";
    // A declared linear encoding whose definition is a curve no exponent
    // describes. Nothing measured can replace the declaration here, so the
    // honored 1.0 is the only answer there is.
    text += "  - !<ColorSpace>\n    name: FalseLinearSRGB\n"
            "    encoding: scene-linear\n"
            "    to_scene_reference: !<GroupTransform>\n      children:\n"
            "        - !<ExponentWithLinearTransform> {gamma: 2.4, "
            "offset: 0.055, direction: inverse}\n        - "
            + rec709_to_ap0 + "\n";

    text += "display_colorspaces:\n"
            "  - !<ColorSpace>\n    name: XYZ\n"
            "    aliases: [cie_xyz_d65]\n    encoding: display-linear\n";
    auto display = [&](string_view name, string_view alias,
                       string_view encoding, const std::string& matrix,
                       string_view curve) {
        text += "  - !<ColorSpace>\n    name: " + std::string(name) + "\n";
        if (!alias.empty())
            text += "    aliases: [" + std::string(alias) + "]\n";
        text += "    encoding: " + std::string(encoding)
                + "\n    from_display_reference: !<GroupTransform>\n"
                  "      children:\n        - "
                + matrix + "\n        - " + std::string(curve) + "\n";
    };
    display("AppleDisplay", "srgb_p3d65", "sdr-video", xyz_to_p3d65,
            "!<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055, "
            "direction: inverse}");
    display("Gamma22Rec709", "", "sdr-video", xyz_to_rec709,
            "!<ExponentTransform> {value: 2.2, direction: inverse}");
    display("Rec1886Rec2020", "", "sdr-video", xyz_to_rec2020,
            "!<ExponentTransform> {value: 2.4, direction: inverse}");
    display("CinemaP3", "", "sdr-cinema", xyz_to_p3dci,
            "!<ExponentTransform> {value: 2.6, style: mirror, "
            "direction: inverse}");
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(filename, text));

    ColorConfig config(filename);
    OIIO_CHECK_FALSE(config.has_error());

    // A pure 2.2 power aliased for the sRGB texture encoding is a 2.2 power.
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Pure22"), "g22_rec709_scene");
    // A scene-referred definition aliased for a display encoding keeps the
    // image state its configuration authored. Its author's alias does not
    // move it, and nothing infers the state from the alias.
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Rec1886Scene"),
                     "g24_rec709_scene");
    // A state-less alias on a display-referred definition reports the display
    // identity, and the same alias would report the scene one on a
    // scene-referred definition. The configuration decides, not the alias.
    OIIO_CHECK_EQUAL(config.get_color_interop_id("AppleDisplay"),
                     "srgb_p3d65_display");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("XYZ"),
                     "ocio:lin_ciexyzd65_display");
    // The alias names a 2.6 cinema encoding and the definition is a 2.2
    // power. Nothing measured supports the alias, so it is withdrawn rather
    // than published, and what was measured survives the withdrawal.
    OIIO_CHECK_EQUAL(config.get_color_interop_id("WideGamutPure22"), "");

    // Measured with the naming and the declared transfer both withheld. The
    // aliases above change nothing, because measurement already answered over
    // them. The declared linearity does change the identity query: it makes it
    // decline the encoding it measured, which is why no linearity hint may
    // seed a measured query.
    OIIO_CHECK_EQUAL(equality_id(config, "Pure22"), "g22_rec709_scene");
    OIIO_CHECK_EQUAL(equality_id(config, "Rec1886Scene"), "g24_rec709_scene");
    OIIO_CHECK_EQUAL(equality_id(config, "AppleDisplay"), "srgb_p3d65_display");
    OIIO_CHECK_EQUAL(equality_id(config, "WideGamutPure22"), "");
    OIIO_CHECK_EQUAL(equality_id(config, "FalseLinear"), "g22_rec709_scene");
    OIIO_CHECK_ASSERT(config.get_color_interop_id("FalseLinear")
                      != "g22_rec709_scene");
    // Its declared linear encoding does not stop derivation from reporting
    // the 2.2 power the definition measures as.
    auto false_linear = config.derive_color_space_info("FalseLinear");
    OIIO_CHECK_ASSERT(ColorSpaceInfoAccess::transfer_function_kind(false_linear)
                      == ColorTransferFunctionKind::Power);
    OIIO_CHECK_EQUAL_THRESH(false_linear.transfer_function_gamma(), 2.2f,
                            1.0e-6f);
    // An authored linear encoding is honored rather than measured: only a
    // measured pure-power exponent replaces it, and this definition has none.
    auto false_linear_srgb = config.derive_color_space_info("FalseLinearSRGB");
    OIIO_CHECK_EQUAL(false_linear_srgb.transfer_function_gamma(), 1.0f);
    OIIO_CHECK_ASSERT(
        ColorSpaceInfoAccess::transfer_function_kind(false_linear_srgb)
        == ColorTransferFunctionKind::Linear);

    auto wide = config.derive_color_space_info("WideGamutPure22");
    OIIO_CHECK_ASSERT(wide.valid());
    OIIO_CHECK_EQUAL(wide.transfer_function_gamma(), 2.2f);

    // Display encodings whose only difference from the built-in config is
    // negative handling, which the encoded gamut never reaches.
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Gamma22Rec709"),
                     "g22_rec709_display");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("Rec1886Rec2020"),
                     "oiio:g24_rec2020_display");

    // A cinema primary set whose white is not the interchange white. The
    // published library names it by constructing each hypothesis forward, so
    // it reports the DCI white rather than the adapted coordinates that
    // reading the matrix back would give.
    OIIO_CHECK_EQUAL(config.get_color_interop_id("CinemaP3"),
                     "oiio:g26_p3dci_display");
    auto cinema = config.derive_color_space_info("CinemaP3");
    OIIO_CHECK_EQUAL(cinema.transfer_function_gamma(), 2.6f);
    const float p3dci_xy[]
        = { .68f, .32f, .265f, .69f, .15f, .06f, .314f, .351f };
    auto cinema_xy = cinema.chromaticities();
    OIIO_CHECK_EQUAL(cinema_xy.size(), 8);
    if (cinema_xy.size() == 8)
        for (int i = 0; i < 8; ++i)
            OIIO_CHECK_EQUAL_THRESH(cinema_xy[i], p3dci_xy[i], 1e-6f);
    Filesystem::remove(filename);
}



static void
test_spi_conventions()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    const char* srgb_names[]   = { "srgbf", "srgbh", "srgb16", "srgb8" };
    const char* linear_names[] = { "srgblnf", "srgblnh", "srgbln16",
                                   "srgbln8" };
    auto fixture               = [&](bool authored) {
        std::string text
            = "ocio_profile_version: 2.3\n"
              "roles: {default: Reference, aces_interchange: Reference";
        if (authored)
            text += ", lin_rec709_scene: Reference";
        text += "}\nfile_rules:\n"
                "  - !<Rule> {name: Default, colorspace: default}\n"
                "colorspaces:\n"
                "  - !<ColorSpace>\n    name: Reference\n    isdata: false\n";
        if (authored)
            text += "    aliases: [lin_ap1_scene, srgb_texture, sRGB]\n";
        auto add = [&](const char* name, bool data = false) {
            text += Strutil::fmt::format(
                "  - !<ColorSpace>\n    name: {}\n    isdata: {}\n", name,
                data ? "true" : "false");
            if (!data)
                text
                    += "    to_scene_reference: !<ExponentTransform> {value: 2.2}\n";
            if (string_view(name) == "srgb8"
                && ColorConfig::OpenColorIO_version_hex() >= 0x02050000)
                text += "    interop_id: custom:spi\n";
        };
        add("cgln_a");
        add("cgln_b");
        add("cgln_data", true);
        add("nc_pixels");
        add("lnf");
        add("CGLN_case");
        for (auto name : srgb_names)
            add(name);
        for (auto name : linear_names)
            add(name);
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(filename, text));
    };
    fixture(false);
    ColorConfig config(filename);
    OIIO_CHECK_FALSE(config.has_error());
    OIIO_CHECK_ASSERT(config.isData("cgln_data"));
    OIIO_CHECK_EQUAL(config.get_color_interop_id("cgln_data"), "data");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("lnf"), "");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("CGLN_case"), "");
#ifdef OIIO_SITE_spi
    OIIO_CHECK_ASSERT(config.isData("nc_pixels"));
    OIIO_CHECK_EQUAL(config.get_color_interop_id("nc_pixels"), "data");
    OIIO_CHECK_EQUAL(config.resolve("data"), "data");
    OIIO_CHECK_EQUAL(config.resolve("linear"), "srgblnf");
    OIIO_CHECK_EQUAL(config.resolve("sRGB"), "srgbf");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("cgln_a"), "lin_ap1_scene");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("cgln_b"), "lin_ap1_scene");
    OIIO_CHECK_ASSERT(config.equivalent("cgln_a", "cgln_b"));
    OIIO_CHECK_EQUAL(config.resolve("lin_ap1_scene"), "cgln_a");
    OIIO_CHECK_EQUAL(config.resolve("srgb_rec709_scene"), "srgbf");
    OIIO_CHECK_EQUAL(config.resolve("lin_rec709_scene"), "srgblnf");
    OIIO_CHECK_ASSERT(config.equivalent("srgbf", "srgbh"));
    for (auto name : srgb_names) {
        const char* expected = string_view(name) == "srgb8"
                                       && ColorConfig::OpenColorIO_version_hex()
                                              >= 0x02050000
                                   ? "custom:spi"
                                   : "srgb_rec709_scene";
        OIIO_CHECK_EQUAL(config.get_color_interop_id(name), expected);
    }
    for (auto name : linear_names)
        OIIO_CHECK_EQUAL(config.get_color_interop_id(name), "lin_rec709_scene");
#else
    OIIO_CHECK_FALSE(config.isData("nc_pixels"));
    OIIO_CHECK_FALSE(config.equivalent("cgln_a", "cgln_b"));
    OIIO_CHECK_EQUAL(config.get_color_interop_id("cgln_a"), "");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("srgbf"), "");
    OIIO_CHECK_EQUAL(config.get_color_interop_id("srgblnf"), "");
#endif
    if (ColorConfig::OpenColorIO_version_hex() >= 0x02050000)
        OIIO_CHECK_EQUAL(config.get_color_interop_id("srgb8"), "custom:spi");
    fixture(true);
    ColorConfig authored(filename);
    OIIO_CHECK_FALSE(authored.has_error());
    OIIO_CHECK_EQUAL(authored.resolve("lin_ap1_scene"), "Reference");
    OIIO_CHECK_EQUAL(authored.resolve("lin_rec709_scene"), "Reference");
    OIIO_CHECK_EQUAL(authored.resolve("srgb_rec709_scene"), "Reference");
    OIIO_CHECK_EQUAL(authored.resolve("sRGB"), "Reference");
#ifdef OIIO_SITE_spi
    OIIO_CHECK_EQUAL(authored.get_color_interop_id("cgln_b"), "lin_ap1_scene");
    OIIO_CHECK_ASSERT(authored.equivalent("cgln_a", "cgln_b"));
#endif
    Filesystem::remove(filename);
}



static void
test_color_space_info_context()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;
    const std::string directory = Filesystem::temp_directory_path() + "/"
                                  + Filesystem::unique_path();
    OIIO_CHECK_ASSERT(Filesystem::create_directory(directory));
    auto matrix = [&](string_view shot, string_view values) {
        return Filesystem::write_text_file(
            Strutil::fmt::format("{}/gamut_{}.ctf", directory, shot),
            Strutil::fmt::format(
                "<ProcessList version=\"1.3\" id=\"gamut\">\n"
                "  <Matrix inBitDepth=\"32f\" outBitDepth=\"32f\">\n"
                "    <Array dim=\"3 3\">{}</Array>\n"
                "  </Matrix>\n</ProcessList>\n",
                values));
    };
    OIIO_CHECK_ASSERT(
        matrix("rec2020",
               "0.679085634706912 0.157700914643159 0.163213450649929 "
               "0.0460020030800595 0.859054673002908 0.0949433239170327 "
               "-0.000573943187616196 0.0284677684080264 0.97210617477959"));
    OIIO_CHECK_ASSERT(
        matrix("bmdwg5",
               "0.647091325580708 0.242595385134207 0.110313289285085 "
               "0.0651915997328519 1.02504756760476 -0.0902391673376125 "
               "-0.0275570729194699 -0.0805887097177784 1.10814578263725"));
    const std::string filename = directory + "/context.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        filename,
        "ocio_profile_version: 2.3\n"
        "environment: {SHOT: rec2020}\nsearch_path: .\n"
        "roles: {default: Reference, aces_interchange: Reference}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n  - !<ColorSpace> {name: Reference}\n"
        "  - !<ColorSpace>\n    name: Plate\n"
        "    to_scene_reference: !<FileTransform> {src: gamut_$SHOT.ctf}\n"));
    ColorConfig config(filename);
    OIIO_CHECK_FALSE(config.has_error());
    auto check = [](const ColorSpaceInfo& info, cspan<float> expected) {
        OIIO_CHECK_EQUAL(info.transfer_function_gamma(), 1.0f);
        const auto xy = info.chromaticities();
        OIIO_CHECK_EQUAL(xy.size(), expected.size());
        if (xy.size() == expected.size())
            for (size_t i = 0; i < expected.size(); ++i)
                OIIO_CHECK_EQUAL_THRESH(xy[i], expected[i], 1.0e-6f);
    };
    const float rec2020[] = { .708f, .292f, .170f,  .797f,
                              .131f, .046f, .3127f, .3290f };
    const float bmdwg5[]  = { .7177215f, .3171181f,  .228041f, .861569f,
                              .1005841f, -.0820452f, .312717f, .3290312f };
    OIIO_CHECK_ASSERT(
        pvt::color_space_info(config, "Plate", false, "SHOT", "rec2020")
            .chromaticities()
            .empty());
    check(pvt::color_space_info(config, "Plate", true, "SHOT", "rec2020"),
          rec2020);
    OIIO_CHECK_ASSERT(
        pvt::color_space_info(config, "Plate", false, "SHOT", "bmdwg5")
            .chromaticities()
            .empty());
    check(pvt::color_space_info(config, "Plate", true, "SHOT", "bmdwg5"),
          bmdwg5);
    check(pvt::color_space_info(config, "Plate", false, "SHOT", "rec2020"),
          rec2020);
    check(config.derive_color_space_info("Plate"), rec2020);
    // A variable the config never reads builds no view of its own, so the
    // cheap query answers what the default context derived, for any value.
    for (int i = 0; i < 100; ++i)
        check(pvt::color_space_info(config, "Plate", false, "UNUSED",
                                    Strutil::fmt::format("v{}", i)),
              rec2020);

    // String variables that name no file still select their own results.
    const std::string strings = directory + "/strings.ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        strings,
        "ocio_profile_version: 2.3\n"
        "environment: {SHOT: Hidden}\n"
        "roles: {default: ACES2065-1, scene_linear: ACES2065-1,\n"
        "  aces_interchange: ACES2065-1, cie_xyz_d65_interchange: XYZ}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n"
        "  - !<ColorSpace> {name: ACES2065-1, encoding: scene-linear}\n"
        "  - !<ColorSpace>\n    name: XYZ\n    from_scene_reference: "
        "!<BuiltinTransform> {style: UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD}\n"
        "  - !<ColorSpace>\n    name: acescg\n    from_scene_reference: "
        "!<BuiltinTransform> {style: ACEScg_to_ACES2065-1, direction: "
        "inverse}\n"
        "  - !<ColorSpace>\n    name: Plate\n    to_scene_reference: "
        "!<ColorSpaceTransform> {src: $SHOT, dst: ACES2065-1}\n"
        "  - !<ColorSpace>\n    name: Hidden\n    to_scene_reference:\n"
        "      !<GroupTransform>\n      children:\n"
        "        - !<MatrixTransform> {matrix: [0.4123908, 0.3575843, "
        "0.1804808, 0, 0.2126390, 0.7151687, 0.0721923, 0, 0.0193308, "
        "0.1191948, 0.9505322, 0, 0, 0, 0, 1]}\n"
        "        - !<BuiltinTransform> {style: UTILITY - "
        "ACES-AP0_to_CIE-XYZ-D65_BFD, direction: inverse}\n"));
    ColorConfig vars(strings);
    OIIO_CHECK_FALSE(vars.has_error());
    OIIO_CHECK_EQUAL(ColorSpaceInfoAccess::color_interop_id(
                         vars.derive_color_space_info("Plate")),
                     "lin_rec709_scene");
    for (string_view name : { "Plate", "$SHOT" }) {
        OIIO_CHECK_EQUAL(ColorSpaceInfoAccess::color_interop_id(
                             pvt::color_space_info(vars, name, true, "SHOT",
                                                   "acescg")),
                         "lin_ap1_scene");
        OIIO_CHECK_EQUAL(equality_id(vars, name, "SHOT", "acescg"),
                         "lin_ap1_scene");
        OIIO_CHECK_EQUAL(equality_id(vars, name, "UNUSED,SHOT", "x,acescg"),
                         "lin_ap1_scene");
    }
    // A variable only the queried name reads still selects its own result.
    OIIO_CHECK_EQUAL(ColorSpaceInfoAccess::color_interop_id(
                         pvt::color_space_info(vars, "$SHOT2", true, "SHOT2",
                                               "acescg")),
                     "lin_ap1_scene");
    Filesystem::remove_all(directory);
}



// Between readings a measurement cannot separate, the one declaring the
// space's encoding answers: srgb_p3d65_display and srgbe_p3d65_display share
// their operations and differ only in encoding.
static void
test_encoding_tie_break()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    for (string_view encoding : { "sdr-video", "hdr-video" }) {
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(
            filename,
            Strutil::fmt::format(
                "ocio_profile_version: 2.3\n"
                "roles: {{default: XYZ, cie_xyz_d65_interchange: XYZ,\n"
                "  aces_interchange: ACES}}\n"
                "file_rules:\n  - !<Rule> {{name: Default, colorspace: default}}\n"
                "colorspaces:\n  - !<ColorSpace> {{name: ACES}}\n"
                "display_colorspaces:\n  - !<ColorSpace> {{name: XYZ}}\n"
                "  - !<ColorSpace>\n    name: Panel\n    encoding: {}\n"
                "    from_display_reference: !<GroupTransform>\n"
                "      children:\n"
                "        - !<MatrixTransform> {{matrix: [2.49349691194143, "
                "-0.931383617919124, -0.402710784450717, 0, -0.829488969561575, "
                "1.76266406031835, 0.0236246858419436, 0, 0.0358458302437845, "
                "-0.0761723892680418, 0.956884524007688, 0, 0, 0, 0, 1]}}\n"
                "        - !<ExponentWithLinearTransform> {{gamma: 2.4, "
                "offset: 0.055, style: mirror, direction: inverse}}\n",
                encoding)));
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_EQUAL(ColorSpaceInfoAccess::color_interop_id(
                             config.derive_color_space_info("Panel")),
                         encoding == "hdr-video" ? "srgbe_p3d65_display"
                                                 : "srgb_p3d65_display");
    }
    Filesystem::remove(filename);
}



// The facts the library reads through the private access, for the built-in
// interop-identities config and for built-in identities a config does not
// define. Python sees only the public accessors.
static void
test_private_properties()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;
    using Access      = ColorSpaceInfoAccess;
    using Field       = ColorSpaceInfoField;
    using Kind        = ColorTransferFunctionKind;
    const bool ocio25 = ColorConfig::OpenColorIO_version_hex() >= 0x02050000;
    // OpenColorIO before 2.5 cannot read interop_id.
    std::string text;
    OIIO_CHECK_ASSERT(
        Filesystem::read_text_file(OIIO_COLOR_TEST_IDENTITIES, text));
    if (!ocio25) {
        std::string kept;
        for (string_view line : Strutil::splitsv(text, "\n"))
            if (!Strutil::starts_with(line, "    interop_id:"))
                kept += std::string(line) + "\n";
        text = kept;
    }
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(filename, text));
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        // Before derivation, the cheap query counts what the config declares
        // as computed: the image state its encoding names, and its ID.
        auto cheap = config.get_color_space_info("lin_rec2020_scene");
        OIIO_CHECK_ASSERT(Access::computed(cheap, Field::ImageState));
        OIIO_CHECK_EQUAL(Access::image_state(cheap), "scene");
        if (ocio25) {
            OIIO_CHECK_ASSERT(Access::computed(cheap, Field::ColorInteropID));
            OIIO_CHECK_FALSE(Access::derived(cheap, Field::ColorInteropID));
        }
        auto info = config.derive_color_space_info("lin_ap1_scene");
        OIIO_CHECK_ASSERT(Access::transfer_function_kind(info) == Kind::Linear);
        OIIO_CHECK_ASSERT(Access::transfer_function_name(info).empty());
        OIIO_CHECK_EQUAL(Access::equality_id(info), "lin_ap1_scene");
        OIIO_CHECK_EQUAL(Access::color_interop_id(info), "lin_ap1_scene");
        for (auto field : { Field::Chromaticities, Field::TransferFunction,
                            Field::EqualityID, Field::ColorInteropID,
                            Field::Encoding, Field::ImageState })
            OIIO_CHECK_ASSERT(Access::computed(info, field));
        OIIO_CHECK_ASSERT(Access::available(info, Field::ColorInteropID));
        OIIO_CHECK_EQUAL(Access::derived(info, Field::ColorInteropID), !ocio25);
        for (auto name :
             { "g18_rec709_scene", "g22_ap1_scene", "g24_rec709_scene" })
            OIIO_CHECK_ASSERT(Access::transfer_function_kind(
                                  config.derive_color_space_info(name))
                              == Kind::Power);
        // DCDM is a read-only input identity, reached by CICP 12/17 and never
        // won by measurement, so even its own definition measures as none.
        OIIO_CHECK_EQUAL(equality_id(config, "dcdm_p3d65_display"), "");
    }
    // Any valid ID names its image state by its suffix.
    for (auto [name, state] :
         { std::pair { "ocio:acescc_ap1_scene", "scene" },
           std::pair { "oiio:g22_p3d65_display", "display" } })
        OIIO_CHECK_EQUAL(Access::image_state(
                             ColorConfig(filename).derive_color_space_info(
                                 name)),
                         state);
    Filesystem::remove(filename);

    // A built-in identity the config does not define is described by the
    // built-in interop-identities config, named exactly (in any case).
    ColorConfig builtin("ocio://default");
    for (bool derive : { false, true })
        for (auto name :
             { "oiio:g24_rec2020_display", "OIIO:G24_REC2020_DISPLAY",
               "g22_adobergb_display", "oiio:lin_p3dci_display" })
            OIIO_CHECK_EQUAL(Access::color_interop_id(
                                 pvt::color_space_info(builtin, name, derive)),
                             Strutil::lower(name));
    OIIO_CHECK_ASSERT(
        Access::transfer_function_kind(
            builtin.derive_color_space_info("g22_adobergb_display"))
        == Kind::Power);

    // An is-unique space is never named by comparison with a reference
    // definition, but its separately measured curve and primaries may still
    // identify it, and a derived ID completes its image state.
    const std::string to_rec709
        = "!<MatrixTransform> {matrix: [2.52168618674388, -1.13413098823972, "
          "-0.387555198504164, 0, -0.276479914229922, 1.37271908766826, "
          "-0.096239173438334, 0, -0.0153780649660342, -0.152975335867399, "
          "1.16835340083343, 0, 0, 0, 0, 1]}";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        filename,
        "ocio_profile_version: 2.3\n"
        "roles: {aces_interchange: ACES2065-1, scene_linear: ACES2065-1, "
        "default: ACES2065-1}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n"
        "  - !<ColorSpace> {name: ACES2065-1, encoding: scene-linear}\n"
        "  - !<ColorSpace>\n    name: UniqueSRGB\n"
        "    categories: [is-unique]\n"
        "    from_scene_reference: !<GroupTransform> {children: ["
            + to_rec709
            + ", !<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055, "
              "direction: inverse}]}\n"
              "  - !<ColorSpace>\n    name: UniqueLinear\n"
              "    categories: [is-unique]\n"
              "    from_scene_reference: "
            + to_rec709 + "\n"));
    {
        ColorConfig unique(filename);
        OIIO_CHECK_FALSE(unique.has_error());
        auto srgb = unique.derive_color_space_info("UniqueSRGB");
        OIIO_CHECK_EQUAL(Access::equality_id(srgb), "");
        OIIO_CHECK_EQUAL(Access::color_interop_id(srgb), "srgb_rec709_scene");
        OIIO_CHECK_EQUAL(Access::image_state(srgb), "scene");
        auto linear = unique.derive_color_space_info("UniqueLinear");
        OIIO_CHECK_EQUAL(linear.chromaticities().size(), 8);
        OIIO_CHECK_EQUAL(Access::equality_id(linear), "");
        OIIO_CHECK_EQUAL(Access::color_interop_id(linear), "");
    }
    Filesystem::remove(filename);

    // A definition with a 3D LUT is not measured, even behind a FileTransform.
    const std::string directory = Filesystem::temp_directory_path() + "/"
                                  + Filesystem::unique_path();
    OIIO_CHECK_ASSERT(Filesystem::create_directory(directory));
    OIIO_CHECK_ASSERT(
        Filesystem::write_text_file(directory + "/identity3d.cube",
                                    "LUT_3D_SIZE 2\n0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
                                    "0 0 1\n1 0 1\n0 1 1\n1 1 1\n"));
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        directory + "/lut3d.ocio",
        "ocio_profile_version: 2.3\nsearch_path: .\n"
        "roles: {default: ACES2065-1, scene_linear: ACES2065-1, "
        "aces_interchange: ACES2065-1}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n"
        "  - !<ColorSpace> {name: ACES2065-1, encoding: scene-linear}\n"
        "  - !<ColorSpace>\n    name: Lut3DPlate\n"
        "    to_scene_reference: !<FileTransform> {src: identity3d.cube, "
        "interpolation: linear}\n"));
    {
        ColorConfig lut3d(directory + "/lut3d.ocio");
        OIIO_CHECK_FALSE(lut3d.has_error());
        OIIO_CHECK_EQUAL(equality_id(lut3d, "Lut3DPlate"), "");
    }

    // The ImageSpec helper answers as the default config does, with or
    // without an unused context.
    ImageSpec spec;
    spec.attribute("oiio:ColorSpace", "scene_linear");
    auto plain     = pvt::get_colorspace_info(spec, true);
    auto explicit_ = pvt::get_colorspace_info(spec, true, "SHOT", "rec2020");
    OIIO_CHECK_EQUAL(plain.valid(), explicit_.valid());
    OIIO_CHECK_EQUAL(plain.transfer_function_gamma(),
                     explicit_.transfer_function_gamma());
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

    test_colorspace_without_config();
    test_sRGB_conversion();
    test_Rec709_conversion();
    test_declared_color_space_info();
    test_gamma_pair_conversion();
    test_interop_id_memo();
    test_recognized_id_equivalence();
    test_measured_equality_id();
    test_declared_id_equivalence();
    test_color_space_info();
    test_color_space_info_concurrent();
    test_numeric_recognition();
    test_gamut_recognition();
    test_naming_versus_measurement();
    test_spi_conventions();
    test_color_space_info_context();
    test_encoding_tie_break();
    test_private_properties();

    return unit_test_failures != 0;
}
